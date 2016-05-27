#!/usr/bin/env qore

class Base1 {
    constructor($a) {
        printf("Base1::constructor(%s)\n", $a);
    }
    hello($from) {
        printf("Base1 hello from %n\n", $from);
    }
}

class Base2 {
    constructor($a) {
        printf("Base2::constructor(%s)\n", $a);
    }
    hello($from) {
        printf("Base2 hello from %n\n", $from);
    }
}

class Mid inherits Base1, Base2
{
    # here Mid gives explicit arguments to its base classes
    # note that class "Final" also gives explicit arguments to Base1 and Base2
    # constructors, which take precedence over these
    constructor($a) : Base1($a + " & Mid"), Base2($a + " & Mid") {
        printf("Mid::constructor(%s)\n", $a);
    }
    hello($from) {
        # mid calls base class methods with the same name here
        Base1::$.hello($from + " & Mid");
        Base2::$.hello($from + " & Mid");
        # the '%n' format specifier means "display the value", this will display
        # strings with quotes, integers, floats, dates, even hashes and lists on a single line
        # '%N' does the same but formats complex data structures on multiple lines
        # otherwise the *printf() functions take mostly standard C-style printf arguments
        printf("Mid hello from %n\n", $from);
    }
}

class Final inherits Mid, Base1, Base2
{
    # here Final gives explicit arguments to its base classes
    # the arguments to Base1 and Base2 constructors will take precedence over
    # the Base1 and Base2 class constructor arguments given by class Mid
    constructor($a) : Mid($a), Base1($a), Base2($a) {
        printf("Final::constructor(%s)\n", $a);
    }
    hello() {
        Mid::$.hello("Final");
        Base1::$.hello("Final");
        Base2::$.hello("Final");
        printf("Final hello\n");
    }
}

our $f = new Final("final");
$f.hello();

printf("goodbye!\n");
