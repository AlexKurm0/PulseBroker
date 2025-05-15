#include "../include/NATSProtocolParser.h"
#include <sstream>
#include <algorithm>

namespace pulse_broker {

std::vector<std::string> NATSProtocolParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool NATSProtocolParser::parse(const std::string& buffer, Command& command) {
    command.type = CommandType::UNKNOWN;
    
    size_t headerEnd = buffer.find("\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }
    
    std::string header = buffer.substr(0, headerEnd);
    std::vector<std::string> tokens = tokenize(header);
    
    if (tokens.empty()) {
        return false;
    }
    
    std::string cmdStr = tokens[0];
    
    if (cmdStr == "CONNECT") {
        return parseConnect(tokens, command);
    } else if (cmdStr == "PING") {
        return parsePing(tokens, command);
    } else if (cmdStr == "PONG") {
        return parsePong(tokens, command);
    } else if (cmdStr == "SUB") {
        return parseSub(tokens, command);
    } else if (cmdStr == "PUB") {
        return parsePub(tokens, buffer, headerEnd, command);
    } else if (cmdStr == "UNSUB") {
        return parseUnsub(tokens, command);
    }
    
    return false;
}

bool NATSProtocolParser::parseConnect(const std::vector<std::string>& tokens, Command& command) {
    if (tokens.size() < 2) {
        return false;
    }
    
    command.type = CommandType::CONNECT;
    command.options["connect"] = "{}";
    
    return true;
}

bool NATSProtocolParser::parsePing(const std::vector<std::string>& tokens, Command& command) {
    command.type = CommandType::PING;
    return true;
}

bool NATSProtocolParser::parsePong(const std::vector<std::string>& tokens, Command& command) {
    command.type = CommandType::PONG;
    return true;
}

bool NATSProtocolParser::parseSub(const std::vector<std::string>& tokens, Command& command) {
    if (tokens.size() < 3) {
        return false;
    }
    
    command.type = CommandType::SUB;
    command.subject = tokens[1];
    
    if (tokens.size() > 3) {
        command.options["queue_group"] = tokens[2];
        command.sid = tokens[3];
    } else {
        command.sid = tokens[2];
    }
    
    return true;
}

bool NATSProtocolParser::parsePub(const std::vector<std::string>& tokens, const std::string& buffer, size_t headerEnd, Command& command) {
    if (tokens.size() < 3) {
        return false;
    }
    
    command.type = CommandType::PUB;
    command.subject = tokens[1];
    
    size_t payloadSize;
    
    if (tokens.size() > 3) {
        command.replyTo = tokens[2];
        try {
            payloadSize = std::stoi(tokens[3]);
        } catch (const std::exception&) {
            return false;
        }
    } else {
        try {
            payloadSize = std::stoi(tokens[2]);
        } catch (const std::exception&) {
            return false;
        }
    }
    
    command.payloadSize = payloadSize;
    
    if (buffer.size() < headerEnd + 2 + payloadSize + 2) {
        return false;
    }
    
    command.payload = buffer.substr(headerEnd + 2, payloadSize);
    
    return true;
}

bool NATSProtocolParser::parseUnsub(const std::vector<std::string>& tokens, Command& command) {
    if (tokens.size() < 2) {
        return false;
    }
    
    command.type = CommandType::UNSUB;
    command.sid = tokens[1];
    
    if (tokens.size() > 2) {
        command.options["max_msgs"] = tokens[2];
    }
    
    return true;
}

std::string NATSProtocolParser::generateMessage(const Command& command) {
    switch (command.type) {
        case CommandType::INFO:
            return generateInfoMessage("0.0.0.0", 4222, "0.0.0.0");
        case CommandType::OK:
            return generateOkMessage();
        case CommandType::PONG:
            return generatePongMessage();
        case CommandType::MSG:
            return generateMsgMessage(command.subject, command.sid, command.replyTo, command.payload);
        default:
            return "";
    }
}

std::string NATSProtocolParser::generateInfoMessage(const std::string& host, int port, const std::string& clientIP) {
    std::ostringstream oss;
    oss << "INFO {\"host\":\"" << host << "\",\"port\":" << port << ",\"client_ip\":\"" << clientIP << "\"}\r\n";
    return oss.str();
}

std::string NATSProtocolParser::generateOkMessage() {
    return "+OK\r\n";
}

std::string NATSProtocolParser::generatePongMessage() {
    return "PONG\r\n";
}

std::string NATSProtocolParser::generateMsgMessage(const std::string& subject, const std::string& sid, 
                                              const std::string& replyTo, const std::string& payload) {
    std::ostringstream oss;
    oss << "MSG " << subject << " " << sid;
    
    if (!replyTo.empty()) {
        oss << " " << replyTo;
    }
    
    oss << " " << payload.size() << "\r\n" << payload << "\r\n";
    return oss.str();
}

} // namespace pulse_broker 