#ifndef __ASK_ROOM_H__
#define __ASK_ROOM_H__

#include <vector>
#include <boost/thread/recursive_mutex.hpp>
#include "../service/game_client.h"
#include "questions.h"
#include "../service/tick_count.h"

using namespace dooqu_server::service;

namespace dooqu_server
{
	namespace plugins
	{
		#define __lock_room_status__(room) boost::recursive_mutex::scoped_lock _lock_(room->get_lock());
		#define send_message_to_clients(room, message_format, ...)\
				{\
					for (int i = 0; i < ask_room::ROOM_CAPACITY; ++i)\
					{\
						game_client* curr_client= room->get_pos_client(i);\
						if (curr_client == NULL || (curr_client)->available() == false)\
							continue;\
						(curr_client)->write(message_format, ##__VA_ARGS__);\
					}\
				};

		class ask_room
		{
		public:
			enum { ROOM_CAPACITY = 8 };
			int verify_code;

		protected:
			game_client* clients_[ROOM_CAPACITY];
			bool is_running_;
			bool is_team_mode_;

			int curr_pos_index_;
			int room_leader_index_;
			int room_id_;

			//激活时间戳
			tick_count last_actived_time_;
			boost::recursive_mutex status_lock_;
			vector<question*> questions_;
			int question_index_;
		public:
			ask_room(int room_id) :
				is_running_(false),
				is_team_mode_(true),
				curr_pos_index_(-1),
				room_id_(room_id)
			{
				std::memset(clients_, 0, sizeof(clients_));
			}

			void set_pos_client(unsigned int index, game_client* client)
			{
				this->clients_[index] = client;
			}

			game_client* get_pos_client(unsigned int index)
			{
				return this->clients_[index];
			}

			game_client* clients()
			{
				return this->clients_[0];
			}

			void set_pos_index(int index)
			{
				this->curr_pos_index_ = index;
			}

			int get_pos_index()
			{
				return this->curr_pos_index_;
			}

			int move_next()
			{
				if(this->is_team_mode_)
				{
					this->curr_pos_index_ = (this->curr_pos_index_ + 1) % 2;
					return this->curr_pos_index_;
				}
				else
				{
					for(int i = 0, curr_pos = (this->curr_pos_index_ + 1) % ROOM_CAPACITY; i < ROOM_CAPACITY; ++i)
					{
						if(this->get_pos_client(curr_pos) != NULL)
						{
							this->set_pos_index(curr_pos);
							return curr_pos;
						}

						curr_pos = (this->curr_pos_index_ + 1) % ROOM_CAPACITY;
					}
					return 0;
				}
			}

			boost::recursive_mutex& get_lock()
			{
				return this->status_lock_;
			}

			bool is_running()
			{
				return this->is_running_;
			}

			void active()
			{
				this->last_actived_time_.restart();
			}

			long long elapsed_active_time()
			{
				return this->last_actived_time_.elapsed();
			}


			void reset()
			{
				this->is_running_ = false;
				this->curr_pos_index_ = -1;
				this->question_index_ = -1;
				last_actived_time_.restart();
			}

			void set_running(bool is_running)
			{
				this->is_running_ = is_running;
			}

			void set_team_mode(bool is_team_mode)
			{
				this->is_team_mode_ = is_team_mode;
			}

			bool is_team_mode()
			{
				return this->is_team_mode_;
			}

			int get_free_pos()
			{
				for (int i = 0; i < ask_room::ROOM_CAPACITY; i++)
				{
					if (this->clients_[i] == NULL)
						return i;
				}

				return -1;
			}

			bool is_null()
			{
				return (this->clients_[0] == NULL
					&& this->clients_[1] == NULL
					&& this->clients_[2] == NULL
					&& this->clients_[3] == NULL
					&& this->clients_[4] == NULL
					&& this->clients_[5] == NULL
					&& this->clients_[6] == NULL
					&& this->clients_[7] == NULL );
			}


			bool is_full()
			{
				return (this->clients_[0] != NULL
					&& this->clients_[1] != NULL
					&& this->clients_[2] != NULL
					&& this->clients_[3] != NULL
					&& this->clients_[4] != NULL
					&& this->clients_[5] != NULL
					&& this->clients_[6] != NULL
					&& this->clients_[7] != NULL );
			}

			int index_of_client(game_client* client)
			{
				for (int i = 0; i < ask_room::ROOM_CAPACITY; i++)
				{
					if (this->clients_[i] == client)
					{
						return i;
					}
				}

				return -1;
			}

			void send_all(char* message)
			{
				for (int i = 0; i < ask_room::ROOM_CAPACITY; ++i)
				{
					game_client* curr_client = this->clients_[i];

					if (curr_client == NULL || curr_client->available() == false)
						continue;

					curr_client->write(message);
				}
			}


			vector<question*>* get_questions()
			{
				return &questions_;
			}

			int get_room_leader_index()
			{
				return this->room_leader_index_;
			}

			void set_room_leader_index(int index)
			{
				this->room_leader_index_ = index;
			}

			int question_index()
			{
				return this->question_index_;
			}

			int move_question_index()
			{
				return ++this->question_index_;
			}

			game_client** get_first_client_addr()
			{
				return &this->clients_[0];
			}
		};
	}
}
#endif
