add_executable(
    gwbridgeTest
    src/gwbridge/BridgeSupervisorTest.cpp
    src/gwbridge/CanIoBridgeTest.cpp
    src/gwbridge/IoBridgeTest.cpp
    src/gwbridge/RouterBridgeTest.cpp)

target_include_directories(gwbridgeTest PUBLIC include)

target_link_libraries(
    gwbridgeTest
    PRIVATE gwbridge
            asyncMock
            asyncMockImpl
            gmock_main)

gtest_discover_tests(gwbridgeTest PROPERTIES LABELS "gwbridgeTest")
