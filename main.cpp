
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using spdlog::trace;
using spdlog::debug;
using spdlog::info;
using spdlog::warn;
using spdlog::error;
using spdlog::critical;


std::atomic<bool> keepRunning(true);

void setupSerialPort(int serial_port) {
    struct termios tty;

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
}


void readFromSerial(int serial_port) {
    char read_buf[256];
    while (keepRunning) {
        memset(&read_buf, '\0', sizeof(read_buf));
        int num_bytes = read(serial_port, &read_buf, sizeof(read_buf));

        //trace("num_bytes is {}", num_bytes);
        if (num_bytes < 0) {
            error("Error reading: {}", strerror(errno));
        } else {
            if(num_bytes > 0) {
                debug("Read {} bytes. Received message: {}", num_bytes, read_buf);
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