#ifndef __ASK_PLUGIN__
#define __ASK_PLUGIN__

#include <boost/thread/recursive_mutex.hpp>
#include "../service/game_plugin.h"
#include "ask_zone.h"
#include "ask_room.h"
#include "ask_game_info.h"

namespace dooqu_server
{
	namespace plugins
	{
		using namespace dooqu_server::service;

		class ask_plugin : public game_plugin
		{
		public:
			enum{ROOM_COUNT = 100, QUESTION_COUNT = 20, QUESTION_RESP_MILLITIMES = 15000};
		protected:

			void on_load();

			void on_unload();

			void on_update();

			int on_auth_client(game_client* client, const std::string&);

			int on_befor_client_join(game_client* client);

			void on_client_join(game_client* client);

			void on_client_leave(game_client* client, int code);

			void on_room_game_start(ask_room* , int);

			void on_room_game_question(ask_room*, int);

			void on_question_no_answer(ask_room*);

			void on_room_game_stop(ask_room* room);

			////////////////////////////////////////////////////////////////
			void client_join_room_handle(game_client* client, command* command);

			void on_client_join_room(ask_room* room, game_client* client, int pos_index);

			void client_leave_room_handle(game_client* client, command* command);

			void on_client_leave_room(ask_room* room, game_client* client, int pos_index);

			void client_ready_handle(game_client* client, command* command);

			void client_answer_handler(game_client* client, command*);

			game_zone* on_create_zone(char* zone_id);

			ask_room* rooms_[ROOM_COUNT];

		public:

			ask_plugin(game_service* service, char* game_id, char* title) : game_plugin(service, game_id, title)
			{

			}
		};
	}
}

#endif
