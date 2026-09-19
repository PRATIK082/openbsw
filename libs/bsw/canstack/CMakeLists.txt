add_library(
    canstack
    src/canstack/CanChannel.cpp
    src/canstack/CanFrame.cpp
    src/canstack/CanHwMcp2515.cpp
    src/canstack/CanHwSocketCan.cpp
    src/canstack/CanHwStub.cpp
    src/canstack/CanHwTransceiverAdapter.cpp
    src/canstack/CanInterface.cpp
    src/canstack/CanRouter.cpp
    src/canstack/CanStackLogger.cpp
    src/canstack/CanSubsystem.cpp
    src/canstack/CanTxScheduler.cpp
    src/canstack/SignalDb.cpp)

target_include_directories(canstack PUBLIC include)

target_link_libraries(canstack PUBLIC async cpp2can lifecycle util)

if (BUILD_EXECUTABLE STREQUAL "referenceApp" AND BUILD_TARGET_PLATFORM STREQUAL "POSIX")
    target_compile_definitions(canstack PUBLIC CANSTACK_WITH_SOCKETCAN=1)
endif ()
