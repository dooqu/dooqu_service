#include "threads_lock_status.h"
//std::map<std::thread::id, char*> thread_status::messages;
//

    thread_status* thread_status::_instance = new thread_status();
    thread_status* thread_status::instance()
    {
        if(_instance == NULL)
            _instance = new thread_status();

        return _instance;
    }


    void thread_status::log(char* message)
    {
        status_[std::this_thread::get_id()] = message;
    }

    thread_lock_status* thread_status::status()
    {
        return &this->status_;
    }
