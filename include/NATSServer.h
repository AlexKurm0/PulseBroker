#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "NATSProtocolParser.h"

namespace pulse_broker {

class Client;
class Subscription;

class NATSServer {
public:
    NATSServer(const std::string& host = "0.0.0.0", int port = 4222);
    ~NATSServer();

    bool start();
    void stop();
    bool isRunning() const { return running_; }

    bool subscribe(std::shared_ptr<Client> client, const std::string& subject, const std::string& sid);
    bool unsubscribe(std::shared_ptr<Client> client, const std::string& sid);
    
    bool publish(const std::string& subject, const std::string& message, const std::string& replyTo = "");
    
    void addClient(std::shared_ptr<Client> client);
    void removeClient(std::shared_ptr<Client> client);

private:
    std::string host_;
    int port_;
    int serverSocket_;
    std::atomic<bool> running_;
    
    NATSProtocolParser parser_;
    
    std::vector<std::shared_ptr<Client>> clients_;
    std::mutex clientsMutex_;
    
    std::unordered_map<std::string, std::vector<std::shared_ptr<Subscription>>> subscriptions_;
    std::mutex subscriptionsMutex_;
    
    std::thread acceptThread_;
    std::vector<std::thread> clientThreads_;
    std::mutex threadsMutex_;
    
    void acceptConnections();
    void handleClient(std::shared_ptr<Client> client);
    void processCommand(std::shared_ptr<Client> client, const Command& command);
    
    void deliverMessageToSubscribers(const std::string& subject, const std::string& payload, const std::string& replyTo = "");
};

} // namespace pulse_broker 