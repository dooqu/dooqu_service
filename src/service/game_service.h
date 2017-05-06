#ifndef __GAME_SERVICE_H__
#define __GAME_SERVICE_H__

#include <set>
#include <queue>
#include <list>
#include <map>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio/deadline_timer.hpp>
#include <boost/pool/singleton_pool.hpp>
#include "../net/tcp_server.h"
#include "../service/command_dispatcher.h"
#include "../util/utility.h"
#include "../util/char_key_op.h"
#include "game_client.h"
#include "service_error.h"
#include "http_request.h"

using namespace boost::asio;
using namespace dooqu_service::net;

namespace dooqu_service
{
	namespace service
	{
		class game_zone;
		class game_plugin;

		class game_service : public command_dispatcher, public tcp_server
		{
			typedef std::map<const char*, game_plugin*, char_key_op> game_plugin_map;
			typedef std::map<const char*, game_zone*, char_key_op> game_zone_map;
			typedef std::set<game_client*> game_client_map;
			typedef std::list<game_client*> game_client_destroy_list;

		protected:
			enum{ MAX_AUTH_SESSION_COUNT = 200 };
			game_plugin_map plugins_;
			game_zone_map zones_;
			game_client_map clients_;
			game_client_destroy_list client_list_for_destroy_;
			std::mutex clients_mutex_;
			std::mutex plugins_mutex_;
			std::mutex zones_mutex_;
			std::mutex destroy_list_mutex_;

			boost::asio::deadline_timer check_timeout_timer;

			std::set<http_request*> http_request_working_;
			std::recursive_mutex http_request_mutex_;


			virtual void on_init();
			virtual void on_start();
			virtual void on_stop();
			inline virtual void on_client_command(game_client* client, command* command);
			virtual tcp_client* on_create_client();
			virtual void on_client_join(tcp_client* client);
			virtual void on_client_leave(game_client* client, int code);
			virtual void on_destroy_client(tcp_client*);
			virtual void on_check_timeout_clients(const boost::system::error_code &error);
			virtual void on_destroy_clients_in_destroy_list(bool force_destroy);

			//非虚方法
			void begin_auth(game_plugin* plugin, game_client* client, command* cmd);
			void end_auth(const boost::system::error_code& code, const int, const std::string& response_string, http_request* request, game_plugin* plugin, game_client* client);
			void dispatch_bye(game_client* client);

			//应用层注册
			void client_login_handle(game_client* client, command* command);
			void robot_login_handle(game_client* client, command* command);
			void check_client_on_service_handle(game_client* host, command* command);

		public:
			game_service(unsigned int port);
			virtual ~game_service();
			int load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg = NULL);
			bool unload_plugin(game_plugin* game_plugin, int seconds_wait_for_complete = 0);
			game_plugin_map* get_plugins(){ return &this->plugins_; }
			http_request* get_http_request();
			void free_http_request(http_request*);
		};
	}
}
#endif
