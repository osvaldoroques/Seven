# CMake generated Testfile for 
# Source directory: /workspaces/Seven/tests
# Build directory: /workspaces/Seven/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(service_host_test "/workspaces/Seven/build/tests/test_service_host")
set_tests_properties(service_host_test PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/Seven/tests/CMakeLists.txt;19;add_test;/workspaces/Seven/tests/CMakeLists.txt;0;")
add_test(graceful_shutdown_test "/workspaces/Seven/build/tests/test_graceful_shutdown")
set_tests_properties(graceful_shutdown_test PROPERTIES  _BACKTRACE_TRIPLES "/workspaces/Seven/tests/CMakeLists.txt;41;add_test;/workspaces/Seven/tests/CMakeLists.txt;0;")
