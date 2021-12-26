#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <dpp/fmt/format.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "console.h"
#include "channel_manager.hpp"

#include <memory>

using namespace Lunaris;


bool get_from(const std::vector<dpp::command_data_option>& opts, dpp::command_value& gt, const std::string& key)
{
    for (const auto& it : opts) {
        if (it.name == key) {
            gt = it.value;
            return true;
        }
        if (it.options.size())
        {
            if (get_from(it.options, gt, key))
                return true;
        }
    }
    return false;
}

template<typename T>
bool find_recursive_def(const std::vector<dpp::command_data_option>& opts, T& st, const std::string& key)
{
    for(const auto& o : opts)
    {
        if (o.name == key) {
            st = std::get<T>(o.value);
            return true;
        }
        if (o.options.size())
        {
            if (find_recursive_def(o.options, st, key)) return true;
        }
    }
    return false;
}

template<typename T>
T find_recursive(const dpp::command_interaction& opts, bool& fnd, const std::string& key)
{
    T t;
    fnd = find_recursive_def<T>(opts.options, t, key);
    return t;
}

template<typename T>
T& log_get(dpp::command_value& d)
{
    try{
        return std::get<T>(d);
    }
    catch(const std::exception& e)
    {
        cout << console::color::RED << "[FATAL ERROR] log_get type " << typeid(T).name() << " failed: " << e.what();
        throw e;
    }
    catch(...)
    {
        cout << console::color::RED << "[FATAL ERROR] log_get type " << typeid(T).name() << " failed: uncaught";
        throw std::current_exception();
    }
}

int main()
{
	cout << "Loading Redirektor bot...";
	cout << "Opening file 'config.json'...";

    std::string toeknn;

    {
        std::ifstream fp("./config.json");
        if (fp.bad() || !fp.is_open()){
	        cout << console::color::RED << "FATAL ERROR: NO CONFIG FILE!";
            return 1;
        }

        nlohmann::json mj;
        fp >> mj;

        if (!mj.contains("token") || !mj["token"].is_string()){
	        cout << console::color::RED << "FATAL ERROR: INVALID CONFIG FILE: need token json set";
            return 1;
        }

        toeknn = mj["token"].get<std::string>();
    }

	dpp::cluster bot(toeknn, dpp::intents::i_default_intents | dpp::intents::i_guild_messages);
    ChannelManager chmng;
 
    bot.on_interaction_create([&bot,&chmng](const dpp::interaction_create_t& event) {
        if (event.command.type == dpp::it_application_command)
        {
            {
                dpp::message msg;
                msg.content = "*";
                msg.guild_id = event.command.guild_id;
                msg.channel_id = event.command.channel_id;
                msg.flags = 64;
                event.reply(dpp::interaction_response_type::ir_deferred_channel_message_with_source, msg);
            }

            dpp::command_interaction cmd = std::get<dpp::command_interaction>(event.command.data);

            if (cmd.options.size() == 0) {
                cout << console::color::RED << "Empty command?! Abort.";
                event.edit_response(dpp::message().set_flags(64).set_content("Something went wrong."));
                return;
            }

            bool found = false;
            const int64_t confnum = find_recursive<int64_t>(cmd, found, "config-number");

            if (!found) {
                cout << console::color::RED << "Empty command?! Abort.";
                event.edit_response(dpp::message().set_flags(64).set_content("Something went wrong."));
                return;
            }
            if (confnum < 0 || confnum >= 10) { // for now allow up to 10 configs per chat
                event.edit_response(dpp::message().set_flags(64).set_content("Invalid number. Please try in this range: [0, 10)"));
            }

            bot.roles_get(event.command.guild_id, [event, cmd, &chmng, confnum](const dpp::confirmation_callback_t& ev){ // role_map
                if (ev.is_error()) {
                    event.edit_response(dpp::message().set_flags(64).set_content("Something went wrong whilst doing this action."));
                    return;
                }

                dpp::role_map rols = std::get<dpp::role_map>(ev.value);

                bool is_admin = false;
                for (const auto& rl : rols)
                {
                    for (const auto& mrl : event.command.member.roles)
                    {
                        if (mrl == rl.first)
                        {
                            is_admin = rl.second.has_administrator();
                            break;
                        }
                    }

                    if (is_admin)
                        break;
                }

                if (!is_admin) {
                    event.edit_response(dpp::message().set_flags(64).set_content("You must have administrator rights (from roles)."));
                    return;
                }

                auto chconf = chmng.get(event.command.channel_id);
                dpp::command_value wrk;

                try{
                    if (cmd.options[0].name == "get")
                    {
                        chconf->index(confnum, [&](SingleConf& sc) { 
                            dpp::message msg;
                            msg.guild_id = event.command.guild_id;
                            msg.channel_id = event.command.channel_id;
                            msg.flags = 64;

                            msg.content = "```ini\n"
                                "config_number   = '" + std::to_string(confnum) + "'\n"
                                "trigger_type    = '" + gconf_to_str(sc.trigger_type) + "'\n" 
                                "regex_pattern   = '" + sc.regx + "'\n"
                                "minimum_size    = '" + (sc.min_size >= 0 ? std::to_string(sc.min_size) : "none") + "'\n"
                                "maximum_size    = '" + (sc.min_size >= 0 ? std::to_string(sc.max_size) : "none") + "'\n"
                                "delete_trigger  = '" + std::string(sc.del_trigger ? "Yes" : "No") + "'\n"
                                "redirect_to_ch  = '" + (sc.redir_id != 0 ? std::to_string(sc.redir_id) : "none") + "'\n"
                                "post_errors     = '" + std::string(sc.post_error ? "Yes" : "No") + "'\n"
                                "detailed        = '" + std::string(sc.detailed ? "Yes" : "No") + "'\n"
                                "```";
                            
                            event.edit_response(msg);
                        });
                    }
                    else
                    {
                        if (get_from(cmd.options, wrk, "trigger-type"))     chconf->index(confnum, [&](SingleConf& sc) { sc.trigger_type = static_cast<SingleConf::gconf_type>(log_get<int64_t>(wrk)); });
                        if (get_from(cmd.options, wrk, "regex"))            chconf->index(confnum, [&](SingleConf& sc) { sc.regx = log_get<std::string>(wrk); });
                        if (get_from(cmd.options, wrk, "min-size"))         chconf->index(confnum, [&](SingleConf& sc) { sc.min_size = static_cast<int32_t>(log_get<int64_t>(wrk)); });
                        if (get_from(cmd.options, wrk, "max-size"))         chconf->index(confnum, [&](SingleConf& sc) { sc.max_size = static_cast<int32_t>(log_get<int64_t>(wrk)); });
                        if (get_from(cmd.options, wrk, "delete-trigger"))   chconf->index(confnum, [&](SingleConf& sc) { sc.del_trigger = log_get<bool>(wrk); });
                        if (get_from(cmd.options, wrk, "redirect-to"))      chconf->index(confnum, [&](SingleConf& sc) { sc.redir_id = log_get<dpp::snowflake>(wrk); });
                        if (get_from(cmd.options, wrk, "redirect-reset"))   chconf->index(confnum, [&](SingleConf& sc) { if (log_get<bool>(wrk)) sc.redir_id = 0; });
                        if (get_from(cmd.options, wrk, "post-errors"))      chconf->index(confnum, [&](SingleConf& sc) { sc.post_error = log_get<bool>(wrk); });   
                        if (get_from(cmd.options, wrk, "detailed"))         chconf->index(confnum, [&](SingleConf& sc) { sc.detailed = log_get<bool>(wrk); });   
                        event.edit_response(dpp::message().set_flags(64).set_content("Good."));         
                    }
                }
                catch(const std::exception& e)
                {
                    event.edit_response(dpp::message().set_flags(64).set_content("An internal error occurred: " + std::string(e.what())));
                    return;
                }
                catch(...)
                {
                    event.edit_response(dpp::message().set_flags(64).set_content("An internal error occurred. Please check your arguments and try again later."));
                    return;
                }            

            });
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        cout << "Logged in as " << bot.me.username << "!";

        // delete guild commandsexit
        //bot.guild_commands_get(508808506569261100, [&bot](const dpp::confirmation_callback_t& ev) {
        //    if (ev.is_error()) return;
        //    dpp::slashcommand_map slmap = std::get<dpp::slashcommand_map>(ev.value);
        //    for(const auto& it : slmap){
        //        bot.guild_command_delete(it.first, 508808506569261100);
        //    }
        //});
 
        bot.global_commands_get([&bot](const dpp::confirmation_callback_t& ev) {

            dpp::slashcommand newcommand;
            
            {
                newcommand
                    .set_name("config")
                    .set_description("Setup bot")
                    .set_application_id(bot.me.id)
                    .add_option(
                        dpp::command_option(dpp::co_sub_command, "get", "Read a configuration")
                            .add_option(
                                dpp::command_option(dpp::co_integer, "config-number", "Select one slot (starts at 0)", true)
                            )

                    )
                    .add_option(
                        dpp::command_option(dpp::co_sub_command, "set", "Configure the bot")
                            .add_option(
                                dpp::command_option(dpp::co_integer, "config-number", "Select one slot (starts at 0)", true)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_integer, "trigger-type", "Configuration trigger type (default: disabled)", false)
                                    .add_choice(dpp::command_option_choice("Disabled (default)",      static_cast<int64_t>(SingleConf::gconf_type::NONE)))
                                    .add_choice(dpp::command_option_choice("Text (message)",          static_cast<int64_t>(SingleConf::gconf_type::TEXT)))
                                    .add_choice(dpp::command_option_choice("File (name and/or size)", static_cast<int64_t>(SingleConf::gconf_type::FILE)))
                                    .add_choice(dpp::command_option_choice("Sticker (name)",          static_cast<int64_t>(SingleConf::gconf_type::STICKER)))
                            )
                            .add_option(
                                dpp::command_option(dpp::co_string, "regex", "Regex trigger (default: .*)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_integer, "min-size", "Minimum size (text types: length, file: file amount) (default: -1)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_integer, "max-size", "Maximum size (text types: length, file: file amount) (default: -1)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_boolean, "delete-trigger", "Delete message trigger? (default: false)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_channel, "redirect-to", "Redirect triggered content to? (default: none)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_boolean, "redirect-reset", "Delete redirect setting? (true = deletes channel redirect setting)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_boolean, "post-errors", "Post any error in chat? (default: true)", false)
                            )
                            .add_option(
                                dpp::command_option(dpp::co_boolean, "detailed", "Resulting messages with detailed info or simple?", false)
                            )
                    );
            }

            if (ev.is_error()){
                cout << console::color::GOLD << "Could not get global commands. Setting up anyway.";
            }
            else {
                dpp::slashcommand_map slmap = std::get<dpp::slashcommand_map>(ev.value);
            
                for(const auto& it : slmap)
                {
                    if (it.second.name == "config")
                    {
                        cout << console::color::GREEN << "Found command set. Assuming good.";
                        return;
                    }
                }
            }
            
            cout << console::color::GREEN << "Global command is being set soon.";

            /* Register the command */
            bot.global_command_create(newcommand, [](const dpp::confirmation_callback_t& conf) {
            //bot.guild_command_create(newcommand, 508808506569261100, [](const dpp::confirmation_callback_t& conf) {
                if (conf.is_error()) cout << console::color::RED << "Could not setup global command properly! Error:\n" << conf.get_error().message << "\n\n" << conf.http_info.body;
                else cout << console::color::GREEN << "Global command sent!";
            });
        });
    });
 
    bot.on_message_create([&bot,&chmng](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) return;

        chmng.get(event.msg.channel_id)->run(event);

        //if (event.msg.content == "!ping") {
        //    bot.message_create(dpp::message(event.msg.channel_id, "Pong!"));
        //}
    });

    bot.on_log([](const dpp::log_t& event){
        switch(event.severity)
        {
        case dpp::ll_trace:
            break;
		case dpp::ll_debug:
            cout << console::color::DARK_PURPLE << "[DBUG] " << event.message;
            break;
		case dpp::ll_info:
            cout << console::color::GREEN << "[INFO] " << event.message;
            break;
		case dpp::ll_warning:
            cout << console::color::GOLD << "[WARN] " << event.message;
            break;
		case dpp::ll_error:
            cout << console::color::RED << "[ERRR] " << event.message;
            break;
		case dpp::ll_critical:
            cout << console::color::GREEN << "[CRIT] " << event.message;
            break;
        }
    });
 
    bot.start(true);

    while(1) {
        std::string input;
        std::getline(std::cin, input);

        if (input == "exit")
            break;
    }
}