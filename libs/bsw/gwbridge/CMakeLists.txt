add_library(
    gwbridge
    src/gwbridge/BridgeSupervisor.cpp
    src/gwbridge/CanIoBridge.cpp
    src/gwbridge/EthIoBridge.cpp
    src/gwbridge/LinIoBridge.cpp)

target_include_directories(gwbridge PUBLIC include)

# routingConfiguration provides blob/ConfigType.h (referenceApp config in
# production builds, routing/test include in unitTest builds).
target_link_libraries(
    gwbridge
    PUBLIC async
           canstack
           commgateway
           etl
           io
           lifecycle
           routing
           routingConfiguration
           util)
