#include "../include/Subscription.h"
#include "../include/Client.h"
#include "../include/NATSProtocolParser.h"

namespace pulse_broker {

Subscription::Subscription(std::weak_ptr<Client> client, const std::string& subject, const std::string& sid)
    : client_(client), subject_(subject), sid_(sid) {
}

bool Subscription::deliverMessage(const std::string& subject, const std::string& sid, 
                               const std::string& replyTo, const std::string& payload) {
    auto client = client_.lock();
    if (!client) {
        return false;
    }

    NATSProtocolParser parser;
    std::string message = parser.generateMsgMessage(subject, sid, replyTo, payload);
    
    return client->sendMessage(message);
}

} // namespace pulse_broker 