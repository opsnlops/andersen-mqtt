//
// Created by @opsnlops on 11/22/23.
//

#include <mqtt_client_cpp.hpp>

#include "namespace-stuffs.h"

#include "mqtt.h"

namespace creatures {

    MQTT::MQTT(std::string host, std::string port) {

        info("creating a new MQTT instance for host {} and port {}", host, port);

        // Store these for later
        this->host = host;
        this->port = port;

        // Create the client
        client = mqtt::make_sync_client(this->ioc, this->host, this->port);

        // Setup client
        client->set_client_id("andersen-mqtt");
        client->set_clean_session(true);

        // Bind the member function for the connack handler
        client->set_connack_handler(std::bind(&MQTT::on_connack, this, std::placeholders::_1, std::placeholders::_2));

        client->connect();
        ioc.run();

    }

    bool MQTT::on_connack(bool sp, mqtt::connect_return_code connack_return_code) {

        debug("on_connack called! session present: {}, connack_return_code: {}", sp, mqtt::connect_return_code_to_str(connack_return_code));

        if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
            debug("return code was accepted");
            pid_sub1 = client->subscribe("mqtt_client_cpp/topic1", MQTT_NS::qos::at_most_once);
            pid_sub2 = client->subscribe(
                    std::vector<std::tuple<MQTT_NS::string_view, MQTT_NS::subscribe_options>>{
                            {"mqtt_client_cpp/topic2_1", MQTT_NS::qos::at_least_once},
                            {"mqtt_client_cpp/topic2_2", MQTT_NS::qos::exactly_once}
                    }
            );
        }
        return true;
    }


} // creatures