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

struct http_request_task
{
    string request_host;
    string request_path;
    std::function<void(void)> callback;
    http_request_task(const char* host, const char* path, std::function<void(void)> user_callback) :
        request_host(host), request_path(path), callback(user_callback)
    {
    }

    void reset(const char* host, const char* path, std::function<void(void)> user_callback)
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
    typedef std::function<void(const boost::system::error_code&, const int, const string&)> req_callback;

protected:
    enum { MAX_AUTH_SESSION_COUNT = 40 };
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
    std::queue<void*> auth_request_pool_;
    std::queue<void*> update_request_pool_;

    void* auth_request_block_;
    void* update_request_block_;
    std::recursive_mutex auth_request_mutex_;
    std::recursive_mutex http_request_mutex_;

    std::queue<http_request_task*> request_task_pool_;
    std::queue<http_request_task*> update_tasks_;
    std::recursive_mutex request_task_mutex_;
    std::recursive_mutex update_tasks_mutex_;


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

    bool game_service::start_auth_request(const char* host, const char* path, req_callback callback);
    void game_service::auth_request_handle(const boost::system::error_code& err, const int status_code, const string& response, http_request* req, req_callback callback);
    //非虚方法
    void begin_auth(game_plugin* plugin, game_client* client, command* cmd);
    void end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, const char* plugin_id, game_client* client);
    void dispatch_bye(game_client* client);
    inline void post_handle_to_another_thread(std::function<void(void)> handle);

    //应用层注册
    void client_login_handle(game_client* client, command* command);
    void robot_login_handle(game_client* client, command* command);
    void check_client_on_service_handle(game_client* host, command* command);

    template<typename TYPE>
    inline void* memory_pool_malloc()
    {
        boost::singleton_pool<TYPE, sizeof(TYPE)>::malloc();
    }

    template <typename TYPE>
    inline void memory_pool_free(void* chunk)
    {
         boost::singleton_pool<TYPE, sizeof(TYPE)>::free(chunk);
    }

    void* get_auth_request();
    void release_auth_request(void* request);

//    void* malloc_http_request();
//    void free_http_request(void* request);


//    void queue_http_request(char* host, char* path, std::function callback)
//    {
//        http_request_task* task = NULL;
//        ___lock___(request_task_mutex_, "queue_http");
//        {
//            if(request_task_pool_.empty() == false)
//            {
//                task = request_task_pool_.front();
//                request_task_pool_.pop();
//                task.reset(host, path, callback);
//            }
//            else
//            {
//                task = new http_request_task(host, path, callback);
//            }
//        }
//
//        assert(task != NULL);
//
//        if(task == NULL)
//            return;
//
//        ___lock___(update_tasks_mutex_, "queue_http_request");
//
//    }


public:
    game_service(unsigned int port);
    virtual ~game_service();
    int load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg = NULL);
    bool unload_plugin(game_plugin* game_plugin, int seconds_wait_for_complete = -1);
    game_plugin_list* get_plugins()
    {
        return &this->plugin_list_;
    }
};
}
}
#endif
