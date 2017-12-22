#include "service_status.h"
#include <iostream>
//std::map<std::thread::id, char*> thread_status::messages;
//
namespace dooqu_service
{
namespace util
{
service_status* service_status::_instance = NULL;

service_status::service_status() : echo_timers_status(false)
{
    //printf("thread_status\n");
}

service_status::~service_status()
{
    for(thread_lock_stack::iterator e = this->lock_stack.begin(); e != this->lock_stack.end(); ++ e)
    {
        delete (*e).second;
    }
    this->lock_stack.clear();
}


service_status* service_status::instance()
{
    return _instance;
}

void service_status::txtlog(std::thread::id thread_id, char* content)
{
    return;
    char filename[64] = {0};
    sprintf(filename, "thread_log_{%d}", thread_id);
    FILE* fp = fopen(filename, "at");

    if (fp != NULL)
    {
        fputs(content, fp);
        fputc((int)'\n', fp);

        fclose(fp);
    }
}

void service_status::log(char* message)
{
//    status_map[std::this_thread::get_id()] = message;
}

void service_status::init(std::thread::id thread_id)
{
    lock_stack[thread_id] = new std::deque<char*>();
}

void service_status::wait(char* message)
{
    lock_stack[std::this_thread::get_id()]->push_front(message);
    //this->txtlog(std::this_thread::get_id(), message);
}

void service_status::hold(char* message)
{
    lock_stack[std::this_thread::get_id()]->pop_front();
    lock_stack[std::this_thread::get_id()]->push_front(message);
    //this->txtlog(std::this_thread::get_id(), message);
}

void service_status::exit(char* message)
{
    //map_status[std::this_thread::get_id()]->push_front(message);
    lock_stack[std::this_thread::get_id()]->pop_front();
    //this->txtlog(std::this_thread::get_id(), message);
}

service_status* service_status::create_new()
{
    if(_instance == NULL)
    {
        _instance = new service_status();
    }
    return _instance;
}

void service_status::set_instance(service_status* instance)
{
    if(_instance == NULL)
    {
        _instance = instance;
    }
}

void service_status::destroy()
{
    if(_instance != NULL)
    {
        delete _instance;
    }
}

}
}


