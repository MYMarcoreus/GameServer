#include <iostream>
#include <thread>
#include <vector>
#include <latch>
#include <memory>
#include <string>

#include "ConfigManager.h"
#include "log.h"
#include "UnorderedMapInLoop.hpp"

int main() {
    yy::config::ConfigManager::AddFilePath("../../config/configs_gate.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    START_YLOG_AFTER_CONFIG()

    yy::net::EventLoop loop{500ms};
    yy::net::UnorderedMapInLoop<int, std::shared_ptr<std::string>> M(&loop);

    std::jthread test_thread([&M]
    {
        constexpr int num_threads = 20;
        std::latch done(num_threads);
        std::vector<std::thread> workers;
        workers.reserve(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back([i, &M, &done] {
                for (int j = 0; j < 100; ++j) {
                    int key = i * 100 + j;
                    M.Insert(key, std::make_shared<std::string>("val_" + std::to_string(key)));
                }
                done.count_down();
            });
        }

        done.wait();

        for (int i = 0; i < num_threads * 100; i += 100) {
            M.Get(i, [i](const std::optional<std::shared_ptr<std::string>>& val) {
                std::cout << "Key " << i << " = "
                          << (val.has_value() ? *val.value() : std::string("<nullopt>")) << "\n";
            });
        }

        for (auto& t : workers)
            t.join();
    });

    loop.Loop(); // 放主线程执行

    return 0;
}
