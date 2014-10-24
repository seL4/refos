/*! @file */

/*! @page design Design Overview

@section Dataspace

The design revolves around the abstraction of dataspace, and the interfaces needed to manage them
and "back" them with data. 

The concept of a dataspace is a just stream of bytes; analogous to a unix file. 
A dataspace may represent thing such as:
<ul>
    <li> A physical file on disk (e.g. unix /bin/sh). </li>
    <li> A hardware device (e.g. unix /dev/hda1). </li>
    <li> A bit of anonymous memory (i.e. physical RAM). </li>
    <li> Any stream of bytes (e.g. unix /dev/urandom). </li>
    <li> ...etc. </li>
</ul>

@image html dataspace_example.png "Fig. 1 - Dataspace example."

Dataspace servers (i.e. dataservers) implement the dataspace protocol interface, and provide a
dataspace service to its clients. In the example given in <i>fig. 1</i> diagram, a client has a
portion of dataspace A mapped into its memory window (region of client's virtual memory). When the
client reads into the addresses within the window, the client will effectively read the coresponding
content in dataspace A. Dataspace A may then itself be initialised by the contents of another
dataspace, served by another dataserver.

@section Interfaces

The two main interfaces are process server and data server. In addition to these two main
interfaces, there are also some other smaller interfaces which provide extra features (such as
name server interface).

<h3>Process server interface</h3>

The main function of the process server interface is to provide process abstraction.
The process server interface provides methods for:
<ul>
    <li> Starting, ending and managing of threads & processes. </li>
    <li> Memory window abstraction. (creation, deletion..etc) </li>
    <li> Kernel page fault handling & delegation. </li>
</ul>

<h3>Data server interface</h3>

The main function of the data server itnerface is to provide dataspace abstraction.
The data server interface provides methods for:
<ul>
    <li> Creation, deletion and management of dataspaces. </li>
    <li> Accessing the contents of the dataspace by mapping it to a memory window. </li>
    <li> Content initiation by another dataspace and of another dataspace. </li>
    <li> Pager service for memory windows mapped to the dataspace. </li>
</ul>

@section Bootup

@image html startup.png "Fig. 3 - Initial startup."

The process server is started first, and initialises itself. Once the process server is up and 
ready, it starts the file server using its own internal elf-loader, and then starts up the
selfloader. (see Fig.3) 

@image html elfload.png "Fig. 4 - ELF Loading."

The selfloader is a small process which the process server spawns and runs in the same address space
as the process it is starting. It establishes connection to the file server, reads the elf
executable headers and then creates the corresponding memory windows mapped to the correct bytes of
the file content served by the file server, and also sets up a special memory window for the stack
pointer. After everything is all set up, the selfloader  Then jumps to the entry point of the
executable, which will cause a page fault on the first instruction read. (see Fig.4)

@image html fault.png "Fig. 5 - Fault handling."

When a page fault happens in the client program, the process server is the thread which is set up
to recieve the fault message from the kernel. The process server then looks up its book keeping, and
since the selfloader should have set up the memory windows correctly, the process server will see
that the window that the client faulted in is data mapped to one of the process server's own 
dataspaces (representing anonymous memory) which is content-initialised by an external file server.

The process server will then notify the file server, which will pick up the fault event, handle it,
and reply via IPC providing the contents that the process server asked for with its notification.
The process server then copies the content to its own RAM dataspace, and then replies back to the
faulting message, which tells the kernel to resume the client. (see Fig.5)

@section Dispatcher

The RefOS servers uses a dispatcher design to handle incoming IPC messages.
    
@image html dispatcher.png "Fig.1 - Dispatcher design overview."

For example, an incoming message arrives at the endpoint. It is first passed onto dispatcher 1,
who may decide that this is not the correct type of message for it to handle, so it passes
on the message onto the next dispatcher, who may pass it on to the 3rd dispatcher. The 3rd
dispatcher may for example, decide that it is the correct dispatcher for this message, and then
handles it accordingly by calling the appropriate handle function depending on the contents
of the message. Since the correct dispatcher for this type of message has been found, the
message is now processed and it is not passed onto the 4th dispatcher.
*/