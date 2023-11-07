#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12345);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    fd_set readSet;
    fd_set tempSet;

    FD_ZERO(&readSet);
    FD_SET(listenSocket, &readSet);

    std::cout << "Server listening on port 12345..." << std::endl;

    while (true) {
        tempSet = readSet;
        int result = select(0, &tempSet, nullptr, nullptr, nullptr);
        if (result == SOCKET_ERROR) {
            std::cerr << "Select failed." << std::endl;
            break;
        }

        for (int socketId = 0; socketId < readSet.fd_count; ++socketId) {
            SOCKET currentSocket = readSet.fd_array[socketId];

            if (FD_ISSET(currentSocket, &tempSet)) {
                if (currentSocket == listenSocket) {
                    SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
                    std::cout << "Client connected." << std::endl;
                    FD_SET(clientSocket, &readSet);
                } else {
                    char buffer[1024]{};
                    int bytesRead = recv(currentSocket, buffer, sizeof(buffer), 0);
                    if (bytesRead == 0) {
                        std::cout << "Client disconnected." << std::endl;
                        closesocket(currentSocket);
                        FD_CLR(currentSocket, &readSet);
                    } else if (bytesRead == SOCKET_ERROR) {
                        std::cerr << "Recv failed." << std::endl;
                        closesocket(currentSocket);
                        FD_CLR(currentSocket, &readSet);
                    } else {
                        // Echo the received data back to the client.
                        printf("%s\n", buffer);
                        send(currentSocket, buffer, bytesRead, 0);
                    }
                }
            }
        }
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}
