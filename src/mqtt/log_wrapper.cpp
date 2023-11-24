//
// Created by April White on 11/23/23.
//

#include <mqtt_client_cpp.hpp>
#include <mqtt/setup_log.hpp>

#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>
#include <spdlog/spdlog.h>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


// Map Boost severity to spdlog levels
spdlog::level::level_enum map_severity_level(logging::trivial::severity_level level) {
    using boost_sev = logging::trivial::severity_level;
    switch (level) {
        case boost_sev::trace:   return spdlog::level::trace;
        case boost_sev::debug:   return spdlog::level::debug;
        case boost_sev::info:    return spdlog::level::info;
        case boost_sev::warning: return spdlog::level::warn;
        case boost_sev::error:   return spdlog::level::err;
        case boost_sev::fatal:   return spdlog::level::critical;
        default:                 return spdlog::level::debug;
    }
}



// Custom sink backend that forwards messages to spdlog
class spdlog_sink_backend :
        public sinks::basic_formatted_sink_backend<char, sinks::synchronized_feeding>
{
public:
    // The function is called for every log record to push it to spdlog
    // The function is called for every log record to push it to spdlog
    static void consume(logging::record_view const& rec, string_type const& formatted_message)
    {
        // Extract the severity level using Boost.Log attribute value extraction
        auto severity_attr = rec[logging::trivial::severity];
        if (severity_attr) {
            // Get the actual severity level
            auto severity = severity_attr.get();
            // Map the Boost severity level to spdlog level
            auto spdlog_level = map_severity_level(severity);
            spdlog::log(spdlog_level, formatted_message);
        } else {
            // Default to info level if we can't get the severity level
            spdlog::info(formatted_message);
        }
    }
};


void init_boost_logging()
{
    typedef sinks::synchronous_sink<spdlog_sink_backend> sink_t;
    boost::shared_ptr<sink_t> sink(new sink_t());

    logging::core::get()->add_sink(sink);

    spdlog::debug("Boost logging initialized");

}
