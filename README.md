# Porting Lingua Franca to XMOS

## Prerequisits
- XTC v15.1.4 (see installation guide here: https://www.xmos.ai/documentation/XM-014363-PC-5/html/tools-guide/index.html)
- XTC tools on the path:
```
$ xcc --version
xcc: Build 14-f1fa60c, Jan-13-2022
XTC version: 15.1.4
Copyright (C) XMOS Limited 2008-2021. All Rights Reserved.

```
- LFC compiler built from source at a771294763f56e686fbe0f9ef16ec40dad40c3a9. Check out here https://github.com/lf-lang/lingua-franca
```
~/tools/lingua-franca/bin/lfc --version
lfc 0.3.1-SNAPSHOT
```


## Brief intro
XCore is a commercial PRET machine by XMOS. It delivers predictable timing with hardware multithreading. 
I have implemented both the single-threaded and the multi-threaded API here.
Still some unresolved issues relating to appropriate stack-sizes and how to deal with Physical actions.

This has only been tested using the XCore cycle accurate simulator "xsim"


## Quick start
1. Use LFC to code-generate a C file and also build the resulting project into a XMOS binary XE
```
~/tools/lingua-franca/bin/lfc src/HelloWorld.lf
```

2. Run XCore simulator on the program.
```
xsim bin/HelloWorld.xe
```