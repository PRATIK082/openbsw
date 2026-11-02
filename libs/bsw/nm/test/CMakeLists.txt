add_executable(nmTest src/nm/CanNmTest.cpp)

target_link_libraries(nmTest PRIVATE nm gmock_main)

gtest_discover_tests(nmTest PROPERTIES LABELS "nmTest")
