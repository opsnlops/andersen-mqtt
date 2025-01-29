//
// Created by @opsnlops on 11/22/23.
//

#include <string>
#include <sstream>
#include <boost/asio/signal_set.hpp>

#include <mqtt_client_cpp.hpp>
#include <mqtt/setup_log.hpp>

#include "namespace-stuffs.h"

#include "mqtt.h"

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
                std::bind(&MQTTClient::on_connack, this, std::placeholders::_1, std::placeholders::_2));
        client->set_close_handler(std::bind(&MQTTClient::on_close, this));
        client->set_error_handler(std::bind(&MQTTClient::on_error, this, std::placeholders::_1));
        client->set_suback_handler(
                std::bind(&MQTTClient::on_suback, this, std::placeholders::_1, std::placeholders::_2));
        client->set_publish_handler(std::bind(&MQTTClient::on_publish, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3,
                                              std::placeholders::_4));

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

    void MQTTClient::addWindow(std::shared_ptr<Window> window) {
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

    bool MQTTClient::publishWindows() {

        if (connected) {
            info("publishing all windows to MQTT");
            for (const auto &window: windows) {

                std::string prefix = "andersen-mqtt/windows/" + window->getName() + "/";

                if(window->hasOpenUpdated()) {
                    client->publish(prefix + "open", yesOrNo(window->isOpen()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if(window->hasMovementObstructedUpdated()) {
                    client->publish(prefix + "movement_obstructed", yesOrNo(window->isMovementObstructed()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if(window->hasScreenMissingUpdated()) {
                    client->publish(prefix + "screen_missing", yesOrNo(window->isScreenMissing()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if(window->hasRfHeardUpdated()) {
                    client->publish(prefix + "rf_heard", yesOrNo(window->isRfHeard()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if(window->hasRainSensedUpdated()) {
                    client->publish(prefix + "rain_sensed", yesOrNo(window->isRainSensed()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                if(window->hasRainOverrideActiveUpdated()) {
                    client->publish(prefix + "rain_override_active", yesOrNo(window->isRainOverrideActive()), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes );
                }

                if(window->hasLastPolledUpdated()) {
                    client->publish(prefix + "last_polled", window->getLastPolled(), MQTT_NS::qos::at_least_once | MQTT_NS::retain::yes);
                }

                // Now go mark the window as not updated
                window->resetUpdatedFlags();
            }
            return true;
        }

        error("not publishing since we're not connected");
        return false;
    }


    std::string MQTTClient::yesOrNo(bool value) {
        return  value ? "yes" : "no";
    }


    bool MQTTClient::on_connack(bool sp, mqtt::connect_return_code connack_return_code) {

        debug("connection acknowledged! session present: {}, connect return code: {}",
              sp, MQTT_NS::connect_return_code_to_str(connack_return_code));

        connected = true;

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
        std::ostringstream oss;
        oss << "publish received! packet_id: " << *packet_id << ", topic: " << topic_name
            << ", dup: " << pubopts.get_dup() << ", qos: " << pubopts.get_qos() << ", retain: " << pubopts.get_retain();

        debug("publish received! {}", oss.str());

        return true;
    }


} // creatures