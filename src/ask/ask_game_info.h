#ifndef __ASK_GAME_INFO_H__
#define __ASK_GAME_INFO_H__

#include "../service/game_info.h"
#include "ask_room.h"

using namespace dooqu_server::service;

namespace dooqu_server
{
	namespace plugins
	{
		class ask_game_info : public game_info
		{
		protected:
			ask_room* room_;
			int pos_index_;
			bool is_ready_;
			int room_finder_index_;
		public:
			ask_game_info()
			{
				this->room_ = NULL;
				this->pos_index_ = -1;
				this->is_ready_ = false;
				this->room_finder_index_ = 0;
			}

			void reset()
			{

			}

			ask_room* get_room()
			{
				return this->room_;
			}

			void set_room(ask_room* room, int pos_index = -1)
			{
				this->room_ = room;
				this->pos_index_ = pos_index;
			}

			int get_room_pos()
			{
				return this->pos_index_;
			}

			bool is_ready()
			{
				return this->is_ready_;
			}

			void set_ready(bool is_ready)
			{
				this->is_ready_ = is_ready;
			}

			void set_room_finder_index(int finder_pos)
			{
				this->room_finder_index_ = finder_pos;
			}

			int room_finder_index()
			{
				return this->room_finder_index_;
			}
		};
	}
}

#endif
