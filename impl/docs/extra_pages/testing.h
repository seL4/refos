/*! @file */

/*! @page testing Testing

@image html testing_levels.png

Testing in RefOS is broken into three levels:
<ul>
    <li> \ref root_task_tests.</li>
</ul>

The reason for splitting theses up is that the next level of tests is unlikely to be run at all if
the pervious layer is broken. For example, if the root task process server itself is broken, the os-
level tests are unlikely to start up at all. Conversely, we cannot test user-level functionality
without first booting up the servers responsible for the functionality. Thus, dividing the tests
into the 3 layers allow for the both user-friendliness and test coverage.

@section root_task_tests Root-task Tests

Root task tests attempt to verify that the basic process server environment has been bootstrapped
correctly, that the lowest level kernel-facing libraries work, kernel object allocation may be
performed, static heap allocation works, and so forth. If the root task environment is broken it is
unlikely system applications will start successfully, and OS level tests will unlikely to be run at
all. Root task test failures indicate a process server bug.

@section os_level_tests OS Level Tests

OS level tests test that the low-level RefOS environment works correctly, and the RefOS system level
servers are started and function as expected. If the OS level environment is broken, then the
userland selfloader will likely fail to start any userland processes, and user-level tests are
likely to fail to run at all. A breakage here usually indicates a CPIO file server, device server
bug, low-level library or RPC communication bug.

@section user_level_tests User Level Tests

User level tests are the highest level of tests for RefOS, and test that the userland environment
behaves as expected. These tests test that dynamic heap & mmap work, file open / close works,
time functionality, and so forth.

*/