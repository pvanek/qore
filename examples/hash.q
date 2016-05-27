#!/usr/bin/env qore

#
# simple test program to demonstrate the basic use of
# hashes in qore
#
# by Wolfgang.Ritzinger <aargon@rat.at>

$h={};

$h.gugu="hatscha";
$h.cool="cool";

printf("gugu: %s cool: %s\n", $h.gugu, $h.cool);

foreach $a in ( keys $h )
    printf("A %s -> %s\n", $a, $h.$a);

