cmake_minimum_required(VERSION 3.10)
project(PulseBroker)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/NATSProtocolParser.cpp
    src/Client.cpp
    src/Subscription.cpp
    src/NATSServer.cpp
    src/main.cpp
)

add_executable(pulse_broker ${SOURCES})

if(WIN32)
    target_link_libraries(pulse_broker ws2_32)
endif()

set(TEST_SOURCES
    test/test_parser.cpp
    test/test_server.cpp
)

add_executable(pulse_broker_tests ${TEST_SOURCES} 
    src/NATSProtocolParser.cpp
    src/Client.cpp
    src/Subscription.cpp
    src/NATSServer.cpp
)

if(WIN32)
    target_link_libraries(pulse_broker_tests ws2_32)
endif()

set(EXAMPLE_SOURCES
    examples/publisher_subscriber.cpp
)

add_executable(publisher_subscriber ${EXAMPLE_SOURCES})

if(WIN32)
    target_link_libraries(publisher_subscriber ws2_32)
endif() 