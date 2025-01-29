//
// Created by @opsnlops on 11/22/23.
//

#ifndef ANDERSEN_MQTT_MQTT_H
#define ANDERSEN_MQTT_MQTT_H

#include <string>
#include <thread>

#include "window/window.h"

#include <mqtt_client_cpp.hpp>


#define MQTT_USE_LOG 1
#define MQTT_USE_STR_CHECK 1
#define MQTT_STD_OPTIONAL 1
#define MQTT_STD_VARIANT 1
#define MQTT_STD_STRING_VIEW 1



namespace creatures {

    using MQTTClientType = decltype(MQTT_NS::make_sync_client(std::declval<boost::asio::io_context&>(), "localhost", "1883"));
    using packet_id_t = typename MQTTClientType::element_type::packet_id_t;

    class MQTTClient {
    public:
        MQTTClient(std::string host, std::string port);
        ~MQTTClient() = default;

        void start();
        void stop();

        void addWindow(std::shared_ptr<Window> window);

        bool subscribe(std::string topic, MQTT_NS::qos qos);

        bool on_connack(bool sp, mqtt::connect_return_code connack_return_code);
        void on_close();
        void on_error(MQTT_NS::error_code ec);
        bool on_suback(packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results);
        bool on_publish(MQTT_NS::optional<packet_id_t> packet_id,
                        MQTT_NS::publish_options pubopts,
                        MQTT_NS::buffer topic_name,
                        MQTT_NS::buffer contents);


        bool publishWindows();

    private:

        bool connected;

        static std::string yesOrNo(bool value);

        // Keep track of our windows
        std::vector<std::shared_ptr<Window>> windows;

        boost::asio::io_context ioc;
        std::string host;
        std::string port;

        std::shared_ptr<MQTTClientType::element_type> client;


        packet_id_t pid_sub1;
        packet_id_t pid_sub2;

        std::thread ioThread;

    };

} // creatures

#endif //ANDERSEN_MQTT_MQTT_H
