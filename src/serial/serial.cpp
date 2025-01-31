//
// Created by April White on 1/26/25.
//

#include <sstream>
#include <vector>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <string>
#include <iomanip>
#include <iostream>


#include "namespace-stuffs.h"

#include "serial.h"

void setupSerialPort(int serial_port) {
    struct termios tty;

    debug("configuring the serial port to 9600 N81");

    // Read in existing settings, and handle any error
    if (tcgetattr(serial_port, &tty) != 0) {
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
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                     ICRNL); // Disable any special handling of received bytes

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


void readFromFileDescriptor(int file_descriptor) {
    uint8_t read_buf[256];
    memset(&read_buf, '\0', sizeof(read_buf));
    int num_bytes = read(file_descriptor, &read_buf, sizeof(read_buf));

    //trace("num_bytes is {}", num_bytes);
    if (num_bytes < 0) {
        error("Error reading: {}", strerror(errno));
    } else {
        if (num_bytes > 0) {
            debug("Read {} bytes", num_bytes);
            std::vector<std::string> byteString = bytesToHexStrings(read_buf, num_bytes);

            std::cout << "Read: [";
            for (size_t i = 0; i < byteString.size(); ++i) {
                std::cout << "0x" << byteString[i];
                if (i < byteString.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << ']' << std::endl;
        }
    }

}

// Function to convert hex strings to bytes
std::vector<uint8_t> hexStringsToBytes(const std::vector<std::string> &hexStrings) {
    std::vector<uint8_t> bytes;
    for (const auto &hexValue: hexStrings) {
        unsigned int buffer;
        std::istringstream hexStream(hexValue);

        hexStream >> std::hex >> buffer;
        bytes.push_back(static_cast<uint8_t>(buffer));
    }

    return bytes;
}

// Function to write bytes to serial port
void writeToSerial(int serial_port, const std::vector<uint8_t> &bytes) {
    if (!bytes.empty()) {
        int num_bytes = write(serial_port, bytes.data(), bytes.size());

        if (num_bytes < 0) {
            error("Error writing to serial port: {}", strerror(errno));
        } else {
            info("Wrote {} bytes to the serial port", num_bytes);
        }
    } else {
        error("No bytes to write");
    }
}

std::vector<std::string> bytesToHexStrings(const uint8_t *bytes, size_t size) {
    std::vector<std::string> hexStrings;
    hexStrings.reserve(size); // Reserve space for efficiency

    for (size_t i = 0; i < size; ++i) {
        std::stringstream hexStream;
        hexStream << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
        hexStrings.push_back(hexStream.str());
    }

    return hexStrings;
}
