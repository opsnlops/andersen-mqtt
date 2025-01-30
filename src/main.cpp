
#include <csignal>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <string>
#include <iomanip>
#include <iostream>

// spdlog
#include "spdlog/sinks/stdout_color_sinks.h"

#include <nlohmann/json.hpp>

#include "namespace-stuffs.h"

#include "mqtt/mqtt.h"
#include "mqtt/log_wrapper.h"
#include "socket/socket.h"
#include "window/window.h"

#include "blockingconcurrentqueue.h"


// Old school pointer so the signal handler knows what to do
creatures::MQTTClient* mqttClient = nullptr;
std::atomic<bool> keepRunning(true);

std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> outgoingSocketMessages;
std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> incomingSocketMessages;


std::shared_ptr<creatures::Window> window1;
std::shared_ptr<creatures::Window> window2;
std::shared_ptr<creatures::Window> window3;
std::shared_ptr<creatures::Window> window4;

extern "C" void signalHandler(int signalNumber) {
    if (signalNumber == SIGINT) {

        std::cerr << "Exiting..." << std::endl;

        // Stop the serial reader
        keepRunning = false;

        // Stop the MQTT client
        if (mqttClient) {
            mqttClient->stop();
        }

    }
}

// Helper to join strings for logging
std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter = ", ") {
    std::ostringstream oss;
    for (size_t i = 0; i < strings.size(); ++i) {
        oss << strings[i];
        if (i < strings.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

void reader_thread(int socket_fd) {
    std::vector<uint8_t> buffer; // Persistent buffer for accumulating bytes
    while (keepRunning) {
        std::vector<uint8_t> tempBuffer(1024); // Temporary buffer for reading
        ssize_t bytes_received = recv(socket_fd, tempBuffer.data(), tempBuffer.size(), 0);

        debug("Received {} bytes", bytes_received);

        if (bytes_received <= 0) {
            std::cerr << "Connection closed or error occurred\n";
            error("Connection closed or error occurred");
            keepRunning = false;
            break;
        }

        // Append newly received bytes to the persistent buffer
        buffer.insert(buffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytes_received);

        // Process complete messages
        while (!buffer.empty()) {
            // Synchronize to the header byte (0xFF)
            auto it = std::find(buffer.begin(), buffer.end(), 0xFF);
            if (it != buffer.begin()) {
                if (it == buffer.end()) {
                    debug("No valid header found. Clearing buffer.");
                    buffer.clear();
                    break;
                }
                debug("Synchronizing buffer. Removing {} invalid bytes.", std::distance(buffer.begin(), it));
                buffer.erase(buffer.begin(), it); // Remove invalid bytes
            }

            // Ensure we have enough bytes to determine the message type (at least 3 bytes)
            if (buffer.size() < 3) {
                debug("Not enough bytes to determine message type. Waiting for more data...");
                break;
            }

            // Determine the expected message size
            size_t expectedSize = 0;
            uint8_t messageType = buffer[2]; // Assuming the third byte is the message type

            if (messageType == CMD_STATUS_WITHOUT_POLL || messageType == CMD_STATUS_WITH_POLL) { // STATUS message (always all windows)
                expectedSize = 8; // Always 8 bytes for STATUS with all windows
                debug("Detected STATUS message for all windows. Expected size: 8 bytes.");
            } else if (messageType == 0xB1 || messageType == 0x27) { // ACK or BUSY
                expectedSize = 5;
                debug("Detected ACK or BUSY message. Expected size: 5 bytes.");
            } else {
                debug("Unknown message type: 0x{:02X}. Discarding header byte.", messageType);
                buffer.erase(buffer.begin()); // Remove the header and try again
                continue;
            }

            // If we don't have enough bytes for the full message, wait for more
            if (buffer.size() < expectedSize) {
                debug("Not enough bytes yet. Expected {}, but have {}.", expectedSize, buffer.size());
                break;
            }

            // Extract the full message
            std::vector<uint8_t> message(buffer.begin(), buffer.begin() + expectedSize);

            // Debug: Log the full message being processed
            debug("Processing full message: [{}]", joinStrings(creatures::Window::bytesToHexStrings(message)));

            // Create a slice of the message that excludes the checksum byte
            std::vector<uint8_t> messageWithoutChecksum(message.begin(), message.end());

            // Validate the checksum
            if (!creatures::Window::validateChecksum(messageWithoutChecksum)) {
                debug("Checksum validation failed. Message: [{}], Calculated checksum: 0x{:x}, Provided checksum: 0x{:x}",
                      joinStrings(creatures::Window::bytesToHexStrings(messageWithoutChecksum)),
                      creatures::Window::calculateChecksum(messageWithoutChecksum),
                      message.back());
                std::cerr << "Invalid checksum received. Discarding message.\n";
                buffer.erase(buffer.begin(), buffer.begin() + expectedSize); // Remove processed bytes
                continue;
            }

            // Log the valid message
            debug("Valid message received: [{}]", joinStrings(creatures::Window::bytesToHexStrings(message)));

            // Enqueue the valid message
            incomingSocketMessages->enqueue(std::move(message));

            // Remove the processed message from the buffer
            buffer.erase(buffer.begin(), buffer.begin() + expectedSize);
        }
    }
}

void writer_thread(int socket_fd) {
    while (keepRunning) {
        std::vector<uint8_t> message;
        if (outgoingSocketMessages->wait_dequeue_timed(message, std::chrono::seconds(1))) {
            if(!message.empty()) {
                debug("Sending message of size {}", message.size());
                send(socket_fd, message.data(), message.size(), 0); // Send binary data
            }
            else
                debug("No message to send");
        }
    }
}

void process_message_thread() {

    // Force a publish the first time around
    bool firstRun = true;

    while (keepRunning) {
        std::vector<uint8_t> message;

        // Block until a new message is available in the queue
        incomingSocketMessages->wait_dequeue(message);

        // Log the received message
        debug("Processing message: [{}]", joinStrings(creatures::Window::bytesToHexStrings(message)));

        // Ensure the message has at least 3 bytes (enough to determine the type)
        if (message.size() < 3) {
            error("Message too short: {} bytes. Ignoring.", message.size());
            continue;
        }

        // Determine the message type
        uint8_t messageType = message[2];

        switch (messageType) {
            case 0x5C: { // STATUS message
                debug("Detected STATUS message.");

                // Ensure the message is the correct size for a STATUS message
                if (message.size() != 8) {
                    error("Invalid STATUS message size: {}. Expected 8 bytes.", message.size());
                    break;
                }

                // Extract the window statuses
                uint8_t window1Status = message[3];
                uint8_t window2Status = message[4];
                uint8_t window3Status = message[5];
                uint8_t window4Status = message[6];

                // Update the window statuses
                window1->setStatus(window1Status);
                window2->setStatus(window2Status);
                window3->setStatus(window3Status);
                window4->setStatus(window4Status);

                // Log the updated statuses
                debug("Updated window statuses");
                debug("Window 1: {}", window1->toJson());
                debug("Window 2: {}", window2->toJson());
                debug("Window 3: {}", window3->toJson());
                debug("Window 4: {}", window4->toJson());

                // Publish this update on MQTT
                mqttClient->publishWindows(firstRun);
                firstRun = false;
                break;
            }

            case 0xB1: { // ACK message
                debug("Detected ACK message.");
                // Handle ACK-specific processing here
                break;
            }

            case 0x27: { // BUSY message
                debug("Detected BUSY message.");
                // Handle BUSY-specific processing here
                break;
            }

            default:
                debug("Unknown message type: 0x{:02X}. Ignoring message.", messageType);
                break;
        }
    }
}


int main() {

    std::signal(SIGINT, signalHandler);

    try {
        // Set up our locale. If this vomits, install `locales-all`
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch (const std::runtime_error &e) {
        critical("Unable to set the locale: '{}' (Hint: Make sure package locales-all is installed!)", e.what());
        return EXIT_FAILURE;
    }

    // Console logger
    spdlog::set_level(spdlog::level::trace);

    info("Welcome to Andersen to MQTT! 🪟");


    // Leave some version info to be found
    debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    debug("fmt version {}", FMT_VERSION);
    debug("json version {}.{}.{}", NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH);

    // Set up our boost -> spdlog wrapper
    init_boost_logging();
    MQTT_NS::setup_log();

    // Make our queues
    outgoingSocketMessages = std::make_shared<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>>();
    incomingSocketMessages = std::make_shared<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>>();

    // Make the windows
    window1 = std::make_shared<creatures::Window>("window1", 1);
    window2 = std::make_shared<creatures::Window>("window2", 2);
    window3 = std::make_shared<creatures::Window>("window3", 3);
    window4 = std::make_shared<creatures::Window>("window4", 4);


    mqttClient = new creatures::MQTTClient("10.3.2.5", "1883");
    mqttClient->addWindow(window1);
    mqttClient->addWindow(window2);
    mqttClient->addWindow(window3);
    mqttClient->addWindow(window4);

    mqttClient->start();


    const char* host = "10.3.2.5";
    int port = 6000;

    int socket_fd = connect_to_server(host, port);
    if (socket_fd < 0) {
        return 1;
    }



    std::thread reader(reader_thread, socket_fd);
    std::thread writer(writer_thread, socket_fd);
    std::thread processor(process_message_thread);


    // Define a vector of hex string values
    //std::vector<std::string> hexStrings = {SRC_CONTROLLER, DST_PANEL_1, WINDOW_ALL, CMD_STATUS_WITHOUT_POLL, "0x62"};



    // Attempt to open a window
    //std::vector<uint8_t> openWindowOne = {SRC_CONTROLLER, DST_PANEL_1, WINDOW_ALL, CMD_CLOSE};
    //checksum = calculateChecksum(openWindowOne);
    //openWindowOne.push_back(checksum);
    //writeToSerial(serial_port, openWindowOne);



    // Do something else or just wait here
    int count = 0;
    while(keepRunning /*&& count++ < 25*/) {
        debug("Polling all windows...");
        std::vector<uint8_t> event = {SRC_CONTROLLER, DST_PANEL_1, WINDOW_ALL, CMD_STATUS_WITHOUT_POLL};
        auto ck = creatures::Window::calculateChecksum(event);
        event.push_back(ck);
        outgoingSocketMessages->enqueue(std::move(event));

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
    keepRunning = false; // Tell the threads to finish up

    // Signal to the writer to stop
    outgoingSocketMessages->enqueue({});

    // Make sure to join your threads
    if (mqttClient) {
        mqttClient->stop();
    }
    reader.join();
    writer.join();

    close(socket_fd);

    return 0;
}