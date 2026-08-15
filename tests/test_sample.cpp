#pragma once
#include <stdexcept>
#include <gtest/gtest.h>

class Calculator {
public:
    int add(int a, int b) const;
    int divide(int a, int b) const; // throws std::runtime_error if b == 0
};

int Calculator::add(int a, int b) const {
    return a + b;
}

int Calculator::divide(int a, int b) const {
    if (b == 0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}



// 测试夹具（Test Fixture）
class CalculatorTest : public ::testing::Test {
protected:
    Calculator calc;

    void SetUp() override {
        // 每个测试开始前执行
    }

    void TearDown() override {
        // 每个测试结束后执行
    }
};

TEST_F(CalculatorTest, TestAddition) {
    EXPECT_EQ(calc.add(1, 2), 3);
    EXPECT_EQ(calc.add(1, 2), 4); //!错误
    EXPECT_EQ(calc.add(-3, 3), 0);
    EXPECT_NE(calc.add(100, 200), 50); // 不相等测试
}

TEST_F(CalculatorTest, TestDivision) {
    EXPECT_EQ(calc.divide(10, 2), 5);
    EXPECT_EQ(calc.divide(9, 3), 3);
}

TEST_F(CalculatorTest, TestDivisionByZeroThrows) {
    EXPECT_THROW(calc.divide(10, 0), std::runtime_error);
    EXPECT_ANY_THROW(calc.divide(1, 0));
}

// 参数化测试
class CalculatorDivideParamTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {
protected:
    Calculator calc;
};

TEST_P(CalculatorDivideParamTest, HandlesVariousDivisions) {
    auto [a, b, expected] = GetParam();
    EXPECT_EQ(calc.divide(a, b), expected);
}

INSTANTIATE_TEST_SUITE_P(
    DivTestCases,
    CalculatorDivideParamTest,
    ::testing::Values(
        std::make_tuple(10, 2, 5),
        std::make_tuple(20, 5, 4),
        std::make_tuple(9, 3, 3)
    )
);
