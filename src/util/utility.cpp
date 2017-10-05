#include "utility.h"
namespace dooqu_service
{
namespace util
{
void print_success_info(const char* format, ...)
{
    printf("[ %sOK%s ] ", BOLDGREEN, RESET);
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vprintf(format, arg_ptr);
    va_end(arg_ptr);
    printf("\n");
}


void print_error_info(const char* format, ...)
{
    printf("[ %sERROR%s ] ", BOLDRED, RESET);
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vprintf(format, arg_ptr);
    va_end(arg_ptr);
    printf("\n");
}

int random(int a, int b)
{
    return (int)((double)rand() / (double)RAND_MAX * (b - a + 1) + a);
}

//
//template<typename TYPE>
//void* memory_pool_malloc()
//{
//    return boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
//    //service_status::instance()->memory_pool_malloc<TYPE>();
//}
//
//template <typename TYPE>
//void memory_pool_free(void* chunk)
//{
//    if(boost::singleton_pool<TYPE, sizeof(TYPE)>::is_from(chunk))
//    {
//        boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
//    }
//    else
//    {
//        throw "error";
//    }
//   // service_status::instance()->memory_pool_free<TYPE>(chunk);
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

}
}
