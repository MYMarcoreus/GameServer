#include "EventLoopThreadPool.h"
#include "log.h"
#include "RedisClient.h"
#include "RemoteXmlConfig.h"

int main(int argc, char* argv[])
{

    yy::config::ConfigManager::AddFilePath("../config/configs_logic.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_logic.xml");
    yy::config::ConfigManager::LoadXmlConfigs();
    yy::Ylog::LoggerManager::Instance().ReadConfigs();

    auto loop = new yy::net::EventLoop(500ms);

    yy::net::EventLoopThreadPool io_threadpool(loop);
    io_threadpool.Start(10, 500ms);

    for (int i = 0; i < 10; ++i) {
        const auto ioloop = io_threadpool.GetNextLoop();
        ioloop->RunEvery(1s, [&loop]()
        {
            yy::core::redis::RedisClient::Instance().Start(loop, 1);
            yy::core::redis::RedisClient::Instance().Set("key1", "value1");
            auto val = yy::core::redis::RedisClient::Instance().Get("key1");
            if (val) {
                YLOG_INFO("key1: {}", val.value());
            }
        });
    }

    loop->Loop();
}
