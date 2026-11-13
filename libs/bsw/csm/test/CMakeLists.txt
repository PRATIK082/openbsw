add_executable(csmTest src/csm/CsmTest.cpp)

target_link_libraries(csmTest PRIVATE csm gmock_main)

gtest_discover_tests(csmTest PROPERTIES LABELS "csmTest")