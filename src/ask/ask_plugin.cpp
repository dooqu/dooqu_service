#include "ask_plugin.h"

namespace dooqu_server
{
	namespace plugins
	{
		void ask_plugin::on_load()
		{
			for (int i = 0; i < 100; i++)
			{
				this->rooms_[i] = new ask_room(i);
			}

			this->regist_handle("IDK", make_handler(ask_plugin::client_join_room_handle));
			this->regist_handle("RDY", make_handler(ask_plugin::client_ready_handle));
			this->regist_handle("ASR", make_handler(ask_plugin::client_answer_handler));
		}

		void ask_plugin::on_unload()
		{
			this->unregist_all_handles();
		}

		void ask_plugin::on_update()
		{
			for (int i = 0; i < ROOM_COUNT; ++i)
			{
				ask_room* curr_room = this->rooms_[i];

				boost::recursive_mutex::scoped_lock lock(curr_room->get_lock());

				if (curr_room->is_running() && curr_room->get_pos_index() >= 0)
				{
					if (curr_room->elapsed_active_time() > QUESTION_RESP_MILLITIMES)
					{

					}
				}
			}
		}

		int ask_plugin::on_auth_client(game_client* client, const std::string&)
		{
			return service_error::OK;
		}

		int ask_plugin::on_befor_client_join(game_client* client)
		{
			//检查房间是否已满
			if (this->clients()->size() >= 8 * 100)
				return service_error::GAME_IS_FULL;

			//防止用户重复登录
			return game_plugin::on_befor_client_join(client);
		}


		void ask_plugin::on_client_join(game_client* client)
		{
			ask_game_info* gameinfo = new ask_game_info();

			client->set_game_info(gameinfo);
			game_plugin::on_client_join(client);

			client->write("LOG %s %s %c", this->game_id(), client->id(), NULL);
		}


		void ask_plugin::on_client_leave(game_client* client, int reason_code)
		{
			ask_game_info* game_info = client->get_game_info<ask_game_info>();
			ask_room* room = game_info->get_room();

			if (room != NULL)
			{
				__lock_room_status__(room);

				int pos_index = room->index_of_client(client);
				if (pos_index == -1)
					return;

				this->on_client_leave_room(room, client, pos_index);
			}

			client->set_game_info(NULL);
			delete game_info;

			ask_plugin::on_client_leave(client, reason_code);
		}


		game_zone* ask_plugin::on_create_zone(char* zone_id)
		{
			return new ask_zone(this->game_service_, zone_id);
		}


		void ask_plugin::client_join_room_handle(game_client* client, command* command)
		{
			ask_game_info* game_info = client->get_game_info<ask_game_info>();
			ask_room* room = game_info->get_room();

			if (room != NULL)
			{
				__lock_room_status__(room);
				int client_pos = room->index_of_client(client);

				if (client_pos == -1)
				{
					return;
				}

				this->on_client_leave_room(room, client, client_pos);
			}

			if (command->param_size() == 0)
			{
				/*查找算法要考虑到极限情况
				 1、房间全空、2房间全满 */

				int room_count = 100;
				int find_null_count = 1;

				int start_index = game_info->room_finder_index();

				//如果玩家之前已经进入过一个房间，那么从下一个房间开始查找合适的；
				if (start_index > 0) start_index = (start_index + 1) % room_count;

				for (int curr_pos = start_index, i = 0; i < room_count; )
				{
					ask_room* curr_room = this->rooms_[curr_pos];

					__lock_room_status__(curr_room);

					if (curr_room->is_running())
					{
						++i;
						curr_pos = (curr_pos + 1) % room_count;
						continue;
					}

					if (curr_room->is_null())
					{
						//如果第二次碰到空房间，或者第一个就是空房间；或者本身就是从头查的，那么就地给玩家安置到空房间上；
						if (find_null_count-- <= 0 || start_index == 0 || game_info->room_finder_index() == 0)
						{
							game_info->set_room_finder_index(i);
							this->on_client_join_room(curr_room, client, 0);
							return;
						}
						else
						{
							//如果第一次碰到空房间，那么折返，重头开查
							i = 0;
							curr_pos = 0;
							continue;
						}
					}

					int free_pos = curr_room->get_free_pos();

					if (free_pos != -1)
					{
						game_info->set_room_finder_index(i);
						this->on_client_join_room(curr_room, client, free_pos);
						return;
					}
					++i;
					curr_pos = (curr_pos + 1) % room_count;
				}
			}
			else if (command->param_size() == 1)
			{
				int room_index = atoi(command->params(0));

				if (room_index < 0 || room_index >= ask_room::ROOM_CAPACITY)
				{
					client->write("IDK %d %c", -1, NULL);
					return;
				}

				ask_room* room = this->rooms_[room_index];

				__lock_room_status__(room);

				if (room->is_running())
				{
					client->write("IDK %d %c", -2, NULL);
					return;
				}

				int pos_index = room->get_free_pos();

				if (pos_index == -1)
				{
					client->write("IDK %d %c", -3, NULL);
					return;
				}

				this->on_client_join_room(room, client, pos_index);
			}
		}



		//当某个玩家进入游戏房间后，调用的事件;
		void ask_plugin::on_client_join_room(ask_room* room, game_client* client, int pos_index)
		{
			ask_game_info* game_info = client->get_game_info<ask_game_info>();

			game_info->set_room(room, pos_index);
			room->set_pos_client(pos_index, client);

			if (room->get_room_leader_index() == -1)
			{
				room->set_room_leader_index(pos_index);
			}

			for (int i = 0; i < ask_room::ROOM_CAPACITY; i++)
			{
				game_client* room_client = room->get_pos_client(i);

				if (room_client == NULL)
					continue;

				client->write("LSD %d %s %c", i, room_client->id(), NULL);

				if (room_client != client)
				{
					room_client->write("JDK %d %s %c", pos_index, client->id(), NULL);
				}
			}

			client->write("LED %d %c", room->get_room_leader_index(), NULL);
		}


		void ask_plugin::client_leave_room_handle(game_client* client, command* command)
		{

		}


		//当某个玩家离开游戏房间后，事件调用；
		void ask_plugin::on_client_leave_room(ask_room* room, game_client* client, int pos_index)
		{
			room->set_pos_client(pos_index, NULL);
			ask_game_info* game_info = client->get_game_info<ask_game_info>();
			game_info->set_room(NULL);

			send_message_to_clients(room, "LDK %d %s%c", pos_index, client->id(), NULL);

			if (room->is_running())
			{
				//如果一个正在运行组队模式的房间
				if (room->is_team_mode())
				{
					if (pos_index % 2 == 0)
					{
						//如果是偶数位置，蓝队
						if (room->get_pos_client(0) == NULL && room->get_pos_client(2) == NULL && room->get_pos_client(4) == NULL && room->get_pos_client(6) == NULL)
						{
							this->on_room_game_stop(room);
							return;
						}
					}
					else
					{
						if (room->get_pos_client(1) == NULL && room->get_pos_client(3) == NULL && room->get_pos_client(5) == NULL && room->get_pos_client(7) == NULL)
						{
							this->on_room_game_stop(room);
							return;
						}
					}
				}
				else
				{
					//如果是个人赛
					int left_count = 0;
					for (int i = 0; i < ask_room::ROOM_CAPACITY; ++i)
					{
						if (room->get_pos_client(i) != NULL)
							++left_count;
					}

					if (left_count < 2)
					{
						this->on_room_game_stop(room);
						return;
					}
					//如果一个正在运行个人赛模式的房间
				}
			}
			//如果离开的玩家是房主;
			else if (pos_index == room->get_room_leader_index())
			{
				//先置为-1;
				room->set_room_leader_index(-1);
				//从离开的玩家的位置下一个开始查找还留下的玩家，把他作为新的房主;
				for (int curr_pos = pos_index + 1, n = 0; n < ask_room::ROOM_CAPACITY - 1; ++n, ++curr_pos)
				{
					curr_pos = curr_pos % ask_room::ROOM_CAPACITY;

					if (room->get_pos_client(curr_pos) != NULL)
					{
						room->set_room_leader_index(curr_pos);
						send_message_to_clients(room, "LED %d %c", curr_pos, NULL);
						break;
					}
				}
			}
		}


		//处理玩家发送“准备”命令的处理机;
		void ask_plugin::client_ready_handle(game_client* client, command* command)
		{
			ask_game_info* game_info = client->get_game_info<ask_game_info>();

			if (game_info == NULL || game_info->is_ready())
				return;

			ask_room* curr_room = game_info->get_room();

			if (curr_room == NULL)
				return;

			//基本校验完毕，开始上锁检查;
			__lock_room_status__(curr_room);

			//如果当前房间是游戏进行中，那么返回;
			if (curr_room->is_running())
				return;

			int client_pos = -1;

			//如果当前游戏房间中没有找到该玩家，那么返回;
			if ((client_pos = curr_room->index_of_client(client)) == -1)
				return;

			//如果当前玩家不是房主
			if (client_pos != curr_room->get_room_leader_index())
			{
				game_info->set_ready(true);
				send_message_to_clients(curr_room, "RDY %d %s%c", client_pos, client->id(), NULL);
			}
			else
			{
				//如果当前发出准备动作的玩家是房主；
				int blue_c = 0, red_c = 0;

				for (int i = 0; i < ask_room::ROOM_CAPACITY; ++i)
				{
					game_client* curr_client = curr_room->get_pos_client(i);

					if (curr_client == NULL)
						continue;

					ask_game_info* curr_game_info = curr_client->get_game_info<ask_game_info>();

					if (curr_game_info->is_ready() == false && i != curr_room->get_room_leader_index())
					{
						//如果有玩家还没准备，同时这个玩家还不是房主
						client->write("RDY -1");
						return;
					}

					((i % 2) == 0) ? ++red_c : ++blue_c;
				}

				//如果两队的人数不等，不能开始
				if (red_c != blue_c)
				{
					client->write("RDY -2");
					return;
				}
				else
				{
					if (this->zone_ == NULL)
					{
						client->write("RDY -3");
						return;
					}

					((ask_zone*)this->zone_)->fill_questions(20, curr_room->get_questions());
					if (curr_room->get_questions()->size() < 20)
					{
						client->write("RDY -4");
						return;
					}
					game_info->set_ready(true);
					curr_room->reset();
					curr_room->set_running(true);

					++curr_room->verify_code;
					this->zone_->queue_task(boost::bind(&ask_plugin::on_room_game_start, this, curr_room, curr_room->verify_code), 500);

					send_message_to_clients(curr_room, "RDY %d %s%c", client_pos, client->id(), NULL);
				}
			}
		}


		void ask_plugin::on_room_game_start(ask_room* room, int verify_code)
		{
			if (room == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(room->get_lock());

			if (room->verify_code != verify_code )
			{
				printf("verify\n");
				return;
			}

			if ( room->is_running() == false)
			{
				printf("not running\n");
				return;
			}

			if (room->get_questions()->size() <= 0)
			{
				printf("questions\n");
				return;
			}


			room->set_pos_index(-1);

			room->active();

			send_message_to_clients(room, "STT 0%c", NULL);

			++room->verify_code;
			this->zone_->queue_task(boost::bind(&ask_plugin::on_room_game_question, this, room, room->verify_code), 2000);
		}


		void ask_plugin::on_room_game_question(ask_room* room, int verify_code)
		{
			if (room == NULL)
				return;

			__lock_room_status__(room);

			if ((room->verify_code != verify_code) || room->is_running() == false)
			{
				return;
			}

			room->active();

			room->set_pos_index(0);

			vector<question*>* qs = room->get_questions();

			if (room->move_question_index() < qs->size())
			{
				question* q = qs->at(room->question_index());
				send_message_to_clients(room, "QUS %d %d %s %s %s %s %s %s%c", room->question_index(), room->get_pos_index(),
					q->get_title(),
					(q->get_option(0) == NULL) ? "" : q->get_option(0),
					(q->get_option(1) == NULL) ? "" : q->get_option(1),
					(q->get_option(2) == NULL) ? "" : q->get_option(2),
					(q->get_option(3) == NULL) ? "" : q->get_option(3),
					(q->get_option(4) == NULL) ? "" : q->get_option(4), NULL);
			}
		}

		void ask_plugin::client_answer_handler(game_client* client, command* command)
		{
			if (command->param_size() != 2)
				return;

			int question_index = atoi(command->params(0));
			int key = atoi(command->params(1));

			if (question_index < 0 || key < 0)
				return;

			ask_game_info* game_info = client->get_game_info<ask_game_info>();

			if (game_info == NULL)
				return;

			ask_room* room = game_info->get_room();

			if (room == NULL)
				return;

			__lock_room_status__(room);

			if (room->is_running() == false)
				return;

			int client_pos = room->index_of_client(client);
			if (client_pos == -1)
				return;

			if ((client_pos % 2) != room->get_pos_index())
				return;

			if (question_index != room->question_index())
				return;

			vector<question*>* qs = room->get_questions();

			if (question_index >= qs->size())
				return;

			question* q = qs->at(question_index);

			if (q->get_key() == key)
			{

			}
			else
			{
			}

			send_message_to_clients(room, "ASR %d %s %d %d%c",
				client_pos,
				client->id(),
				question_index,
				(q->get_key() == key) ? 1 : 0, NULL);

			if (question_index < (qs->size() - 1))
			{
				int next_question_index = room->move_question_index();
				int next_client_index = room->move_next();

				question* q = qs->at(next_question_index);

				std::cout << q->get_title() << endl;

				send_message_to_clients(room, "QUS %d %d %s %s %s %s %s %s%c", room->question_index(), room->get_pos_index(),
					q->get_title(),
					(q->get_option(0) == NULL) ? "" : q->get_option(0),
					(q->get_option(1) == NULL) ? "" : q->get_option(1),
					(q->get_option(2) == NULL) ? "" : q->get_option(2),
					(q->get_option(3) == NULL) ? "" : q->get_option(3),
					(q->get_option(4) == NULL) ? "" : q->get_option(4), NULL);

				room->active();
			}
			else
			{
				this->on_room_game_stop(room);
			}
		}


		void ask_plugin::on_room_game_stop(ask_room* room)
		{
			room->set_running(false);

			//如果游戏结束后，发现房主不见了，说明他在游戏中途离开了
			if (room->get_room_leader_index() == -1 || room->get_pos_client(room->get_room_leader_index()) == NULL)
			{
				int pos_index = room->get_room_leader_index();
				for (int curr_pos = pos_index + 1, n = 0; n < ask_room::ROOM_CAPACITY - 1; ++n, ++curr_pos)
				{
					curr_pos = curr_pos % ask_room::ROOM_CAPACITY;

					if (room->get_pos_client(curr_pos) != NULL)
					{
						room->set_room_leader_index(curr_pos);
						send_message_to_clients(room, "LED %d %c", curr_pos, NULL);
						break;
					}
				}
			}
		}
	}
}
