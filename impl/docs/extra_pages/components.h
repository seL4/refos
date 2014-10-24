
/*! @page components Components

RefOS embraces the component-based multi-server OS design. The two main components are process
server and fileserver. The process server runs as the initial thread, and is responsible for
starting up the rest of the sysem. Other components of the system can be anything from clients,
device drivers, file servers, terminal programs, tetris ...etc.

@image html component_design.png "Fig. 2 - Components concept."

@section procserv Process server
@image html procserv.png

The process server is the most trusted component in the system. It runs as the initial kernel thread
and does not depend on any other component (this avoids deadlock). The process server implementation
is single-threaded. 
The process server also implements the dataspace interface for anonymous memory, and acts as
the memory manager.

The process server implements the following interfaces:

<ul>
    <li> Process server interface. (naming, mem. windows, processes..etc) </li>
    <li> Dataspace server interface. (For anonymous memory) </li>
    <li> Name server interface (Acts as the root name server) </li>
</ul>

<center><h2>See code here: @ref main.c</h2></center>

@section fileserv CPIO File server
@image html fileserv.png

The cpio file server is more trusted than clients, but less trusted than the process server (avoids
deadlock). The current implementation does not use a disk driver, and the actual file contents are
compiled into the file server executable itself using a CPIO archive.

The cpio file server implements the following interfaces:

<ul>
    <li> Dataspace server interface. (for stored file data) </li>
    <li> Server connection interface. (for clients to connect to it) </li>
</ul>

<center><h2>See code here: @ref file_server.c</h2></center>

@section conserv  Console Server
@image html conserv.png

The Console server provides serial / EGA input & output functionality, exposed through
the dataspace interface. It also provides terminal emulation for EGA screen output.

The Console server implements the following interfaces:

<ul>
    <li> Dataspace server interface. (for serial input / output, and EGA screen devices) </li>
    <li> Server connection interface. (for clients to connect to it) </li>
</ul>

<center><h2>See code here: @ref console_server.c</h2></center>

@section timeserv  Timer Server
@image html timeserv.png

The Timer server provides timer gettime & sleep functionality, exposed through
the dataspace interface.

The Timer server implements the following interfaces:

<ul>
    <li> Dataspace server interface. (for timer devices) </li>
    <li> Server connection interface. (for clients to connect to it) </li>
</ul>

<center><h2>See code here: @ref timer_server.c</h2></center>

*/