//
// Created by April White on 11/23/23.
//

#ifndef ANDERSEN_MQTT_WINDOW_H
#define ANDERSEN_MQTT_WINDOW_H

#include <chrono>

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


        std::string createPrefix();

        [[nodiscard]] std::string getName() const;
        uint8_t getNumber() const;
        bool isOpen() const;
        bool isMovementObstructed() const;
        bool isScreenMissing() const;
        bool isRfHeard() const;
        bool isRainSensed() const;
        bool isRainOverrideActive() const;

        std::string getLastPolled() const { return timePointToISO8601(lastPolled); }

        void resetUpdatedFlags();

        bool hasOpenUpdated() const { return openUpdated; }
        bool hasMovementObstructedUpdated() const { return movementObstructedUpdated; }
        bool hasScreenMissingUpdated() const { return screenMissingUpdated; }
        bool hasRfHeardUpdated() const { return rfHeardUpdated; }
        bool hasRainSensedUpdated() const { return rainSensedUpdated; }
        bool hasRainOverrideActiveUpdated() const { return rainOverrideActiveUpdated; }
        bool hasLastPolledUpdated() const { return lastPolledUpdated; }

        [[nodiscard]]
        std::string toJson() const;

        static std::vector<std::string> bytesToHexStrings(const uint8_t* bytes, size_t size);
        static std::vector<std::string> bytesToHexStrings(const std::vector<uint8_t>& bytes);
        static uint8_t calculateChecksum(const std::vector<uint8_t>& message);
        static bool validateChecksum(const std::vector<uint8_t> &message);

    private:

        std::string name;
        std::uint8_t number;

        bool open;
        bool movementObstructed;
        bool screenMissing;
        bool rfHeard;
        bool rainSensed;
        bool rainOverrideActive;
        std::chrono::system_clock::time_point lastPolled;

        bool openUpdated = true;
        bool movementObstructedUpdated = true;
        bool screenMissingUpdated = true;
        bool rfHeardUpdated = true;
        bool rainSensedUpdated = true;
        bool rainOverrideActiveUpdated = true;
        bool lastPolledUpdated = true;




        [[nodiscard]] std::string timePointToISO8601(const std::chrono::system_clock::time_point& tp) const;

    };



}


#endif //ANDERSEN_MQTT_WINDOW_H
