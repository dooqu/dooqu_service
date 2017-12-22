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


}
}
service_status service_status_;
