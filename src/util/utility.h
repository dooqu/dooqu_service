#ifndef UTILITY_H
#define UTILITY_H


#include <cstdio>
#include <chrono>
#include <stdarg.h>
#include <stdlib.h>
#include "threads_lock_status.h"
#include "boost/pool/singleton_pool.hpp"



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
//#define __boostlock__(state, message) \
//	thread_status::log("STAR->"#message);\
//	decltype(state)::scoped_lock name_var_by_line(lock)(state);\
//	thread_status::log("END->"#message)
//
//#define __lock__(state, message) \
//	thread_status::log("STAR->"#message);\
//	std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
//	thread_status::log("END->"#message)



class RAIIInstance
{
public:
    char* message_;
    RAIIInstance(char* message) : message_(NULL)
    {
        this->message_ = message;
    }
    ~RAIIInstance()
    {
         thread_status::instance()->exit(this->message_);
    }
};

//#define __lock__(state, message)    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);
//
//
//#define ___lock___(state, message)   std::lock_guard<decltype(state)> name_var_by_line(lock)(state);

#define __lock__(state, message) \
    thread_status::instance()->wait("WAITING->"#message);\
    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
    thread_status::instance()->hold("HOLDING->"#message);\
    RAIIInstance name_var_by_line(ra)("DESTROYING->"#message);\
    std::this_thread::sleep_for(std::chrono::milliseconds(50));\


#define ___lock___(state, message) \
    thread_status::instance()->wait("WAIT->"#message);\
    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
    thread_status::instance()->hold("HOLD->"#message);\
    RAIIInstance name_var_by_line(ra)("DESTROY->"#message);\
    std::this_thread::sleep_for(std::chrono::milliseconds(60));\

//#define __lock__(state, message) \
//    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
//    RAIIInstance name_var_by_line(ra)("DESTROYING->"#message);\
//    std::this_thread::sleep_for(std::chrono::milliseconds(50));\
//
//
//#define ___lock___(state, message) \
//    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
//    RAIIInstance name_var_by_line(ra)("DESTROY->"#message);\
//    std::this_thread::sleep_for(std::chrono::milliseconds(60));\

template<typename TYPE>
void* memory_pool_malloc()
{
    boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
}

template <typename TYPE>
void memory_pool_free(void* chunk)
{
    if(boost::singleton_pool<TYPE, sizeof(TYPE)>::is_from(chunk) == false)
    {
        throw "error";
    }
    boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
}

template <typename TYPE>
void memory_pool_release()
{
    boost::singleton_pool<TYPE, sizeof(TYPE)>::release_memory();
}

template <typename TYPE>
void memory_pool_purge()
{
    boost::singleton_pool<TYPE, sizeof(TYPE)>::purge_memory();
}

namespace dooqu_service
{
namespace util
{
void print_success_info(const char* format, ...);
void print_error_info(const char* format, ...);
int random(int a, int b);
}
}


#endif // UTILITY_H
