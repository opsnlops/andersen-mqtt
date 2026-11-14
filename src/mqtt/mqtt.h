//
// Created by @opsnlops on 11/22/23.
//

#ifndef ANDERSEN_MQTT_MQTT_H
#define ANDERSEN_MQTT_MQTT_H

#include <atomic>
#include <functional>
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
        // connectionLost is called, from the MQTT thread, if the connection to the
        // broker closes or fails other than through stop().
        MQTTClient(std::string host, std::string port, std::string clientId,
                   std::function<void(const std::string &)> connectionLost);
        ~MQTTClient() = default;

        void start();
        void stop();

        void addWindow(std::shared_ptr<Window> window);

        bool subscribe(std::string topic, MQTT_NS::qos qos);

        bool on_connack(bool sp, mqtt::connect_return_code connack_return_code);
        void on_close();
        void on_error(MQTT_NS::error_code ec);
        static bool on_suback(packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results);
        bool on_publish(MQTT_NS::optional<packet_id_t> packet_id,
                        MQTT_NS::publish_options pubopts,
                        MQTT_NS::buffer topic_name,
                        MQTT_NS::buffer contents);

        // Apply a status poll (one status byte per window, in window-number
        // order) and publish what changed. Safe to call from any thread: the
        // work runs on the MQTT thread, because the client isn't thread-safe.
        void updateWindows(std::vector<uint8_t> statuses);

    private:

        std::atomic<bool> connected{false};
        std::atomic<bool> stopping{false};
        std::function<void(const std::string &)> connectionLost;

        static std::string yesOrNo(bool value);

        bool publishWindows(bool forcePublish);
        bool subscribe(std::shared_ptr<Window> window);

        // Publish every value after connecting, retained topics included.
        // Only touched on the MQTT thread.
        bool publishedOnce = false;


        // Keep track of our windows
        std::vector<std::shared_ptr<Window>> windows;

        boost::asio::io_context ioc;
        std::string host;
        std::string port;

        std::shared_ptr<MQTTClientType::element_type> client;

        std::thread ioThread;

    };

} // creatures

#endif //ANDERSEN_MQTT_MQTT_H
