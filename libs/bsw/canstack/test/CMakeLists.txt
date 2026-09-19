add_executable(
    canstackTest
    src/canstack/CanChannelTest.cpp
    src/canstack/CanFrameTest.cpp
    src/canstack/CanHwMcp2515Test.cpp
    src/canstack/CanHwStubTest.cpp
    src/canstack/CanInterfaceTest.cpp
    src/canstack/CanRouterTest.cpp
    src/canstack/CanSubsystemTest.cpp
    src/canstack/CanTxSchedulerTest.cpp
    src/canstack/SignalDbTest.cpp)

target_include_directories(canstackTest PUBLIC include)

target_link_libraries(
    canstackTest
    PRIVATE canstack
            asyncMock
            asyncMockImpl
            gmock_main)

gtest_discover_tests(canstackTest PROPERTIES LABELS "canstackTest")
