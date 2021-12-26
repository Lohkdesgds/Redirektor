#include "each_conf.hpp"
#include "console.h"
#include <dpp/nlohmann/json.hpp>

#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <shared_mutex>
#include <sstream>

using namespace Lunaris;

const std::string savepath = "channels";

class ChannelConf {
    std::vector<SingleConf> confs;
    dpp::snowflake last_id = 0;
    bool saved_already = true; // empty = no save
    mutable std::shared_mutex saf;
public:
    ChannelConf() = default;
    ChannelConf(const dpp::snowflake&);

    ChannelConf(const ChannelConf&) = delete;
    void operator=(const ChannelConf&) = delete;
    
    ChannelConf(ChannelConf&&) noexcept;
    void operator=(ChannelConf&&) noexcept;

    ~ChannelConf();

    bool load(const dpp::snowflake&);
    bool save();

    void index(const size_t, std::function<void(SingleConf&)>);

    void run(const dpp::message_create_t&) const;

    bool is_id(const dpp::snowflake&) const;
    dpp::snowflake get_id() const;
};