//
// Created by April White on 11/23/23.
//

#ifndef ANDERSEN_MQTT_WINDOW_H
#define ANDERSEN_MQTT_WINDOW_H

#include <bitset>
#include <string>
#include <utility>


#include "namespace-stuffs.h"

namespace creatures {

    class Window {

    public:
        explicit Window(std::string name, std::uint8_t number) : name(std::move(name)), number(number) {}

        void setStatus(uint8_t statusByte);


        std::string getName() const;
        uint8_t getNumber() const;
        bool isOpen() const;
        bool isMovementObstructed() const;
        bool isScreenMissing() const;
        bool isRfHeard() const;
        bool isRainSensed() const;
        bool isRainOverrideActive() const;

        std::string toJson() const;

    private:

        std::string name;
        std::uint8_t number;



        bool open;
        bool movementObstructed;
        bool screenMissing;
        bool rfHeard;
        bool rainSensed;
        bool rainOverrideActive;


    };



}


#endif //ANDERSEN_MQTT_WINDOW_H
