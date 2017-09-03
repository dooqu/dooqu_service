#ifndef __SERVICE_SERVICE_ERROR_H__
#define __SERVICE_SERVICE_ERROR_H__

#include "../net/service_error.h"

namespace dooqu_service
{
namespace service
{
struct service_error : dooqu_service::net::service_error
{
    static const int CLIENT_HAS_LOGINED = 11;
    static const int GAME_IS_FULL = 12;
    static const int GAME_NOT_EXISTED = 15;
    static const int TIME_OUT = 16;
    static const int SERVER_CLOSEING = 17;
    static const int ARGUMENT_ERROR = 18;
    static const int LOGIN_SERVICE_ERROR = 19;
    static const int LOGIN_CONCURENCY_LIMIT = 20;
    static const int CONSTANT_REQUEST = 21;
    static const int COMMAND_ERROR = 22;

};
}
}

#endif
