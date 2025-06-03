#include "ConcurrentQueue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>





moodycamel::ConcurrentQueue<int> queue;
std::atomic<int> produceCount{0};
std::atomic<int> consumeCount{0};

std::mutex outputMutex;
std::unordered_set<int> results;

void producer(int id, int countPerThread) {
    for (int i = 0; i < countPerThread; ++i) {
        int val = id * 100000 + i;
        queue.enqueue(val);
        produceCount.fetch_add(1);
    }
}

void consumer(int totalToConsume) {
    while (consumeCount.load() < totalToConsume) {
        int value;
        if (queue.try_dequeue(value)) {
            {
                std::lock_guard<std::mutex> lock(outputMutex);
                results.insert(value);
            }
            consumeCount.fetch_add(1);
        }
        // 可加 sleep 或 pause() 防止忙等
    }
}

int main() {
    const int producerThreads = 8;
    const int consumerThreads = 8;
    const int itemsPerProducer = 100000;
    const int totalItems = producerThreads * itemsPerProducer;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 启动消费者
    for (int i = 0; i < consumerThreads; ++i) {
        consumers.emplace_back(consumer, totalItems);
    }

    // 启动生产者
    for (int i = 0; i < producerThreads; ++i) {
        producers.emplace_back(producer, i, itemsPerProducer);
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::cout << "Produced: " << produceCount << ", Consumed: " << consumeCount << "\n";
    std::cout << "Unique values in result: " << results.size() << "\n";

    if (results.size() != totalItems) {
        std::cerr << "❌ Error: Data loss or duplication!\n";
    } else {
        std::cout << "✅ Test Passed.\n";
    }
    return 0;
}
