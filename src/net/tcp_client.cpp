#include "tcp_client.h"
#include "tcp_server.h"


namespace dooqu_service
{
	namespace net
	{
		bool tcp_client::LOG_IO_DATA = false;
		long tcp_client::CURR_RECE_TOTAL = 0;
		long tcp_client::CURR_SEND_TOTAL = 0;

		tcp_client::tcp_client(io_service& ios)
			: ios(ios),
			t_socket(ios),
//			send_buffer_sequence_(3, buffer_stream(MAX_BUFFER_SIZE)),
			read_pos_(-1), write_pos_(0),
			buffer_pos(0),
			available_(false)
		{
			this->p_buffer = &this->buffer[0];
			memset(this->buffer, 0, sizeof(MAX_BUFFER_SIZE));
		}


		void tcp_client::read_from_client()
		{
			__lock__(this->status_lock_, "tcp_client::read_from_client");

			if (this->available() == false)
				return;

			this->t_socket.async_read_some(boost::asio::buffer(this->p_buffer, tcp_client::MAX_BUFFER_SIZE - this->buffer_pos),
				std::bind(&tcp_client::on_data_received, this, std::placeholders::_1, std::placeholders::_2));
		}


        //注意，send_buffer_sequence默认构造了一定的量，正常情况下是够用的，如果不够用，最大可创建
        //MAX_BUFFER_SEQUENCE_SIZE个，但MAX_BUFFER_SEQUENCE_SIZE-8个，要引发复制构造函数，效率低下
		bool tcp_client::alloc_available_buffer(buffer_stream** buffer_alloc)
		{
			//如果当前的写入位置的指针指向存在一个可用的send_buffer，那么直接取这个集合；
			if (write_pos_ < this->send_buffer_sequence_.size())
			{
				//得到当前的send_buffer
				*buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);
				this->write_pos_++;

				return true;
			}
			else
			{
				//如果指向的位置不存在，需要新申请对象；
				//如果已经存在了超过16个对象，说明网络异常那么断开用户；不再新申请数据;
				if (this->send_buffer_sequence_.size() > MAX_BUFFER_SEQUENCE_SIZE)
				{
					*buffer_alloc = NULL;
					return false;
				}

                void* buffer_mem = boost::singleton_pool<buffer_stream, sizeof(buffer_stream)>::malloc();
                buffer_stream* curr_buffer = new(buffer_mem) buffer_stream(MAX_BUFFER_SIZE);

				//将新申请的消息承载体推入队列，注意插入的是栈类型，会引发复制构造函数,已经内建了n个
				this->send_buffer_sequence_.push_back(curr_buffer);

				//将写入指针指向下一个预置位置
				this->write_pos_ = this->send_buffer_sequence_.size() - 1;

				//得到当前的send_buffer/
				*buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);

				this->write_pos_++;

				return true;
			}
		}


		void tcp_client::write(char* data)
		{
			//默认调用的是异步的写
			this->write("%s%c", data, NULL);
		}


		void tcp_client::write(const char* format, ...)
		{
			__lock__(this->status_lock_, "tcp_client::write::status_lock");

			if (this->available() == false)
				return;

			__lock__(this->send_buffer_lock_, "tcp_client::write::send_buffer_lock_");

			buffer_stream* curr_buffer = NULL;

			//获取可用的buffer;
			bool ret = this->alloc_available_buffer(&curr_buffer);

			if (ret == false)
			{
				//如果堆积了很多未发送成功的的消息，说明客户端网络状况已经不可到达。
				this->available_ = false;
				this->on_error(service_error::CLIENT_DATA_SEND_BLOCK);
				return;
			}

			//代码到这里 send_buffer已经获取到，下面准备向内填写数据;
			int buff_size = 0;
			int try_count = MAX_BUFFER_SIZE_DOUBLE_TIMES;

			do
			{
				if (try_count < MAX_BUFFER_SIZE_DOUBLE_TIMES)
				{
					curr_buffer->double_size();
				}

				va_list arg_ptr;
				va_start(arg_ptr, format);

				buff_size = curr_buffer->write(format, arg_ptr);

				va_end(arg_ptr);

			}
			while ((buff_size == -1 ||
				(curr_buffer->size() == curr_buffer->capacity() && *curr_buffer->at(curr_buffer->size() - 1) != 0)) && try_count-- > 0);


			if (buff_size == -1)
			{
				printf("ERROR: server message queue limited,the message can not be send.\n");
				this->write_pos_--;
				return;
			}


			//如果正在发送的索引为-1，说明空闲
			if (read_pos_ == -1)
			{
				read_pos_++;

				//std::cout << curr_buffer->read() << std::endl;
				//只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
				boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
					std::bind(&tcp_client::send_handle, this, std::placeholders::_1));

				this->is_data_sending_ = true;
			}
			//printf("END send\n");
		}

		///在发送完毕后，对发送队列中的数据进行处理；
		///发送数据是靠async_send 来实现的， 它的内部是使用多次async_send_same来实现数据全部发送；
		///所以，如果想保障数据的有序、正确到达，一定不要在第一次async_send结束之前发起第二次async_send；
		///实现采用一个发送的数据报队列，按数据报进行依次的发送,靠回调来驱动循环发送，直到当次队列中的数据全部发送完毕；
		void tcp_client::send_handle(const boost::system::error_code& error)
		{
			//std::cout << "START:send_handle" <<  "{"  << std::endl;
			__lock__(this->status_lock_, "tcp_client::send_handle::status_lock_");

			if (error)
			{
				this->is_data_sending_ = false;
				return;
			}


			//std::cout << "START:send_handle_inlock" << std::endl;
			//这里考虑下极限情况，如果this已经被销毁？
			if (this->available() == false)
			{
				this->is_data_sending_ = false;
				return;
			}

			__lock__(this->send_buffer_lock_, "tcp_client::send_handle::send_buffer_lock_");

			buffer_stream* curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);

			if (read_pos_ >= (write_pos_ - 1))
			{
				//全部处理完
				read_pos_ = -1;
				write_pos_ = 0;
				this->is_data_sending_ = false;
			}
			else
			{
				read_pos_++;

				buffer_stream* curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);

				if (curr_buffer->is_bye_signal())
				{
					this->is_data_sending_ = false;
					this->disconnect();
					return;
				}

				boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
					std::bind(&tcp_client::send_handle, this, std::placeholders::_1));
			}
		}


		/*
		disconnect_when_io_end不直接对外提供调用，在状态锁放买你，外部调用一定要保障已经
		调用了status_lock锁，防止死锁,因为在disconnect中对status_lock 进行了锁定
		*/
		void tcp_client::disconnect_when_io_end()
		{
			if (this->available() == false)
				return;

			__lock__(this->send_buffer_lock_, "tcp_client::disconnection_when_io_end");

			//如果有要发送的数据，那么新申请一个离开的信号数据包，并加到数据发送队列中
			if (this->read_pos_ != -1)
			{
//				buffer_stream* curr_buffer = NULL;
//				bool alloc_ret = this->alloc_available_buffer(&curr_buffer);
//
//				if (alloc_ret)
//				{
//					curr_buffer->set_bye_signal();
//					return;
//				}
			}

			//如果没有要发送的数据，那么立即断开
			this->disconnect();
		}


		void tcp_client::disconnect()
		{
			__lock__(this->status_lock_, "tcp_client::disconnect");

			if (this->available_)
			{
				this->available_ = false;

				boost::system::error_code err_code;

				this->t_socket.shutdown(boost::asio::socket_base::shutdown_both, err_code);
				this->t_socket.close(err_code);
			}
		}


		void tcp_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
			if (!error && tcp_client::LOG_IO_DATA && bytes_received)
			{
				tcp_client::CURR_RECE_TOTAL += (long)bytes_received;
			}
		}


		tcp_client::~tcp_client()
		{
			//printf("~game_client.\n");

			__lock__(this->send_buffer_lock_, "tcp_client::~tcp_client::lock");

			for(std::vector<buffer_stream*>::iterator e = this->send_buffer_sequence_.begin(); e != this->send_buffer_sequence_.end(); ++e)
            {
                (*e)->~buffer_stream();
                boost::singleton_pool<buffer_stream, sizeof(buffer_stream)>::free(*e);
			}

			this->send_buffer_sequence_.clear();
		}
	}
}


