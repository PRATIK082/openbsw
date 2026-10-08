add_executable(e2eTest src/e2e/E2eTest.cpp)

target_link_libraries(e2eTest PRIVATE e2e gmock_main)

gtest_discover_tests(e2eTest PROPERTIES LABELS "e2eTest")
