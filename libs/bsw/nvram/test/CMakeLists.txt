add_executable(nvramTest src/nvram/NvramTest.cpp)

target_link_libraries(nvramTest PRIVATE nvram gmock_main)

gtest_discover_tests(nvramTest PROPERTIES LABELS "nvramTest")
