add_executable(test_sample test_sample.cpp)
target_link_libraries(test_sample
    PRIVATE
        CommonLibrary
        GTest::gtest
        GTest::gtest_main
)

add_executable(test_mapinloop test_mapinloop.cpp)
target_link_libraries(test_mapinloop CommonLibrary)

add_executable(test_main test_main.cpp)
target_link_libraries(test_main CommonLibrary)

include(GoogleTest)
gtest_discover_tests(test_sample)