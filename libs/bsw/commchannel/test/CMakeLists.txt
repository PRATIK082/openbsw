add_executable(commchannelTest src/commchannel/CommChannelManagerTest.cpp)

target_link_libraries(commchannelTest PRIVATE commchannel gmock_main)

gtest_discover_tests(commchannelTest PROPERTIES LABELS "commchannelTest")
