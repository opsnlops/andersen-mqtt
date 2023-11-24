//
// Created by April White on 11/23/23.
//

#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>
#include <spdlog/spdlog.h>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


#ifndef ANDERSEN_MQTT_LOG_WRAPPER_H
#define ANDERSEN_MQTT_LOG_WRAPPER_H

spdlog::level::level_enum map_severity_level(logging::trivial::severity_level level);
void init_boost_logging();


#endif //ANDERSEN_MQTT_LOG_WRAPPER_H
