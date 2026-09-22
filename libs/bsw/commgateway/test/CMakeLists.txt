add_executable(
    commgatewayTest
    src/commgateway/CommStackTest.cpp
    src/commgateway/CommStateManagerTest.cpp
    src/commgateway/EthIpduTest.cpp
    src/commgateway/GatewayConfigParserTest.cpp
    src/commgateway/InterruptRouterTest.cpp
    src/commgateway/LinChannelTest.cpp
    src/commgateway/SignalGatewayTest.cpp
    src/commgateway/TimeoutMonitorTest.cpp
    src/commgateway/TxConfirmationMgrTest.cpp)

target_include_directories(commgatewayTest PUBLIC include)

target_link_libraries(
    commgatewayTest
    PRIVATE commgateway
            asyncMock
            asyncMockImpl
            gmock_main)

gtest_discover_tests(commgatewayTest PROPERTIES LABELS "commgatewayTest")
