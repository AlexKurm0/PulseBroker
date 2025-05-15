#include "../include/NATSServer.h"
#include "../include/Client.h"
#include "../include/Subscription.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

namespace pulse_broker {

NATSServer::NATSServer(const std::string& host, int port)
    : host_(host), port_(port), serverSocket_(INVALID_SOCKET), running_(false) {
}

NATSServer::~NATSServer() {
    stop();
}

bool NATSServer::start() {
    if (running_) {
        return true;
    }

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }

    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    
    if (host_ == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host_.c_str(), &(serverAddr.sin_addr));
    }

    result = bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        WSACleanup();
        return false;
    }

    result = listen(serverSocket_, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        WSACleanup();
        return false;
    }

    running_ = true;

    acceptThread_ = std::thread(&NATSServer::acceptConnections, this);

    std::cout << "NATS server started on " << host_ << ":" << port_ << std::endl;
    return true;
}

void NATSServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (serverSocket_ != INVALID_SOCKET) {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            client->disconnect();
        }
        clients_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        for (auto& thread : clientThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        clientThreads_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscriptions_.clear();
    }

    WSACleanup();

    std::cout << "NATS server stopped" << std::endl;
}

void NATSServer::acceptConnections() {
    while (running_) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            }
            break;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

        auto client = std::make_shared<Client>(clientSocket, host_, clientIP);

        addClient(client);

        std::string infoMessage = parser_.generateInfoMessage(host_, port_, clientIP);
        client->sendMessage(infoMessage);

        std::lock_guard<std::mutex> lock(threadsMutex_);
        clientThreads_.emplace_back(&NATSServer::handleClient, this, client);
    }
}

void NATSServer::handleClient(std::shared_ptr<Client> client) {
    while (running_ && client->isConnected()) {
        std::string message = client->receiveMessage();
        if (message.empty()) {
            break;
        }

        Command command;
        if (parser_.parse(message, command)) {
            processCommand(client, command);
        }
    }

    removeClient(client);
}

void NATSServer::processCommand(std::shared_ptr<Client> client, const Command& command) {
    switch (command.type) {
        case CommandType::CONNECT:
            client->sendMessage(parser_.generateOkMessage());
            break;

        case CommandType::PING:
            client->sendMessage(parser_.generatePongMessage());
            break;

        case CommandType::PONG:
            break;

        case CommandType::SUB:
            if (subscribe(client, command.subject, command.sid)) {
                client->sendMessage(parser_.generateOkMessage());
            }
            break;

        case CommandType::PUB:
            if (publish(command.subject, command.payload, command.replyTo)) {
                client->sendMessage(parser_.generateOkMessage());
            }
            break;

        case CommandType::UNSUB:
            if (unsubscribe(client, command.sid)) {
                client->sendMessage(parser_.generateOkMessage());
            }
            break;

        default:
            break;
    }
}

bool NATSServer::subscribe(std::shared_ptr<Client> client, const std::string& subject, const std::string& sid) {
    if (!client->addSubscription(subject, sid)) {
        return false;
    }

    auto subscription = std::make_shared<Subscription>(
        std::weak_ptr<Client>(client), subject, sid);

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    subscriptions_[subject].push_back(subscription);

    return true;
}

bool NATSServer::unsubscribe(std::shared_ptr<Client> client, const std::string& sid) {
    if (!client->removeSubscription(sid)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    for (auto& pair : subscriptions_) {
        auto& subList = pair.second;
        auto it = std::remove_if(subList.begin(), subList.end(),
            [&](const std::shared_ptr<Subscription>& sub) {
                return sub->getSID() == sid && sub->getClient().lock() == client;
            });

        if (it != subList.end()) {
            subList.erase(it, subList.end());
            if (subList.empty()) {
                subscriptions_.erase(pair.first);
            }
            return true;
        }
    }

    return false;
}

bool NATSServer::publish(const std::string& subject, const std::string& message, const std::string& replyTo) {
    deliverMessageToSubscribers(subject, message, replyTo);
    return true;
}

void NATSServer::deliverMessageToSubscribers(const std::string& subject, const std::string& payload, const std::string& replyTo) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    auto it = subscriptions_.find(subject);
    if (it != subscriptions_.end()) {
        for (auto& subscription : it->second) {
            subscription->deliverMessage(subject, subscription->getSID(), replyTo, payload);
        }
    }
}

void NATSServer::addClient(std::shared_ptr<Client> client) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.push_back(client);
}

void NATSServer::removeClient(std::shared_ptr<Client> client) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = std::find(clients_.begin(), clients_.end(), client);
    if (it != clients_.end()) {
        clients_.erase(it);
    }
}

} // namespace pulse_broker 