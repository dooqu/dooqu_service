#include "async_task.h"
#include <iostream>
namespace dooqu_service
{
namespace service
{
async_task::async_task(io_service& ios) : io_service_async_task_(ios)
{
    //ctor
}

async_task::~async_task()
{
    {
        ___lock___(this->working_timers_mutex_, "~game_zone::working_timers_mutex");
        assert(this->working_timers_.size() == 0);
        for (std::set<task_timer*>::iterator pos_timer = this->working_timers_.begin();
                pos_timer != this->working_timers_.end();
                ++pos_timer)
        {
            //printf("~async_task working dispose:%d\n", *pos_timer);
            (*pos_timer)->cancel();
            (*pos_timer)->~task_timer();
            memory_pool_free<task_timer>(*pos_timer);
            //boost::singleton_pool<task_timer, sizeof(task_timer)>::free(*pos_timer);
        }
        this->working_timers_.clear();
    }

    {
        ___lock___(this->free_timers_mutex_, "~game_zone::free_timers_mutex");
        int free_timers_size = this->free_timers_.size();
        printf("free_timers_size is: %d\n", free_timers_size);

        for (int i = 0; i < free_timers_size; ++i)
        {
            task_timer* curr_timer = this->free_timers_.at(i);
            curr_timer->~task_timer();
            memory_pool_free<task_timer>(curr_timer);
            //boost::singleton_pool<task_timer, sizeof(task_timer)>::free(curr_timer);
        }
        this->free_timers_.clear();
    }
}
//启动一个指定延时的回调操作，因为timer对象要频繁的实例化，所以采用deque的结构对timer对象进行池缓冲
//queue_task会从deque的头部弹出有效的timer对象，用完后，从新放回的头部，这样deque头部的对象即为活跃timer
//如timer对象池中后部的对象长时间未被使用，说明当前对象被空闲，可以回收。
//注意：｛如果game_zone所使用的io_service对象被cancel掉，那么用户层所注册的callback_handle是不会被调用的！｝
task_timer* async_task::queue_task(std::function<void(void)> callback_handle, int sleep_duration, bool cancel_enabled)
{
    if(sleep_duration <= 0)
    {
        this->io_service_async_task_.post(callback_handle);
        return;
    }

    bool from_pool = false;
    int pool_size = 0;
    int working_size = 0;
    task_timer* curr_timer_ = NULL;

    if(cancel_enabled == false)
    {
        ___lock___(this->free_timers_mutex_,  "game_zone::queue_task::free_timers_mutex");
        if (this->free_timers_.size() > 0)
        {
            pool_size = this->free_timers_.size();
            curr_timer_ = this->free_timers_.front();
            this->free_timers_.pop_front();
            from_pool = true;
        }
    }
    //如果对象池中无有效的timer对象，再进行实例化
    if (curr_timer_ == NULL)
    {
        void* timer_mem = memory_pool_malloc<task_timer>();//boost::singleton_pool<task_timer, sizeof(task_timer)>::malloc();
        assert(timer_mem != NULL);
        curr_timer_ = new(timer_mem) task_timer(this->io_service_async_task_, cancel_enabled);
    }
    //调用操作
    curr_timer_->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
    curr_timer_->async_wait(std::bind(&async_task::task_handle, this,
                                      std::placeholders::_1, curr_timer_, callback_handle));
    {
        ___lock___(this->working_timers_mutex_,  "game_zone::queue_task::working_timers_mutex");
        this->working_timers_.insert(curr_timer_);
        working_size = this->working_timers_.size();
    }

    if(service_status::instance()->echo_timers_status)
    {
        printf("queue_task: pool_size:(%d), working_size(%d), is_from_pool(%d)\n", pool_size, working_size, from_pool);
    }
    return curr_timer_;
}


//能够cancel的task，一定不能回收利用，无法解决“轮回”问题；
bool async_task::cancel_task(task_timer* timer)
{
    if(timer == NULL || timer->is_cancel_eanbled() == false)
        return false;

    boost::system::error_code err_code;
    size_t n = timer->cancel(err_code);

//    if(n > 0)
//    {
//        {
//            ___lock___(this->working_timers_mutex_, "game_zone::cancel_handle::working_timers_mutex");
//            this->working_timers_.erase(timer);
//        }
//
//        timer->~task_timer();
//        memory_pool_free<task_timer>(timer);
//        //boost::singleton_pool<task_timer, sizeof(task_timer)>::free(timer);
//        return true;
//    }
    //如果==0， 已经在task_handle那边销毁了
    return n > 0;
}


void async_task::cancel_all_task()
{
    //std::vector<task_timer*> tasks;
    {
        ___lock___(this->working_timers_mutex_, "game_zone::cancel_handle::working_timers_mutex");
        for(std::set<task_timer*>::iterator e = this->working_timers_.begin();
                e != this->working_timers_.end(); ++ e)
        {
            (*e)->cancel();
            //tasks.push_back(*e);
        }
        //this->working_timers_.clear();
    }
//
//
//    {
//        ___lock___(this->free_timers_mutex_, "game_zone::task_handle::free_timers_mutex");
//
//        for(int i = 0; i < tasks.size(); i++)
//        {
//            task_timer* curr_free_timer = tasks.at(i);
//
//            if(curr_free_timer->is_cancel_eanbled())
//            {
//                curr_free_timer->~task_timer();
//                //boost::singleton_pool<task_timer, sizeof(task_timer)>::free(curr_free_timer);
//                memory_pool_free<task_timer>(curr_free_timer);
//            }
//            else
//            {
//                free_timers_.push_back(tasks.at(i));
//            }
//        }
//    }

    printf("end cancel all task\n");
}
//queue_task的内置回调函数
//1、判断回调状态
//2、处理timer资源
//3、调用上层回调
void async_task::task_handle(const boost::system::error_code& error, task_timer* timer_, std::function<void(void)> callback_handle)
{
    int pool_size = 0;
    int working_size = 0;
    {
        ___lock___(this->working_timers_mutex_, "game_zone::task_handle::working_timers_mutex");
        this->working_timers_.erase(timer_);
        working_size = this->working_timers_.size();
    }

    if(timer_->is_cancel_eanbled() == false)
    {
        timer_->last_actived_time.restart();//标记最后的激活时间
        ___lock___(this->free_timers_mutex_, "game_zone::task_handle::free_timers_mutex");
        //将用用完的timer返回给队列池，放在池的前部
        this->free_timers_.push_front(timer_);
        pool_size = this->free_timers_.size();
        //从后方检查栈队列中最后的元素是否是空闲了指定时间
        if (pool_size > MIN_ACTIVED_TIMER
                && this->free_timers_.back()->last_actived_time.elapsed() > MAX_TIMER_FREE_TICK)
        {
            task_timer* free_timer = this->free_timers_.back();
            this->free_timers_.pop_back();
            free_timer->~task_timer();
            memory_pool_free<task_timer>(free_timer);
        }
    }
    else
    {
        timer_->~task_timer();
        memory_pool_free<task_timer>(timer_);
    }

    if(service_status::instance()->echo_timers_status)
    {
        printf("task_handle: pool_size(%d), working_size(%d)\n", pool_size, working_size);
    }

    if (!error)
    {
        callback_handle();        //返回不执行后续逻辑
    }
}
}
}
