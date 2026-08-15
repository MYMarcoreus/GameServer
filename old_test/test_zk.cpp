#include "ConfigManager.h"
#include "ZkServiceManager.h"
#include "EventLoop.h"
#include "IPAddress.h"
#include "log.h"
#include "ProtobufDispatcher.h"

void testfun1()
{
    auto & zkma = yy::core::zk::ZkServiceManager::Instance();
    zkma.Start("/services");

    zkma.Register("test_service1", "127.0.0.1", "114514");
    zkma.Register("test_service1", "127.0.0.2", "13140");
    zkma.Register("test_service1", "127.0.0.3", "7878");
    zkma.Register("test_service2", "127.0.0.1", "451411");
    zkma.Register("test_service2", "127.0.0.9", "4514");
    zkma.Register("test_service3", "127.0.0.2", "14013");
    zkma.Register("test_service4", "127.0.0.3", "7979");

    const auto addr_local1 = zkma.FetchLocalCache("test_service1");
    const auto addr_local2 = zkma.FetchLocalCache("test_service2");
    const auto addr_local3 = zkma.FetchLocalCache("test_service3");
    const auto addr_local4 = zkma.FetchLocalCache("test_service4");
    assert(addr_local1.size() == 3);
    assert(addr_local2.size() == 2);
    assert(addr_local3.size() == 1);
    assert(addr_local4.size() == 1);

    auto router_map_local = zkma.FetchAllLocalCache();
    assert(router_map_local.size() >= 4);
    assert(router_map_local["/services/test_service1"].size() == 3);
    assert(router_map_local["/services/test_service2"].size() == 2);
    assert(router_map_local["/services/test_service3"].size() == 1);
    assert(router_map_local["/services/test_service4"].size() == 1);
    for (auto & [service_name, service_addrs]: router_map_local) {
        for (const auto & service_addr: service_addrs) {
            YLOG_INFO("[GateServer Service Local] {} in {}:{}", service_name, service_addr->GetIPStr(), service_addr->GetPort());
        }
    }

    const auto addr_remote1 = zkma.FetchRemote("test_service1");
    const auto addr_remote2 = zkma.FetchRemote("test_service2");
    const auto addr_remote3 = zkma.FetchRemote("test_service3");
    const auto addr_remote4 = zkma.FetchRemote("test_service4");
    assert(addr_remote1.size() == 3);
    assert(addr_remote2.size() == 2);
    assert(addr_remote3.size() == 1);
    assert(addr_remote4.size() == 1);

    auto router_map_remote = zkma.FetchAllRemote();
    assert(router_map_remote.size() >= 4);
    assert(router_map_remote["/services/test_service1"].size() == 3);
    assert(router_map_remote["/services/test_service2"].size() == 2);
    assert(router_map_remote["/services/test_service3"].size() == 1);
    assert(router_map_remote["/services/test_service4"].size() == 1);
    for (auto & [service_name, service_addrs]: router_map_remote) {
        for (const auto & service_addr: service_addrs) {
            YLOG_INFO("[GateServer Service Remote] {} in {}:{}", service_name, service_addr->GetIPStr(), service_addr->GetPort());
        }
    }
}





void testfun2()
{
    auto & zkma = yy::core::zk::ZkServiceManager::Instance();
    zkma.Start("/services");

    zkma.Register("test_service1", "127.0.0.1", "114514");
    zkma.Register("test_service1", "127.0.0.2", "13140");
    zkma.Register("test_service1", "127.0.0.3", "7878");
    zkma.Register("test_service2", "127.0.0.1", "451411");
    zkma.Register("test_service2", "127.0.0.9", "4514");
    zkma.Register("test_service3", "127.0.0.2", "14013");
    zkma.Register("test_service4", "127.0.0.3", "7979");



    const auto addr_remote1 = zkma.FetchRemote("test_service1");
    const auto addr_remote2 = zkma.FetchRemote("test_service2");
    const auto addr_remote3 = zkma.FetchRemote("test_service3");
    const auto addr_remote4 = zkma.FetchRemote("test_service4");
    assert(addr_remote1.size() == 3);
    assert(addr_remote2.size() == 2);
    assert(addr_remote3.size() == 1);
    assert(addr_remote4.size() == 1);

    auto router_map_remote = zkma.FetchAllRemote();
    assert(router_map_remote.size() >= 4);
    assert(router_map_remote["/services/test_service1"].size() == 3);
    assert(router_map_remote["/services/test_service2"].size() == 2);
    assert(router_map_remote["/services/test_service3"].size() == 1);
    assert(router_map_remote["/services/test_service4"].size() == 1);
    for (auto & [service_name, service_addrs]: router_map_remote) {
        for (const auto & service_addr: service_addrs) {
            YLOG_INFO("[GateServer Service Remote] {} in {}:{}", service_name, service_addr->GetIPStr(), service_addr->GetPort());
        }
    }



    const auto addr_local1 = zkma.FetchLocalCache("test_service1");
    const auto addr_local2 = zkma.FetchLocalCache("test_service2");
    const auto addr_local3 = zkma.FetchLocalCache("test_service3");
    const auto addr_local4 = zkma.FetchLocalCache("test_service4");
    assert(addr_local1.size() == 3);
    assert(addr_local2.size() == 2);
    assert(addr_local3.size() == 1);
    assert(addr_local4.size() == 1);

    auto router_map_local = zkma.FetchAllLocalCache();
    assert(router_map_local.size() >= 4);
    assert(router_map_local["/services/test_service1"].size() == 3);
    assert(router_map_local["/services/test_service2"].size() == 2);
    assert(router_map_local["/services/test_service3"].size() == 1);
    assert(router_map_local["/services/test_service4"].size() == 1);
    for (auto & [service_name, service_addrs]: router_map_local) {
        for (const auto & service_addr: service_addrs) {
            YLOG_INFO("[GateServer Service Local] {} in {}:{}", service_name, service_addr->GetIPStr(), service_addr->GetPort());
        }
    }
}

void testfun3()
{
    auto & zkma = yy::core::zk::ZkServiceManager::Instance();
    zkma.Start("/services");

    zkma.Register("test_service1", "127.0.0.1", "114514");
    zkma.Register("test_service1", "127.0.0.2", "13140");
    zkma.Register("test_service1", "127.0.0.3", "7878");
    zkma.Register("test_service2", "127.0.0.1", "451411");
    zkma.Register("test_service2", "127.0.0.9", "4514");
    zkma.Register("test_service3", "127.0.0.2", "14013");
    zkma.Register("test_service4", "127.0.0.3", "7979");

    zkma.Watch("test_service1", [sv_name = "test_service1"](const std::string & name, std::vector<yy::net::IPAddressPtr> && endpoints) {
        for (const auto & endpoint: endpoints) {
            YLOG_INFO("[Watch!!!!!] {} == {} in {}", sv_name, name, endpoint->ToString());
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(100000000));
}



int main(int argc, char* argv[])
{
    yy::config::ConfigManager::AddFilePath("../config/configs_account.xml");
    yy::config::ConfigManager::AddFilePath("../../config/configs_account.xml");
    yy::config::ConfigManager::LoadXmlConfigs();

    START_YLOG_AFTER_CONFIG()

    // std::cout << "---------testfun1---------" << std::endl;
    // testfun1();
    // std::cout << "---------testfun2---------" << std::endl;
    // testfun2();
    std::cout << "---------testfun3---------" << std::endl;
    testfun3();

    std::cout << "---------main end---------" << std::endl;
    return 0;
}
