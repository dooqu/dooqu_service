#include "threads_lock_status.h"
#include <iostream>
//std::map<std::thread::id, char*> thread_status::messages;
//

thread_status* thread_status::_instance = NULL;

thread_status::thread_status()
{
    //printf("thread_status\n");
}

thread_status::~thread_status()
{
    for(thread_lock_stack::iterator e = this->lock_stack.begin(); e != this->lock_stack.end(); ++ e)
    {
        delete (*e).second;
    }
    this->lock_stack.clear();
}


thread_status* thread_status::instance()
{
    return _instance;
}

void thread_status::txtlog(std::thread::id thread_id, char* content)
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

void thread_status::log(char* message)
{
//    status_map[std::this_thread::get_id()] = message;
}

void thread_status::init(std::thread::id thread_id)
{
    lock_stack[thread_id] = new std::deque<char*>();
}

void thread_status::wait(char* message)
{
    lock_stack[std::this_thread::get_id()]->push_front(message);
    //this->txtlog(std::this_thread::get_id(), message);
}

void thread_status::hold(char* message)
{
    lock_stack[std::this_thread::get_id()]->pop_front();
    lock_stack[std::this_thread::get_id()]->push_front(message);
    //this->txtlog(std::this_thread::get_id(), message);
}

void thread_status::exit(char* message)
{
    //map_status[std::this_thread::get_id()]->push_front(message);
    lock_stack[std::this_thread::get_id()]->pop_front();
    //this->txtlog(std::this_thread::get_id(), message);
}

//thread_lock_status* thread_status::status()
//{
//    return &this->status_;
//}

thread_status* thread_status::create_new()
{
    if(_instance == NULL)
    {
        _instance = new thread_status();
    }
    return _instance;
}

void thread_status::set_instance(thread_status* instance)
{
    if(_instance == NULL)
    {
        _instance = instance;

        std::cout << "set_instance" << std::endl;
    }
}

void thread_status::destroy()
{
    if(_instance != NULL)
    {
        delete _instance;
    }
}
