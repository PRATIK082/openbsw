add_executable(wdgmTest src/wdgm/WdgMTest.cpp)

target_link_libraries(wdgmTest PRIVATE wdgm gmock_main)

gtest_discover_tests(wdgmTest PROPERTIES LABELS "wdgmTest")