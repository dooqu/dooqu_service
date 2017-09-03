#ifndef __THREADS_LOCK_STATUS__
#define __THREADS_LOCK_STATUS__
#include <map>
#include <thread>
#include <mutex>
#include <deque>

typedef std::map<std::thread::id, char*> thread_lock_status;

typedef std::map<std::thread::id, std::deque<char*>*> thread_map_status;

class thread_status
{
public:
    thread_lock_status status_;
    thread_map_status map_status;
    static thread_status* _instance ;
public:
    static thread_status* instance();

    void log(char* message);

    void txtlog(std::thread::id thread_id, char* logcontent);

    void wait(char* message);
    void hold(char* message);
    void exit(char* message);

    thread_status();
    thread_lock_status* status();
    static thread_status* create_new();
    static void set_instance(thread_status* instance);

};

//extern thread_status::_instance;
//thread_status* thread_status::_instance = NULL;

//extern thread_lock_status thread_mutex_message;
//#include <map>
//#include <thread>

//class thread_status
//{
//public:
//	static std::map<std::thread::id, char*> messages;
//
//	static inline void log(char* message)
//	{
//		thread_mutex_message[std::this_thread::get_id()] = message;
//		//printf("%s\n", message);
//	}
//};


//std::map<boost::thread::id, char*> thread_status::messages;
#endif;
