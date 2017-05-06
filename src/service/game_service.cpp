#include "game_service.h"
#include "game_plugin.h"
#include "game_zone.h"

namespace dooqu_service
{
	namespace service
	{
		game_service::game_service(unsigned int port) :
			tcp_server(port), check_timeout_timer(io_service_)
		{
		}


		int game_service::load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg)
		{
			if (game_plugin == NULL || game_plugin->game_id() == NULL || game_plugin->game_service_ != this)
			{
                //dooqu_service::util::print_error_info("plugin not created, argument error.");
                (*errorMsg) = "argument error.";
                return -1;
			}

            if(game_plugin->is_onlined_ || game_plugin->clients()->size() > 0)
            {
                //dooqu_service::util::print_error_info("plugin {%s} is in using.", game_plugin->game_id());
                (*errorMsg) = "plugin is in using.";
                return -2;
            }

			__lock__(this->plugins_mutex_, "create_plugin");

			game_plugin_map::iterator curr_plugin = this->plugins_.find(game_plugin->game_id());

			if (curr_plugin != this->plugins_.end())
			{
                //dooqu_service::util::print_error_info("plugin's id {%s} is in use.", game_plugin->game_id());
                (*errorMsg) = "plugin is in using.";
				return -2;
			}

			this->plugins_[game_plugin->game_id()] = game_plugin;

			//如果传递的zoneid不为空，那么为游戏挂载游戏区
			if (zone_id != NULL)
			{
				game_zone_map::iterator curr_zone = this->zones_.find(zone_id);

				if (curr_zone != this->zones_.end())
				{
					game_plugin->zone_ = (*curr_zone).second;
				}
				else
				{
					game_zone* zone = game_plugin->on_create_zone(zone_id);

					if (this->is_running_ && zone != NULL)
					{
						zone->load();
						//为这个分区创建工作者线程
						//this->create_worker_thread();
					}
					this->zones_[zone->get_id()] = zone;
					game_plugin->zone_ = zone;
				}
			}

			if (this->is_running_)
			{
				game_plugin->load();
			}

			return 1;
		}


		bool game_service::unload_plugin(game_plugin* plugin, int seconds_for_wait_compl)
		{
            if(plugin == NULL || plugin->game_id() == NULL)
                return false;

            plugin->unload();

            for(int i = 0; i < seconds_for_wait_compl * 10; i++)
            {
                if(plugin->clients()->size() > 0)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
                return true;
            }
            return false;
        }


		void game_service::on_init()
		{
			std::srand((unsigned)time(NULL));

			this->regist_handle("LOG", make_handler(game_service::client_login_handle));
			this->regist_handle("RLG", make_handler(game_service::robot_login_handle));

			//加载所有的所有分区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
			curr_zone != this->zones_.end(); ++curr_zone)
			{
				(*curr_zone).second->load();

				//为这个分区创建工作者线程
				//this->create_worker_thread();
			}

			{
				__lock__(this->plugins_mutex_, "game_servcie::on_init::plugin_mutex");
				//加载所有的游戏逻辑
				for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); ++curr_game)
				{
					(*curr_game).second->load();
				}
			}
		}


		void game_service::on_start()
		{
			///!!!!!!!!!!!
			//启动检查登录超时用户的定时器
			this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(3));
			this->check_timeout_timer.async_wait(std::bind(&game_service::on_check_timeout_clients, this, std::placeholders::_1));
		}


		void game_service::on_stop()
		{
			//停止对登录超时的检测
			this->check_timeout_timer.cancel();

			//清理临时登录用户的内存资源
			{
				__lock__(this->clients_mutex_, "game_service::on_stop::client_mutex");

				for (game_client_map::iterator curr_client = this->clients_.begin();
				curr_client != this->clients_.end(); ++curr_client)
				{
					game_client* client = (*curr_client);
					client->disconnect(service_error::SERVER_CLOSEING);
				}
			}

			this->on_destroy_clients_in_destroy_list(true);

			{
				__lock__(this->plugins_mutex_, "game_service::on_stop::plugins_mutex");
				//停止所有的游戏逻辑
				for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); ++curr_game)
				{
					(*curr_game).second->unload();
				}
			}

			//boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
			//停止所有的游戏区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
			curr_zone != this->zones_.end(); ++curr_zone)
			{
				(*curr_zone).second->unload();
			}
		}


		void game_service::dispatch_bye(game_client* client)
		{
			this->on_client_leave(client, client->error_code());
		}


		void game_service::on_client_command(game_client* client, command* command)
		{
			if (this->is_running_ == false)
				return;

			if (client->can_active() == false)
			{
				client->disconnect(service_error::CONSTANT_REQUEST);
				return;
			}

			command_dispatcher::on_client_command(client, command);
		}


		tcp_client* game_service::on_create_client()
		{
			void* client_mem = boost::singleton_pool<game_client, sizeof(game_client)>::malloc();

			return new(client_mem)game_client(this->get_io_service());
		}


		void game_service::on_client_join(tcp_client* client)
		{
			printf("game_client connected.\n");

			game_client* g_client = (game_client*)client;
			{
				//对用户组上锁
				__lock__(this->clients_mutex_, "game_service::on_client_join");
				//在登录用户组中注册
				this->clients_.insert(g_client);
			}
			//重置活动时间戳
			g_client->active();

			//设置socket为激活状态
			g_client->available_ = true;

			//设置命令的监听者为当前service
			g_client->set_command_dispatcher(this);

			//开始读取用户的网络消息
			g_client->read_from_client();
		}



		void game_service::on_client_leave(game_client* client, int leave_code)
		{
			printf("{%s} leave game_service. code={%d}\n", client->id(), leave_code);

			client->set_command_dispatcher(NULL);

			{
				__lock__(this->clients_mutex_, "game_service::on_client_leave::clients_mutex");
				this->clients_.erase(client);
			}

			bool is_client_sending_not_completed;

			{
				__lock__(client->status_lock_, "game_service::on_client_leave::status_lock");
				is_client_sending_not_completed = client->is_data_sending_;
			}

			if (is_client_sending_not_completed == false)
			{
				this->on_destroy_client(client);
			}
			else
			{
                __lock__(this->destroy_list_mutex_, "game_service::on_client_leave::destroy_list_mutext");
				this->client_list_for_destroy_.push_back(client);
                printf("INSERT client to destroy list.\n");
			}
		}


		void game_service::on_destroy_client(tcp_client* t_client)
		{
			game_client* client = (game_client*)t_client;
			client->~game_client();
			boost::singleton_pool<game_client, sizeof(game_client)>::free(client);
		}

        void game_service::on_destroy_clients_in_destroy_list(bool force_destroy)
        {
            __lock__(this->destroy_list_mutex_, "game_service::on_destroy_clients_in_destroy_list");

            for(list<game_client*>::iterator e = this->client_list_for_destroy_.begin(); e != this->client_list_for_destroy_.end(); )
            {
                game_client* client = *e;

                if(force_destroy)
                {
                    this->on_destroy_client(client);
                    this->client_list_for_destroy_.erase(e++);
                }
                else
                {
                    if(client->is_data_sending_ == false)
                    {
                        this->on_destroy_client(client);
                        this->client_list_for_destroy_.erase(e++);
                        continue;
                    }
                    ++e;
                }
            }
        }


		//使用了定时器
		void game_service::on_check_timeout_clients(const boost::system::error_code &error)
		{
			//printf("game_service::on_run's thread id:%d\n", boost::this_thread::get_id());

			//thread_mutex_map::iterator curr_mutex = this->thread_mutexs()->find(boost::this_thread::get_id());
			//boost::recursive_mutex::scoped_lock lock(*curr_mutex->second);
			if (!error && this->is_running_)
			{
				//如果禁用超时检测，请注释return;
				{
					//对登录用户上锁
					__lock__(this->clients_mutex_, "game_service::on_check_timeout_clients::clients_mutex");

					//对所有用户的active时间进行对比，超过20秒还没有登录动作的用户被装进临时数组
					for (game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); ++curr_client)
					{
						game_client* client = (*curr_client);

						if (client->get_actived() > 10 * 1000)
						{
							client->disconnect(service_error::TIME_OUT);
						}
					}
				}

				static tick_count timeout_count;

				if (timeout_count.elapsed() > 60 * 1000)
				{
					__lock__(this->plugins_mutex_, "game_service::on_check_timeout_clients::plugins_mutex");
					for (game_plugin_map::iterator curr_game = this->get_plugins()->begin();
					curr_game != this->get_plugins()->end();
						++curr_game)
					{
						(*curr_game).second->on_update_timeout_clients();
					}

					timeout_count.restart();
				}

				this->on_destroy_clients_in_destroy_list(false);


				if (tcp_client::LOG_IO_DATA)
				{
					char io_buffer_msg[64] = { 0 };
					sprintf(io_buffer_msg, "I/O : %ld / %ld\r", tcp_client::CURR_RECE_TOTAL / 5, tcp_client::CURR_SEND_TOTAL / 5);
					printf(io_buffer_msg);

					tcp_client::CURR_RECE_TOTAL = 0;
					tcp_client::CURR_SEND_TOTAL = 0;
				}

				//如果服务没有停止， 那么继续下一次计时
				if (this->is_running_)
				{
					this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
					this->check_timeout_timer.async_wait(std::bind(&game_service::on_check_timeout_clients, this, std::placeholders::_1));
				}
			}
		}


		void game_service::begin_auth(game_plugin* plugin, game_client* client, command* cmd)
		{
			if (this->http_request_working_.size() > MAX_AUTH_SESSION_COUNT)
			{
				client->disconnect(service_error::LOGIN_CONCURENCY_LIMIT);
				return;
			}

			//如果需要不请求http server请拿掉此注释


			//分配http_request的对象内存
			//void* request_mem = this->get_http_request();

			this->end_auth(boost::asio::error::eof, 200, std::string(), NULL, plugin, client);
			//http_request* request = new(request_mem)http_request(this->get_io_service(),
			//	"127.0.0.1",
			//	"/auth.aspx",
			//	boost::bind(&game_service::end_auth, this, _1, _2, _3, (http_request*)request_mem, plugin, client));
		}


		void game_service::end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, http_request* request, game_plugin* plugin, game_client* client)
		{
			do
			{
				if (this->is_running_ == false || plugin->is_onlined() == false)
					break;

				int ret = service_error::OK;

				{
					//对登录的用户列表进行上锁
					__lock__(this->clients_mutex_, "game_service::end_auth::clients_mutex");
					game_client_map::iterator finder = this->clients_.find(client);

					//client 在另外一个线程已经被销毁，超时了，或者主动离开等等情况。
					//本质上就是依靠this->clients_来判断当前的client对象是否已经被销毁了。
					if (finder == this->clients_.end())
						break;

					//用户登录动作完成，从排队用户中删除。
					this->clients_.erase(client);

					if (client->available() == false)
						break;
				}

				//http ok
				if (code == boost::asio::error::eof && status_code == 200)
				{
					ret = plugin->auth_client(client, response_string);
				}
				else
				{
					printf("http authentication server error.\n");
					ret = service_error::LOGIN_SERVICE_ERROR;
				}

				if (ret != service_error::OK)
				{
					printf("disconnect client because auth error.\n");

					client->disconnect(ret);
					//client->write("ERR %d%c", ret, NULL);
					//client->disconnect_when_io_end();
				}

				break;

			} while (true);

			//this->free_http_request(request);
		}


		http_request* game_service::get_http_request()
		{
			__lock__(this->http_request_mutex_, "game_service::get_http_request");

			void* request_mem = boost::singleton_pool<http_request, sizeof(http_request)>::malloc();
			this->http_request_working_.insert((http_request*)request_mem);

			return (http_request*)request_mem;
		}


		void game_service::free_http_request(http_request* request)
		{
			{
				__lock__(this->http_request_mutex_, "game_service::free_http_request");
				//这里要确认http_request对象的访问还合法， 极限情况，在on_stop中已经销毁掉了。
				//if (this->http_request_working_.find(request) != this->http_request_working_.end())
				{
					this->http_request_working_.erase(request);
				}
			}

			request->~http_request();
			boost::singleton_pool<http_request, sizeof(http_request)>::free((void*)request);
		}


		void game_service::client_login_handle(game_client* client, command* command)
		{
			if (command->param_size() != 2)
			{
				client->disconnect(service_error::COMMAND_ERROR);
				return;
			}

			if (client->available() == false)
				return;

			//更新激活时间，防止被另外一个线程的更新销毁掉。
			//client->active();

			int result_code = service_error::OK;

			//查找是否存在对应玩家想加入的的game_plugin
			game_plugin_map::iterator it = this->plugins_.find(command->params(0));
			if (it != this->plugins_.end())
			{
				//没法用plugin.begin,因为回调函数回来之后，plugin可能已经被销毁了。
				//所以要统一回调到game_service的方法上，然后首先对is_running进行检测。
				client->fill(command->params(1), command->params(1), NULL);
				this->begin_auth((*it).second, client, command);
			}
			else
			{
				//printf("game not existed.\n");
				client->disconnect(service_error::GAME_NOT_EXISTED);
			}
		}


		void game_service::robot_login_handle(game_client* client, command* command)
		{
		}


		game_service::~game_service()
		{
			printf("start ~game_service\n");

			if (this->is_running_ == true)
			{
				this->stop();
			}

			//销毁所有的游戏插件
//			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
//			curr_game != this->plugins_.end(); ++curr_game)
//			{
//				delete (*curr_game).second;
//			}
			this->plugins_.clear();

			//销毁所有的游戏区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
			curr_zone != this->zones_.end(); ++curr_zone)
			{
				delete (*curr_zone).second;
			}
			this->zones_.clear();


			for (thread_status_map::iterator pos_thread_status_pair = this->threads_status_.begin();
			pos_thread_status_pair != this->threads_status_.end();
				++pos_thread_status_pair)
			{
				delete (*pos_thread_status_pair).second;
			}
			this->threads_status_.clear();

			//清理未返回的http_request对象
			__lock__(this->http_request_mutex_, "~game_servcie::http_request_mutex");
			for (std::set<http_request*>::iterator pos_http_request = this->http_request_working_.begin();
			pos_http_request != this->http_request_working_.end();)
			{
				//这里一定要注意写法，因为这里是边遍历、边删除
				//pos_http_request++返回一个临时对象， 对临时对象删除，并自动向下指向
				this->free_http_request((*pos_http_request++));
			}

			boost::singleton_pool<game_client, sizeof(game_client)>::purge_memory();
			boost::singleton_pool<http_request, sizeof(http_request)>::purge_memory();
			boost::singleton_pool<timer, sizeof(timer)>::purge_memory();

			printf("game_service destroyed.\n");
		}
	}
}
