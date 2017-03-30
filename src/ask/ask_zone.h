#ifndef __ASK_ZONE_H__
#define __ASK_ZONE_H__

#include <vector>
#include "../service/game_zone.h"
#include "questions.h"

namespace dooqu_server
{
	namespace plugins
	{
		using namespace dooqu_server::service;

		class ask_zone : public game_zone
		{
		protected:
			question_collection questions_;

			void on_load()
			{
				questions_.fill();

				game_zone::on_load();
			}

			void on_unload()
			{
				questions_.clear();

				game_zone::on_unload();
			}

		public:
			ask_zone(game_service* service, const char* zone_id) : game_zone(service, zone_id)
			{
			}

			void fill_questions(int count, vector<question*>* question_coll_to_fill)
			{
				this->questions_.get(count, question_coll_to_fill);
			}
		};
	}
}

#endif
