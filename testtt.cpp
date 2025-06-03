#include <iostream>
#include <cassert>
#include <cstring>
#include "RingBuffer.h"

void TestBasicFunctionality() {
    yy::util::RingBuffer buf(10); // 实际申请内存大小为 16
    const size_t buffer_size = buf.GetCapacity();     // 应为 16
    const size_t usable_size = buffer_size - 1;      // 应为 15

    // 1. 初始状态检查
    assert(buf.GetFreeSize() == usable_size);
    assert(buf.GetDataSize() == 0);
    assert(buf.IsDataEmpty());
    assert(!buf.IsDataFull());

    // 2. 写入最大可用数据
    const char* testData = "abcdefghijklmno"; // 15 字节
    assert(std::strlen(testData) == usable_size);
    assert(buf.AppendDataFromCBuffer(testData, usable_size)); // 应成功
    assert(buf.IsDataFull());
    assert(!buf.IsDataEmpty());
    assert(buf.GetFreeSize() == 0);
    assert(buf.GetDataSize() == usable_size);

    // 3. 再写 1 字节应失败
    assert(!buf.AppendDataFromCBuffer("X", 1));

    // 4. Peek 全部数据
    char output[16] = {};
    assert(buf.PeekToCBuffer(0, output, usable_size));
    assert(std::strncmp(output, testData, usable_size) == 0);

    // 5. Pop 前 7 字节
    char part[8] = {};
    assert(buf.PopDataToCBuffer(part, 7));
    assert(std::strncmp(part, "abcdefg", 7) == 0);
    assert(buf.GetDataSize() == usable_size - 7);
    assert(!buf.IsDataFull());
    assert(!buf.IsDataEmpty());

    // 6. 写入 7 字节（应发生 wrap-around）
    assert(buf.AppendDataFromCBuffer("XYZWVUQ", 7));
    assert(buf.IsDataFull());

    // 7. 读出所有数据
    std::string all = buf.PopAllDataAsString();
    assert(all.size() == usable_size);
    assert(all.substr(0, 8) == "hijklmno");
    assert(all.substr(8, 7) == "XYZWVUQ");


    // 8. 状态归零
    assert(buf.IsDataEmpty());
    assert(buf.GetFreeSize() == usable_size);
    std::cout << "[PASS] BasicFunctionality Test (Revised)\n";
}





int main()
{
    TestBasicFunctionality();
}