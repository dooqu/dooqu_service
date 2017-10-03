#ifndef __GAME_SERVICE_H__
#define __GAME_SERVICE_H__

#include <set>
#include <queue>
#include <list>
#include <map>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio/deadline_timer.hpp>
#include <boost/pool/singleton_pool.hpp>
#include "../net/tcp_server.h"
#include "../service/command_dispatcher.h"
#include "../util/utility.h"
#include "../util/char_key_op.h"
#include "game_client.h"
#include "service_error.h"
#include "http_request.h"

using namespace boost::asio;
using namespace dooqu_service::net;
using namespace std::placeholders;

namespace dooqu_service
{
namespace service
{
typedef std::function<void(const boost::system::error_code&, const int, const string&)> req_callback;
struct http_request_task
{
    string request_host;
    string request_path;
    req_callback callback;
    http_request_task(const char* host, const char* path, req_callback user_callback) :
        request_host(host), request_path(path), callback(user_callback)
    {
    }

    void reset(const char* host, const char* path,req_callback user_callback)
    {
        request_host.assign(host);
        request_path.assign(path);
        callback = user_callback;
    }
};

class game_zone;
class game_plugin;

class game_service : public command_dispatcher, public tcp_server
{
public:
    typedef std::map<const char*, game_plugin*, char_key_op> game_plugin_map;
    typedef std::list<game_plugin*> game_plugin_list;
    typedef std::map<const char*, game_zone*, char_key_op> game_zone_map;
    typedef std::set<game_client*> game_client_map;
    typedef std::list<game_client*> game_client_destroy_list;


protected:
    enum { MAX_AUTH_SESSION_COUNT = 50 };
    game_plugin_map plugins_;
    game_plugin_list plugin_list_;
    game_zone_map zones_;
    game_client_map clients_;
    game_client_destroy_list client_list_for_destroy_;
    std::mutex clients_mutex_;
    std::mutex plugins_mutex_;
    std::mutex zones_mutex_;
    std::mutex destroy_list_mutex_;

    boost::asio::deadline_timer check_timeout_timer;

    //std::set<http_request*> http_request_working_;
    void* request_block_[2];
    std::queue<void*> request_pool_[2];
    std::recursive_mutex request_pool_mutex_[2];

    std::queue<http_request_task*> task_queue_;
    std::recursive_mutex task_queue_mutex_;
//    std::recursive_mutex update_tasks_mutex_;

    /*start_events*/
    virtual void on_start();
    virtual void on_started();
    virtual void on_stop();
    virtual void on_stoped();
    inline virtual void on_client_command(game_client* client, command* command);
    virtual tcp_client* on_create_client();
    virtual void on_client_connected(tcp_client* client);
    virtual void on_client_leave(game_client* client, int code);
    virtual void on_destroy_client(tcp_client*);
    virtual void on_check_timeout_clients(const boost::system::error_code &error);
    virtual void on_destroy_clients_in_destroy_list(bool force_destroy);
    /*end_events;*/

    /*start functions*/
    void* malloc_http_request(int pool_index);
    void free_http_request(int pool_index, void* request);
    void http_request_handle(const boost::system::error_code& err, const int status_code,  http_request* req, req_callback callback);
    void queue_http_request_handle(const boost::system::error_code& err, const int status_code, http_request* req, req_callback callback);
    /*end_functions*/
    //非虚方法
    void begin_auth(game_plugin* plugin, game_client* client, command* cmd);
    void end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, const char* plugin_id, game_client* client);
    void dispatch_bye(game_client* client);
    inline void post_handle_to_another_thread(std::function<void(void)> handle);

    //应用层注册
    void client_login_handle(game_client* client, command* command);
    void robot_login_handle(game_client* client, command* command);
    void check_client_on_service_handle(game_client* host, command* command);
//
//    template<typename TYPE>
//    inline void* memory_pool_malloc()
//    {
//        boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
//    }
//
//    template <typename TYPE>
//    inline void memory_pool_free(void* chunk)
//    {
//        boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
//    }

public:
    game_service(unsigned int port);
    virtual ~game_service();
    int load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg = NULL);
    bool unload_plugin(game_plugin* game_plugin, int seconds_wait_for_complete = -1);
    virtual bool start_http_request(const char* host, const char* path, req_callback callback);
    virtual bool queue_http_request(const char* host, const char* path, req_callback callback);
    inline game_plugin_list* get_plugins()
    {
        return &this->plugin_list_;
    }
};
}
}
#endif
