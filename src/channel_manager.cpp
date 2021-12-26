#include "channel_manager.hpp"

ChannelManager::__chcon::__chcon(const uint64_t v, std::shared_ptr<ChannelConf> c)
    : deatht(v), ch(std::move(c))
{
}

void ChannelManager::async_clean()
{
    cout << console::color::DARK_GRAY << "Async clean is up and running!";
    while(keep_running)
    {
        try {
            std::this_thread::sleep_for(std::chrono::seconds(delta_loop));
            
            std::unique_lock<std::shared_mutex> luck(access);
            const uint64_t time_now_s = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1, 1>>>(std::chrono::system_clock::now().time_since_epoch()).count();

            for(auto it = chs.begin(); it != chs.end();)
            {
                if (it->deatht < time_now_s) {
                    if (it->ch->save()){
                        cout << console::color::DARK_GRAY << "Cleaned #" << it->ch->get_id() << " from RAM.";
                        it = chs.erase(it);
                    }
                    else {
                        cout << console::color::RED << "CAN'T CLEAN #" << it->ch->get_id() << " FROM RAM. Skipping for now.";
                        ++it;
                    }
                }
                else ++it;
            }
        }
        catch (const std::exception& e)
        {
            cout << console::color::DARK_RED << "Async clean exception! " << e.what();
        }
        catch(...)
        {
            cout << console::color::DARK_RED << "Async clean exception uncaught!";
        }
    }
}

ChannelManager::ChannelManager()
{
    keep_running = true;
    cleanup_thr = std::thread([this]{ async_clean(); });
}

ChannelManager::~ChannelManager()
{
    keep_running = false;
    if (cleanup_thr.joinable()) cleanup_thr.join();
}

std::shared_ptr<ChannelConf> ChannelManager::get(const dpp::snowflake& sf)
{
    {
        std::shared_lock<std::shared_mutex> luck(access);
        for(auto& it : chs)
        {
            if (it.ch->is_id(sf)) return it.ch;
        }
    }
    
    cout << console::color::DARK_GRAY << "Loading config for #" << sf << "...";
    std::unique_lock<std::shared_mutex> luck(access);
    const uint64_t time_now_s = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1, 1>>>(std::chrono::system_clock::now().time_since_epoch()).count();

    cout << console::color::DARK_GRAY << "#" << sf << " is good to go.";
    return chs.emplace_back(time_now_s + delta_time_cleanup, std::shared_ptr<ChannelConf>(new ChannelConf(sf))).ch;
}
