#include "../include/NATSServer.h"
#include "../include/Client.h"
#include "../include/Subscription.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace pulse_broker;

#define TEST(name) void test_##name()
#define RUN_TEST(name) std::cout << "Running test: " << #name << "... "; test_##name(); std::cout << "PASSED" << std::endl;

SOCKET connectToServer(const std::string& host, int port) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return INVALID_SOCKET;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &(serverAddr.sin_addr));

    result = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return INVALID_SOCKET;
    }

    return clientSocket;
}

bool sendToServer(SOCKET socket, const std::string& message) {
    int result = send(socket, message.c_str(), static_cast<int>(message.size()), 0);
    return result != SOCKET_ERROR;
}

std::string receiveFromServer(SOCKET socket) {
    char buffer[4096];
    int bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived <= 0) {
        return "";
    }
    
    buffer[bytesReceived] = '\0';
    return std::string(buffer, bytesReceived);
}

TEST(server_start_stop) {
    NATSServer server("127.0.0.1", 4222);
    
    bool started = server.start();
    assert(started == true);
    assert(server.isRunning() == true);
    
    server.stop();
    assert(server.isRunning() == false);
}

TEST(client_connection) {
    NATSServer server("127.0.0.1", 4223);
    server.start();
    
    SOCKET clientSocket = connectToServer("127.0.0.1", 4223);
    assert(clientSocket != INVALID_SOCKET);
    
    std::string response = receiveFromServer(clientSocket);
    assert(response.substr(0, 4) == "INFO");
    
    closesocket(clientSocket);
    WSACleanup();
    server.stop();
}

TEST(ping_command) {
    NATSServer server("127.0.0.1", 4224);
    server.start();
    
    SOCKET clientSocket = connectToServer("127.0.0.1", 4224);

    receiveFromServer(clientSocket);

    sendToServer(clientSocket, "PING\r\n");

    std::string response = receiveFromServer(clientSocket);
    assert(response == "PONG\r\n");

    closesocket(clientSocket);
    WSACleanup();
    server.stop();
}

TEST(connect_command) {
    NATSServer server("127.0.0.1", 4225);
    server.start();

    SOCKET clientSocket = connectToServer("127.0.0.1", 4225);
    
    receiveFromServer(clientSocket);
    
    sendToServer(clientSocket, "CONNECT {}\r\n");
    
    std::string response = receiveFromServer(clientSocket);
    assert(response == "+OK\r\n");
    
    closesocket(clientSocket);
    WSACleanup();
    server.stop();
}

void server_tests() {
    std::cout << "Running NATSServer tests...\n";
    
    RUN_TEST(server_start_stop);
    RUN_TEST(client_connection);
    RUN_TEST(ping_command);
    RUN_TEST(connect_command);
    
    std::cout << "All server tests PASSED!\n";
}

void parser_tests();

int main() {
    parser_tests();
    server_tests();
    
    std::cout << "All tests completed successfully!\n";
    return 0;
} 