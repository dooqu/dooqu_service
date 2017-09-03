#include "game_zone.h"
#include <functional>

namespace dooqu_service
{
	namespace service
	{
		bool game_zone::LOG_TIMERS_INFO = false;

		game_zone::game_zone(game_service* service, const char* id)
		{
			int n = std::strlen(id);
			this->id_ = new char[n + 1];
			std::strcpy(this->id_, id);
			this->id_[n] = '\0';

			this->game_service_ = service;
			this->is_onlined_ = false;
		}


		void game_zone::load()
		{
			//状态锁
			__lock__(this->status_mutex_, "game_zone::load");

			if (this->is_onlined_ == false)
			{
				this->is_onlined_ = true;

				this->on_load();

				print_success_info("zone {%s} loaded.", this->get_id());
			}
		}


		void game_zone::unload()
		{
			__lock__(this->status_mutex_, "game_zone::unload");

			if (this->is_onlined_ == true)
			{
				this->is_onlined_ = false;

				this->on_unload();

                print_success_info("zone {%s} unloaded.", this->get_id());
			}
		}


		void game_zone::on_load()
		{
		}


		void game_zone::on_unload()
		{
		}


		//启动一个指定延时的回调操作，因为timer对象要频繁的实例化，所以采用deque的结构对timer对象进行池缓冲
		//queue_task会从deque的头部弹出有效的timer对象，用完后，从新放回的头部，这样deque头部的对象即为活跃timer
		//如timer对象池中后部的对象长时间未被使用，说明当前对象被空闲，可以回收。
		//注意：｛如果game_zone所使用的io_service对象被cancel掉，那么用户层所注册的callback_handle是不会被调用的！｝
		void game_zone::queue_task(std::function<void(void)> callback_handle, int sleep_duration)
		{
			//状态锁
			//__lock__(this->status_mutex_, "game_zone::queue_task::status_mutex");

			//if (this->game_service_->is_running() && this->is_onlined_)
			{
				//预备timer的指针，并尝试从timer对象池中取出一个空闲的timer进行使用
				//########################################################
				timer* curr_timer_ = NULL;
				{
					//队列上锁，先检查队列中是否有可用的timer
					__lock__(this->free_timers_mutex_,  "game_zone::queue_task::free_timers_mutex");

					if (this->free_timers_.size() > 0)
					{
						curr_timer_ = this->free_timers_.front();
						this->free_timers_.pop_front();

						if (game_zone::LOG_TIMERS_INFO)
						{
							printf("{%s} use the existed timer of 1 / %d : %d.\n", this->get_id(), this->free_timers_.size(), this->working_timers_.size());
						}
					}
				}
				//########################################################

				//如果对象池中无有效的timer对象，再进行实例化
				if (curr_timer_ == NULL)
				{
					void* timer_mem = boost::singleton_pool<timer, sizeof(timer)>::malloc();
					curr_timer_ = new(timer_mem)timer(this->game_service_->get_io_service());

					if (game_zone::LOG_TIMERS_INFO)
					{
						//printf("##################>new timer");
					}
				}

				{
					__lock__(this->working_timers_mutex_,  "game_zone::queue_task::working_timers_mutex");
					//考虑把if(curr_timer == NULL)的一段代码放置到此位置，防止出现野timer
					//极限情况，后续lock + destroy之后，这里又加入了
					this->working_timers_.insert(curr_timer_);
				}

				//调用操作
				curr_timer_->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
				curr_timer_->async_wait(std::bind(&game_zone::task_handle, this,
					std::placeholders::_1, curr_timer_, callback_handle));
			}
		}

		//queue_task的内置回调函数
		//1、判断回调状态
		//2、处理timer资源
		//3、调用上层回调
		void game_zone::task_handle(const boost::system::error_code& error, timer* timer_, std::function<void(void)> callback_handle)
		{
			//如果当前的io操作还正常
			if (!error)
			{
				//先锁状态，看game_zone的状态，如果还在running，那么继续处理逻辑，否则处理timer
				//__lock__(this->status_mutex_, "game_zone::task_handle::status_mutex");

				//if (this->game_service_->is_running() && this->is_onlined_ == true)
				{
					//#######此处代码处理用完的timer，返还给队列池##########
					//#################################################
					{
						__lock__(this->working_timers_mutex_, "game_zone::task_handle::working_timers_mutex");
						this->working_timers_.erase(timer_);
					}

					{
						__lock__(this->free_timers_mutex_, "game_zone::task_handle::free_timers_mutex");

						//将用用完的timer返回给队列池，放在池的前部
						this->free_timers_.push_front(timer_);

						//标记最后的激活时间
						timer_->last_actived_time.restart();

						//从后方检查栈队列中最后的元素是否是空闲了指定时间
						if (this->free_timers_.size() > MIN_ACTIVED_TIMER
							&& this->free_timers_.back()->last_actived_time.elapsed() > MAX_TIMER_FREE_TICK)
						{
							if (game_zone::LOG_TIMERS_INFO)
							{
								printf("{%s} free timers were destroyed,left=%d.\n", this->get_id(), this->free_timers_.size());
							}
							timer* free_timer = this->free_timers_.back();
							this->free_timers_.pop_back();

							//delete free_timer;
							free_timer->~timer();
							boost::singleton_pool<timer, sizeof(timer)>::free(free_timer);
						}
					}
					//########处理timer完毕###############################
					//###################################################

					//回调上层逻辑callback
					callback_handle();
					//返回不执行后续逻辑
					return;
				}
			}

			//如果调用失效，或者已经unload，那就不用返还了额，直接delete
			//delete timer_;
			//timer_->~timer();
			//boost::singleton_pool<timer, sizeof(timer)>::free(timer_);
			//printf("timer callback was canceld, timer has deleted.\n");
		}



		///为什么不用指定调用onunload？
		///因为game_zone的启动和停止完全由game_service托管，
		///如果一个game_zone已经onload，那么他的onunload一定会随着service的onunload被调用；
		///而service的onunload一定会被调用，so、game_zone~不用管理自己的onunload状态
		game_zone::~game_zone()
		{
			{
				__lock__(this->working_timers_mutex_, "~game_zone::working_timers_mutex");

				for (std::set<timer*>::iterator pos_timer = this->working_timers_.begin();
					pos_timer != this->working_timers_.end();
					++pos_timer)
				{
					(*pos_timer)->cancel();
					(*pos_timer)->~timer();
					boost::singleton_pool<timer, sizeof(timer)>::free(*pos_timer);
				}
				this->working_timers_.clear();
			}


			{
				__lock__(this->free_timers_mutex_, "~game_zone::free_timers_mutex");

				for (std::list<timer*>::iterator pos_timer = this->free_timers_.begin();
                    pos_timer != this->free_timers_.end(); ++pos_timer)
				{
					(*pos_timer)->cancel();
					(*pos_timer)->~timer();
					boost::singleton_pool<timer, sizeof(timer)>::free(*pos_timer);
				}

				this->free_timers_.clear();
			}

			delete[] this->id_;
		}
	}
}
