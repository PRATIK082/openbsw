add_executable(modearbiterTest src/modearbiter/ModeArbiterTest.cpp)

target_link_libraries(modearbiterTest PRIVATE modearbiter gmock_main)

gtest_discover_tests(modearbiterTest PROPERTIES LABELS "modearbiterTest")
