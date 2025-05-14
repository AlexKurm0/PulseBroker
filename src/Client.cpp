#include "../include/Client.h"
#include "../include/Subscription.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

namespace pulse_broker {

Client::Client(int socket, const std::string& host, const std::string& ip)
    : socket_(socket), host_(host), ip_(ip), connected_(true) {
}

Client::~Client() {
    disconnect();
}

bool Client::sendMessage(const std::string& message) {
    if (!connected_) {
        return false;
    }

    int result = send(socket_, message.c_str(), static_cast<int>(message.size()), 0);
    return result != SOCKET_ERROR;
}

std::string Client::receiveMessage() {
    if (!connected_) {
        return "";
    }

    char buffer[4096];
    int bytesReceived = recv(socket_, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived <= 0) {
        disconnect();
        return "";
    }
    
    buffer[bytesReceived] = '\0';
    return std::string(buffer, bytesReceived);
}

bool Client::addSubscription(const std::string& subject, const std::string& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (subscriptions_.find(sid) != subscriptions_.end()) {
        return false;
    }
    
    auto subscription = std::make_shared<Subscription>(
        std::weak_ptr<Client>{}, subject, sid);
    
    subscriptions_[sid] = subscription;
    
    return true;
}

bool Client::removeSubscription(const std::string& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = subscriptions_.find(sid);
    if (it == subscriptions_.end()) {
        return false;
    }
    
    subscriptions_.erase(it);
    return true;
}

bool Client::hasSubscription(const std::string& subject) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& pair : subscriptions_) {
        if (pair.second->getSubject() == subject) {
            return true;
        }
    }
    
    return false;
}

std::shared_ptr<Subscription> Client::getSubscription(const std::string& sid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = subscriptions_.find(sid);
    if (it != subscriptions_.end()) {
        return it->second;
    }
    
    return nullptr;
}

void Client::disconnect() {
    if (connected_) {
        connected_ = false;
        closesocket(socket_);
    }
}

} // namespace pulse_broker 