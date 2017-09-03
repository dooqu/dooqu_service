#ifndef UTILITY_H
#define UTILITY_H


#include <cstdio>
#include <chrono>
#include <stdarg.h>
#include <stdlib.h>
#include "threads_lock_status.h"



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
        thread_status::instance()->log(message_);
        thread_status::instance()->exit(message_);
    }
};

#define __lock__(state, message) \
    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\


#define ___lock___(state, message) \
    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\

//#define __lock__(state, message) \
//    thread_status::instance()->wait("WAITING->"#message);\
//    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
//    thread_status::instance()->hold("HOLDING->"#message);\
//    RAIIInstance name_var_by_line(ra)("DESTROYING->"#message);\
//    std::this_thread::sleep_for(std::chrono::milliseconds(30));\
//
//
//#define ___lock___(state, message) \
//    thread_status::instance()->wait("WAIT->"#message);\
//    std::lock_guard<decltype(state)> name_var_by_line(lock)(state);\
//    thread_status::instance()->hold("HOLD->"#message);\
//    RAIIInstance name_var_by_line(ra)("DESTROY->"#message);\
//    std::this_thread::sleep_for(std::chrono::milliseconds(60));\


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
