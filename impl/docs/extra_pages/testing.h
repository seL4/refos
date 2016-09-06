/*! @file */

/*! @page testing Testing

@image html testing_levels.png "Figure 1 - RefOS Testing Levels"

Testing in RefOS is divided into three levels:
<ul>
    <li> Root Level Tests </li>
    <li> Operating System Level Tests </li>
    <li> User Level Tests </li>
</ul>

The reason for having three testing levels is to be able to test different parts of the system. For example, if the root task process server is not working correctly, the  operating system level tests are quite unlikely to pass, and thus as programmers we require a lower level test to identify the problem. Dividing the tests into three layers creates better usability and test coverage.

@section root_level_tests Root Level Tests

Root level tests verify that the the process server works as expected. These tests verify (among other things) that the process server is bootstrapped correctly, that the lowest level kernel libraries perform correctly, that kernel object allocation works correctly and that static heap allocation works correctly. If this test suite fails, it is very unlikely that system applications will run successfully. Root level test failures indicate that the process server is not behaving correctly.

@section os_level_tests Operating System Level Tests

Operating system level tests ensure that the low level RefOS environment operates correctly and that the RefOS system level servers have been started and function as expected. These tests verify (among other things) that stack and heap memory space behave as expected, that anonymous dataspaces behave as expected and that dataspace mappping operations work correctly. If the operating system level environment is broken, the userland selfloader will quite likely not be able to start userland processes. If this test suite fails, there is likely something wrong with the file server, a device server, a low-level library or a remote procedure call.

@section user_level_tests User Level Tests

User level tests are the highest level of RefOS tests. They ensure that the userland environment behaves correctly. These tests include verifying that the dynamic heap and mmap work correctly, that files can be opened and closed correctly and that time functionality works correctly. If this test suite fails, it is possible that userland programs will not work correctly.

*/
