#ifndef TASK_TIMER_H
#define TASK_TIMER_H

#include <set>
#include <deque>
#include <mutex>
#include "boost/asio.hpp"
#include "boost/pool/singleton_pool.hpp"
#include "../util/tick_count.h"
#include "../util/utility.h"

using namespace dooqu_service::util;
using namespace boost::asio;

namespace dooqu_service
{
    namespace service
    {
		//继承deadline_timer，并按照需求，增加一个标示最后激活的字段。
		class deadline_timer_ex : public boost::asio::deadline_timer
		{
		public:
			tick_count last_actived_time;
			deadline_timer_ex(io_service& ios) : deadline_timer(ios){}
			virtual ~deadline_timer_ex(){ printf("~timer\n"); };
		};


        class task_timer
        {

        public:
            task_timer(io_service& ios);
            virtual ~task_timer();
            void queue_task(std::function<void(void)> callback_handle, int sleep_duration);

        protected:
            io_service& io_service_;
            //timer池中最小保有的数量，timer_.size > 此数量后，会启动空闲timer检查
            const static int MIN_ACTIVED_TIMER = 50;
            //如timer池中的数量超过{MIN_ACTIVED_TIMER}定义的数量， 并且队列后部的timer空闲时间超过
            //MAX_TIMER_FREE_TICK的值，会被强制回收
            const static int MAX_TIMER_FREE_TICK = 1 * 60 * 1000;

            //存放定时器的双向队列
            //越是靠近队列前部的timer越活跃，越是靠近尾部的timer越空闲
            std::deque<deadline_timer_ex*> free_timers_;

            std::set<deadline_timer_ex*> working_timers_;

            //timer队列池的状态锁
            std::recursive_mutex free_timers_mutex_;

            std::recursive_mutex working_timers_mutex_;

            void task_handle(const boost::system::error_code& error, deadline_timer_ex* timer_, std::function<void(void)> callback_handle);

        };
    }
}


#endif // TASK_TIMER_H
