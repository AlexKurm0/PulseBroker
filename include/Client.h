#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace pulse_broker {

class Subscription;

class Client {
public:
    Client(int socket, const std::string& host, const std::string& ip);
    ~Client();

    bool sendMessage(const std::string& message);
    std::string receiveMessage();
    
    bool addSubscription(const std::string& subject, const std::string& sid);
    bool removeSubscription(const std::string& sid);
    bool hasSubscription(const std::string& subject) const;
    std::shared_ptr<Subscription> getSubscription(const std::string& sid) const;

    int getSocket() const { return socket_; }
    std::string getHost() const { return host_; }
    std::string getIP() const { return ip_; }
    bool isConnected() const { return connected_; }
    
    void disconnect();

private:
    int socket_;
    std::string host_;
    std::string ip_;
    bool connected_;
    
    std::unordered_map<std::string, std::shared_ptr<Subscription>> subscriptions_;
    
    mutable std::mutex mutex_;
};

} // namespace pulse_broker 