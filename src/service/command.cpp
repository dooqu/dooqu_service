#include "command.h"
#include <cstring>

namespace dooqu_service
{
	namespace service
	{
		command::command()
		{
			this->name_ = NULL;
			//this->command_data_ = NULL;
			this->is_correct_ = false;

			this->null();

			this->command_data_ = NULL;
		}

		void command::null()
		{
			this->is_correct_ = false;
			this->name_ = NULL;
			this->params_.clear();

			//if (this->command_data_ != NULL)
			//{
			//	delete[] this->command_data_;
			//	this->command_data_ = NULL;
			//}
		}

		char* command::name()
		{
			return this->name_;
		}

		char* command::params(int pos)
		{
			return this->params_.at(pos);
		}

		int command::param_size()
		{
			return this->params_.size();
		}

		bool command::is_correct()
		{
			return this->is_correct_;
		}

		void command::reset(char* command_data)
		{
			this->null();
			this->is_correct_ = this->init(command_data);
		}

		bool command::init(char* command_data)
		{
			int str_length = std::strlen(command_data);

			if (str_length <= 0)
			{
				return false;
			}


			int curr_pos = 0;

			//curr_index 游标索引 curr_pos命令片段开始
			for (int curr_index = 0, cmd_segment_pos = 0; curr_index < str_length; ++curr_index)
			{
				char curr_c = command_data[curr_index];

				if (curr_c == ' ')
				{
					command_data[curr_index] = 0;

					if (curr_index == cmd_segment_pos)
					{
						++cmd_segment_pos;
						continue;
					}

					if (this->name_ == NULL)
					{
						this->name_ = &command_data[cmd_segment_pos];
					}
					else
					{
						this->params_.push_back(&command_data[cmd_segment_pos]);
					}

					cmd_segment_pos = curr_index + 1;
				}
				else if (curr_index == (str_length - 1))
				{
					if (this->name_ == NULL)
					{
						this->name_ = &command_data[cmd_segment_pos];
					}
					else
					{
						this->params_.push_back(&command_data[cmd_segment_pos]);
					}
				}
			}

			return (this->name_ != NULL);
		}

		command::~command()
		{
			this->null();
		}
	}
}
