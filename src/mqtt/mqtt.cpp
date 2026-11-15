//
// Created by @opsnlops on 11/22/23.
//

#include <cctype>
#include <string>
#include <sstream>
#include <boost/asio/post.hpp>

#include <mqtt_client_cpp.hpp>
#include <mqtt/setup_log.hpp>

#include "blockingconcurrentqueue.h"

#include "namespace-stuffs.h"

#include "mqtt.h"

extern std::shared_ptr<moodycamel::BlockingConcurrentQueue<std::vector<uint8_t>>> outgoingSocketMessages;


namespace creatures {

    MQTTClient::MQTTClient(std::string host, std::string port, std::string clientId,
                           std::function<void(const std::string &)> connectionLost)
            : connectionLost(std::move(connectionLost)) {

        info("creating a new MQTT instance for host {} and port {}", host, port);

        // Store these for later
        this->host = host;
        this->port = port;

        // Create the client
        client = mqtt::make_sync_client(this->ioc, this->host, this->port);

        // Setup client
        client->set_client_id(clientId);
        client->set_clean_session(true);
        // Ping the broker so a connection that dies silently is noticed
        client->set_keep_alive_sec(30);
        // Without this, a missing PINGRESP is never treated as a failure
        client->set_pingresp_timeout(std::chrono::seconds(10));

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
        this->ioThread = std::thread([this] {
            try {
                ioc.run();
            }
            catch (const std::exception &e) {
                connected = false;
                if (!stopping && connectionLost) {
                    connectionLost(std::string("MQTT failure: ") + e.what());
                }
            }
        });

    }

    void MQTTClient::stop() {
        stopping = true;
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

    void MQTTClient::updateWindows(std::vector<uint8_t> statuses) {
        boost::asio::post(ioc, [this, statuses = std::move(statuses)] {
            for (const auto &window: windows) {
                size_t index = window->getNumber() - 1;
                if (index < statuses.size()) {
                    window->setStatus(statuses[index]);
                    debug("{}", window->toJson());
                }
            }
            if (publishWindows(!publishedOnce)) {
                publishedOnce = true;
            }
        });
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

        if (connack_return_code != MQTT_NS::connect_return_code::accepted) {
            if (connectionLost) {
                connectionLost(fmt::format("the MQTT broker refused the connection: {}",
                                           MQTT_NS::connect_return_code_to_str(connack_return_code)));
            }
            return true;
        }

        connected = true;

        // Now that we're connected, let's subscribe to all the windows
        for (const auto &window: windows) {
            subscribe(window);
        }

        return true;

    }

    void MQTTClient::on_close() {

        connected = false;
        if (!stopping && connectionLost) {
            connectionLost("the connection to the MQTT broker closed");
        }
    }

    void MQTTClient::on_error(MQTT_NS::error_code ec) {
        connected = false;
        if (!stopping && connectionLost) {
            connectionLost("MQTT error: " + ec.message());
        }
    }

    bool MQTTClient::on_suback(packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results) {

        info("subscribe acknowledged! packet_id: {}", packet_id);

        for (auto const &e: results) {
            if (e == MQTT_NS::suback_return_code::failure) {
                error("the broker refused a subscription (packet_id {}); commands for that window won't arrive",
                      packet_id);
            } else {
                debug("subscribe packet_id: {}, result: {}", packet_id, MQTT_NS::suback_return_code_to_str(e));
            }
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

        // A retained command would be replayed every time we subscribe, which
        // is every restart. Only act on commands sent while we're listening.
        if (pubopts.get_retain() == MQTT_NS::retain::yes) {
            warn("ignoring a retained message on {}; publish commands without retain", topic_str);
            return true;
        }

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
                        // Returning false would tell mqtt_cpp to stop receiving for good
                        return true;
                }
                debug("windowId: {}", windowId);

                // Now figure out which command
                uint8_t commandId;
                // Home Assistant sends OPEN, CLOSE and STOP; accept any case
                switch (std::tolower(static_cast<unsigned char>(contents_str.empty() ? '\0' : contents_str[0]))) {
                    case 'o':
                        info("opening window {}", window->getName());
                        commandId = CMD_OPEN;
                        break;
                    case 'c':
                        info("closing window {}", window->getName());
                        commandId = CMD_CLOSE;
                        break;
                    case 's':
                        info("stopping window {}", window->getName());
                        commandId = CMD_STOP;
                        break;
                    default:
                        error("unknown command received for {}: '{}'", window->getName(), contents_str);
                        // Returning false would tell mqtt_cpp to stop receiving for good
                        return true;
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