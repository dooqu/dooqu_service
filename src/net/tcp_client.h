#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <cstdarg>
#include <string>
#include <iostream>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <boost/noncopyable.hpp>
#include "service_error.h"
#include "../util/utility.h"

namespace dooqu_service
{
	namespace net
	{

		using namespace boost::asio;
		using namespace boost::asio::ip;
//		template<typename T> class RAIILock
//		{
//		protected:
//			T& mutex_;
//			bool is_locked_;
//		public:
//			RAIILock(T & mutex) : mutex_(mutex)
//			{
//				this->lock();
//			}
//
//			void lock()
//			{
//				this->mutex_.lock();
//				this->is_locked_ = true;
//			}
//
//			void unlock()
//			{
//				this->mutex_.unlock();
//				this->is_locked_ = false;
//			}
//
//			operator bool()
//			{
//				return this->is_locked_;
//			}
//
//			virtual ~RAIILock()
//			{
//				this->unlock();
//			}
//		};

		class buffer_stream : boost::noncopyable
		{
		protected:
			std::vector<char> buffer_;
			int size_;

		public:			//从buffer_stream中向外读取
			char* read()
			{
				return &*this->buffer_.begin();
			}

			//设定buffer的默认size 和默认值
			buffer_stream(size_t size) : buffer_(size, 0)
			{
				this->size_ = 0;
			}

			virtual ~buffer_stream()
			{
                this->buffer_.clear();
			}

			int size()
			{
				return this->size_;
			}
			//向buffer_stream中写入数据
			int write(const char* format, va_list arg_ptr)
			{
				int bytes_readed = vsnprintf(read(), this->buffer_.size(), format, arg_ptr);

				if (bytes_readed >= this->buffer_.size())
				{
					return -1;
				}

				this->size_ = bytes_readed;
				return bytes_readed;
			}

			int capacity()
			{
				return this->buffer_.capacity();
			}

			void double_size()
			{
				this->buffer_.resize(this->buffer_.size() * 2, 0);
			}

			char* at(int pos)
			{
				return &this->buffer_.at(pos);
			}

			void set_bye_signal()
			{
				this->buffer_.clear();
				this->buffer_.resize(0);
				this->size_ = -1;
			}

			bool is_bye_signal()
			{
				return this->size_ == -1;
			}
		};


		class tcp_server;

		class tcp_client : boost::noncopyable
		{
		public:
			enum
			{
				MAX_BUFFER_SIZE = 128,
				MAX_BUFFER_SEQUENCE_SIZE = 32,
				MAX_BUFFER_SIZE_DOUBLE_TIMES = 4
			};

		protected:
			bool is_data_sending_;

			//发送数据缓冲区
			std::vector<buffer_stream*> send_buffer_sequence_;

			//数据锁
			std::recursive_mutex send_buffer_lock_;

			//正在处理的位置，这个位置是发送函数正在读取的位置
			int read_pos_;

			//write_pos_永远指向该写入的位置
			int write_pos_;


			//接收数据缓冲区
			char buffer[MAX_BUFFER_SIZE];


			//指向缓冲区的指针，使用asio::buffer会丢失位置
			char* p_buffer;


			//当前缓冲区的位置
			unsigned int buffer_pos;


			//io_service对象
			io_service& ios;


			//包含的socket对象
			tcp::socket t_socket;

			//用来标识socket对象是否可用
			bool available_;


			//开始从客户端读取数据
			void read_from_client();

			//用来接收数据的回调方法
			virtual void on_data_received(const boost::system::error_code& error, size_t bytes_received);


			//发送数据的回调方法
			void send_handle(const boost::system::error_code& error);
			//当有数据到达时会被调用，该方法为虚方法，由子类进行具体逻辑的处理和运用
			//inline virtual void on_data(char* client_data) = 0;

			//当客户端出现规则错误时进行调用， 该方法为虚方法，由子类进行具体的逻辑的实现。
			virtual void on_error(const int error) = 0;


			inline bool alloc_available_buffer(buffer_stream** buffer_alloc);

		public:
			tcp_client(io_service& ios);

			virtual ~tcp_client();

			tcp::socket& socket() { return this->t_socket; }


			inline bool available() { return this->available_ ; }


			void write(char* data);


			void write(const char* format, ...);


			static long CURR_RECE_TOTAL;


			static long CURR_SEND_TOTAL;


			static bool LOG_IO_DATA;

		};
	}
}


#endif
