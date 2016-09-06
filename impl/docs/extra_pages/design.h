/*! @file */

/*! @page design Design Overview

@section Dataspace

The overall design of RefOS revolves around the abstraction of a dataspace and the interfaces needed to manage them.

A dataspace is a memory space and is represented by a stream of bytes. Dataspaces may represent entities such as:
<ul>
    <li> A physical file on disk (e.g. unix /bin/sh) </li>
    <li> A hardware device (e.g. unix /dev/hda1) </li>
    <li> A bit of anonymous memory (e.g. physical RAM) </li>
    <li> A stream of bytes (e.g. unix /dev/urandom) </li>
</ul>

@image html dataspace_example.png "Figure 1 - Example Dataspace Setup"

Dataspace servers (dataservers) implement the dataspace protocol interface and provide a dataspace abstraction service to clients. For example, in Figure 1, a portion of dataspace A is mapped into a client's memory window (client's virtual memory). When the client reads this memory, the client effectively reads the content in dataspace A. Another aspect of this figure is that dataspace A could initialise dataspace B with dataspace A's contents.

@section Interfaces

The two main interfaces in RefOS are the process server and the data server. In addition to these two interfaces, there are a number of less major (but still important) interfaces, such as the name server interface, that provide additional functionality.

<h3>Process Server Interface</h3>

The process server interface creates process abstractions. The process server interface provides methods for:
<ul>
    <li> Starting, ending and managing processes and threads </li>
    <li> Creating, deleting and managing memory windows </li>
    <li> Handling and delegating kernel page faults </li>
</ul>

<h3>Data Server Interface</h3>

The main goal of the data server interface is to provide dataspace abstraction. The data server interface provides methods for:
<ul>
    <li> Creating, deleting and managing dataspaces </li>
    <li> Mapping dataspaces to memory windows and thus allowing access to the contents of dataspaces </li>
    <li> Initialising dataspaces with the contents of other dataspaces </li>
    <li> Establishing pager services to map memory frames for memory windows mapped to dataspaces </li>
</ul>

@section Startup
@image html startup.png "Figure 2 - RefOS Boot Protocol"

In the RefOS boot protocol, the process server is started and initialises itself. Once the process server is running, the process server uses its interal ELF loader to start the file server and then starts the selfloader.

@image html elfload.png "Figure 3 - ELF Loading"

The selfloader is a process that the process server spawns, and it is used for starting user processes. The selfloader runs in the same address space as the process it is starting. The selfloader establishes a connection to the file server, reads the ELF executable headers and then creates memory windows mapped to the bytes of the file content served by the file server. The selfloader also sets up a memory window for the stack pointer. After performing these tasks, the selfloader jumps to the entry point of the executable which causes a page fault on the first instruction read.

@section page_faults Page Faults
@image html fault.png "Figure 4 - Page Fault Handling"

When a page fault occurs in a client program, the process server receives a page fault notification from the kernel. Since the selfloader initialised the memory windows for the client, the window that the client faulted in is data mapped to one of the process server's own dataspaces (representing anonymous memory).

The process server then notifies the file server that a page fault occurred. Having received the notifiction, the file server registers the fault event, handles the fault event and replies via interprocess communication. The file server provides any data that the process server asked for in its notification such as the window capability, the window offset and the memory page address.

The process server then copies the contents of the memory page to its own RAM dataspace and replies to the faulting message with the contents of the memory page, which causes the client to resume execution.

@section Dispatcher

RefOS's servers use a dispatcher algorithm to handle messages that they receive.
    
@image html dispatcher.png "Figure 5 - Dispatcher Design Overview"

The best way to illustrate the RefOS dispatcher algorithm is through an example. Say a server receives a message. The message is analysed by dispatcher 1. If dispatcher 1 determines that the message is the type of message that it handles, dispatcher 1 handles the message. Otherwise, dispatcher 1 passes the message onto dispatcher 2. If dispatcher 1 passes the message onto dispatcher 2, dispatcher 2 also analyses the message and determines whether the message is the type of message that it handles. Just like dispatcher 1, dispatcher 2 either handles the message or passes the message onto dispatcher 3. This algorithm is repeated until the final dispatcher if necessary. If a dispatcher determines that a message is the correct message type for it to handle, the dispatcher handles the message by calling the appropriate handler function depending on the message contents. Of course, if a dispatcher handles a message, the message is now processed and is not passed onto the next dispatcher. If the final dispatcher determines that the message is not the correct message type for it to handle, the server produces an error message.

Note that in implementation each server does not necessarily have exactly four dispactchers.
*/
