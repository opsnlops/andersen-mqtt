//
// Created by April White on 11/23/23.
//

#ifndef ANDERSEN_MQTT_WINDOW_H
#define ANDERSEN_MQTT_WINDOW_H

#define SRC_CONTROLLER              0xFF
#define DST_PANEL_1                 0x01
#define DST_PANEL_2                 0x02
#define DST_PANEL_3                 0x03
#define DST_PANEL_4                 0x04


#define WINDOW_1                    0x01
#define WINDOW_2                    0x02
#define WINDOW_3                    0x03
#define WINDOW_4                    0x04
#define WINDOW_ALL                  0x05

#define CMD_OPEN                    0x55
#define CMD_CLOSE                   0xAA
#define CMD_STATUS_WITH_POLL        0x5A
#define CMD_STOP                    0xA5
#define CMD_STATUS_WITHOUT_POLL     0x5C

#define CONTROLLER_BUSY             0x27
#define CONTROLLER_ACK              0xB1



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
