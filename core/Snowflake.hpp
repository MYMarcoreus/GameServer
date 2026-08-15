#pragma once
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <mutex>

//
// 空锁实现：用于在单线程场景下禁用锁开销
//
class snowflake_nonlock {
public:
    void lock() { }
    void unlock() { }
};

//
// 雪花算法模板类
// 参数：s
//   - Twepoch: 时间起点，是过去的时间点
//   - Lock: 可选的锁类型，默认使用无锁 (snowflake_nonlock)
//
template<int64_t Twepoch = 1622476800000L, typename Lock = snowflake_nonlock>
requires std::default_initializable<Lock> && requires(Lock l) {
    { l.lock() } -> std::same_as<void>;
    { l.unlock() } -> std::same_as<void>;
}
class Snowflake {
    using lock_type = Lock;

    // 雪花算法参数配置
    static constexpr int64_t TWEPOCH = Twepoch;                    // 时间起点
    static constexpr int64_t WORKER_ID_BITS = 5L;                  // 机器ID所占位数
    static constexpr int64_t DATACENTER_ID_BITS = 5L;              // 数据中心ID所占位数
    static constexpr int64_t MAX_WORKER_ID = (1 << WORKER_ID_BITS) - 1;
    static constexpr int64_t MAX_DATACENTER_ID = (1 << DATACENTER_ID_BITS) - 1;
    static constexpr int64_t SEQUENCE_BITS = 12L;                  // 每毫秒序列号的位数

    static constexpr int64_t WORKER_ID_SHIFT = SEQUENCE_BITS;
    static constexpr int64_t DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
    static constexpr int64_t TIMESTAMP_LEFT_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;
    static constexpr int64_t SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;

    // 定义类型
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    // 起始时间点（用于转换 steady_clock 为系统时间）
    time_point start_time_point_ = std::chrono::steady_clock::now();
    int64_t start_millsecond_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 雪花状态变量
    int64_t last_timestamp_ = -1;    // 上次生成ID的时间戳
    int64_t workerid_ = 0;           // 当前机器ID
    int64_t datacenterid_ = 0;       // 当前数据中心ID
    int64_t sequence_ = 0;           // 当前毫秒内的序列号
    lock_type lock_;                 // 锁（可能是空锁）

public:
    Snowflake() = default;
    Snowflake(const Snowflake&) = delete;
    Snowflake& operator=(const Snowflake&) = delete;

    // 初始化数据中心 ID 和 机器 ID
    void init(const int64_t workerid, const int64_t datacenterid) {
        if (workerid > MAX_WORKER_ID || workerid < 0) {
            throw std::runtime_error("worker Id can't be greater than 31 or less than 0");
        }

        if (datacenterid > MAX_DATACENTER_ID || datacenterid < 0) {
            throw std::runtime_error("datacenter Id can't be greater than 31 or less than 0");
        }

        workerid_ = workerid;
        datacenterid_ = datacenterid;
    }

    // 生成唯一 ID
    int64_t nextid() {
        std::lock_guard<lock_type> lock(lock_);

        auto timestamp = current_ms();

        if (last_timestamp_ == timestamp) {
            // 同一毫秒内，递增序列号
            sequence_ = (sequence_ + 1) & SEQUENCE_MASK;

            // 如果序列号溢出（超出 4095），则等待下一毫秒
            if (sequence_ == 0) {
                timestamp = wait_next_millis(last_timestamp_);
            }
        } else {
            // 不同毫秒，序列号归0
            sequence_ = 0;
        }

        last_timestamp_ = timestamp;

        // 雪花ID结构：timestamp | datacenter_id | worker_id | sequence
        return ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT)
            | (datacenterid_ << DATACENTER_ID_SHIFT)
            | (workerid_ << WORKER_ID_SHIFT)
            | sequence_;
    }

private:
    // 获取当前毫秒数（相对系统时间）
    int64_t current_ms() const noexcept {
        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_point_);
        return start_millsecond_ + diff.count();
    }

    // 等待下一毫秒（用于序列号溢出时）
    int64_t wait_next_millis(const int64_t last) const noexcept {
        auto timestamp = current_ms();
        while (timestamp <= last) {
            timestamp = current_ms();
        }
        return timestamp;
    }
};
