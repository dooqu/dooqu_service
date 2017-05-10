#include "command_dispatcher.h"
#include "game_client.h"

namespace dooqu_service
{
	namespace service
	{
		command_dispatcher::~command_dispatcher()
		{
			this->unregist_all_handles();
		}


		bool command_dispatcher::regist_handle(const char* cmd_name, command_handler handler)
		{
			std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(cmd_name);

			if (handle_pair == this->handles.end())
			{
				this->handles[cmd_name] = handler;
				return true;
			}
			else
			{
				return false;
			}
		}


//		void command_dispatcher::on_client_data_received(game_client* client, size_t bytes_received)
//		{
//			client->buffer_pos += bytes_received;
//
//			if (client->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
//			{
//				thread_status::log("start->game_client::on_date_received.status_lock_");
//				boost::recursive_mutex::scoped_lock status_lock(client->status_lock_);
//				thread_status::log("end->game_client::on_date_received.");
//
//				//不建议使用client->disconnect/disconnect_when_io_end
//				//在on_error之后，会自动调用socket的close.
//				client->write("ERR %d%c", service_error::DATA_OUT_OF_BUFFER, NULL);
//
//				//先设定available 会使write代码不能执行
//				client->available_ = false;
//
//				//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
//				client->on_error(service_error::DATA_OUT_OF_BUFFER);
//				return;
//			}
//
//			for (int i = 0, cmd_start_pos = 0; i < client->buffer_pos; ++i)
//			{
//				if (client->buffer[i] == 0)
//				{
//					this->on_client_data(client, &client->buffer[cmd_start_pos]);
//
//					//如果on_data 中如需要对用户进行离线处理，那么只需调用disconnect
//					//在disconnect中，设置available = false，并关闭接收，关闭socket。
//					//那么在之后的检查中，判断需要对用户的离开进行处理，调用on_error进行清理，并离开大逻辑循环。
//                                        {
//						thread_status::log("start->game_client::on_date_received.status_lock_");
//						boost::recursive_mutex::scoped_lock status_lock(client->status_lock_);
//						thread_status::log("end->game_client::on_date_received.st");
//
//						if (client->available() == false)
//						{
//							client->on_error(NULL);
//							return;
//						}
//                                        }
//
//					if (i < (client->buffer_pos - 1))
//					{
//						cmd_start_pos = i + 1;
//					}
//				}
//
//				if (i == (client->buffer_pos - 1))
//				{
//					if (client->buffer[i] != 0)
//					{
//						if ((client->buffer_pos - cmd_start_pos) >= tcp_client::MAX_BUFFER_SIZE)
//						{
//							thread_status::log("start->game_client::on_date_received.status_lock_");
//							boost::recursive_mutex::scoped_lock status_lock(client->status_lock_);
//							thread_status::log("end->game_client::on_date_received.st");
//
//							client->write("ERR %d%c", service_error::DATA_OUT_OF_BUFFER, NULL);
//							client->available_ = false;
//
//							//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
//							client->on_error(service_error::DATA_OUT_OF_BUFFER);
//							return;
//						}
//
//						if (cmd_start_pos != 0)
//						{
//							std::copy(client->buffer + cmd_start_pos, client->buffer + client->buffer_pos, client->buffer);
//							client->buffer_pos -= cmd_start_pos;
//						}
//					}
//					else
//					{
//						//设定bufferPos = 0;表示buffer可以从头开用了。
//						client->buffer_pos = 0;
//					}
//				}
//			}
//
//			thread_status::log("start->game_client::on_date_received.status_lock_");
//			boost::recursive_mutex::scoped_lock status_lock(client->status_lock_);
//			thread_status::log("end->game_client::on_date_received.st");
//
//			if (client->available() == false)
//			{
//				client->on_error(service_error::CLIENT_NET_ERROR);
//				return;
//			}
//
//
//			//设定好缓冲区的起始，继续接收数据
//			client->p_buffer = &client->buffer[client->buffer_pos];
//			client->read_from_client();
//		}


		int command_dispatcher::on_client_data_received(game_client* client, size_t bytes_received, int* next_receive_buffer_pos)
		{
			client->buffer_pos += bytes_received;

			//防止意外，正常逻辑来说，收的字节数是可以控制的，不会产生缓冲区溢出
			if (client->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
			{
				client->write("ERR %d%c",  service_error::DATA_OUT_OF_BUFFER, NULL);
				return service_error::DATA_OUT_OF_BUFFER;
			}

			for (int i = 0, cmd_start_pos = 0; i < client->buffer_pos; ++i)
			{
				if (client->buffer[i] == 0)
				{
					this->on_client_data(client, &client->buffer[cmd_start_pos]);

					//如果on_data 中如需要对用户进行离线处理，那么只需调用disconnect
					//在disconnect中，设置available = false，并关闭接收，关闭socket。
					//那么在之后的检查中，判断需要对用户的离开进行处理，调用on_error进行清理，并离开大逻辑循环。
					if (client->available() == false)
					{
						return NULL;
					}

					//i此时是'\0'，预先将下一个cmd的开始位置进行重置;
					if (i < (client->buffer_pos - 1))
					{
						cmd_start_pos = i + 1;
					}
				}

				//如果是最后一个有效的命令字符
				if (i == (client->buffer_pos - 1))
				{
                    //如果最后一个字符，不是命令的结束符号，那么当前命令还需要继续接收才完整
                    //那么为了让接收的空间更富裕，如果当前命令是从半截开始，把这个命令向前串，拷贝到缓冲区的头部
					if (client->buffer[i] != 0)
					{
//						if ((client->buffer_pos - cmd_start_pos) >= tcp_client::MAX_BUFFER_SIZE)
//						{
//							client->write("ERR %d%c",  service_error::DATA_OUT_OF_BUFFER, NULL);
//							printf("error->>");
//							return service_error::DATA_OUT_OF_BUFFER;
//						}
                        //如果命令从0开始
                        if(cmd_start_pos == 0)
                        {
                            //如果命令从头开始，那就没有拷贝的必要了,但要检查命令是否会越界
                            //如果当前的命令从0开始，同时buffer_pos已经越界，那么关掉用的链接
                            if(client->buffer_pos == tcp_client::MAX_BUFFER_SIZE)
                            {
                                client->write("ERR %d%c",  service_error::DATA_OUT_OF_BUFFER, NULL);
                                return service_error::DATA_OUT_OF_BUFFER;
                            }
                        }
						else
						{
                            //如果命令不是从0开始， 那么为了给继续接受其余的数据疼出空间，要做一次向前串的拷贝
							std::copy(client->buffer + cmd_start_pos, client->buffer + client->buffer_pos, client->buffer);
							*next_receive_buffer_pos = client->buffer_pos -= cmd_start_pos;
						}
					}
					else
					{
						//设定bufferPos = 0;表示buffer可以从头开用了。
						*next_receive_buffer_pos = client->buffer_pos = 0;
					}
				}
			}

			return -1;
		}


		void command_dispatcher::on_client_data(game_client* client, char* data)
		{
			__lock__(client->commander_mutex_, "command_dispatcher::on_client_data");

			//printf("command_dispatcher::on_client_data:%s\n", data);

			client->commander_.reset(data);

			if (client->commander_.is_correct())
			{
				this->on_client_command(client, &client->commander_);
			}
		}


		void command_dispatcher::simulate_client_data(game_client* client, char* data)
		{
			this->on_client_data(client, data);
		}


		void command_dispatcher::on_client_command(game_client* client, command* command)
		{
			std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(command->name());

			if (handle_pair != this->handles.end())
			{
				command_handler* handle = &handle_pair->second;
				(this->**handle)(client, command);
			}
		}


		void command_dispatcher::unregist_handle(char* cmd_name)
		{
			this->handles.erase(cmd_name);
		}


		void command_dispatcher::unregist_all_handles()
		{
			this->handles.clear();
		}
	}
}
