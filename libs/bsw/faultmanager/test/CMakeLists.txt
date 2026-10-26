add_executable(faultmanagerTest src/faultmanager/FaultManagerTest.cpp)

target_link_libraries(faultmanagerTest PRIVATE faultmanager gmock_main)

gtest_discover_tests(faultmanagerTest PROPERTIES LABELS "faultmanagerTest")
