#pragma once

#include <string>
#include <memory>

namespace pulse_broker {

class Client;

class Subscription {
public:
    Subscription(std::weak_ptr<Client> client, const std::string& subject, const std::string& sid);
    ~Subscription() = default;

    std::string getSubject() const { return subject_; }
    std::string getSID() const { return sid_; }
    std::weak_ptr<Client> getClient() const { return client_; }
    
    bool deliverMessage(const std::string& subject, const std::string& sid, 
                       const std::string& replyTo, const std::string& payload);

private:
    std::weak_ptr<Client> client_;
    std::string subject_;
    std::string sid_;
};

} // namespace pulse_broker 