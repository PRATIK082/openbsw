add_executable(demTest src/dem/DemTest.cpp)

target_link_libraries(demTest PRIVATE dem gmock_main)

gtest_discover_tests(demTest PROPERTIES LABELS "demTest")
