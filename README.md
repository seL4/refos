<!--
     Copyright 2014, NICTA

     This software may be distributed and modified according to the terms of
     the BSD 2-Clause license. Note that NO WARRANTY is provided.
     See "LICENSE_BSD2.txt" for details.

     @TAG(NICTA_BSD)
  -->

RefOS Repository
================

This is the git repository for RefOS.
This repository is meant to be used as part of a Google repo setup. Instead of cloning it directly,
please go to the following repository and follow the instructions there:

    https://github.com/seL4/refos-manifest

RefOS is currently supported on iMX3.1 KZM and iMX6 Sabre Lite hardware platforms, and
on qemu-ARM kzm.

Overview
--------

The repository is organised as follows.

 * [`impl/apps`](impl/apps/): RefOS system and userlevel applications
    * [`bootstrap`](impl/apps/bootstrap/): Bootstrap application, responsible for starting new
      userlevel processes
    * [`process_server`](impl/apps/process_server/): The process server, which runs as the root
      task and is responsible for providing process and thread abstraction, and also responsible
      for initialising the entire system.
    * [`file_server`](impl/apps/file_server/): The CPIO file server, which stores files and
      executables in a CPIO archive and exposes them via a dataspace interface.
    * [`console_server`](impl/apps/console_server/): The console server, a system process which acts
      as the console device driver, managing serial input / output and EGA text mode output.
    * [`timer_server`](impl/apps/timer_server/): The timer server, a userland driver process which
      manages the timer device, and provides gettime and sleep functionality.
    * [`terminal`](impl/apps/terminal/): The interactive terminal application.
    * [`test_os`](impl/apps/test_os/): RefOS OS-level test application, responsible for testing the
      OS environment.
    * [`test_user`](impl/apps/test_os/): RefOS user-level test application, responsible for testing
      the OS user environment.
    * [`snake`](impl/apps/snake/): Example snake game, as a demo.
    * [`tetris`](impl/apps/tetris/): Example tetris game, as a demo
    * [`nethack`](impl/apps/nethack/): Port of Nethack 3.4.3 roguelike game, as a demo.
 * [`impl/libs`](impl/libs/): RefOS system and userlevel applications
    * [`libdatastruct`](impl/libs/libdatastruct/): Provides simple C data structures such as
      vectors, hash tables, allocation tables...etc.
    * [`librefos`](impl/libs/librefos/): RefOS user / server shared definitions, RPC specs and
      generated stubs, low level helper libraries.
    * [`librefossys`](impl/libs/librefossys/): Implements some POSIX syscalls using low-level
      RefOS, for the C library to work. This is to simplify RefOS userlevel and make porting things
      easier.
 * [`impl/docs`](impl/docs/): RefOS Doxygen documentation.
 * [`design`](design/): The RefOS protocol design document.

License
-------

The files in this repository are released under standard open source
licenses. RefOS code is released under the BSD license where possible, and GPL for some
external software. Please see the individual file headers and
[`LICENSE_BSD2.txt`](LICENSE_BSD2.txt) for details.

Please note that RefOS is intended to be sample code and is not high assurance software.
We are not reponsible for any consequences if you choose to deploy this software in
production.
