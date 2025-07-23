add_executable(test_sample test_sample.cpp)
target_link_libraries(test_sample
    PRIVATE
        CommonLibrary
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_sample)