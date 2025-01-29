//
// Created by @opsnlops on 11/22/23.
//

#include <string>
#include <sstream>
#include <boost/asio/signal_set.hpp>

#include <mqtt_client_cpp.hpp>
#include <mqtt/setup_log.hpp>

#include "blockingconcurrentqueue.h"

#include "namespace-stuffs.h"

#include "mqtt.h"

extern std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> outgoingSocketMessages;


namespace creatures {

    MQTTClient::MQTTClient(std::string host, std::string port) {

        info("creating a new MQTT instance for host {} and port {}", host, port);

        // Store these for later
        this->host = host;
        this->port = port;

        this->connected = false;

        // Create the client
        client = mqtt::make_sync_client(this->ioc, this->host, this->port);

        // Setup client
        client->set_client_id("andersen-mqtt");
        client->set_clean_session(true);

        // Bind the member function for the connack handler
        client->set_connack_handler(
                [this](auto &&PH1, auto &&PH2) {
                    return on_connack(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
                });
        client->set_close_handler([this] { on_close(); });
        client->set_error_handler([this](auto &&PH1) { on_error(std::forward<decltype(PH1)>(PH1)); });
        client->set_suback_handler(
                [this](auto &&PH1, auto &&PH2) {
                    return on_suback(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
                });
        client->set_publish_handler([this](auto &&PH1, auto &&PH2, auto &&PH3, auto &&PH4) {
            return on_publish(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                              std::forward<decltype(PH3)>(PH3), std::forward<decltype(PH4)>(PH4));
        });

    }

    void MQTTClient::start() {

        info("starting the MQTT worker");


        debug("connecting");
        client->connect();

        debug("starting the ioc");
        this->ioThread = std::thread([this] { ioc.run(); });

    }

    void MQTTClient::stop() {
        debug("stopping the ioc");
        ioc.stop();

        debug("waiting for the ioc thread to finish");
        if (ioThread.joinable()) {
            ioThread.join();
            debug("ioThread joined!");
        }

        info("MQTT Client stopped");
    }

    void MQTTClient::addWindow(const std::shared_ptr<Window> window) {
        info("adding window {} to MQTT client", window->getName());
        windows.push_back(window);
    }

    bool MQTTClient::subscribe(std::string topic, MQTT_NS::qos qos) {

        if (connected) {
            info("subscribing to topic {}", topic);
            client->subscribe(topic, qos);
            return true;
        }

        error("not subscribing since we're not connected");
        return false;
    }

    bool MQTTClient::publishWindows(bool forcePublish) {

        if (connected) {
            info("publishing all windows to MQTT");
            for (const auto &window: windows) {

                std::string prefix = window->createPrefix();

                if (window->hasOpenUpdated() || forcePublish) {
                    client->publish(prefix + "open", yesOrNo(window->isOpen()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasMovementObstructedUpdated() || forcePublish) {
                    client->publish(prefix + "movement_obstructed", yesOrNo(window->isMovementObstructed()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasScreenMissingUpdated() || forcePublish) {
                    client->publish(prefix + "screen_missing", yesOrNo(window->isScreenMissing()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasRfHeardUpdated() || forcePublish) {
                    client->publish(prefix + "rf_heard", yesOrNo(window->isRfHeard()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasRainSensedUpdated() || forcePublish) {
                    client->publish(prefix + "rain_sensed", yesOrNo(window->isRainSensed()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasRainOverrideActiveUpdated() || forcePublish) {
                    client->publish(prefix + "rain_override_active", yesOrNo(window->isRainOverrideActive()),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if (window->hasLastPolledUpdated() || forcePublish) {
                    client->publish(prefix + "last_polled", window->getLastPolled(),
                                    MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                // Now go mark the window as not updated
                window->resetUpdatedFlags();
            }
            return true;
        }

        error("not publishing since we're not connected");
        return false;
    }


    bool MQTTClient::subscribe(const std::shared_ptr<Window> window) {

        std::string topic = window->createPrefix() + "command";

        debug("subscribing to window {} ({})", window->getName(), topic);
        client->subscribe(topic, MQTT_NS::qos::at_least_once);

        return true;
    }


    std::string MQTTClient::yesOrNo(bool value) {
        return value ? "yes" : "no";
    }


    bool MQTTClient::on_connack(bool sp, mqtt::connect_return_code connack_return_code) {

        debug("connection acknowledged! session present: {}, connect return code: {}",
              sp, MQTT_NS::connect_return_code_to_str(connack_return_code));

        connected = true;

        // Now that we're connected, let's subscribe to all the windows
        for (const auto &window: windows) {
            subscribe(window);
        }

        return true;

    }

    void MQTTClient::on_close() {

        this->connected = false;
        info("MQTT connection closed");
    }

    void MQTTClient::on_error(MQTT_NS::error_code ec) {
        error("MQTT error: {}", ec.message());
    }

    bool MQTTClient::on_suback(packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {

        info("subscribe acknowledged! packet_id: {}", packet_id);

        for (auto const &e: results) {
            debug("[client] subscribe packet_id: {}, result: {}", packet_id, MQTT_NS::suback_return_code_to_str(e));
        }

        return true;

    }

    bool MQTTClient::on_publish(MQTT_NS::optional<packet_id_t> packet_id, MQTT_NS::publish_options pubopts,
                                MQTT_NS::buffer topic_name, MQTT_NS::buffer contents) {

        /*
         * This isn't my normal style, but the MQTT library makes heavy use of the stream operators. That's fine,
         * that's how you should do it, but that's why this looks a bit different than normal.
         */
        std::ostringstream toss;
        toss << topic_name;
        std::string topic_str = toss.str();

        std::ostringstream coss;
        coss << contents;
        std::string contents_str = coss.str();

        debug("received a message on topic {}: {}", topic_str, contents_str);

        // Figure out which window
        for (auto const &window: windows) {
            std::string prefix = window->createPrefix();
            if (topic_str == prefix + "command") {
                debug("received a command for window {}", window->getName());

                uint8_t windowId;
                switch (window->getNumber()) {
                    case 1:
                        windowId = WINDOW_1;
                        break;
                    case 2:
                        windowId = WINDOW_2;
                        break;
                    case 3:
                        windowId = WINDOW_3;
                        break;
                    case 4:
                        windowId = WINDOW_4;
                        break;
                    default:
                        error("unknown window number: {}", window->getNumber());
                        return false;
                }
                debug("windowId: {}", windowId);

                // Now figure out which command
                uint8_t commandId;
                switch (contents_str[0]) {
                    case 'o':
                        info("opening window {}", window->getName());
                        commandId = CMD_OPEN;
                        break;
                    case 'c':
                        info("closing window {}", window->getName());
                        commandId = CMD_CLOSE;
                        break;
                    case 's':
                        info("opening stopping {}", window->getName());
                        commandId = CMD_STOP;
                        break;
                    default:
                        error("unknown command received for {}: ", window->getName(), contents_str);
                        return false;
                }

                // ...and send it
                debug("sending command {} to window {}", commandId, window->getName());
                std::vector<uint8_t> command = {SRC_CONTROLLER, DST_PANEL_1, windowId, commandId};
                command.push_back(creatures::Window::calculateChecksum(command));
                outgoingSocketMessages->enqueue(std::move(command));
            }
        }


        return true;
    }


} // creatures