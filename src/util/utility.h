#ifndef UTILITY_H
#define UTILITY_H

#include <cstdio>
#include <chrono>
#include <stdarg.h>
#include <stdlib.h>
#include "boost/pool/singleton_pool.hpp"
#include "service_status.h"

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define concat_var(text1,text2) text1##text2
#define concat_var_name(text1,text2) concat_var(text1,text2)
#define name_var_by_line(text) concat_var_name(text,__LINE__)
#define ___lock___(state, message)   std::lock_guard<decltype(state)> name_var_by_line(lock)(state);
#define ___LOCK___(state, message) \
    service_status::instance()->wait("WAITING->"#message);\
    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
    service_status::instance()->hold("HOLDING->"#message);\
    RAIIInstance name_var_by_line(ra)("DESTROY->"#message);\
    std::this_thread::sleep_for(std::chrono::milliseconds(60));\

using namespace dooqu_service::util;
namespace dooqu_service
{
namespace util
{
void print_success_info(const char* format, ...);
void print_error_info(const char* format, ...);
int random(int a, int b);
//template<typename TYPE>
//void* memory_pool_malloc();
//
//template <typename TYPE>
//void memory_pool_free(void* chunk);
//
//template <typename TYPE>
//void memory_pool_release();
//
//template <typename TYPE>
//void memory_pool_purge();

//
template<typename TYPE>
void* memory_pool_malloc()
{
    //game_client;
    //task_timer;
    //http_request_task;
    //buffer_stream

    //printf("memeory_pool_malloc:(%s)\n", typeid(TYPE).name());
    return boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
    //return service_status::instance()->memory_pool_malloc<TYPE>();
}

template <typename TYPE>
void memory_pool_free(void* chunk)
{
    //printf("memeory_pool_free:(%s)\n", typeid(TYPE).name());
    if(boost::singleton_pool<TYPE, sizeof(TYPE)>::is_from(chunk))
    {
        boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
    }
    else
    {
        throw "error";
    }
    //service_status::instance()->memory_pool_free<TYPE>(chunk);
}

template <typename TYPE>
void memory_pool_release()
{
    boost::singleton_pool<TYPE, sizeof(TYPE)>::release_memory();
    //service_status::instance()->memory_pool_release<TYPE>();
}

template <typename TYPE>
void memory_pool_purge()
{
    boost::singleton_pool<TYPE, sizeof(TYPE)>::purge_memory();
   // service_status::instance()->memory_pool_purge<TYPE>();
}

}
}


#endif // UTILITY_H
