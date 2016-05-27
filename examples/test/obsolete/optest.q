#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args
%no-child-restrictions

%requires qore >= 0.7.2

hash opt; # global options

const opts = ( "verb" : "v,verbose",
	       "help" : "h,help" );

const infix = ( "+", "-", "*", "/", "&", "|", "&&", "||", "<", "<=", "==",
		"===", ">=", ">", "<=>", "%", "!=", "!==", ">>", "<<",
		".", "^" );

const assign = ( "=", "+=", "-=", "*=", "/=", "&=", "|=", "%=", "^=",
		 ">>=", "<<=" );

const lop = ( "!", "~", "-" );

const lv_pre_post = ( "++", "--" );

const lv = (
    "0" : ( "trim", "chomp", "pop", "shift" ),
    "1" : ( "unshift", "push", "splice" ),
    "2" : ( "splice" ),
    "3" : ( "splice" )
    );

const pre = (
    "1" : ( "elements", "exists", "keys" ),
    "2" : ( "map", "foldl", "foldr", "select" ),
    "3" : ( "map" )
    );

# strings that will be parsed to be all basic Qore types
const TypeHash = (
    "any-int": ("decl": "any", "val": "1"),
    #"any-bool": ("decl": "any", "val": "True"),
    #"any-float": ("decl": "any", "val": "50.45"),
    #"any-string": ("decl": "any", "val": '"hi"'),
    "any-list": ("decl": "any", "val": "(1,'two')"),
    #"any-hash": ("decl": "any", "val": "('a':1,'b':2)"),
    #"any-object": ("decl": "any", "val": "new Mutex()"),
    "any-nothing": ("decl": "any", "val": "NOTHING"),
    "NOTHING" : ("decl": "nothing", "val": 'NOTHING'),
    "integer" : ("decl": "int", "val": '1'),
    "float" : ("decl": "float", "val": '2.3'),
    "string" : ("decl": "string", "val": '"string"'),
    "date" : ("decl": "date", "val": '2008-12-03T10:09:14.123'),
    "boolean" : ("decl": "bool", "val": 'True'),
    "NULL" : ("decl": "null", "val": 'NULL'),
    "binary" : ("decl": "binary", "val": 'binary("hello")'),
    "list" : ("decl": "list", "val": '(1, "two", 3.0)'),
    "hash" : ("decl": "hash", "val": '( "a" : "value", "another" : 123 )'),
    "object" : ("decl": "object", "val": '(new Mutex())'),
    "NOTHING0" : ("decl": "string", "val": 'my nothing'),
    );

const TypeKeys = keys TypeHash;
const TypeValues = map TypeHash.$1.val, TypeKeys;
const TypeDecls = map TypeHash.$1.decl, TypeKeys;

sub eval(Program p, string name, string desc, string str) {
    try {
	if (opt.verb)
	    stdout.printf("%s: ", desc);
	p.parse(str, desc);
	p.callFunction(name);
	stdout.print(opt.verb ? "OK\n" : ".");
    }
    catch (hash ex) {
	if (opt.verb)
	    stdout.printf("INVALID (%s: %s) code: %s\n", ex.err, ex.desc, str);
	else
	    stdout.printf("X");
    }
}

sub infix_test() {
    if (!opt.verb)
	stdout.printf("infix tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;
    foreach string op in (infix) {
	foreach string t in (TypeKeys) {
	    int lp = $#;
	    foreach string t1 in (TypeKeys) {
		int rp = $#;
		string name = sprintf("test%d", cnt++);
		string desc = sprintf("infix: %s %s %s", t, op, t1);
		string str = sprintf("any sub %s(){my %s l=%s;my %s r=%s; return l %s r;}\n", name,
					 TypeDecls[lp], TypeValues[lp], TypeDecls[rp], TypeValues[rp], op);
		eval(p, name, desc, str);
	    }
	}
    }
    if (!opt.verb)
	stdout.printf("\n");
}

sub assign_test() {
    if (!opt.verb)
	stdout.printf("assignment tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;
    foreach string op in (assign) {
	foreach string t in (TypeKeys) {
	    int lp = $#;
	    foreach string t1 in (TypeKeys) {
		int rp = $#;
		string name = sprintf("test%d", cnt++);
		string desc = sprintf("assign: evaluating %s %s: ", op, t);
		string str = sprintf("any sub %s(){my %s v=%s;my %s x = %s; x %s v;}\n", name, TypeDecls[lp], TypeValues[lp], TypeDecls[rp], TypeValues[rp], op);
		eval(p, name, desc, str);
	    }
	}
    }
    if (!opt.verb)
	stdout.printf("\n");
}

sub lop_tests() {
    if (!opt.verb)
	stdout.printf("unary prefix operator tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;
    foreach string op in (lop) {
	foreach string t in (TypeKeys) {
	    string name = sprintf("test%d", cnt++);
	    string desc = sprintf("unary prefix: evaluating %s%s: ", op, t);
	    string str = sprintf("any sub %s(){my %s v=%s; return %sv;}\n", name, TypeDecls[$#], TypeValues[$#], op);
	    eval(p, name, desc, str);
	}
    }
    if (!opt.verb)
	stdout.printf("\n");
}

sub lv_pre_tests() {
    if (!opt.verb)
	stdout.printf("lvalue prefix operator tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;
    foreach string op in (lv_pre_post) {
	foreach string t in (TypeKeys) {
	    string name = sprintf("test%d", cnt++);
	    string desc = sprintf("lvalue prefix: evaluating %s%s: ", op, t);
	    string str = sprintf("any sub %s(){my %s v=%s; return %sv;}\n", name, TypeDecls[$#], TypeValues[$#], op);
	    eval(p, name, desc, str);
	}
    }
    if (!opt.verb)
	stdout.printf("\n");
}

sub lv_post_tests() {
    if (!opt.verb)
	stdout.printf("lvalue postfix operator tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;
    foreach string op in (lv_pre_post) {
	foreach string t in (TypeKeys) {
	    string name = sprintf("test%d", cnt++);
	    string desc = sprintf("lvalue postfix: evaluating %s%s: ", t, op);
	    string str = sprintf("any sub %s(){my %s v=%s; return v%s;}\n", name, TypeDecls[$#], TypeValues[$#], op);
	    eval(p, name, desc, str);
	}
    }
    if (!opt.verb)
	stdout.printf("\n");
}

sub lv_tests() {
    if (!opt.verb)
	stdout.printf("lvalue operator tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;

    foreach string num in (lv.keyIterator()) {
	int limit = int(num) + 1;

	foreach string op in (lv{num}) {
	    number max = pow(elements TypeKeys, limit);

	    list pm;
	    # calculate offsets
	    for (int i = 0; i < limit; ++i)
		pm[i] = pow(elements TypeKeys, i);

	    #printf("DEBUG: offsets: %N\n", pm);

	    for (int i = 0; i < max; ++i) {
		string str;
		string args;
		string dargs;
		for (int j = limit - 1; j >= 0; --j) {
		    # get each argument's position in the type hash
		    #printf("DEBUG: i=%n,j=%n: %n\n", i, j, pm[j]);
		    int x = (i / pm[j]) % (elements TypeKeys);
		    str += sprintf("my %s l%d=%s;", TypeDecls[x], j, TypeValues[x]);
		    args += sprintf("l%d, ", j);
		    dargs += sprintf("%s, ", TypeKeys[x]);
		}
		splice args, -2;
		splice dargs, -2;
		string name = sprintf("test%d", cnt++);
		string desc = sprintf("lvalue: evaluating %s, %s: ", op, dargs);
		str += sprintf("%s %s;", op, args);
		str = sprintf("any sub %s(){%s}\n", name, str);
		eval(p, name, desc, str);
	    }
	}
    }

    if (!opt.verb)
	stdout.printf("\n");
}

sub pre_tests() {
    if (!opt.verb)
	stdout.printf("pre operator tests: ");

    Program p(PO_NEW_STYLE);
    int cnt = 0;

    foreach string num in (pre.keyIterator()) {
	int limit = int(num);

	foreach string op in (pre{num}) {
	    number max = pow(elements TypeKeys, limit);

	    list pm;
	    # calculate offsets
	    for (int i = 0; i < limit; ++i)
		pm[i] = pow(elements TypeKeys, i);

	    for (int i = 0; i < max; ++i) {
		string str;
		string args;
		string dargs;
		for (int j = limit - 1; j >= 0; --j) {
		    # get each argument's position in the type hash
		    #printf("DEBUG: i=%n,j=%n: %n\n", i, j, pm[j]);
		    int x = (i / pm[j]) % (elements TypeKeys);
		    str += sprintf("my %s l%d=%s;", TypeDecls[x], j, TypeValues[x]);
		    args += sprintf("l%d, ", j);
		    dargs += sprintf("%s, ", TypeKeys[x]);
		}
		splice args, -2;
		splice dargs, -2;
		string name = sprintf("test%d", cnt++);
		string desc = sprintf("lvalue: evaluating %s, %s: ", op, dargs);
		str += sprintf("return %s %s;", op, args);
		str = sprintf("any sub %s(){%s}\n", name, str);
		eval(p, name, desc, str);
	    }
	}
    }

    if (!opt.verb)
	stdout.printf("\n");
}

sub main() {
    GetOpt g(opts);
    try
	opt = g.parse2(\ARGV);
    catch (ex) {
	printf("option error: %s\n", ex.desc);
        exit(1);
    }

    infix_test();
    assign_test();
    lop_tests();
    lv_pre_tests();
    lv_post_tests();
    lv_tests();
    pre_tests();
}

main();
