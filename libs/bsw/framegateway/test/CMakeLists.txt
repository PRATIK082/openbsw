add_executable(
    framegatewayTest
    src/framegateway/DiagLinkTest.cpp
    src/framegateway/DoIpHandlerTest.cpp
    src/framegateway/FrameGatewayTest.cpp
    src/framegateway/GatewayConfigParserTest.cpp
    src/framegateway/GatewayPolicyTest.cpp
    src/framegateway/GatewayStackTest.cpp
    src/framegateway/PduAssemblerTest.cpp
    src/framegateway/TpGatewayTest.cpp
    src/framegateway/XcpServerTest.cpp)

target_include_directories(framegatewayTest PUBLIC include)

target_link_libraries(
    framegatewayTest
    PRIVATE framegateway
            asyncMock
            asyncMockImpl
            gmock_main)

gtest_discover_tests(framegatewayTest PROPERTIES LABELS "framegatewayTest")
