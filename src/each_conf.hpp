#include <dpp/dpp.h>
#include <iostream>
#include <dpp/nlohmann/json.hpp>
#include <dpp/fmt/format.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex>
#include <atomic>

#include "console.h"

using namespace Lunaris;

// sticker format: https://media.discordapp.net/stickers/873762778589044766.webp
std::string sticker_to_url(const dpp::sticker&);

struct SingleConf {
    enum class gconf_type {
        NONE, 
        TEXT, 
        FILE, 
        STICKER
    };
    struct handle_result {
        bool triggered_one = false;
        bool should_delete_message = false;
        bool should_show_error = false;
        std::atomic<size_t> going_on_yet = 0;
    };

    using handptr = std::shared_ptr<handle_result>;

    gconf_type trigger_type = gconf_type::NONE;

    std::string regx = ".*"; // text: text, sticker: name, file: file name, reaction: full name regex
    int32_t min_size = -1; // text: len, sticker: name len, file: size, reaction: name len (full)
    int32_t max_size = -1; // text: len, sticker: name len, file: size, reaction: name len (full)
    bool del_trigger = false; // remove trigger?
    dpp::snowflake redir_id = 0; // chat to copy this to (disabled for reaction)
    bool post_error = true;
    bool detailed = false;

    SingleConf() = default;
    SingleConf(const nlohmann::json&);

    void handle(const dpp::message_create_t&, handptr) const;

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json&);
};

template<typename T> 
inline T any_get(const nlohmann::json* j, const char* s, const T& d = {})
{
    try{
        auto k = j->find(s);
        if (k != j->end()) {
            return k->get<T>();
        } else {
            return d;
        }
    }
    catch(...){ // safe bet
        return d;
    }
}

std::string gconf_to_str(const SingleConf::gconf_type&);