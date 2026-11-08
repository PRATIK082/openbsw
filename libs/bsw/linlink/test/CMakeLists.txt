add_executable(linlinkTest src/linlink/LinLinkManagerTest.cpp)

target_link_libraries(linlinkTest PRIVATE linlink gmock_main)

gtest_discover_tests(linlinkTest PROPERTIES LABELS "linlinkTest")
