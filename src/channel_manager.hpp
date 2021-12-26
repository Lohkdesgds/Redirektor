#include "channel_conf.hpp"
#include "console.h"

#include <vector>
#include <chrono>

const uint64_t delta_time_cleanup = 20; // seconds
const uint64_t delta_loop = 5;

class ChannelManager {
    struct __chcon {
        uint64_t deatht;
        std::shared_ptr<ChannelConf> ch;

        __chcon() = default;
        __chcon(const uint64_t, std::shared_ptr<ChannelConf>);
    };

    std::thread cleanup_thr;
    std::vector<__chcon> chs;
    mutable std::shared_mutex access;
    bool keep_running = false;

    void async_clean();
public:
    ChannelManager();
    ~ChannelManager();

    std::shared_ptr<ChannelConf> get(const dpp::snowflake&);
};