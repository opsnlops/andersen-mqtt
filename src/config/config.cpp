#include "config.h"

#include <cstdint>
#include <fstream>
#include <set>
#include <stdexcept>

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

namespace creatures {

    namespace {

        using json = nlohmann::json;

        void rejectUnknownKeys(const json &object, const std::set<std::string> &known, const std::string &where) {
            for (const auto &[key, value]: object.items()) {
                if (!known.contains(key)) {
                    throw std::runtime_error("unknown key '" + where + key + "'");
                }
            }
        }

        const json &requireObject(const json &value, const std::string &where) {
            if (!value.is_object()) {
                throw std::runtime_error("'" + where + "' must be an object");
            }
            return value;
        }

        // nlohmann's get<> converts freely (true -> 1, 0.5 -> 0), so check the
        // JSON type exactly and range-check integers before narrowing them.
        void readValue(const json &object, const char *key, std::string &target, const std::string &where) {
            if (!object.contains(key)) {
                return;
            }
            if (!object.at(key).is_string()) {
                throw std::runtime_error("'" + where + key + "' must be a string");
            }
            target = object.at(key).get<std::string>();
        }

        void readValue(const json &object, const char *key, int &target, const std::string &where) {
            if (!object.contains(key)) {
                return;
            }
            const auto &value = object.at(key);
            if (!value.is_number_integer()) {
                throw std::runtime_error("'" + where + key + "' must be an integer");
            }
            if (value.is_number_unsigned() ? value.get<std::uint64_t>() > 1000000
                                           : (value.get<std::int64_t>() < -1000000 ||
                                              value.get<std::int64_t>() > 1000000)) {
                throw std::runtime_error("'" + where + key + "' is out of range");
            }
            target = static_cast<int>(value.get<std::int64_t>());
        }

        void checkPort(int port, const std::string &where) {
            if (port < 1 || port > 65535) {
                throw std::runtime_error("'" + where + "' must be between 1 and 65535");
            }
        }

        spdlog::level::level_enum parseLogLevel(const std::string &name) {
            auto level = spdlog::level::from_str(name);
            // from_str returns "off" for anything it doesn't recognize
            if (level == spdlog::level::off && name != "off") {
                throw std::runtime_error("unknown log level '" + name +
                                         "' (use trace, debug, info, warning, error, critical, or off)");
            }
            return level;
        }

    } // namespace


    Config loadConfigFile(const std::string &path, Config config) {
        std::ifstream in(path);
        if (!in) {
            throw std::runtime_error("unable to open configuration file " + path);
        }

        json root;
        try {
            root = json::parse(in);
        } catch (const json::parse_error &e) {
            throw std::runtime_error("unable to parse " + path + ": " + e.what());
        }

        requireObject(root, "(top level)");
        rejectUnknownKeys(root, {"mqtt", "gateway", "pollIntervalSeconds", "logLevel"}, "");

        if (root.contains("mqtt")) {
            const auto &mqtt = requireObject(root.at("mqtt"), "mqtt");
            rejectUnknownKeys(mqtt, {"host", "port", "clientId", "topicPrefix"}, "mqtt.");
            readValue(mqtt, "host", config.mqttHost, "mqtt.");
            readValue(mqtt, "port", config.mqttPort, "mqtt.");
            readValue(mqtt, "clientId", config.mqttClientId, "mqtt.");
            readValue(mqtt, "topicPrefix", config.topicPrefix, "mqtt.");
        }

        if (root.contains("gateway")) {
            const auto &gateway = requireObject(root.at("gateway"), "gateway");
            rejectUnknownKeys(gateway, {"host", "port"}, "gateway.");
            readValue(gateway, "host", config.gatewayHost, "gateway.");
            readValue(gateway, "port", config.gatewayPort, "gateway.");
        }

        readValue(root, "pollIntervalSeconds", config.pollIntervalSeconds, "");

        std::string logLevel;
        readValue(root, "logLevel", logLevel, "");
        if (!logLevel.empty()) {
            config.logLevel = parseLogLevel(logLevel);
        }

        checkPort(config.mqttPort, "mqtt.port");
        checkPort(config.gatewayPort, "gateway.port");
        if (config.pollIntervalSeconds < 1 || config.pollIntervalSeconds > 3600) {
            throw std::runtime_error("'pollIntervalSeconds' must be between 1 and 3600");
        }
        if (config.mqttClientId.empty()) {
            throw std::runtime_error("'mqtt.clientId' must not be empty");
        }
        if (config.mqttHost.empty() || config.gatewayHost.empty()) {
            throw std::runtime_error("'mqtt.host' and 'gateway.host' must not be empty");
        }
        // Topics are built as <topicPrefix>/<window>/<attribute>
        while (!config.topicPrefix.empty() && config.topicPrefix.back() == '/') {
            config.topicPrefix.pop_back();
        }
        if (config.topicPrefix.empty()) {
            throw std::runtime_error("'mqtt.topicPrefix' must not be empty");
        }
        if (config.topicPrefix.find_first_of("+#") != std::string::npos) {
            throw std::runtime_error("'mqtt.topicPrefix' must not contain the MQTT wildcards + or #");
        }

        return config;
    }


    Config loadConfiguration(int argc, char **argv) {
        argparse::ArgumentParser program("andersen-mqtt", ANDERSEN_MQTT_VERSION);
        program.add_argument("-c", "--config")
                .help("path to the JSON configuration file")
                .default_value(std::string(ANDERSEN_MQTT_DEFAULT_CONFIG))
                .nargs(1)
                .metavar("FILE");

        program.add_argument("-l", "--log-level")
                .help("override the configured log level (trace, debug, info, warning, error, critical, off)")
                .metavar("LEVEL");

        try {
            program.parse_args(argc, argv);
        } catch (const std::exception &e) {
            throw std::runtime_error(std::string(e.what()) + "\nTry 'andersen-mqtt --help'.");
        }

        Config config = loadConfigFile(program.get<std::string>("--config"));

        if (auto level = program.present<std::string>("--log-level")) {
            config.logLevel = parseLogLevel(*level);
        }

        return config;
    }

} // creatures
