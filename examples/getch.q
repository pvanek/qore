#!/usr/bin/env qore

# this gives an example of how to set the terminal mode for
# reading a single character from stdin at a time without
# echoing it

# require all variables to be declared
%require-our

# require current version of qore
%requires qore >= 0.8.3

# enable all warnings
%enable-all-warnings

sub main() {
    my TermIOS $t();
    # get current terminal attributes for stdin
    stdin.getTerminalAttributes($t);

    # save a copy
    my TermIOS $orig = $t.copy();

    # restore terminal attributes on exit
    on_exit
        stdin.setTerminalAttributes(TCSADRAIN, $orig);

    # get local flags
    my int $lflag = $t.getLFlag();

    # disable canonical input mode (= turn on "raw" mode)
    $lflag &= ~ICANON;

    # turn off echo mode
    $lflag &= ~ECHO;

    # do not check for special input characters (INTR, QUIT, and SUSP)
    $lflag &= ~ISIG;

    # set the new local flags
    $t.setLFlag($lflag);

    # set minimum characters to return on a read
    $t.setCC(VMIN, 1);

    # set character input timer in 0.1 second increments (= no timer)
    $t.setCC(VTIME, 0);

    # make these terminal attributes active
    stdin.setTerminalAttributes(TCSADRAIN, $t);

    # print a message
    stdout.printf("Press any key: ");

    # print out dots every 20ms to show that we're waiting impatiently for data :-)
    while (!stdin.isDataAvailable(20ms)) {
        # print a dot to stdout 
        stdout.printf(".");
        # flush output buffers
        stdout.sync();
    }

    # read the character input
    my string $c = stdin.read(1);

    # output the character read
    stdout.printf(" GOT: ASCII 0x%02x (%d) '%s'\n", ord($c), ord($c), $c);
}

main();
