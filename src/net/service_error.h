#ifndef __NET_SERVICE_ERROR_H__
#define __NET_SERVICE_ERROR_H__

namespace dooqu_service
{
	namespace net
	{
		struct service_error
		{
			//NET ERROR是指客户端的网络层断开
			static const int CLIENT_NET_ERROR = 0;
			///
			static const int CLIENT_EXIT = 1;
			static const int DATA_OUT_OF_BUFFER = 2;
			static const int SERVER_CLOSEING = 3;
			static const int CLIENT_DATA_SEND_BLOCK = 4;
			static const int OK = 99;
		};
	}
}

#endif
