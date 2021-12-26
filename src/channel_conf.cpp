#include "channel_conf.hpp"

ChannelConf::ChannelConf(const dpp::snowflake& sf)
{
    load(sf);
}
/*
    std::vector<SingleConf> confs;
    dpp::snowflake last_id = 0;
    bool saved_already = true; // empty = no save
*/
ChannelConf::ChannelConf(ChannelConf&& o) noexcept
    : confs(std::move(o.confs)), last_id(o.last_id), saved_already(o.saved_already)
{
    o.last_id = 0;
    o.saved_already = true;
}

void ChannelConf::operator=(ChannelConf&& o) noexcept
{
    confs = std::move(o.confs);
    last_id = o.last_id;
    saved_already = o.saved_already;
    o.last_id = 0;
    o.saved_already = true;
}

ChannelConf::~ChannelConf()
{
    if (last_id != 0) cout << console::color::DARK_GRAY << "Saving #" << last_id << "...";
    const bool gud = save();
    if (last_id != 0) cout << console::color::DARK_GRAY << (gud ? ("Saved #" + std::to_string(last_id) + ".") : ("Failed to save #" + std::to_string(last_id) + "."));
}

bool ChannelConf::load(const dpp::snowflake& id)
{
    try {
        last_id = id;
        saved_already = false;
        if (last_id == 0) return false;

        std::ifstream fp(savepath + "/" + std::to_string(last_id));
        if (!fp.good() || !fp.is_open()) return false;

        std::stringstream ss;
        ss << fp.rdbuf();

        nlohmann::json parsed = nlohmann::json::parse(ss.str());

        for(const auto& _field : parsed["msgev"]) {
            confs.push_back(SingleConf{_field});
        }

        return true;
    }
    catch(const std::exception& e)
    {
        cout << console::color::RED << "Error loading ID #" << last_id << ": " << e.what();
        return false;
    }
    catch(...)
    {
        cout << console::color::RED << "Error loading ID #" << last_id << ": UNCAUGHT";
        return false;
    }
}

bool ChannelConf::save()
{
    try {
        if (saved_already) return true;
        if (last_id == 0) return false;
        std::error_code err;
        std::filesystem::create_directories(savepath, err);

        std::ofstream fp(savepath + "/" + std::to_string(last_id));
        if (!fp.good() || !fp.is_open()) return false;

        nlohmann::json j;
        
        std::unique_lock<std::shared_mutex> luck(saf);
        for(const auto& it : this->confs)
        {
            j["msgev"].push_back(it.to_json());
        }
        
        const auto jdump = j.dump();

        fp.write(jdump.c_str(), jdump.size());

        return (saved_already = !fp.bad());
    }
    catch(const std::exception& e)
    {
        cout << console::color::RED << "Error loading ID #" << last_id << ": " << e.what();
        saved_already = false;
        return false;
    }
    catch(...)
    {
        cout << console::color::RED << "Error loading ID #" << last_id << ": UNCAUGHT";
        saved_already = false;
        return false;
    }
}

void ChannelConf::index(const size_t i, std::function<void(SingleConf&)> f)
{
    if (!f) return;
    std::unique_lock<std::shared_mutex> luck(saf);
    if (i == confs.size()) confs.push_back({});
    if (i >= confs.size()) return;
    f(confs[i]);
}

void ChannelConf::run(const dpp::message_create_t& ev) const
{
    std::shared_lock<std::shared_mutex> luck(saf);

    auto vv = std::make_shared<SingleConf::handle_result>();
    vv->going_on_yet = 1;    

    for(const auto& it : this->confs)
        it.handle(ev, vv);

    if (--vv->going_on_yet == 0 && vv->should_delete_message)
    {
        ev.from->creator->message_delete(ev.msg.id, ev.msg.channel_id,
            [creat = ev.from->creator, mid = ev.msg.id, chid = ev.msg.channel_id, gid = ev.msg.guild_id, epost = vv->should_show_error](const dpp::confirmation_callback_t& ret) {
                if (ret.is_error() && epost) {
                    dpp::message buck;
                    buck.set_reference(mid, gid);
                    buck.channel_id = chid;
                    buck.content = "**Error handling this:**\n```md\n# ERROR_CODE: " + std::to_string(ret.get_error().code) + "\n- Resumed: " + ret.get_error().message + "\n- Full error:\n\n" + ret.http_info.body + "\n```";
                    creat->message_create(buck);
                }
            }
        );
    }
}

bool ChannelConf::is_id(const dpp::snowflake& id) const
{
    return last_id == id;
}

dpp::snowflake ChannelConf::get_id() const
{
    return last_id;
}