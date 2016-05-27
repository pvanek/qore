#!/usr/bin/env qore

# set the exec-class option and give the class name in case the file is renamed
%exec-class HelloWorld

# an object-oriented, threaded hello world :-)
class HelloWorld
{
    constructor() {
        background $.output("Hello, world!");
    }
    private output($arg) {
        printf("%s\n", $arg);
    }
}
