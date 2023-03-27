#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"

#include "command.hpp"
#include "console.hpp"
#include "scheduler.hpp"
#include "party.hpp"
#include "network.hpp"
#include "server_list.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>
#include <utils/cryptography.hpp>

namespace bots
{
	namespace
	{
		bool can_add()
		{
			return party::get_client_count() < *game::mp::svs_clientCount;
		}

		void bot_team_join(const int entity_num)
		{
			// schedule the select team call
			scheduler::once([entity_num]()
			{
				game::SV_ExecuteClientCommand(&game::mp::svs_clients[entity_num],
				                              utils::string::va("lui 68 2 %i", *game::mp::sv_serverId_value),
				                              false);

				// scheduler the select class call
				scheduler::once([entity_num]()
				{
					game::SV_ExecuteClientCommand(&game::mp::svs_clients[entity_num],
					                              utils::string::va("lui 5 %i %i", (rand() % 5) + 10,
					                                                *game::mp::sv_serverId_value), false);
				}, scheduler::pipeline::server, 1s);
			}, scheduler::pipeline::server, 1s);
		}

		void bot_team(const int entity_num)
		{
			if (game::SV_BotIsBot(game::mp::g_entities[entity_num].s.clientNum))
			{
				if (game::mp::g_entities[entity_num].client->sess.cs.team == game::mp::team_t::TEAM_SPECTATOR)
				{
					bot_team_join(entity_num);
				}

				scheduler::once([entity_num]()
				{
					bot_team(entity_num);
				}, scheduler::pipeline::server, 3s);
			}
		}

		void spawn_bot(const int entity_num)
		{
			scheduler::once([entity_num]()
			{
				game::SV_SpawnTestClient(&game::mp::g_entities[entity_num]);
				bot_team(entity_num);
			}, scheduler::pipeline::server, 1s);
		}

		std::string botnames[100];
		int results = 0;
		int current = 0;
		void get_bot_names()
		{
			std::string filename = "iw6x\\bots.txt";
			std::string line;
			std::ifstream file(filename);

			if (file.is_open())
			{
				while (!file.eof())
				{
					std::getline(file, line);
					botnames[results] = line;
					results++;
				}
				file.close();
			}
		}

		const char* SV_BotGetRandomName_stub()
		{
			if (current == results)
			{
				current = 0;
			}
			const char* name = botnames[current].c_str();
			current++;

			return name;
		}

		void add_bot()
		{
			if (!can_add())
			{
				return;
			}

			auto* bot_name = game::SV_BotGetRandomName();
			auto* bot_ent = game::SV_AddBot(bot_name, 26, 62, 0);
			if (bot_ent)
			{
				spawn_bot(bot_ent->s.entityNum);
			}
		}

		utils::hook::detour get_bot_name_hook;
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_sp())
			{
				return;
			}

			get_bot_name_hook.create(game::SV_BotGetRandomName, SV_BotGetRandomName_stub);

			command::add("spawnBot", [](const command::params& params)
			{
				if (!game::SV_Loaded()) return;

				auto num_bots = 1;
				if (params.size() == 2)
				{
					num_bots = atoi(params.get(1));
				}

				num_bots = std::min(num_bots, *game::mp::svs_clientCount);

				for (auto i = 0; i < num_bots; i++)
				{
					scheduler::once(add_bot, scheduler::pipeline::server, 100ms * i);
				}
			});

			scheduler::on_game_initialized([]()
			{
				get_bot_names();
			}, scheduler::main);

		}
	};
}

REGISTER_COMPONENT(bots::component)
