#include "../include/NATSServer.h"
#include <iostream>
#include <csignal>
#include <string>

using namespace pulse_broker;

static NATSServer* server = nullptr;

void signalHandler(int signal) {
    if (server) {
        std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
        server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 4222;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid port number: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --host <host>     Server host (default: 0.0.0.0)" << std::endl;
            std::cout << "  --port <port>     Server port (default: 4222)" << std::endl;
            std::cout << "  --help            Show this help message" << std::endl;
            return 0;
        }
    }
    
    server = new NATSServer(host, port);
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (!server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        delete server;
        return 1;
    }
    
    while (server->isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    delete server;
    return 0;
} 