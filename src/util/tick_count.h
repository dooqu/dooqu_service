#ifndef __TICK_COUNT_H__
#define __TICK_COUNT_H__

#include <chrono>
#include <boost/noncopyable.hpp>
//#include "boost/date_time/posix_time/posix_time.hpp"

namespace dooqu_service
{
	namespace util
	{
		//using namespace boost::posix_time;
		using namespace std::chrono;

		/*
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

		*/

		class tick_count : boost::noncopyable
		{

        protected:
            system_clock::time_point start_time_;
        public:
            tick_count(){this->restart();}
            void restart()
            {
                this->start_time_ = system_clock::now();
            }

            void restart(int64_t millisecs)
            {
                this->start_time_ = system_clock::now() + milliseconds(millisecs);
            }

            int64_t elapsed()
            {
                return duration_cast<milliseconds>(system_clock::now() - this->start_time_).count();
            }
		};
	}
}
#endif
