#!/usr/bin/env qore

%require-our

%requires qore >= 0.8

our float $g = 0;

class T {
    # declare public members
    public {
	int $.a = False;
    }

    constructor() {
	# type error in return statement
	return 0;
    }
    test() {
        $.copy("string");
    }
    copy(T $a) returns T {
        printf("a=%N\nargv=%N\n", $a, $argv);
    }
    t1(Mutex $m) returns float {
	my int $i;
	return $i;
    }
    private p(FtpClient $f) {
	# will result in a 'non-existent-method-call' warning
	$self.x();
    }
}

sub test(int $x, Condition $c) returns string {
}

sub t1(Mutex $m) {
}

class MyMutex inherits Mutex;
class OtherMutex inherits private Mutex;

sub main() {
    # ok
    our bool $ob = True;
    # ok
    our int $oi = False;
 
    # ok
    bool $oi = True;

    # error: uninitialized object
    our Mutex $om;

    # error
    Condition $om = 1;

    my int $x = 1;
    my bool $b = False;

    # type error, $m, no value errors: $c, $om, $mm;
    my (int $x1, Mutex $m, Mutex $m1, Thread::Condition $c, OtherMutex $om, MyMutex $mm) = (1, new OtherMutex(), new MyMutex());

    # type error: using a bool as a closure or call reference
    my int $cx = $oi(True, 1.1);

    # type error: $b is a bool
    $b.val = "string";
    # type error: $b is a bool
    $b{"val"} = "string";

    # OK
    t1($m);
    # error: argument type: class inherits required class privately
    t1($om);
    # OK
    t1($mm);

    # error
    my int $x = new Mutex();

    # type errors in list assignment
    my (int $i1, bool $b2) = (True, 1);

    # type error
    $b = 1;

    # type error
    $b++;
    ++$b;

    # parse exception: reference to undefined class
    my moon $p;

    # type error in return type, 1st argument
    my float $f = test($b, $c);
    # type errors in both arguments
    my string $s = test($b, $m);
    # type error in return type, 2nd argument
    $b = test(1, 2);
    # too few arguments to function
    $s = test(1);
    # return type error
    $s = index();

    # OK
    my T $t = new T();

    # error: parse error in member type
    $b = $t.a;

    # error: copy method with arguments
    my $t1 = $t.copy(1 + 1);
    # error: illegal external call to private method
    my $tw = $t.p();
    # error: type error in return type, missing argument
    my int $i1 = $t.t1();

    my hash $h;
    my list $l;

    # error: operation returns NOTHING
    $i1 = $h - $l;
    # error: operation returns NOTHING
    $i1 = NULL + NULL;
    # error: operation returns float
    $i1 = -$f;

    my null $n;
    # type error
    $n += 1;
    # type error
    $n -= 1;
    # type error
    $h *= 5.1;
    # type error
    $h /= 5.1;

    # invalid operation warning
    $b = $i1.a;
    # invalid operation warning
    $b = $i1[0];
    # invalid operation warning
    $b = keys $i1;
    # invalid operation warning
    $b = $h ? True : False;
    # invalid operation warning
    $b = shift $h;
    # invalid operation warning
    $b = pop $h;
    # invalid operation warning
    $b = push $h, True;

    # invalid type for splice operator
    splice $b, 0;
    # invalid type for unshift operator
    unshift $b, 0;
    # warning: invalid type for regex subst operator
    $b =~ s/1/2/g;
    # warning: invalid type for transliteration operator
    $b =~ tr/1/2/;
    # warning: invalid type for chomp operator
    chomp $b;
    # warning: invalid type for chomp operator
    chomp $b;

    my File $f = new File();
    # error: return type and missing required argument
    $b = $f.open();
    # error: return type
    $b = $f.getTerminalAttributes();
    # error: return type and missing required argument
    $b = $f.setTerminalAttributes();
    # error: missing required argument in constructor
    my AutoLock $al = new AutoLock();
}

main();
