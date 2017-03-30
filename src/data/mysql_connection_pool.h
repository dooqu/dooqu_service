#ifndef __SQL_CONNECTION_H__
#define __SQL_CONNECTION_H__

#include <string>
#include <cstring>
#include <map>
#include <deque>
#include <set>
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <statement.h>
#include <boost\thread.hpp>
#include <boost\thread\mutex.hpp>
#include <boost/noncopyable.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"


using namespace sql;
using namespace std;
using namespace boost::posix_time;

namespace dooqu_server
{
	namespace data
	{
		class char_key_op
		{
		public:
			bool operator()(const char* c1, const char* c2)
			{
				return std::strcmp(c1, c2) < 0;
			}
		};


		class tick_count : boost::noncopyable
		{
		protected:
			ptime start_time_;
		public:
			tick_count(){ this->restart(); }
			void restart(){ this->start_time_ = microsec_clock::local_time(); }
			void restart(long long millisecs){ this->start_time_ = microsec_clock::local_time(); this->start_time_ + milliseconds(millisecs); }
			long long elapsed(){ return (microsec_clock::local_time() - this->start_time_).total_milliseconds(); }
		};


		class ConnectionEx
		{
		protected:
			sql::Connection* sql_conn;
		public:
			tick_count actived_date;
			ConnectionEx(sql::Connection* conn)
			{
				this->sql_conn = conn;
				this->actived_date.restart();
			}

			sql::Connection* native()
			{
				return this->sql_conn;
			}

			virtual ~ConnectionEx()
			{
				if (this->sql_conn != NULL)
					delete this->sql_conn;
			}
		};

		struct sql_connection_pool
		{
		public:
			boost::mutex sync_object;
			set<ConnectionEx*> working;
			deque<ConnectionEx*> free;
			int waitting_count;
			int max_size;
			int min_size;

			sql_connection_pool(int min = 5, int max = 50)
			{
				max_size = max;
				min_size = min;
				waitting_count = 0;
			}

			sql_connection_pool(const sql_connection_pool& pool)
			{
				this->working = pool.working;
				this->free = pool.free;
				this->waitting_count = pool.waitting_count;
				this->max_size = pool.max_size;
				this->min_size = pool.min_size;
			}

			sql_connection_pool& operator=(const sql_connection_pool& pool)
			{
				this->working = pool.working;
				this->free = pool.free;
				this->waitting_count = pool.waitting_count;
				this->max_size = pool.max_size;
				this->min_size = pool.min_size;

				return *this;
			}
		};

		typedef map<char*, sql_connection_pool, char_key_op> sql_connection_pool_map;

		class sql_connection
		{
		public:
			static sql_connection_pool_map pool_map;
			static boost::mutex sync_object;
		protected:
			char* host_;
			char* username_;
			char* password_;
			char* database_;
			char* data_key_;
			bool pool_mode_;
			mysql::MySQL_Driver *driver;
			ConnectionEx *instance_;
			bool is_opened_;



			//+n 正确调用
			//0 服务器不可连接
			//-1 连接池已满
			int get_connection_from_pool()
			{
				sql_connection_pool* curr_pool = NULL;
				bool pool_existed = false;

				{
					//boost::mutex::scoped_lock lock(SQLConnection::sync_object);
					//先用host + username 在map中找到对应的mysql_conn_list对象
					sql_connection_pool_map::iterator iter = pool_map.find(this->data_key_);

					//如果对应的mysql_conn_list对象存在，那么在列表中找状态可用的连接对象
					if (iter != pool_map.end())
					{
						pool_existed = true;
						curr_pool = &iter->second;
					}
					else
					{
						//如果data_key没有， 说明表结构没在
						pool_map[this->data_key_] = sql_connection_pool(50, 5);
						curr_pool = &pool_map[this->data_key_];
					}

					//代码执行到这里， 对应池已经找到或者创建好， 往下看是否能在池中找到可用的连接了
				}
				//end lock all pools


				{
					boost::mutex::scoped_lock lock(curr_pool->sync_object);

					if ((curr_pool->free.size() + curr_pool->working.size()) >= curr_pool->max_size)
						throw SQLException("连接池已满，无法创建新的数据库连接。");


					if (pool_existed)
					{
						while (curr_pool->free.size() > 0)
						{
							ConnectionEx* connex = curr_pool->free.front();
							//Connection* conn = connex->native();

							curr_pool->free.pop_front();

							//如果当前的链接状态正常；
							if (connex->native()->isValid() && connex->native()->isClosed() == false)
							{
								printf("use existed.\n");
								//使用这个连接
								this->instance_ = connex;
								curr_pool->working.insert(connex);
								return (int)this->instance_;
							}
							else
							{
								//如果当前从池里拿到的链接已经被关闭， 那就销毁，从新生成；
								delete connex;
							}
						}
					}

					//缓冲池未命中， 那么创建对象

					Connection* conn = driver->connect(host_, username_, password_);

					if (conn->isValid() && conn->isClosed() == false)
					{
						this->instance_ = new ConnectionEx(conn);
						curr_pool->working.insert(this->instance_);
					}

					printf("create new connection.\n");
					return (int)this->instance_;
				}
				// end lock pool object;
			}

		public:
			sql_connection(const char* host,
				const char* username,
				const char* password,
				const char* database,
				bool pool_mode = true,
				int max_pool_size = 50,
				int min_pool_size = 5)
			{

				this->instance_ = NULL;

				host_ = new char[strlen(host) + 1];
				strcpy(host_, host);
				host_[strlen(host_)] = 0;

				printf("host=%d\n", strlen(host));

				username_ = new char[strlen(username) + 1];
				strcpy(username_, username);
				username_[strlen(username_)] = 0;

				password_ = new char[strlen(password) + 1];
				strcpy(password_, password);
				password_[strlen(password_)] = 0;

				database_ = new char[strlen(database) + 1];
				strcpy(database_, database);
				database_[strlen(database_)] = 0;

				this->data_key_ = new char[strlen(this->host_) + strlen(this->username_) + 2];
				sprintf(data_key_, "%s_%s\0", this->host_, this->username_);

				pool_mode_ = pool_mode;

				driver = sql::mysql::get_mysql_driver_instance();
			}

			int open(const char* database = NULL)
			{
				if (this->instance_ != NULL)
				{
					if (this->instance_->native()->isValid() && this->instance_->native()->isClosed() == false)
					{
						return (int)this->instance_;
					}
					else
					{
						delete this->instance_;
						this->instance_ = NULL;
					}
				}

				int ret;

				if (this->pool_mode_)
				{
					int ret = this->get_connection_from_pool();
				}
				else
				{
					this->instance_ = new ConnectionEx(this->driver->connect(this->host_, this->username_, this->password_));

					if (this->instance_->native()->isValid() == false || this->instance_->native()->isClosed())
					{
						delete this->instance_;
						this->instance_ = NULL;

						ret = -1;
					}
					else
					{
						ret = (int)this->instance_;
					}
				}


				if (ret > 0)
				{
					this->instance_->native()->setSchema((database == NULL) ? this->database_ : database);
				}

				return ret;
			}

			void close()
			{
				if (this->instance_ != NULL)
				{
					if (this->pool_mode_)
					{
						{
							boost::mutex::scoped_lock lock(sql_connection::sync_object);

							sql_connection_pool_map::iterator e = pool_map.find(this->data_key_);

							if (e != pool_map.end())
							{
								sql_connection_pool* curr_pool = &e->second;

								{
									boost::mutex::scoped_lock lock(curr_pool->sync_object);

									this->instance_->actived_date.restart();

									curr_pool->working.erase(this->instance_);
									curr_pool->free.push_front(this->instance_);


									if ((curr_pool->working.size() + curr_pool->free.size()) > curr_pool->min_size)
									{
										if (curr_pool->free.size() > 2)
										{
											ConnectionEx* conn = curr_pool->free.front();

											if (conn->actived_date.elapsed() >= 1000 * 60 * 5)
											{
												curr_pool->free.pop_front();

												if (conn->native()->isClosed() == false)
													conn->native()->close();

												delete conn;
											}
										}
									}
								}

								this->instance_ = NULL;
							}
						}
					}
					else
					{
						this->instance_->native()->close();
						delete this->instance_;
						this->instance_ = NULL;
					}
				}
			}

			Connection* native()
			{
				return this->instance_->native();
			}

			virtual ~sql_connection()
			{
				if (this->instance_ != NULL)
				{
					this->close();
				}

				if (this->host_ != NULL)
				{
					delete this->host_;
				}

				if (this->username_ != NULL)
				{
					delete this->username_;
				}

				if (this->password_ != NULL)
				{
					delete this->password_;
				}
			}
		};

		sql_connection_pool_map sql_connection::pool_map;
		boost::mutex sql_connection::sync_object;
	}
}


#endif