#include <iostream>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

constexpr int DEFAULT_PORT = 16666;

int main() {
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    // 创建套接字
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址信息
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(DEFAULT_PORT);

    // 绑定套接字
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 监听连接
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << DEFAULT_PORT << std::endl;

    // 接受和处理客户端连接
    SOCKET clientSocket;
    sockaddr_in clientAddress;
    int clientAddressSize = sizeof(clientAddress);

    while(true){
        clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error." << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        std::cout << "Client connected." << std::endl;

        // 接收和发送数据
        char buffer[1024]{};
        int bytesReceived;

        do {
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = 0;
                // 打印客户端发送的数据
                std::cout << "Received: " << buffer << std::endl;

                // 原样返回给客户端
                send(clientSocket, buffer, bytesReceived, 0);
            } else if (bytesReceived == 0) {
                std::cout << "Client disconnected." << std::endl;
            } else {
                std::cerr << "Recv failed with error." << std::endl;
            }

        } while (bytesReceived > 0);

    }

    // 关闭套接字
    closesocket(clientSocket);
    closesocket(serverSocket);

    // 清理 Winsock
    WSACleanup();

    return 0;
}
