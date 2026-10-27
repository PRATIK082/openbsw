add_executable(integrityTest src/integrity/IntegrityGuardTest.cpp)

target_link_libraries(integrityTest PRIVATE integrity gmock_main)

gtest_discover_tests(integrityTest PROPERTIES LABELS "integrityTest")
