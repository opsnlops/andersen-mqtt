#ifndef ANDERSEN_MQTT_CONFIG_H
#define ANDERSEN_MQTT_CONFIG_H

#include <string>

#include <spdlog/spdlog.h>

#ifndef ANDERSEN_MQTT_DEFAULT_CONFIG
#define ANDERSEN_MQTT_DEFAULT_CONFIG "/etc/andersen-mqtt/andersen-mqtt.json"
#endif

namespace creatures {

    /**
     * Everything that used to be hardcoded. The defaults match the values the
     * program used before it had a configuration file, so a missing key keeps
     * the old behaviour.
     */
    struct Config {
        std::string mqttHost = "10.3.2.5";
        int mqttPort = 1883;
        std::string mqttClientId = "andersen-mqtt";
        std::string topicPrefix = "andersen-mqtt/windows";

        std::string gatewayHost = "10.3.2.5";
        int gatewayPort = 6000;

        int pollIntervalSeconds = 5;
        spdlog::level::level_enum logLevel = spdlog::level::info;
    };

    /**
     * Parse the command line, then load the configuration file it names.
     *
     * Exits the process for --help and --version. Throws std::runtime_error
     * on a bad command line or configuration file. Nothing is written to
     * stdout before the arguments are parsed, because help2man generates the
     * manual page from --help.
     */
    Config loadConfiguration(int argc, char **argv);

    /**
     * Load a configuration file. Every key is optional, but an unknown key or
     * a value of the wrong type is an error rather than silently ignored.
     */
    Config loadConfigFile(const std::string &path, Config config = {});

} // creatures

#endif //ANDERSEN_MQTT_CONFIG_H
