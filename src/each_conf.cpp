#include "each_conf.hpp"

std::string sticker_to_url(const dpp::sticker& stk)
{
    std::string _t = "https://media.discordapp.net/stickers/" + std::to_string(stk.id) + ".";
    switch(stk.format_type) {
    case dpp::sticker_format::sf_apng:
        return _t + ".png";
    case dpp::sticker_format::sf_png:
        return _t + ".png";
    case dpp::sticker_format::sf_lottie:
        return ""; // it's not a file, it's a script.
    }
    return ""; // undef
}

SingleConf::SingleConf(const nlohmann::json& j)
{
    from_json(j);
}

void SingleConf::handle(const dpp::message_create_t& msg, handptr shr) const
{
    if (!shr) return;

    struct __tmp_snd {
        dpp::cluster* master = nullptr;
        dpp::message msg;
        bool sent = false;
        void send(dpp::command_completion_event_t f){if (!sent && master && msg.channel_id != 0) master->message_create(msg, f); sent = true; }
    };

    std::unique_ptr<__tmp_snd> tosend = std::make_unique<__tmp_snd>();

    const auto def_err_hdlr = [creat = msg.from->creator, mid = msg.msg.id, chid = msg.msg.channel_id, gid = msg.msg.guild_id, epost = post_error](const dpp::confirmation_callback_t& ret){
        if (ret.is_error() && epost) {
            dpp::message buck;
            buck.set_reference(mid, gid);
            buck.channel_id = chid;
            buck.content = "**Error handling this:**\n```md\n# ERROR_CODE: " + std::to_string(ret.get_error().code) + "\n- Resumed: " + ret.get_error().message + "\n- Full error:\n\n" + ret.http_info.body + "\n```";
            creat->message_create(buck);
        }
    };
    const auto easy_err = [creat = msg.from->creator, mid = msg.msg.id, chid = msg.msg.channel_id, gid = msg.msg.guild_id, epost = post_error](const std::string& res, const std::string& errstr){
        if (epost) {
            dpp::message buck;
            buck.set_reference(mid, gid);
            buck.channel_id = chid;
            buck.content = "**Error handling this:**\n```md\n# INVALID_STATE\n- Resumed: " + res + "\n- Full error:\n\n" + errstr + "\n```";
            creat->message_create(buck);
        }        
    };
    const auto gen_header = [dt = detailed, chid = msg.msg.channel_id, nick = msg.msg.member.nickname, usrnm = msg.msg.author.username, iddd = msg.msg.author.discriminator](const std::string& str = {}){
        return (dt ? ("**[`" + dpp::utility::current_date_time() + "`]<#" + std::to_string(chid) + ">> " + usrnm + "#" + std::to_string(iddd) + ":**") : ("**" + nick + ":**")) + (str.empty() ? "" : (" " + str));
    };
    
    try{
        const std::regex rgx = std::regex(regx);

        tosend->master = msg.from->creator;
        tosend->msg.guild_id = msg.msg.guild_id;
        tosend->msg.channel_id = redir_id;
        tosend->msg.set_content(gen_header());

        switch(trigger_type)
        {
        case gconf_type::NONE:
            return;
        case gconf_type::TEXT:
        {
            if (
                (min_size < 0 || msg.msg.content.length() >= min_size) &&
                (max_size < 0 || msg.msg.content.length() <= max_size) &&
                std::regex_search(msg.msg.content, rgx, std::regex_constants::match_any)
            )
            {
                if (redir_id != 0) {
                    shr->triggered_one = true;
                    tosend->msg.set_content(gen_header(msg.msg.content));
                }
                shr->should_delete_message |= del_trigger;
                shr->should_show_error |= post_error;
                //if (del_trigger) {
                    //msg.from->creator->message_delete(msg.msg.id, msg.msg.channel_id, def_err_hdlr);
                //}
            }
        }
        break;
        case gconf_type::FILE:
        {
            //std::shared_ptr<std::atomic<uint32_t>> _shh = std::make_shared<std::atomic<uint32_t>>();
            //(*_shh) = 1;

            if (
                (min_size < 0 || msg.msg.attachments.size() >= min_size) &&
                (max_size < 0 || msg.msg.attachments.size() <= max_size) 
            )
            {            
                auto why = std::shared_ptr<__tmp_snd>((__tmp_snd*)tosend.release()); // can't move in lambda to function because it copies that, why? ;-;

                for(const auto& ifp : msg.msg.attachments) {
                    if (std::regex_search(ifp.filename, rgx, std::regex_constants::match_any))
                    {
                        if (redir_id != 0) {
                            //++(*_shh);
                            shr->going_on_yet++;
                            shr->should_delete_message |= del_trigger;
                            shr->should_show_error |= post_error;

                            ifp.download([ptr = why, shr, def_err_hdlr, easy_err, fpnam = ifp.filename, creat = msg.from->creator, ma = msg.msg.id, mb = msg.msg.channel_id](const dpp::http_request_completion_t& pp) mutable {
                                if (pp.error != dpp::http_error::h_success || pp.body.empty()){           
                                    easy_err("Download failed", pp.body);
                                    
                                    if (--shr->going_on_yet == 0 && shr->should_delete_message) {
                                        creat->message_delete(ma, mb, def_err_hdlr);
                                    }
                                    return;
                                }
                                __tmp_snd cpy = *ptr; // copy for each

                                cpy.msg.set_filename(fpnam);
                                cpy.msg.set_file_content(pp.body);
                                cpy.send(def_err_hdlr); // within async body, must send manually here.

                                if (--shr->going_on_yet == 0 && shr->should_delete_message) {
                                    creat->message_delete(ma, mb, def_err_hdlr);
                                }

                                //if (dtg && (--(*_shh) == 0)) {
                                //    creat->message_delete(ma, mb, def_err_hdlr);
                                //}
                            });
                            // TOSEND is INVALID from here!
                        }
                    }
                }
                //if (del_trigger && (--(*_shh) == 0)) {
                //    msg.from->creator->message_delete(msg.msg.id, msg.msg.channel_id, def_err_hdlr);
                //}
            }
            return; // no post later
        }
        break;
        case gconf_type::STICKER:
        {
            for(const auto& stk : msg.msg.stickers) {
                if (
                    (min_size < 0 || stk.name.length() >= min_size) &&
                    (max_size < 0 || stk.name.length() <= max_size) &&
                    std::regex_search(stk.name, rgx, std::regex_constants::match_any)
                )
                {
                    if (redir_id != 0) {
                        
                        const std::string sticker_url = sticker_to_url(stk);
                        if (sticker_url.empty()) {
                            easy_err("Invalid sticker", "Sorry, but the bot don't really know what to do with lottie format...");
                            return;
                        }

                        tosend->msg.add_embed(dpp::embed().set_image(sticker_url));
                    }
                    shr->should_delete_message |= del_trigger;
                    shr->should_show_error |= post_error;
                    //if (del_trigger) {
                    //    msg.from->creator->message_delete(msg.msg.id, msg.msg.channel_id, def_err_hdlr);
                    //}
                }
            }
        }
        break;
        }
    }
    catch (const std::exception& e)
    {
        if (post_error)
            msg.reply(dpp::message().set_content("**Internal error handling this message:**\n```md\n# STD::EXCEPTION\n- " + std::string(e.what()) + "\n```").set_reference(msg.msg.id, msg.msg.guild_id));

        return;
    }
    catch (...)
    {
        if (post_error)
            msg.reply(dpp::message().set_content("**Internal error handling this message:**\n```md\n# UNCAUGHT\n- Unexpected error: uncaught.\n```").set_reference(msg.msg.id, msg.msg.guild_id));

        return;
    }
    
    if (tosend) tosend->send(def_err_hdlr);
    return;
}

/*
gconf_type trigger_type = gconf_type::NONE;

std::string regx = ".*"; // text: text, sticker: name, file: file name, reaction: full name regex
long min_size = -1; // text: len, sticker: name len, file: size, reaction: name len (full)
long max_size = -1; // text: len, sticker: name len, file: size, reaction: name len (full)
bool del_trigger = false; // remove trigger?
unsigned long long redir_id = 0; // chat to copy this to (disabled for reaction)
bool post_error = true;
*/

nlohmann::json SingleConf::to_json() const
{
    nlohmann::json j;
    j["0rgx"] = regx; // rgx = regex
    j["1lsz"] = min_size; // lsz = lower size
    j["2gsz"] = max_size; // gsz = greater size
    j["3dtg"] = del_trigger; // del trigger
    j["4rid"] = redir_id;
    j["5per"] = post_error;
    j["6ttp"] = static_cast<int32_t>(trigger_type);
    j["7dtl"] = detailed;
    return j;
}

void SingleConf::from_json(const nlohmann::json& j)
{
    regx         = any_get<std::string>(&j, "0rgx", regx); // rgx = regex
    min_size     = any_get<int32_t>(&j, "1lsz", min_size); // lsz = lower size
    max_size     = any_get<int32_t>(&j, "2gsz", max_size); // gsz = greater size
    del_trigger  = any_get<bool>(&j, "3dtg", del_trigger); // del trigger
    redir_id     = any_get<dpp::snowflake>(&j, "4rid", redir_id);
    post_error   = any_get<bool>(&j, "5per", post_error);
    trigger_type = static_cast<gconf_type>(any_get<int32_t>(&j, "6ttp", static_cast<int32_t>(trigger_type)));
    detailed     = any_get<bool>(&j, "7dtl", detailed);
}

std::string gconf_to_str(const SingleConf::gconf_type& g)
{
    switch(g){
    case SingleConf::gconf_type::NONE:
        return "NONE";
    case SingleConf::gconf_type::TEXT:
        return "TEXT";
    case SingleConf::gconf_type::FILE:
        return "FILE";
    case SingleConf::gconf_type::STICKER:
        return "STICKER";
    }
    return "UNKNOWN";
}