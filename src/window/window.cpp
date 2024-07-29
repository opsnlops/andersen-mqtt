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
}