add_executable(ethlinkTest src/ethlink/EthLinkManagerTest.cpp)

target_link_libraries(ethlinkTest PRIVATE ethlink gmock_main)

gtest_discover_tests(ethlinkTest PROPERTIES LABELS "ethlinkTest")
