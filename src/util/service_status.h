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
//
    template<typename TYPE>
    void* memory_pool_malloc();

    template <typename TYPE>
    void memory_pool_free(void* chunk);

    template <typename TYPE>
    void memory_pool_release();

    template <typename TYPE>
    void memory_pool_purge();
};
}
}
#endif;
