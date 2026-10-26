add_executable(persistenceTest src/persistence/PersistenceTest.cpp)

target_link_libraries(persistenceTest PRIVATE persistence gmock_main)

gtest_discover_tests(persistenceTest PROPERTIES LABELS "persistenceTest")
