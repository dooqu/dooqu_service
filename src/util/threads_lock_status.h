#ifndef __THREADS_LOCK_STATUS__
#define __THREADS_LOCK_STATUS__
#include <map>
#include <thread>
#include <mutex>


typedef std::map<std::thread::id, char*> thread_lock_status;

class thread_status
{
protected:
    thread_lock_status status_;
    static thread_status* _instance;
public:
    static thread_status* instance();

    void log(char* message);

    thread_lock_status* status();
};
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
