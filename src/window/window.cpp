//
// Created by April White on 11/23/23.
//


#include <bitset>
#include <string>
#include <utility>


#include "namespace-stuffs.h"

#include <nlohmann/json.hpp>

#include "window.h"

using json = nlohmann::json;


namespace creatures {

    std::string Window::timePointToISO8601(const std::chrono::system_clock::time_point& tp) const {
        auto timeT = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }


    void Window::setStatus(uint8_t statusByte) {

        debug("updating status (0x{:x}) for window {}: {}", statusByte, this->number, this->name);

        // Now for the best thing in modern C++, bitsets!
        std::bitset<8> status(statusByte);
        this->open = status.test(0);
        this->movementObstructed = status.test(1);
        this->screenMissing = status.test(2);
        this->rfHeard = status.test(3);
        this->rainSensed = status.test(4);
        this->rainOverrideActive = status.test(5);

        // Update the last updated time
        this->lastUpdated = std::chrono::system_clock::now();

    }

    std::string Window::toJson() const {
        debug("serializing window {} to json", this->number);

        json j;
        j["name"] = this->name;
        j["number"] = this->number;
        j["state"] = this->open;
        j["movementObstructed"] = this->movementObstructed;
        j["screenMissing"] = this->screenMissing;
        j["rfHeard"] = this->rfHeard;
        j["rainSensed"] = this->rainSensed;
        j["rainOverrideActive"] = this->rainOverrideActive;
        j["lastUpdated"] = this->timePointToISO8601(this->lastUpdated);
        return j.dump();

    }

    std::string Window::getName() const {
        return this->name;
    }

    uint8_t Window::getNumber() const {
        return this->number;
    }

    bool Window::isOpen() const {
        return this->open;
    }

    bool Window::isMovementObstructed() const {
        return this->movementObstructed;
    }

    bool Window::isScreenMissing() const {
        return this->screenMissing;
    }

    bool Window::isRfHeard() const {
        return this->rfHeard;
    }

    bool Window::isRainSensed() const {
        return this->rainSensed;
    }

    bool Window::isRainOverrideActive() const {
        return this->rainOverrideActive;
    }


    std::vector<std::string> Window::bytesToHexStrings(const uint8_t* bytes, size_t size) {
        std::vector<std::string> hexStrings;
        hexStrings.reserve(size); // Reserve space for efficiency

        for (size_t i = 0; i < size; ++i) {
            std::stringstream hexStream;
            hexStream << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
            hexStrings.push_back(hexStream.str());
        }

        return hexStrings;
    }

    std::vector<std::string> Window::bytesToHexStrings(const std::vector<uint8_t>& bytes) {
    return bytesToHexStrings(bytes.data(), bytes.size());
}


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

    /**
     * Calculates the checksum for a given message.
     * The programmer's manual specifies skipping the first byte.
     *
     * @param message The input message as a vector of bytes.
     * @return The calculated checksum.
     */
    uint8_t Window::calculateChecksum(const std::vector<uint8_t>& message) {
        if (message.size() <= 1) {
            return 0; // If the message has 1 or fewer bytes, checksum is meaningless
        }

        uint8_t checksum = 0;

        debug("Calculating checksum for [{}]", joinStrings(creatures::Window::bytesToHexStrings(message)));

        // Start summing from the second byte (index 1)
        for (size_t i = 1; i < message.size(); ++i) {
            debug("Adding byte {}: 0x{:02X} (checksum so far: 0x{:02X})", i, message[i], checksum);
            checksum += message[i];
        }

        debug("Final calculated checksum: 0x{:02X}", checksum);
        return checksum;
    }

    /**
     * Validates the checksum for a given message.
     * Assumes the last byte in the message is the checksum.
     *
     * @param message The input message as a vector of bytes.
     * @return True if the checksum is valid, false otherwise.
     */
    bool Window::validateChecksum(const std::vector<uint8_t>& message) {
        if (message.size() < 2) {
            // Not enough data to validate (minimum 1 byte + checksum)
            return false;
        }

        // Extract the provided checksum (last byte)
        uint8_t providedChecksum = message.back();

        // Create a slice of the message without the checksum byte
        std::vector<uint8_t> messageWithoutChecksum(message.begin(), message.end() - 1);

        // Calculate the checksum
        uint8_t calculatedChecksum = calculateChecksum(messageWithoutChecksum);

        // Debug: Show calculated and provided checksum
        debug("Provided checksum: 0x{:02X}, Calculated checksum: 0x{:02X}", providedChecksum, calculatedChecksum);

        // Validate
        debug("Checksum validation: {}", calculatedChecksum == providedChecksum);
        return calculatedChecksum == providedChecksum;
    }

} // creatures