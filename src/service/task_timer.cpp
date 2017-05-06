#include "task_timer.h"

namespace dooqu_service
{
    namespace service
    {

        async_task::async_task(io_service& ios) : io_service_(ios)
        {
            //ctor
        }

        async_task::~async_task()
        {
            printf("~async_task\n");
            {
                __lock__(this->working_timers_mutex_, "~game_zone::working_timers_mutex");

                for (std::set<task_timer*>::iterator pos_timer = this->working_timers_.begin();
                        pos_timer != this->working_timers_.end();
                        ++pos_timer)
                {
                    (*pos_timer)->cancel();
                    (*pos_timer)->~task_timer();

                    //delete (*pos_timer);
                    boost::singleton_pool<task_timer, sizeof(task_timer)>::free(*pos_timer);
                }
                this->working_timers_.clear();
            }
            printf("~async_task middle\n");

            {
                __lock__(this->free_timers_mutex_, "~game_zone::free_timers_mutex");

                for (int i = 0; i < this->free_timers_.size(); ++i)
                {
                    this->free_timers_.at(i)->cancel();
                    this->free_timers_.at(i)->~task_timer();

                    //delete this->free_timers_.at(i);
                    boost::singleton_pool<task_timer, sizeof(task_timer)>::free(this->free_timers_.at(i));
                }

                this->free_timers_.clear();
            }

            //boost::singleton_pool<task_timer, sizeof(task_timer)>::release_memory();

            printf("~async_task ok\n");
            //dtor
        }


        //启动一个指定延时的回调操作，因为timer对象要频繁的实例化，所以采用deque的结构对timer对象进行池缓冲
        //queue_task会从deque的头部弹出有效的timer对象，用完后，从新放回的头部，这样deque头部的对象即为活跃timer
        //如timer对象池中后部的对象长时间未被使用，说明当前对象被空闲，可以回收。
        //注意：｛如果game_zone所使用的io_service对象被cancel掉，那么用户层所注册的callback_handle是不会被调用的！｝
        task_timer* async_task::queue_task(std::function<void(void)> callback_handle, int sleep_duration, bool cancel_enabled)
        {
            //状态锁
            //__lock__(this->status_mutex_, "game_zone::queue_task::status_mutex");

            //if (this->game_service_->is_running() && this->is_onlined_)
            {
                //预备timer的指针，并尝试从timer对象池中取出一个空闲的timer进行使用
                //########################################################
                task_timer* curr_timer_ = NULL;

                if(cancel_enabled == false)
                {
                    //队列上锁，先检查队列中是否有可用的timer
                    __lock__(this->free_timers_mutex_,  "game_zone::queue_task::free_timers_mutex");

                    if (this->free_timers_.size() > 0)
                    {
                        curr_timer_ = this->free_timers_.front();
                        this->free_timers_.pop_front();

                       // if (game_zone::LOG_TIMERS_INFO)
                        {
                           // printf("{%s} use the existed timer of 1 / %d : %d.\n", this->get_id(), this->free_timers_.size(), this->working_timers_.size());
                        }
                    }
                }
                //########################################################

                //如果对象池中无有效的timer对象，再进行实例化
                if (curr_timer_ == NULL)
                {
                    void* timer_mem = ::operator new(sizeof(task_timer));//boost::singleton_pool<task_timer, sizeof(deadline_timer_ex)>::malloc();
                    curr_timer_ = new(timer_mem)task_timer(this->io_service_, cancel_enabled);
                }

                {
                    __lock__(this->working_timers_mutex_,  "game_zone::queue_task::working_timers_mutex");
                    //考虑把if(curr_timer == NULL)的一段代码放置到此位置，防止出现野timer
                    //极限情况，后续lock + destroy之后，这里又加入了
                    this->working_timers_.insert(curr_timer_);
                }

                //调用操作
                curr_timer_->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
                curr_timer_->async_wait(std::bind(&async_task::task_handle, this,
                                                  std::placeholders::_1, curr_timer_, callback_handle));

                return curr_timer_;
            }
        }


        bool async_task::cancel_task(task_timer* timer)
        {
            if(timer == NULL || timer->is_cancel_eanbled() == false)
                return false;

            boost::system::error_code err_code;
            size_t n = timer->cancel(err_code);

            if(n > 0)
            {
                {
                    __lock__(this->working_timers_mutex_, "game_zone::cancel_handle::working_timers_mutex");
                    this->working_timers_.erase(timer);
                }

                timer->~task_timer();
                        //delete free_timer;
                boost::singleton_pool<task_timer, sizeof(task_timer)>::free(timer);

                return true;
            }

            //如果==0， 已经在task_handle那边销毁了

            return false;
        }
        //queue_task的内置回调函数
        //1、判断回调状态
        //2、处理timer资源
        //3、调用上层回调
        void async_task::task_handle(const boost::system::error_code& error, task_timer* timer_, std::function<void(void)> callback_handle)
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

                    if(timer_->is_cancel_eanbled() == false)
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
//                            if (game_zone::LOG_TIMERS_INFO)
//                            {
//                                printf("{%s} free timers were destroyed,left=%d.\n", this->get_id(), this->free_timers_.size());
//                            }
                            task_timer* free_timer = this->free_timers_.back();
                            this->free_timers_.pop_back();

                            //delete free_timer;
                            free_timer->~task_timer();
                            boost::singleton_pool<task_timer, sizeof(task_timer)>::free(free_timer);
                        }
                    }
                    else
                    {
                        timer_->~task_timer();
                        boost::singleton_pool<task_timer, sizeof(task_timer)>::free(timer_);
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
            printf("timer callback was canceld, timer has deleted.\n");
        }

    }
}
