#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace pulse_broker {

class Client;
class Subscription;

enum class CommandType {
    UNKNOWN,
    CONNECT,
    PING,
    PONG,
    SUB,
    PUB,
    UNSUB,
    MSG,
    INFO,
    OK
};

struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::string subject;
    std::string sid;
    std::string replyTo;
    std::string payload;
    size_t payloadSize = 0;
    std::unordered_map<std::string, std::string> options;
};

class NATSProtocolParser {
public:
    NATSProtocolParser() = default;
    ~NATSProtocolParser() = default;

    bool parse(const std::string& buffer, Command& command);

    std::string generateMessage(const Command& command);
    
    std::string generateInfoMessage(const std::string& host, int port, const std::string& clientIP);
    std::string generateOkMessage();
    std::string generatePongMessage();
    std::string generateMsgMessage(const std::string& subject, const std::string& sid, 
                                 const std::string& replyTo, const std::string& payload);

private:
    bool parseConnect(const std::vector<std::string>& tokens, Command& command);
    bool parsePing(const std::vector<std::string>& tokens, Command& command);
    bool parsePong(const std::vector<std::string>& tokens, Command& command);
    bool parseSub(const std::vector<std::string>& tokens, Command& command);
    bool parsePub(const std::vector<std::string>& tokens, const std::string& buffer, size_t headerEnd, Command& command);
    bool parseUnsub(const std::vector<std::string>& tokens, Command& command);
    
    std::vector<std::string> tokenize(const std::string& line);
};

} // namespace pulse_broker 