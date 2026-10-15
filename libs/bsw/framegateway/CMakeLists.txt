add_library(
    framegateway
    src/framegateway/DiagLink.cpp
    src/framegateway/DoIpHandler.cpp
    src/framegateway/FrameGateway.cpp
    src/framegateway/GatewayConfigParser.cpp
    src/framegateway/GatewayPolicy.cpp
    src/framegateway/GatewayStack.cpp
    src/framegateway/PduAssembler.cpp
    src/framegateway/TpGateway.cpp
    src/framegateway/XcpServer.cpp
    src/framegateway/XcpSymbolTable.cpp)

target_include_directories(framegateway PUBLIC include)

target_link_libraries(framegateway PUBLIC async lifecycle util)
