#!/usr/bin/env qore

#
# test server. listens on any port and prints out command
#
# consists of 2 classes: the srv (server) and the cli (client)
#
# the script starts a server and binds to the port clisrv::port
# after this the client starts and transmits a message via
#   the connection.
# the server receives the message and prints it out.
# the client sends a 'close' for the server to terminate.
#
# by Wolfgang Ritzinger <aargon@rat.at>

namespace clisrv;

const clisrv::port=9988;
const clisrv::host="localhost";

# the great server
class clisrv::srv {
    private $.tCount;

    constructor() {
        # $.tCount will be a Counter object to count the number of threads 
        # running to ensure that the object stops cleanly without throwing 
        # exceptions (if the object were deleted while threads are still 
        # running then exceptions would be thrown)
        $.tCount = new Counter();
        $.run=True;
        $.socket=new Socket();
        printf("clisrv::srv: bind %s\n", clisrv::port);
        if(($.socket.bind(clisrv::port))==-1) { ## error in socket
            throw "ServerError", sprintf("could not bind to socket \"%s\": %s",
                                         clisrv::port,  strerror(errno()));
        }
        
        if($.socket.listen()) {
            throw "ClientError",
            sprintf("listen error on %s", $.socket);
        }

        # increment thread count for acceptor() thread
        $.tCount.inc();
        background $.acceptor();
    }

    destructor() {
        printf("clisrv::srv::destructor()\n");
    }

    private acceptor() {
        while(True) {
            # we check every 1/2 second if we need to exit
            my $rc = $.socket.isDataAvailable(500);
            # if we need to exit, then break out of the loop
            if (!$.run)
                break;
            # if no data is available, then check again
            if (!$rc)
                continue;

            printf("clisrv::srv::acceptor\n");
            
            my $req=$.socket.accept();
            # increment thread count for workerThread()
            $.tCount.inc();
            background $.workerThread($req);
        }
        # decrement thread count for acceptor() thread
        $.tCount.dec();
    }

    private workerThread($req) {
        #printf("clisrv::srv::workerThread() accepted connection from %s\n", $req.source);
        my $inbuff="";

        while(True) {
            # we read char-by-char to get the linebreak...
            my $in=$req.recv(1);

            if($in!="\n" && $in!="\0") {
                if($in!=-1) {
                    #printf("clisrv::srv -> %x\n", $in);
                    #printf("clisrv::srv: runloop got '%s'\n", $in);
                    $inbuff+=$in;
                }
                continue;
            }
            #printf("clisrv::srv: got a string ready :)\n");

            if($inbuff=="close") {
                printf("-> close received (%s)\n", $inbuff);
                $req.close();
                # flag exit
                $.run = False;
                break;
            }
            else {
                printf("-> '%s'\n", $inbuff);
            }

            # clear buffer
            $inbuff="";
        }
        #printf("clisrv::srv::workerThread done.\n");
        # decrement thread count for workerThread()
        $.tCount.dec();
    }

    shutdown() {
        printf("clisrv::srv::shutdown()\n");
        # wait until all threads have exited
        $.tCount.waitForZero();
        $.socket.close();
        delete $self;
    }
}

# the cool client
class clisrv::cli {
    constructor() {
        my $connectionpath=clisrv::host+":"+clisrv::port;
        $.socket=new Socket();
        printf("clisrv::cli: connect %s\n", clisrv::port);
        if(($.socket.connect($connectionpath))==-1) { ## error in socket
            throw "ClientError",
            sprintf("could not bind to socket \"%s\": %s",
                    clisrv::port,  strerror(errno()));
        }
    }

    destructor() {
        printf("clisrv::cli::destructor()\n");
    }

    send($str) {
        printf("clisrv::cli::send: sending '%s' with %d bytes\n",
               $str, strlen($str));
        #$.socket.send($str, strlen($str));
        $.socket.send($str+"\n");
    }

    close() {
        $.socket.close();
    }
}

########
# MAIN #
########

printf("generating server....\n");
$srv=new clisrv::srv();

printf("generating client....\n");
$cli=new clisrv::cli();
$cli.send("gugu");
$cli.send("hatscha");
$cli.send("close");

printf("closing client....\n");
$cli.close();

printf("closing server....\n");
$srv.shutdown();

# Eof #
