<!--
     Copyright 2016, Data61
     Commonwealth Scientific and Industrial Research Organisation (CSIRO)
     ABN 41 687 119 230.

     This software may be distributed and modified according to the terms of
     the BSD 2-Clause license. Note that NO WARRANTY is provided.
     See "LICENSE_BSD2.txt" for details.

     @TAG(D61_BSD)
  -->

RefOS Repository
================

This is the git repository for RefOS.
This repository is meant to be used as part of a Google repo setup. Instead of cloning it directly,
please go to the following repository and follow the instructions there:

    https://github.com/seL4/refos-manifest

RefOS is currently supported on iMX3.1 KZM, iMX6 Sabre Lite and ia32 hardware platforms and
on qemu-ARM kzm and qemu-i386 ia32.

Overview
--------

The repository is organised as follows.

 * [`impl/apps`](impl/apps/): RefOS system and userland applications
    * [`selfloader`](impl/apps/selfloader/): Bootstrap application, which is responsible for starting
      user processes.
    * [`process_server`](impl/apps/process_server/): The process server, which runs as the root
      task and provides process and thread abstraction and initialises the entire system.
    * [`file_server`](impl/apps/file_server/): The cpio file server, which stores files and
      executables in a cpio archive and exposes them via a dataspace interface.
    * [`console_server`](impl/apps/console_server/): The console server, a system process which acts
      as the console device driver and manages serial input and output and EGA text mode output.
    * [`timer_server`](impl/apps/timer_server/): The timer server, a userland driver process which
      manages the timer device and provides timer get time and sleep functionality.
    * [`terminal`](impl/apps/terminal/): The interactive terminal application.
    * [`test_os`](impl/apps/test_os/): RefOS operating system level test suite, which tests the
      operating system environment.
    * [`test_user`](impl/apps/test_os/): RefOS user-level test application, which is responsible for
      testing the operating system user environment.
    * [`snake`](impl/apps/snake/): Example snake game.
    * [`tetris`](impl/apps/tetris/): Example tetris game.
    * [`nethack`](impl/apps/nethack/): Port of Nethack 3.4.3 roguelike game.
 * [`impl/libs`](impl/libs/): RefOS system and userland applications
    * [`libdatastruct`](impl/libs/libdatastruct/): RefOS library that provides simple C data structures such as
      vectors, hash tables and allocation tables.
    * [`librefos`](impl/libs/librefos/): RefOS user and server shared definitions, RPC specifications and
      generated stubs and low level helper libraries.
    * [`librefossys`](impl/libs/librefossys/): RefOS library that implements some POSIX system calls using low-level
      RefOS and thus allows the C library to work. This directory is intended to simplify RefOS 
      userland applications and facilitate porting.
 * [`impl/docs`](impl/docs/): RefOS doxygen code documentation.
 * [`design`](design/): RefOS protocol design document.

Suggested Future Work
---------------------

The following is suggested future work that interested open-source developers could implement:

 * Future Work 1: modify how the process server creates and starts processes and threads (see 'Future Work 1' in code)
 * Future Work 2: fix issue where calls to assert_fail() result in infinite recursion on x86 architecture (see 'Future Work 2' in code)
 * Future Work 3: modify how the selfloader bootstraps user processes (see 'Future Work 3' in code)
 * Future Work 4: remove explicit reference to system call table in processes that the process server creates (see 'Future Work 4' in code)
 * Future Work 5: set up muslc's errno in RefOS (see 'Future Work 5' in code)
 * Future Work 6: get ia32_screen_debug_defconfig and ia32_screen_release_defconfig default configurations running with new seL4 API
 * Future Work 7: get Nethack running with new seL4 API

License
-------

The files in this repository are released under standard open source
licenses. RefOS code is released under the BSD license where possible and GPL for some
external software. Please see the individual file headers and
[`LICENSE_BSD2.txt`](LICENSE_BSD2.txt) for details.

Please note that RefOS is intended to be sample code and is not high assurance software.
We are not reponsible for any consequences if you choose to deploy this software in
production.
