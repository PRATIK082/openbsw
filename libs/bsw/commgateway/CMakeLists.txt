add_library(
    commgateway
    src/commgateway/CommStack.cpp
    src/commgateway/CommStateManager.cpp
    src/commgateway/CommStatistics.cpp
    src/commgateway/EthChannel.cpp
    src/commgateway/EthIpdu.cpp
    src/commgateway/GatewayConfigParser.cpp
    src/commgateway/InterruptRouter.cpp
    src/commgateway/LinChannel.cpp
    src/commgateway/LinScheduler.cpp
    src/commgateway/SignalGateway.cpp
    src/commgateway/TimeoutMonitor.cpp
    src/commgateway/TxConfirmationMgr.cpp)

target_include_directories(commgateway PUBLIC include)

target_link_libraries(commgateway PUBLIC async canstack lifecycle util)
