```protobuf
syntax = "proto3";
package yy.protocol.app;

// 重要，开启该选项才会生成service代码
option cc_generic_services = true;

message C2SLogin {
    uint64 session_id = 1;

    string account_name = 3;
    string password = 4;
}

message S2CLogin {
    uint64 session_id = 1;

    enum Status {
        eSuccess = 0;
        eAccountNoExist = 1;
        ePasswordError = 2;
        eUnknownError = 3;
    }
    Status result_code = 2;
    optional uint64 logic_server_id = 3;
    optional uint64 account_id = 4;
    optional string account_name = 5;
}


service AccountServiceRpc
{
    rpc Login(C2SLogin) returns(S2CLogin);
}
```



```mermaid
sequenceDiagram
    participant C as MyRpcClient
    participant AcntSvc_Stub as AccountServiceRpc_Stub(继承自AccountServiceRpc)
    participant CC as RpcChannel
    participant TCPC as TcpConnection
    participant S as RpcServer
	participant AcntSvc as LoginService(继承自AccountServiceRpc)
	
    activate C
	C->>AcntSvc_Stub: Login(nullptr, request, response, done:LoginFinished )
    AcntSvc_Stub->>CC: CallMethod(service, method, request, response, done)
    activate CC
    Note right of CC: 生成唯一ID
    CC->>CC: 创建PendingCallContext<br/>(response, done, controller)
    CC->>TCPC: codec_.SendTCP(RpcMessage.REQUEST)
    deactivate CC

    TCPC-->>S: 网络传输REQUEST
    deactivate C
    
    activate S
    S->>S: OnRpcRequest(conn, msg)
    alt 服务存在且方法有效
        S->>AcntSvc: service.CallMethod(method, request, response, closure)
        activate AcntSvc
        AcntSvc->>AcntSvc: Login()
        AcntSvc-->>S: 服务处理完成，调用Closure回调SendRpcResponse
        deactivate AcntSvc
    else 服务不存在
        S->>TCPC: 发送NO_SERVICE响应
    else 方法不存在
        S->>TCPC: 发送NO_METHOD响应
    end
    deactivate S

    S->>TCPC: SendRpcResponse(conn, response, id)
    activate S
    S->>TCPC: 发送RpcMessage.RESPONSE
    S->>TCPC: conn->Shutdown()
    deactivate S

    TCPC-->>CC: 网络传输RESPONSE
    activate CC
    CC->>CC: OnRpcResponse(conn, msg)
    CC->>CC: 查找匹配的PendingCallContext
    CC->>+CC: response->ParseFromString()
    CC->>CC: done->Run()
    CC->>C: LoginFinished(response)
    deactivate CC
```







```mermaid
classDiagram
    class Service {
        <<interface>>
        +GetDescriptor() 
        +CallMethod() 接口: 服务端调用
        +GetRequestPrototype()  用于创建请求消息
        +GetResponsePrototype() 用于创建响应消息
    }

    class AccountServiceRpc {
        +Login()    默认实现
        +Register() 默认实现
        +CallMethod()    重写 
        +GetDescriptor() 重写
    }
    class LoginService {
        +Login() 重写
        +CallMethod() 继承：根据Method调用具体的处理函数
    }
    class RegisterService {
        +Register() 重写
        +CallMethod() 继承：根据Method调用具体的处理函数
    }    

    class AccountServiceRpc_Stub {
        +Login()    继承：被客户端调用
        +Register() 继承：被客户端调用
        -channel_: RpcChannel指针
    }

    class RpcChannel {
        <<interface>>
        +CallMethod() 接口
    }
    
    class MyRpcChannel {
        +CallMethod() 重写：向服务端发送调用请求
    }

    class RpcServer {
        -services_: 服务名和服务对象的映射表
        +RegisterService() void
        +OnRpcRequest() void
        +SendRpcResponse() void
    }
    
    


    Service <|.. AccountServiceRpc
    RpcChannel        <|.. MyRpcChannel
    AccountServiceRpc <|.. AccountServiceRpc_Stub
    AccountServiceRpc <|.. LoginService
    AccountServiceRpc <|.. RegisterService
	AccountServiceRpc_Stub --> RpcChannel: 调用RpcChannel的CallMethod(具体要调用的Method指针)
    RpcServer *-- Service: 注册
```

