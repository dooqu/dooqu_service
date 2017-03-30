#ifndef __GAME_CLIENT_H__
#define __GAME_CLIENT_H__

#include <iostream>
#include <mutex>
#include "../net/tcp_client.h"
#include "game_info.h"
#include "command.h"
#include "command_dispatcher.h"
#include "../util/tick_count.h"
#include "post_monitor.h"



using namespace dooqu_service::net;
using namespace boost::asio;

namespace dooqu_service
{
	namespace service
	{
		class game_client : public dooqu_service::net::tcp_client
		{
			friend class game_service;
			friend class game_plugin;
			friend class command_dispatcher;

		protected:
			enum{ID_LEN = 16, NAME_LEN = 32, UP_RE_TIMES = 3};
			game_info* game_info_;
			char id_[ID_LEN];
			char name_[NAME_LEN];
			command_dispatcher* cmd_dispatcher_;
			command commander_;
			tick_count actived_time;
			std::recursive_mutex commander_mutex_;
			post_monitor message_monitor_;
			post_monitor active_monitor_;
			int retry_update_times_;
			int error_code_;

			inline void on_data_received(const boost::system::error_code& error, size_t bytes_received);
			virtual void on_error(const int error);

			void fill(char* id, char* name, char* profile);
		public:
			game_client(io_service& ios);
			virtual ~game_client();
			char* id(){ return this->id_;}
			char* name(){ return this->name_;}

			void simulate_on_command(char* command_data, bool is_const_string);
			void set_command_dispatcher(command_dispatcher* dispather);
			inline void active(){this->actived_time.restart();}
			long long get_actived(){return this->actived_time.elapsed();}
			int error_code(){return this->error_code_;}
			inline game_info* get_game_info(){ return this->game_info_; }

			template<typename T>
			T* get_game_info(){ return (T*)this->game_info_; }

			void set_game_info(game_info* info){this->game_info_ = info;}
			inline bool can_message(){ return this->message_monitor_.can_active(); }
			inline bool can_active(){ return this->active_monitor_.can_active(); }
			void disconnect(int code);
		};
	}
}

#endif
