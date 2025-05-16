#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>

#pragma comment(lib, "Ws2_32.lib")

class NATSClient {
public:
    NATSClient(const std::string& host, int port) 
        : host_(host), port_(port), socket_(INVALID_SOCKET), connected_(false), nextSid_(1) {
    }
    
    ~NATSClient() {
        disconnect();
    }
    
    bool connect() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }
        
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &(serverAddr.sin_addr));
        
        result = ::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (result == SOCKET_ERROR) {
            std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
            closesocket(socket_);
            WSACleanup();
            return false;
        }
        
        std::string infoMessage = receiveMessage();
        if (infoMessage.empty() || infoMessage.substr(0, 4) != "INFO") {
            std::cerr << "Invalid INFO message: " << infoMessage << std::endl;
            closesocket(socket_);
            WSACleanup();
            return false;
        }
        
        sendMessage("CONNECT {}\r\n");
        
        std::string okMessage = receiveMessage();
        if (okMessage != "+OK\r\n") {
            std::cerr << "Invalid response to CONNECT: " << okMessage << std::endl;
            closesocket(socket_);
            WSACleanup();
            return false;
        }
        
        connected_ = true;
        
        receiveThread_ = std::thread(&NATSClient::receiveLoop, this);
        
        return true;
    }
    
    void disconnect() {
        if (!connected_) {
            return;
        }
        
        connected_ = false;
        
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
        
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        
        WSACleanup();
    }
    
    bool subscribe(const std::string& subject, std::function<void(const std::string&, const std::string&)> callback) {
        if (!connected_) {
            return false;
        }
        
        std::string sid = std::to_string(nextSid_++);
        
        std::string subMessage = "SUB " + subject + " " + sid + "\r\n";
        if (!sendMessage(subMessage)) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks_[sid] = callback;
        
        return true;
    }
    
    bool publish(const std::string& subject, const std::string& message) {
        if (!connected_) {
            return false;
        }
        
        std::string pubMessage = "PUB " + subject + " " + std::to_string(message.length()) + "\r\n" + message + "\r\n";
        return sendMessage(pubMessage);
    }
    
private:
    std::string host_;
    int port_;
    SOCKET socket_;
    std::atomic<bool> connected_;
    std::thread receiveThread_;
    int nextSid_;
    
    std::unordered_map<std::string, std::function<void(const std::string&, const std::string&)>> callbacks_;
    std::mutex callbacksMutex_;
    
    bool sendMessage(const std::string& message) {
        int result = send(socket_, message.c_str(), static_cast<int>(message.size()), 0);
        return result != SOCKET_ERROR;
    }
    
    std::string receiveMessage() {
        char buffer[4096];
        int bytesReceived = recv(socket_, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived <= 0) {
            return "";
        }
        
        buffer[bytesReceived] = '\0';
        return std::string(buffer, bytesReceived);
    }
    
    void receiveLoop() {
        while (connected_) {
            std::string message = receiveMessage();
            if (message.empty()) {
                continue;
            }
            
            if (message.substr(0, 3) == "MSG") {
                size_t firstSpace = message.find(' ');
                size_t secondSpace = message.find(' ', firstSpace + 1);
                size_t thirdSpace = message.find(' ', secondSpace + 1);
                size_t fourthSpace = message.find(' ', thirdSpace + 1);
                size_t headerEnd = message.find("\r\n");
                
                if (firstSpace == std::string::npos || secondSpace == std::string::npos ||
                    thirdSpace == std::string::npos || headerEnd == std::string::npos) {
                    continue;
                }
                
                std::string subject = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                std::string sid = message.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
                
                size_t payloadSize;
                size_t payloadStart;
                
                if (fourthSpace != std::string::npos && fourthSpace < headerEnd) {
                    payloadSize = std::stoi(message.substr(fourthSpace + 1, headerEnd - fourthSpace - 1));
                    payloadStart = headerEnd + 2;
                } else {
                    payloadSize = std::stoi(message.substr(thirdSpace + 1, headerEnd - thirdSpace - 1));
                    payloadStart = headerEnd + 2;
                }
                
                std::string payload = message.substr(payloadStart, payloadSize);
                
                std::lock_guard<std::mutex> lock(callbacksMutex_);
                auto it = callbacks_.find(sid);
                if (it != callbacks_.end()) {
                    it->second(subject, payload);
                }
            } else if (message == "PING\r\n") {
                sendMessage("PONG\r\n");
            }
        }
    }
};

int main() {
    NATSClient publisher("127.0.0.1", 4222);
    NATSClient subscriber("127.0.0.1", 4222);
    
    if (!publisher.connect()) {
        std::cerr << "Publisher failed to connect" << std::endl;
        return 1;
    }
    
    if (!subscriber.connect()) {
        std::cerr << "Subscriber failed to connect" << std::endl;
        publisher.disconnect();
        return 1;
    }
    
    std::cout << "Subscribing to 'greetings' topic..." << std::endl;
    subscriber.subscribe("greetings", [](const std::string& subject, const std::string& message) {
        std::cout << "Received message on " << subject << ": " << message << std::endl;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "Publishing messages..." << std::endl;
    publisher.publish("greetings", "Hello, World!");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    publisher.publish("greetings", "How are you?");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    publisher.publish("greetings", "Goodbye!");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    publisher.disconnect();
    subscriber.disconnect();
    
    return 0;
} 