#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>


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
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址信息
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) != 1) {
        std::cerr << "Invalid address format." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    serverAddress.sin_port = htons(DEFAULT_PORT);

    // 连接到服务器
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server." << std::endl;

    // 发送和接收数据
    char buffer[1024]{};
    std::string userInput;

    do {
        // 从用户获取输入
        std::cout << "Enter a message (or 'exit' to quit): ";
        std::getline(std::cin, userInput);

        // 发送用户输入的数据到服务器
        send(clientSocket, userInput.c_str(), userInput.size(), 0);

        // 如果用户输入 "exit" 则退出循环
        if (userInput == "exit") {
            break;
        }

        // 接收服务器返回的数据
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = 0;
            // 打印服务器返回的数据
            std::cout << "Received from server: " << buffer << std::endl;
        } else if (bytesReceived == 0) {
            std::cout << "Server disconnected." << std::endl;
            break;
        } else {
            std::cerr << "Recv failed with error." << std::endl;
            break;
        }

    } while (true);

    // 关闭套接字
    closesocket(clientSocket);

    // 清理 Winsock
    WSACleanup();

    return 0;
}
