/*! @file */

/*! @mainpage Code Documentation

@image html refos.png

<h1>Abstract</h1>

RefOS is a sample implementation of a simple multi-server operating system built on seL4.

The aim of RefOS is to:
<ul>
    <li>Provide a reference design of a simple multi-server operating system built on seL4</li>
    <li>Provide a sample implementation of how seL4 protocols can be used</li>
    <li>Simplify development and porting of userland programs built on seL4</li>
    <li>Serve as an example codebase to start new component-based seL4 projects</li>
</ul>

<h1>RefOS Features</h1>
<ul>
    <li>Simple and light-weight operating system</li>
    <li>Multi-server component based design</li>
    <li>Process and thread abstraction</li>
    <li>Dataspace abstraction</li>
    <li>Shared memory support</li>
    <li>Dynamic mmap and sbrk support</li>
    <li>Userland serial and timer drivers</li>
    <li>Sample user programs include:
        <ul>
            <li>Terminal - a simple terminal program</li>
            <li>Tetris - a port of Micro Tetris</li>
            <li>Snake - a snake game</li>
        </ul>
    </li>
</ul>

Please note that RefOS is intended to be only an example system, a work-in-progress, and is not to be deployed as a real operating sytem solution. RefOS is not verified, is not high assurance, is not optimised for speed and efficiency and may be missing most of the features one would expect in a deployable operating system environment.

<h1>Links</h1>
See page \ref components for a description of the RefOS components.<br>
See page \ref design for a RefOS design overview.<br>
See page \ref testing for RefOS testing information.

<p>
<i>What does the logo mean? Pluto and Charon.</i>

*/
