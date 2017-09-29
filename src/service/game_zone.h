#ifndef __GAME_ZONE_H__
#define __GAME_ZONE_H__

#include <iostream>
#include <cstring>
#include <list>
#include <set>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio/deadline_timer.hpp>
#include <boost/noncopyable.hpp>
#include "game_service.h"
#include "../util/tick_count.h"
#include "../util/utility.h"
#include "async_task.h"


namespace dooqu_service
{
namespace service
{
using namespace boost;
using namespace boost::asio;

class game_plugin;

//typedef void (game_plugin::*plugin_callback_handle)(void*, void*, int verifi_code);

//#define make_plugin_handle(handle) (plugin_callback_handle)&handle
//继承deadline_timer，并按照需求，增加一个标示最后激活的字段。
class timer : public boost::asio::deadline_timer
{
public:
    tick_count last_actived_time;
    timer(io_service& ios) : deadline_timer(ios) {}
    virtual ~timer() {  };
};


class game_zone : public async_task
{
    friend class game_service;

private:

    //game_zone.onload.
    void load();

    //game_zone.unonload.
    void unload();

    //queue_task的回调函数
    //void task_handle(const boost::system::error_code& error, timer* timer_, std::function<void(void)> callback_handle);

protected:

    //timer池中最小保有的数量，timer_.size > 此数量后，会启动空闲timer检查
   // const static int MIN_ACTIVED_TIMER = 50;

    //如timer池中的数量超过{MIN_ACTIVED_TIMER}定义的数量， 并且队列后部的timer空闲时间超过
    //MAX_TIMER_FREE_TICK的值，会被强制回收
    //const static int MAX_TIMER_FREE_TICK = 1 * 60 * 1000;


    //game_zone 的状态锁
   // std::recursive_mutex status_mutex_;

    //存放定时器的双向队列
    //越是靠近队列前部的timer越活跃，越是靠近尾部的timer越空闲
    //std::list<timer*> free_timers_;

    //std::set<timer*> working_timers_;

    //timer队列池的状态锁
    //std::recursive_mutex free_timers_mutex_;

   // std::recursive_mutex working_timers_mutex_;

    //game_zone的唯一id
    char* id_;

    //game_zone是否在线
    bool is_onlined_;

    char* auth_host_;

    char* auth_path_;

    //io_service object.
    game_service* game_service_;

    //onload事件
    virtual void on_load();

    //onunload事件
    virtual void on_unload();

public:

    static bool LOG_TIMERS_INFO;

    game_zone(game_service* service, const char* id);

    game_zone(game_service* service, const char* id, const char* auth_host, const char* auth_path);

    //get game_zone's id
    char* get_id()
    {
        return this->id_;
    }

    //whether game_zone is online.
    bool is_onlined()
    {
        return this->is_onlined_;
    }

    void set_auth_host(const char* host)
    {
        if(this->auth_host_ != NULL)
        {
            if(strcmp(this->auth_host_, host) == 0)
                return;

            delete [] this->auth_host_;
        }

        int strlength = strlen(host);
        this->auth_host_ = new char[strlength + 1];
        strncpy(this->auth_host_, host, strlength);

        auth_host_[strlength] = 0;
    }

    const char* get_auth_host()
    {
        return this->auth_host_;
    }

    void set_auth_path(const char* path)
    {
        if(this->auth_path_ != NULL)
        {
            if(strcmp(this->auth_path_, path) == 0)
                return;

            delete [] this->auth_path_;
        }

        int strlength = strlen(path);
        this->auth_path_ = new char[strlength + 1];
        strncpy(this->auth_path_, path, strlength);

        auth_path_[strlength] = 0;
    }

    const char* get_auth_path()
    {
        return this->auth_path_;
    }

    //get io_service object.
    //io_service* get_io_service(){ return &this->game_service_->get_io_service(); }

    //queue a plugin's delay method.
    //void queue_task(std::function<void(void)> callback_handle, int sleep_duration);
    virtual ~game_zone();
};
}
}

#endif
