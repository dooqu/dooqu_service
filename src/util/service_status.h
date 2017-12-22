#ifndef __THREADS_LOCK_STATUS__
#define __THREADS_LOCK_STATUS__
#include <map>
#include <thread>
#include <mutex>
#include <deque>
#include "boost/pool/singleton_pool.hpp"

using namespace boost;
//typedef std::map<std::thread::id, char*> thread_status_;
typedef std::map<std::thread::id, std::deque<char*>*> thread_lock_stack;
namespace dooqu_service
{
namespace util
{
struct service_status
{
    service_status();
    virtual ~service_status();
    bool echo_timers_status;
    thread_lock_stack lock_stack;
    static service_status* _instance ;
    static service_status* instance();

    void init(std::thread::id thread_id);
    void log(char* message);
    void txtlog(std::thread::id thread_id, char* logcontent);
    void wait(char* message);
    void hold(char* message);
    void exit(char* message);
    //thread_status_map* status();
    static service_status* create_new();
    static void set_instance(service_status* instance);
    static void destroy();
////
//template<typename TYPE>
//void* memory_pool_malloc()
//{
//
//    printf("SOS: memeory_pool_malloc:(%s)\n", typeid(TYPE).name());
//    return boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
//    //return service_status::instance()->memory_pool_malloc<TYPE>();
//}
//
//template <typename TYPE>
//void memory_pool_free(void* chunk)
//{
//    printf("SOS: memeory_pool_free:(%s)\n", typeid(TYPE).name());
//    if(boost::singleton_pool<TYPE, sizeof(TYPE)>::is_from(chunk))
//    {
//        boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
//    }
//    else
//    {
//        throw "error";
//    }
//    //service_status::instance()->memory_pool_free<TYPE>(chunk);
//}
//
//template <typename TYPE>
//void memory_pool_release()
//{
//    boost::singleton_pool<TYPE, sizeof(TYPE)>::release_memory();
//    //service_status::instance()->memory_pool_release<TYPE>();
//}
//
//template <typename TYPE>
//void memory_pool_purge()
//{
//    boost::singleton_pool<TYPE, sizeof(TYPE)>::purge_memory();
//   // service_status::instance()->memory_pool_purge<TYPE>();
//}

};
}
}
#endif;
