
#include <algorithm>
#include <csignal>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <iomanip>
#include <iostream>

// spdlog
#include "spdlog/sinks/stdout_color_sinks.h"

#include <nlohmann/json.hpp>

#include "namespace-stuffs.h"

#include "mqtt/mqtt.h"
#include "mqtt/log_wrapper.h"
#include "config/config.h"
#include "socket/socket.h"
#include "window/window.h"

#include "blockingconcurrentqueue.h"


creatures::MQTTClient* mqttClient = nullptr;

// Cleared to shut down. A signal clears it for a normal exit; losing the
// gateway or the broker clears it and sets a failure status, so that systemd
// starts the program again and it reconnects.
std::atomic<bool> keepRunning(true);
std::atomic<int> exitStatus(EXIT_SUCCESS);

// When the last status reply arrived, as steady_clock ticks
std::atomic<std::chrono::steady_clock::rep> lastStatus(0);

void shutDownWithFailure(const std::string &reason) {
    if (keepRunning.exchange(false)) {
        error("{}; shutting down", reason);
        exitStatus = EXIT_FAILURE;
    }
}

std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> outgoingSocketMessages;
std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> incomingSocketMessages;


std::shared_ptr<creatures::Window> window1;
std::shared_ptr<creatures::Window> window2;
std::shared_ptr<creatures::Window> window3;
std::shared_ptr<creatures::Window> window4;

// Only async-signal-safe work here: main notices and does the rest.
extern "C" void signalHandler(int) {
    keepRunning = false;
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
            // During shutdown main closes the socket under us, which isn't a failure
            if (keepRunning) {
                shutDownWithFailure(bytes_received == 0 ? "the gateway closed the connection"
                                                        : fmt::format("error reading from the gateway: {}",
                                                                      strerror(errno)));
            }
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

            // Validate the checksum. On a mismatch drop only the header byte and
            // resync: the real start of a frame may be inside this one.
            if (!creatures::Window::validateChecksum(message)) {
                warn("Invalid checksum; discarding a header byte. Message: [{}]",
                     joinStrings(creatures::Window::bytesToHexStrings(message)));
                buffer.erase(buffer.begin());
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
                // MSG_NOSIGNAL: a gateway that hung up should end in an error, not SIGPIPE
                if (send(socket_fd, message.data(), message.size(), MSG_NOSIGNAL) < 0) {
                    shutDownWithFailure(fmt::format("error writing to the gateway: {}", strerror(errno)));
                }
            }
            else
                debug("No message to send");
        }
    }
}

void process_message_thread() {

    while (keepRunning) {
        std::vector<uint8_t> message;

        // Wait for a message, waking up now and then to notice a shutdown
        if (!incomingSocketMessages->wait_dequeue_timed(message, std::chrono::seconds(1))) {
            continue;
        }

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
            case CMD_STATUS_WITHOUT_POLL:
            case CMD_STATUS_WITH_POLL: { // STATUS message
                debug("Detected STATUS message.");

                // Ensure the message is the correct size for a STATUS message
                if (message.size() != 8) {
                    error("Invalid STATUS message size: {}. Expected 8 bytes.", message.size());
                    break;
                }

                lastStatus = std::chrono::steady_clock::now().time_since_epoch().count();

                // One status byte per window. The MQTT thread applies and publishes them.
                mqttClient->updateWindows({message.begin() + 3, message.begin() + 7});
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


int main(int argc, char **argv) {

    // Parse the command line first: help2man builds the manual from --help, so
    // nothing may reach stdout before this.
    creatures::Config config;
    try {
        config = creatures::loadConfiguration(argc, argv);
    }
    catch (const std::exception &e) {
        std::cerr << "andersen-mqtt: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Set up our locale. If this vomits, install `locales-all`
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch (const std::runtime_error &e) {
        critical("Unable to set the locale: '{}' (Hint: Make sure package locales-all is installed!)", e.what());
        return EXIT_FAILURE;
    }

    // Console logger
    spdlog::set_level(config.logLevel);

    info("Welcome to Andersen to MQTT {}! 🪟", ANDERSEN_MQTT_VERSION);


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
    window1 = std::make_shared<creatures::Window>("window1", 1, config.topicPrefix);
    window2 = std::make_shared<creatures::Window>("window2", 2, config.topicPrefix);
    window3 = std::make_shared<creatures::Window>("window3", 3, config.topicPrefix);
    window4 = std::make_shared<creatures::Window>("window4", 4, config.topicPrefix);


    mqttClient = new creatures::MQTTClient(config.mqttHost, std::to_string(config.mqttPort), config.mqttClientId,
                                           [](const std::string &reason) { shutDownWithFailure(reason); });
    mqttClient->addWindow(window1);
    mqttClient->addWindow(window2);
    mqttClient->addWindow(window3);
    mqttClient->addWindow(window4);

    try {
        mqttClient->start();
    }
    catch (const std::exception &e) {
        critical("Unable to connect to the MQTT broker at {}:{}: {}", config.mqttHost, config.mqttPort, e.what());
        mqttClient->stop();
        return EXIT_FAILURE;
    }

    int socket_fd = connect_to_server(config.gatewayHost, config.gatewayPort);
    if (socket_fd < 0) {
        mqttClient->stop();
        return EXIT_FAILURE;
    }

    std::thread reader(reader_thread, socket_fd);
    std::thread writer(writer_thread, socket_fd);
    std::thread processor(process_message_thread);

    // A gateway can stop answering without closing the connection, so give up
    // if no status arrives for a while, and let systemd connect again.
    const auto statusTimeout = std::max(std::chrono::seconds(30),
                                        std::chrono::seconds(3 * config.pollIntervalSeconds));
    lastStatus = std::chrono::steady_clock::now().time_since_epoch().count();

    // Poll the windows until something asks us to stop
    auto nextPoll = std::chrono::steady_clock::now();
    while (keepRunning) {
        auto sinceStatus = std::chrono::steady_clock::now() -
                           std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(lastStatus.load()));
        if (sinceStatus > statusTimeout) {
            shutDownWithFailure(fmt::format("no status from the window controller in {} seconds",
                                            std::chrono::duration_cast<std::chrono::seconds>(statusTimeout).count()));
            break;
        }
        if (std::chrono::steady_clock::now() >= nextPoll) {
            debug("Polling all windows...");
            std::vector<uint8_t> event = {SRC_CONTROLLER, DST_PANEL_1, WINDOW_ALL, CMD_STATUS_WITHOUT_POLL};
            auto ck = creatures::Window::calculateChecksum(event);
            event.push_back(ck);
            outgoingSocketMessages->enqueue(std::move(event));
            nextPoll += std::chrono::seconds(config.pollIntervalSeconds);
        }
        // Short sleeps so a signal or a lost connection is noticed quickly
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    info("stopping");

    mqttClient->stop();

    // Wake the reader out of recv(), then wait for every thread
    shutdown(socket_fd, SHUT_RDWR);
    reader.join();
    writer.join();
    processor.join();
    close(socket_fd);

    delete mqttClient;
    mqttClient = nullptr;

    info("stopped{}", exitStatus == EXIT_SUCCESS ? "" : " after a failure");
    return exitStatus;
}