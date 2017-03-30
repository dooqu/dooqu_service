#ifndef __POST_MONITOR_H__
#define __POST_MONITOR_H__

#include "../util/tick_count.h"

namespace dooqu_service
{
	namespace service
	{
		class post_monitor
		{
		protected:
			tick_count last_actived_;
			unsigned int max_actived_count_;
			unsigned int curr_actived_count_;
			unsigned int duration_;
			unsigned int forbid_duration_;
		public:
			post_monitor()
			{
				this->max_actived_count_ = 65535;
				this->curr_actived_count_ = 0;
				this->duration_ = 65535;
				this->forbid_duration_ = 0;
			}

			virtual ~post_monitor()
			{
			}

			void init(unsigned int max_actived_count, unsigned int check_duration)
			{
				this->max_actived_count_ = max_actived_count;
				this->curr_actived_count_ = 0;
				this->duration_ = check_duration;
				this->forbid_duration_ = 5000;
			}

			bool can_active()
			{
				long long duration_millis = last_actived_.elapsed();
				if (duration_millis > (long long)duration_)
				{
					last_actived_.restart();
					this->curr_actived_count_ = 1;
					return true;
				}
				else
				{
					if (++this->curr_actived_count_ > this->max_actived_count_)
					{
						this->last_actived_.restart(this->forbid_duration_);
						return false;
					}
					return true;
				}
			}
		};
	}
}

#endif
