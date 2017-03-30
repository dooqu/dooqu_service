#ifndef __QUESTIONS_H__
#define __QUESTIONS_H__

//#include "../data/mysql_connection_pool.h"
#include "mysql_driver.h"
#include "mysql_connection.h"
#include "cppconn/statement.h"
#include "cppconn/exception.h"
#include <vector>
#include <boost/thread/mutex.hpp>
#include <boost/random.hpp>

//using namespace dooqu_server::data;

namespace dooqu_server
{
	namespace plugins
	{
		struct question
		{
			enum { TITLE_SIZE = 200, OPTION_SIZE = 100 };
			char title_[TITLE_SIZE];
			char options_[5][OPTION_SIZE];
			int key_index_;

			question() :key_index_(-1)
			{
				memset(title_, 0, sizeof(title_));
				memset(options_, 0, sizeof(options_));
			}

			void set_title(const char* q_title)
			{
				strncpy(this->title_, q_title, std::min((int)TITLE_SIZE - 1, (int)strlen(q_title)));
			}

			char* get_title()
			{
				return this->title_;
			}

			void set_option(int option_index, const char* option_str)
			{
				strncpy(&options_[option_index][0], option_str, std::min((int)strlen(option_str), OPTION_SIZE - 1));
			}

			char* get_option(int option_index)
			{
				return &options_[option_index][0];
			}

			int get_key()
			{
				return this->key_index_;
			}
		};

		class question_collection
		{
		protected:

			vector<question*> questions_;
			boost::mutex state_lock_;


		public:
			question_collection() : questions_(0)
			{

			}

			void get(int random_size, vector<question*>* questions_to_fill)
			{
				if (questions_to_fill == NULL || this->questions_.size() == 0)
					return;

				boost::mutex::scoped_lock lock(state_lock_);

				std::set<int> find_indexs;

				int find_count = 0;

				while (find_count < random_size)
				{
					int pos_i = rand() % this->questions_.size();

					if (find_indexs.find(pos_i) == find_indexs.end())
					{
						++find_count;
						find_indexs.insert(pos_i);

						questions_to_fill->push_back(this->questions_.at(pos_i));
					}
				}
			}

			void fill()
			{
				boost::mutex::scoped_lock lock(state_lock_);

				sql::Driver* driver = NULL;
				sql::Connection* connection = NULL;
				sql::Statement* state = NULL;
				sql::ResultSet* result = NULL;
				try
				{
					connection = sql::mysql::get_driver_instance()->connect("tcp://127.0.0.1:3306", "root", "expert79524");
					state = connection->createStatement();

					state->execute("use dooqu_game;");
					state->execute("set names 'utf8'");

					int size = 0;

					result = state->executeQuery("select count(1) from game_ask;");

					if (result->next())
					{
						size = result->getInt(1);
					}

					delete result;

					printf("this->questions.size=%d\n", this->questions_.size());

					this->questions_.resize(size);

					printf("this->questions.size=%d\n", this->questions_.size());

					result = state->executeQuery("select `title`, `option_a`, `option_b`, `option_c`, `option_d`, `option_e`, `key` from game_ask;");

					int data_size = 0;
					while (result->next())
					{
						question* q = new question();

						q->set_title(result->getString(1)->c_str());
						cout << result->getString(1)->c_str() << result->getString(1) << "--" << endl;
						q->set_option(0, result->getString(2)->c_str());
						q->set_option(1, result->getString(3)->c_str());
						q->set_option(2, result->getString(4)->c_str());
						q->set_option(3, result->getString(5)->c_str());
						q->set_option(4, result->getString(6)->c_str());

						q->key_index_ = result->getInt(7);

						if (data_size < size)
						{
							this->questions_[data_size] = q;
						}
						else
						{
							this->questions_.push_back(q);
						}

						data_size++;
					}
				}
				catch (sql::SQLException& ex)
				{
					printf("mysql error:%s\n", ex.getSQLStateCStr());
				}

				delete result;
				delete state;
				connection->close();
				delete connection;

				printf("ask_zone load date completed.\n");
			}

			void clear()
			{
				boost::mutex::scoped_lock lock(state_lock_);

				for (int i = 0; i < this->questions_.size(); i++)
				{
					delete this->questions_.at(i);
				}

				this->questions_.clear();
			}
		};
	}
}

#endif
