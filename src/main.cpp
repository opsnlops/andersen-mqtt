
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string>
#include <iomanip>
#include <iostream>

// spdlog
#include "spdlog/sinks/stdout_color_sinks.h"

#include <nlohmann/json.hpp>

#include "namespace-stuffs.h"

#include "mqtt/mqtt.h"
#include "mqtt/log_wrapper.h"
#include "window/window.h"


std::atomic<bool> keepRunning(true);

void setupSerialPort(int serial_port) {
    struct termios tty;

    debug("configuring the serial port to 9600 N81");

    // Read in existing settings, and handle any error
    if(tcgetattr(serial_port, &tty) != 0) {
        error("Error {} from tcgetattr: {}", errno, strerror(errno));
    }

    // Set Baud Rate to 9600
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    // 8 bits per byte (most common)
    tty.c_cflag &= ~PARENB; // No parity bit
    tty.c_cflag &= ~CSTOPB; // Only one stop bit
    tty.c_cflag &= ~CSIZE;  // Clear all the size bits
    tty.c_cflag |= CS8;     // 8 bits per byte

    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;    // Disable echo
    tty.c_lflag &= ~ECHOE;   // Disable erasure
    tty.c_lflag &= ~ECHONL;  // Disable new-line echo
    tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off software flow control
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g., newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    // VMIN and VTIME are used to control block timing. Here we set VMIN to 0 and VTIME to 1,
    // making read non-blocking. The read function will return immediately if there is nothing to read.
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        error("Error {} from tcgetattr: {}", errno, strerror(errno));
    }

    debug("serial port configured");
}


std::vector<std::string> bytesToHexStrings(const uint8_t* bytes, size_t size) {
    std::vector<std::string> hexStrings;
    hexStrings.reserve(size); // Reserve space for efficiency

    for (size_t i = 0; i < size; ++i) {
        std::stringstream hexStream;
        hexStream << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
        hexStrings.push_back(hexStream.str());
    }

    return hexStrings;
}

void readFromSerial(int serial_port) {
    uint8_t read_buf[256];
    while (keepRunning) {
        memset(&read_buf, '\0', sizeof(read_buf));
        int num_bytes = read(serial_port, &read_buf, sizeof(read_buf));

        //trace("num_bytes is {}", num_bytes);
        if (num_bytes < 0) {
            error("Error reading: {}", strerror(errno));
        } else {
            if(num_bytes > 0) {
                debug("Read {} bytes", num_bytes);
                std::vector<std::string> byteString = bytesToHexStrings(read_buf, num_bytes);

                std::cout << '[';
                for (size_t i = 0; i < byteString.size(); ++i) {
                    std::cout << "0x" << byteString[i];
                    if (i < byteString.size() - 1) {
                        std::cout << ", ";
                    }
                }
                std::cout << ']' << std::endl;
            }
        }

        // Sleep for a bit to avoid hogging the CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Function to convert hex strings to bytes
std::vector<uint8_t> hexStringsToBytes(const std::vector<std::string>& hexStrings) {
    std::vector<uint8_t> bytes;
    for (const auto& hexValue : hexStrings) {
        unsigned int buffer;
        std::istringstream hexStream(hexValue);

        hexStream >> std::hex >> buffer;
        bytes.push_back(static_cast<uint8_t>(buffer));
    }

    return bytes;
}

// Function to write bytes to serial port
void writeToSerial(int serial_port, const std::vector<uint8_t>& bytes) {
    while (true) { // Replace with a condition or a control variable for actual use
        if (!bytes.empty()) {
            int num_bytes = write(serial_port, bytes.data(), bytes.size());

            if (num_bytes < 0) {
                error("Error writing to serial port: {}", strerror(errno));
            } else {
                info("Wrote {} bytes to the serial port", num_bytes);
            }
        }

        // Write every 2 seconds as an example
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main() {

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

    auto window1 = std::make_shared<creatures::Window>("window1", 1);
    auto window2 = std::make_shared<creatures::Window>("window2", 2);

    window1->setStatus(0x0D);
    window2->setStatus(0x0C);

    info("window1 test: {}", window1->toJson());
    info("window2 test: {}", window2->toJson());


    creatures::MQTTClient mqtt = creatures::MQTTClient("10.3.2.5", "1883");

    mqtt.subscribe("anderson-mqtt/window1/command", MQTT_NS::qos::exactly_once);


    // Open the serial port
    debug("opening serial port");
    int serial_port = open("/dev/tty.usbserial-B000PDRX", O_RDWR | O_NONBLOCK);
    info("port is open, fd {}", serial_port);


    setupSerialPort(serial_port);

    // Create the read thread
    std::thread readThread(readFromSerial, serial_port);

    // Define a vector of hex string values
    std::vector<std::string> hexStrings = {"FF", "01", "05", "5C", "62"};

    // Convert hex strings to a byte vector
    info("Asking for window status...");
    std::vector<uint8_t> bytesToWrite = hexStringsToBytes(hexStrings);
    writeToSerial(serial_port, bytesToWrite);

    // Do something else or just wait here
    std::this_thread::sleep_for(std::chrono::seconds(10));
    keepRunning = false; // Tell the threads to finish up

    // Make sure to join your threads
    readThread.join();

    close(serial_port);

    return 0;
}