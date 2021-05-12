// Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
// SPDX-License-Identifier: BSD-2-Clause

/*! @page components Components

RefOS embraces the component-based multi-server operating system design. RefOS's two main components are a process server and a fileserver. The process server runs as the initial thread, and it is responsible for starting the rest of the sysem. Other components of the system include clients, device drivers, file servers, terminal programs, tetris and snake.

@image html component_design.png "Figure 1 - RefOS Components"

@section procserv Process server
@image html procserv.png "Figure 2 - Process Server"

The process server is the most trusted component in the system. It runs as the initial kernel thread and does not depend on any other component (this avoids deadlock). The process server implementation is single-threaded. The process server also implements the dataspace interface for anonymous memory and acts as the memory manager.

The process server implements the following interfaces:

<ul>
    <li> Process server interface (naming, memory windows, processes and so on) </li>
    <li> Dataspace server interface (for anonymous memory) </li>
    <li> Name server interface (acts as the root name server) </li>
</ul>

<center>See code here: @ref apps/process_server/src/main.c </center>

@section fileserv File server
@image html fileserv.png "Figure 3 - File Server"

The file server is more trusted than clients, but it is less trusted than the process server (this avoids deadlock). In RefOS, the file server does not use a disk driver, and the actual file contents are compiled into the file server executable itself using a cpio archive. The file server acts as the main data server in the system.

The file server implements the following interfaces:

<ul>
    <li> Dataspace server interface (for stored file data) </li>
    <li> Server connection interface (for clients to connect to it) </li>
</ul>

<center>See code here: @ref apps/file_server/src/file_server.c </center>

@section conserv  Console Server
@image html conserv.png "Figure 4 - Console Server"

The console server provides serial and EGA input and output functionality, which is exposed through the dataspace interface. The console server also provides terminal emulation for EGA screen output.

The console server implements the following interfaces:

<ul>
    <li> Dataspace server interface (for serial input and output and EGA screen devices) </li>
    <li> Server connection interface (for clients to connect to it) </li>
</ul>

<center>See code here: @ref apps/console_server/src/console_server.c </center>

@section timeserv  Timer Server
@image html timeserv.png "Figure 5 - Timer Server"

The timer server provides timer get time and sleep functionality, which is exposed through the dataspace interface.

The timer server implements the following interfaces:

<ul>
    <li> Dataspace server interface (for timer devices) </li>
    <li> Server connection interface (for clients to connect to it) </li>
</ul>

<center>See code here: @ref apps/timer_server/src/timer_server.c </center>

*/
