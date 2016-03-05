# Pixley's Investigative Programming Repo

This repo is some personal side work I've done as R&D for work projects.  The code here can be viewed as examples for the various problems I've needed to solve, but are basic enough and generic enough that I feel these can be used as examples for folks trying to solve similar problems.

Work projects taken to mean the following:

* *Descent: Underground*

## async_ping.cpp

A first attempt at getting unprivileged (non-superuser/root) ping to work on *NIX systems.  A rather unwieldly implementation that runs the existing `ping` program and pipes its output for analysis.  It's not in a finished state and doesn't yet handle the situation where the target host is unreachable.  Confirmed to run and work on Mac OS X.

## async_ping2.cpp

Second attempt at unpriviledged ping, this time using the special case where `ICMP_ECHO` works over `SOCK_DGRAM`, as opposed to `SOCK_RAW`, which is restricted to root.  Runs with two threads, the parent thread polling for completion.  While it's not exactly necessary to do multithreading in this instance, it is necessary for I implement this in my real project.  Compiles and claims to successfully send a ping packet, but does not receive a reply, at least on Max OS X.  Further testing on Linux required.

## test_ping.cpp

A test program I found on a StackOverflow post that basically does what I want to do, albeit only on a single thread.  Suffers from the same send-but-no-reply issue as async_ping2 on Mac OS X.  This failure is what prompted me to shoot my work over to a Linux box.