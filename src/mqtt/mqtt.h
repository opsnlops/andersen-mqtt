//
// Created by @opsnlops on 11/22/23.
//

#ifndef ANDERSEN_MQTT_MQTT_H
#define ANDERSEN_MQTT_MQTT_H

#include <string>

#include <mqtt_client_cpp.hpp>



namespace creatures {

    class MQTT {
    public:
        MQTT(std::string host, std::string port);
        ~MQTT() = default;

        bool on_connack(bool sp, mqtt::connect_return_code connack_return_code);

    private:
        boost::asio::io_context ioc;
        std::string host;
        std::string port;

        // Figure out what type of shared_ptr to use
        using ClientType = decltype(MQTT_NS::make_sync_client(std::declval<boost::asio::io_context&>(), "localhost", "1883"));
        std::shared_ptr<ClientType::element_type> client;

        using packet_id_t = typename ClientType::element_type::packet_id_t;

        std::uint16_t pid_sub1;
        std::uint16_t pid_sub2;

    };

} // creatures

#endif //ANDERSEN_MQTT_MQTT_H
