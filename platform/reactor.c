/* Runtime infrastructure for the non-threaded version of the C target of Lingua Franca. */

/*************
Copyright (c) 2019, The University of California at Berkeley.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************/

/** Runtime infrastructure for the non-threaded version of the C target
 *  of Lingua Franca.
 *  
 *  @author{Edward A. Lee <eal@berkeley.edu>}
 *  @author{Marten Lohstroh <marten@berkeley.edu>}
 *  @author{Soroush Bateni <soroush@utdallas.edu>}
 */

#include "reactor_common.c"
#include "lf_platform.h"
#include <signal.h> // To trap ctrl-c and invoke termination().
//#include <assert.h>

/**
 * @brief Queue of triggered reactions at the current tag.
 * 
 */
pqueue_t* reaction_q;

/**
 * Unless the "fast" option is given, an LF program will wait until
 * physical time matches logical time before handling an event with
 * a given logical time. The amount of time is less than this given
 * threshold, then no wait will occur. The purpose of this is
 * to prevent unnecessary delays caused by simply setting up and
 * performing the wait.
 */
#define MIN_SLEEP_DURATION USEC(10) // FIXME: https://github.com/lf-lang/reactor-c/issues/109

/**
 * Mark the given port's is_present field as true. This is_present field
 * will later be cleaned up by _lf_start_time_step.
 * @param port A pointer to the port struct.
 */
void _lf_set_present(lf_port_base_t* port) {
	bool* is_present_field = &port->is_present;
    if (_lf_is_present_fields_abbreviated_size < _lf_is_present_fields_size) {
        _lf_is_present_fields_abbreviated[_lf_is_present_fields_abbreviated_size]
            = is_present_field;
    }
    _lf_is_present_fields_abbreviated_size++;
    *is_present_field = true;

    // Support for sparse destination multiports.
    if(port->sparse_record
    		&& port->destination_channel >= 0
			&& port->sparse_record->size >= 0) {
    	size_t next = port->sparse_record->size++;
    	if (next >= port->sparse_record->capacity) {
    		// Buffer is full. Have to revert to the classic iteration.
    		port->sparse_record->size = -1;
    	} else {
    		port->sparse_record->present_channels[next]
				  = port->destination_channel;
    	}
    }
}

/**
 * Wait until physical time matches the given logical time or the time of a 
 * concurrently scheduled physical action, which might be earlier than the 
 * requested logical time.
 * @return 0 if the wait was completed, -1 if it was skipped or interrupted.
 */ 
int wait_until(instant_t wakeup_time) {
    if (!fast) {
        LF_PRINT_LOG("Waiting for elapsed logical time " PRINTF_TIME ".", wakeup_time - start_time);
        interval_t sleep_duration = wakeup_time - lf_time_physical();
    
        if (sleep_duration <= 0) {
            return 0;
        } else if (sleep_duration < MIN_SLEEP_DURATION) {
            // This is a temporary fix. FIXME: factor this out into platform API function.
            // Issue: https://github.com/lf-lang/reactor-c/issues/109
            // What really should be done on embedded platforms:
            // - compute target time
            // - disable interrupts
            // - read current time
            // - compute shortest distance between target time and current time 
            // - shortest distance should be positive and at least 2 ticks(us)
            LF_PRINT_DEBUG("Wait time " PRINTF_TIME " is less than MIN_SLEEP_DURATION %lld. Skipping wait.",
                sleep_duration, MIN_SLEEP_DURATION);
            return -1;
        }
        LF_PRINT_DEBUG("Going to sleep for %"PRId64" ns\n", sleep_duration);
        return lf_sleep_until(wakeup_time);
    }
    return 0;
}

void lf_print_snapshot() {
    if(LOG_LEVEL > LOG_LEVEL_LOG) {
        LF_PRINT_DEBUG(">>> START Snapshot");
        pqueue_dump(reaction_q, reaction_q->prt);
        LF_PRINT_DEBUG(">>> END Snapshot");
    }
}

/**
 * Trigger 'reaction'.
 * 
 * @param reaction The reaction.
 * @param worker_number The ID of the worker that is making this call. 0 should be
 *  used if there is only one worker (e.g., when the program is using the
 *  unthreaded C runtime). -1 is used for an anonymous call in a context where a
 *  worker number does not make sense (e.g., the caller is not a worker thread).
 */
void _lf_trigger_reaction(reaction_t* reaction, int worker_number) {
#ifdef MODAL_REACTORS
    // Check if reaction is disabled by mode inactivity
    if (!_lf_mode_is_active(reaction->mode)) {
        LF_PRINT_DEBUG("Suppressing downstream reaction %s due inactivity of mode %s.", reaction->name, reaction->mode->name);
        return; // Suppress reaction by preventing entering reaction queue
    }
#endif
    // Do not enqueue this reaction twice.
    if (reaction->status == inactive) {
        LF_PRINT_DEBUG("Enqueing downstream reaction %s, which has level %lld.",
        		reaction->name, reaction->index & 0xffffLL);
        reaction->status = queued;
        pqueue_insert(reaction_q, reaction);
    }
}

/**
 * Execute all the reactions in the reaction queue at the current tag.
 * 
 * @return Returns 1 if the execution should continue and 0 if the execution
 *  should stop.
 */
int _lf_do_step(void) {
    // Invoke reactions.
    while(pqueue_size(reaction_q) > 0) {
        // lf_print_snapshot();
        reaction_t* reaction = (reaction_t*)pqueue_pop(reaction_q);
        reaction->status = running;
        
        LF_PRINT_LOG("Invoking reaction %s at elapsed logical tag " PRINTF_TAG ".",
        		reaction->name,
                current_tag.time - start_time, current_tag.microstep);

        bool violation = false;

        // FIXME: These comments look outdated. We may need to update them.
        // If the reaction has a deadline, compare to current physical time
        // and invoke the deadline violation reaction instead of the reaction function
        // if a violation has occurred. Note that the violation reaction will be invoked
        // at most once per logical time value. If the violation reaction triggers the
        // same reaction at the current time value, even if at a future superdense time,
        // then the reaction will be invoked and the violation reaction will not be invoked again.
        if (reaction->deadline >= 0LL) {
            // Get the current physical time.
            instant_t physical_time = lf_time_physical();
            // FIXME: These comments look outdated. We may need to update them.
            // Check for deadline violation.
            // There are currently two distinct deadline mechanisms:
            // local deadlines are defined with the reaction;
            // container deadlines are defined in the container.
            // They can have different deadlines, so we have to check both.
            // Handle the local deadline first.
            if (reaction->deadline == 0 || physical_time > current_tag.time + reaction->deadline) {
                LF_PRINT_LOG("Deadline violation. Invoking deadline handler.");
                // Deadline violation has occurred.
                violation = true;
                // Invoke the local handler, if there is one.
                reaction_function_t handler = reaction->deadline_violation_handler;
                if (handler != NULL) {
                    (*handler)(reaction->self);
                    // If the reaction produced outputs, put the resulting
                    // triggered reactions into the queue.
                    schedule_output_reactions(reaction, 0);
                }
            }
        }
        
        if (!violation) {
            // Invoke the reaction function.
            _lf_invoke_reaction(reaction, 0);   // 0 indicates unthreaded.

            // If the reaction produced outputs, put the resulting triggered
            // reactions into the queue.
            schedule_output_reactions(reaction, 0);
        }
        // There cannot be any subsequent events that trigger this reaction at the
        //  current tag, so it is safe to conclude that it is now inactive.
        reaction->status = inactive;
    }
    
#ifdef MODAL_REACTORS
    // At the end of the step, perform mode transitions
    _lf_handle_mode_changes();
#endif

    // No more reactions should be blocked at this point.
    //assert(pqueue_size(blocked_q) == 0);

    if (lf_tag_compare(current_tag, stop_tag) >= 0) {
        return 0;
    }

    return 1;
}

// Wait until physical time matches or exceeds the time of the least tag
// on the event queue. If there is no event in the queue, return 0.
// After this wait, advance current_tag.time to match
// this tag. Then pop the next event(s) from the
// event queue that all have the same tag, and extract from those events
// the reactions that are to be invoked at this logical time.
// Sort those reactions by index (determined by a topological sort)
// and then execute the reactions in order. Each reaction may produce
// outputs, which places additional reactions into the index-ordered
// priority queue. All of those will also be executed in order of indices.
// If the -timeout option has been given on the command line, then return
// 0 when the logical time duration matches the specified duration.
// Also return 0 if there are no more events in the queue and
// the keepalive command-line option has not been given.
// Otherwise, return 1.
int next(void) {
    // Enter the critical section and do not leave until we have
    // determined which tag to commit to and start invoking reactions for.
    lf_critical_section_enter();
    event_t* event = (event_t*)pqueue_peek(event_q);
    //pqueue_dump(event_q, event_q->prt);
    // If there is no next event and -keepalive has been specified
    // on the command line, then we will wait the maximum time possible.
    tag_t next_tag = FOREVER_TAG_INITIALIZER;
    if (event == NULL) {
        // No event in the queue.
        if (!keepalive_specified) {
            _lf_set_stop_tag(
                (tag_t){.time=current_tag.time, .microstep=current_tag.microstep+1}
            );
        }
    } else {
        next_tag.time = event->time;
        // Deduce the microstep
        if (next_tag.time == current_tag.time) {
            next_tag.microstep = lf_tag().microstep + 1;
        } else {
            next_tag.microstep = 0;
        }
        if (_lf_is_tag_after_stop_tag(next_tag)) {
            // Cannot process events after the stop tag.
            next_tag = stop_tag;
        }
    }
    
    // Wait until physical time >= event.time.
    int finished_sleep = wait_until(next_tag.time);
    LF_PRINT_LOG("Next event (elapsed) time is " PRINTF_TIME ".", next_tag.time - start_time);
    if (finished_sleep != 0) {
        LF_PRINT_DEBUG("***** wait_until was interrupted.");
        // Sleep was interrupted. This could happen when a physical action
        // gets scheduled from an interrupt service routine.
        // In this case, check the event queue again to make sure to
        // advance time to the correct tag.
        lf_critical_section_exit();
        return 1;
    }
    // Advance current time to match that of the first event on the queue.
    // We can now leave the critical section. Any events that will be added
    // to the queue asynchronously will have a later tag than the current one.
    _lf_advance_logical_time(next_tag.time);
    lf_critical_section_exit();
    
    // Trigger shutdown reactions if appropriate.
    if (lf_tag_compare(current_tag, stop_tag) >= 0) {        
        _lf_trigger_shutdown_reactions();
    }

    // Invoke code that must execute before starting a new logical time round,
    // such as initializing outputs to be absent.
    _lf_start_time_step();
    
    // Pop all events from event_q with timestamp equal to current_tag.time,
    // extract all the reactions triggered by these events, and
    // stick them into the reaction queue.
    lf_critical_section_enter();
    _lf_pop_events();
    lf_critical_section_exit();

    return _lf_do_step();
}

/**
 * Stop execution at the conclusion of the next microstep.
 */
void lf_request_stop() {
	tag_t new_stop_tag;
	new_stop_tag.time = current_tag.time;
	new_stop_tag.microstep = current_tag.microstep + 1;
	_lf_set_stop_tag(new_stop_tag);
}

/**
 * Return false.
 * @param reaction The reaction.
 */
bool _lf_is_blocked_by_executing_reaction(void) {
    return false;
}

/**
 * The main loop of the LF program.
 * 
 * An unambiguous function name that can be called
 * by external libraries.
 * 
 * Note: In target languages that use the C core library,
 * there should be an unambiguous way to execute the LF
 * program's main function that will not conflict with
 * other main functions that might get resolved and linked
 * at compile time.
 */
int lf_reactor_c_main(int argc, const char* argv[]) {
    // Invoke the function that optionally provides default command-line options.
    _lf_set_default_command_line_options();

    LF_PRINT_DEBUG("Processing command line arguments.");
    if (process_args(default_argc, default_argv)
            && process_args(argc, argv)) {
        LF_PRINT_DEBUG("Processed command line arguments.");
        LF_PRINT_DEBUG("Registering the termination function.");
        #ifndef LF_TARGET_EMBEDDED
        if (atexit(termination) != 0) {
            lf_print_warning("Failed to register termination function!");
        }
        // The above handles only "normal" termination (via a call to exit).
        // As a consequence, we need to also trap ctrl-C, which issues a SIGINT,
        // and cause it to call exit.
        // We wrap this statement since certain Arduino flavors don't support signals.
        //  ^ Does any "flavors" of Arduino support signals? I dont think so. It is an OS concept
        //  I dont think any embedded targets should support aborting execution through a signal
        signal(SIGINT, exit);
        #endif
        LF_PRINT_DEBUG("Initializing.");
        initialize(); // Sets start_time.
#ifdef MODAL_REACTORS
        // Set up modal infrastructure
        _lf_initialize_modes();
#endif

        // Reaction queue ordered first by deadline, then by level.
        // The index of the reaction holds the deadline in the 48 most significant bits,
        // the level in the 16 least significant bits.
        reaction_q = pqueue_init(INITIAL_REACT_QUEUE_SIZE, in_reverse_order, get_reaction_index,
                get_reaction_position, set_reaction_position, reaction_matches, print_reaction);
                
        current_tag = (tag_t){.time = start_time, .microstep = 0u};
        _lf_execution_started = true;
        _lf_trigger_startup_reactions();
        _lf_initialize_timers(); 
        // If the stop_tag is (0,0), also insert the shutdown
        // reactions. This can only happen if the timeout time
        // was set to 0.
        if (lf_tag_compare(current_tag, stop_tag) >= 0) {
            _lf_trigger_shutdown_reactions();
        }
        LF_PRINT_DEBUG("Running the program's main loop.");
        // Handle reactions triggered at time (T,m).
        if (_lf_do_step()) {
            while (next() != 0);
        }
        // pqueue_free(reaction_q); FIXME: This might be causing weird memory errors
        return 0;
    } else {
        return -1;
    }
}
