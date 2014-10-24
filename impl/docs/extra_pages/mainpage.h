/*! @file */

/*! @mainpage Code Documentation

@image html refos.png

<h1>Abstract</h1>

RefOS is a sample implementation of a simple multi-server OS & protocol on seL4.

The aim of RefOS is to:
<ul>
    <li>Provide reference design of a simple multi-server OS protocol on seL4.</li>
    <li>Provide a sample implementation of the protocol.</li>
    <li>Simplify development & porting of userland programs.</li>
    <li>Serve as an example codebase to start new component-based seL4 projects.</li>
</ul>

<h1>Features</h1>
<ul>
    <li>Simple and light-weight OS.</li>
    <li>Multi-server component based OS design.</li>
    <li>Process & thread abstraction.</li>
    <li>Dataspace abstraction.</li>
    <li>Shared memory support.</li>
    <li>Dynamic mmap and sbrk support.</li>
    <li>User-land serial & timer drivers.</li>
    <li>Well documented, Doxygen supported.</li>
    <li> Sample user programs included:
        <ul>
            <li>Terminal - a small simple terminal program.</li>
            <li>Tetris - a port of Micro Tetris.</li>
            <li>Snake - example snake game.</li>
        </ul>
    </li>
</ul>

Please note that RefOS is currently only an example system, a work-in-progress, and is not to be
deployed as a real OS solution. RefOS is not verified, may not be optimised for speed and
efficiency, and may be missing most of the features you'll expect in a deployable OS environment.

<h1>Links</h1>
See page \ref design for RefOS design overview.<br>
See page \ref components for description of RefOS components.<br>
See page \ref testing for RefOS unit testing information.

<p>
<i>What does the logo mean? Pluto and Charon.</i>

*/