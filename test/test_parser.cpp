#include "../include/NATSProtocolParser.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace pulse_broker;

#define TEST(name) void test_##name()
#define RUN_TEST(name) std::cout << "Running test: " << #name << "... "; test_##name(); std::cout << "PASSED" << std::endl;

TEST(parse_connect) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("CONNECT {}\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::CONNECT);
}

TEST(parse_ping) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("PING\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::PING);
}

TEST(parse_pong) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("PONG\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::PONG);
}

TEST(parse_sub) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("SUB FOO 1\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::SUB);
    assert(command.subject == "FOO");
    assert(command.sid == "1");
}

TEST(parse_sub_with_queue_group) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("SUB FOO BAR 1\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::SUB);
    assert(command.subject == "FOO");
    assert(command.options["queue_group"] == "BAR");
    assert(command.sid == "1");
}

TEST(parse_pub) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("PUB FOO 5\r\nHello\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::PUB);
    assert(command.subject == "FOO");
    assert(command.payload == "Hello");
    assert(command.payloadSize == 5);
}

TEST(parse_pub_with_reply_to) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("PUB FOO BAR 5\r\nHello\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::PUB);
    assert(command.subject == "FOO");
    assert(command.replyTo == "BAR");
    assert(command.payload == "Hello");
    assert(command.payloadSize == 5);
}

TEST(parse_unsub) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("UNSUB 1\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::UNSUB);
    assert(command.sid == "1");
}

TEST(parse_unsub_with_max_msgs) {
    NATSProtocolParser parser;
    Command command;
    
    bool result = parser.parse("UNSUB 1 100\r\n", command);
    
    assert(result == true);
    assert(command.type == CommandType::UNSUB);
    assert(command.sid == "1");
    assert(command.options["max_msgs"] == "100");
}

TEST(generate_info_message) {
    NATSProtocolParser parser;
    std::string message = parser.generateInfoMessage("localhost", 4222, "127.0.0.1");
    
    assert(message == "INFO {\"host\":\"localhost\",\"port\":4222,\"client_ip\":\"127.0.0.1\"}\r\n");
}

TEST(generate_ok_message) {
    NATSProtocolParser parser;
    std::string message = parser.generateOkMessage();
    
    assert(message == "+OK\r\n");
}

TEST(generate_pong_message) {
    NATSProtocolParser parser;
    std::string message = parser.generatePongMessage();
    
    assert(message == "PONG\r\n");
}

TEST(generate_msg_message) {
    NATSProtocolParser parser;
    std::string message = parser.generateMsgMessage("FOO", "1", "", "Hello");
    
    assert(message == "MSG FOO 1 5\r\nHello\r\n");
}

TEST(generate_msg_message_with_reply_to) {
    NATSProtocolParser parser;
    std::string message = parser.generateMsgMessage("FOO", "1", "BAR", "Hello");
    
    assert(message == "MSG FOO 1 BAR 5\r\nHello\r\n");
}

void parser_tests() {
    std::cout << "Running NATSProtocolParser tests...\n";
    
    RUN_TEST(parse_connect);
    RUN_TEST(parse_ping);
    RUN_TEST(parse_pong);
    RUN_TEST(parse_sub);
    RUN_TEST(parse_sub_with_queue_group);
    RUN_TEST(parse_pub);
    RUN_TEST(parse_pub_with_reply_to);
    RUN_TEST(parse_unsub);
    RUN_TEST(parse_unsub_with_max_msgs);
    
    RUN_TEST(generate_info_message);
    RUN_TEST(generate_ok_message);
    RUN_TEST(generate_pong_message);
    RUN_TEST(generate_msg_message);
    RUN_TEST(generate_msg_message_with_reply_to);
    
    std::cout << "All parser tests PASSED!\n";
} 