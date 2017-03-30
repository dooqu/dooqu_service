#ifndef __GAME_COMMAND_H__
#define __GAME_COMMAND_H__

#include <vector>
#include <iostream>
#include <boost/noncopyable.hpp>
using namespace std;

namespace dooqu_service
{
	namespace service
	{
		class command : boost::noncopyable
		{
		private:
			char* command_data_;
			char* name_;
			vector<char*> params_;
			bool is_correct_;
			bool init(char* command_data);
			void null();

		public:
			command();
			command(char* command_data);
			char* name();
			int param_size() ;
			char* params(int pos);
			bool is_correct();
			void reset(char* command_data);
			virtual ~command();
		};
	}
}
#endif
