#include "game_service.h"
#include "game_plugin.h"
#include "game_zone.h"

namespace dooqu_service
{
namespace service
{
game_service::game_service(unsigned int port) :
    tcp_server(port), check_timeout_timer(io_service_)
{
}


int game_service::load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg)
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if (game_plugin == NULL || game_plugin->game_id() == NULL || game_plugin->game_service_ != this)
    {
        //dooqu_service::util::print_error_info("plugin not created, argument error.");
        (*errorMsg) = "plugin's argument error.";
        return -1;
    }

    if(game_plugin->is_onlined_ || game_plugin->clients()->size() > 0)
    {
        //dooqu_service::util::print_error_info("plugin {%s} is in using.", game_plugin->game_id());
        (*errorMsg) = "plugin is in use.";
        return -2;
    }

    __lock__(this->plugins_mutex_, "create_plugin");

    game_plugin_map::iterator curr_plugin = this->plugins_.find(game_plugin->game_id());

    if (curr_plugin != this->plugins_.end())
    {
        //dooqu_service::util::print_error_info("plugin's id {%s} is in use.", game_plugin->game_id());
        (*errorMsg) = "plugin is in use.";
        return -2;
    }

    this->plugins_[game_plugin->game_id()] = game_plugin;
    this->plugin_list_.push_back(game_plugin);

    //如果传递的zoneid不为空，那么为游戏挂载游戏区
    if (zone_id != NULL)
    {
        game_zone_map::iterator curr_zone = this->zones_.find(zone_id);

        if (curr_zone != this->zones_.end())
        {
            game_plugin->zone_ = (*curr_zone).second;
        }
        else
        {
            game_zone* zone = game_plugin->on_create_zone(zone_id);

            if (this->is_running_ && zone != NULL)
            {
                zone->load();
            }
            this->zones_[zone->get_id()] = zone;
            game_plugin->zone_ = zone;
        }
    }

    if (this->is_running_)
    {
        game_plugin->load();
    }

    return 1;
}


/*
unload the plugin, remove the plugin from service's plugin list.
and call plugin's onunload event if plugin not unload.
*/
bool game_service::unload_plugin(game_plugin* plugin, int seconds_for_wait_compl)
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if(plugin == NULL || plugin->game_id() == NULL)
        return false;

    {
        __lock__(this->plugins_mutex_, "game_service::unload_plugin::plugin_mutex_");

        if(this->plugins_.erase(plugin->game_id()) <= 0)
            return false;

        this->plugin_list_.remove(plugin);
    }

    if(plugin->is_onlined())
    {
        plugin->unload();
    }

    int millisecs_for_wait_compl = seconds_for_wait_compl * 1000;
    int millisecs_sleeped = 0;

    for(;;)
    {
        if(plugin->clients()->size() <= 0)
            return true;

        if(seconds_for_wait_compl <= -1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else if(seconds_for_wait_compl == 0)
        {
            return false;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if((millisecs_sleeped += 100) <  millisecs_for_wait_compl)
            {
                continue;
            }
            return false;
        }
    }
}


void game_service::on_start()
{
    std::srand((unsigned)time(NULL));

    this->regist_handle("LOG", make_handler(game_service::client_login_handle));
    this->regist_handle("RLG", make_handler(game_service::robot_login_handle));
    //加载所有的所有分区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        (*curr_zone).second->load();
    }

    {
        __lock__(this->plugins_mutex_, "game_servcie::on_init::plugin_mutex");
        //加载所有的游戏逻辑
        for (game_plugin_list::iterator curr_game = this->plugin_list_.begin();
                curr_game != this->plugin_list_.end(); ++curr_game)
        {
            (*curr_game)->load();
        }
    }

    for(int i = 0; i < 2; i++)
    {
        request_block_[i] = malloc(sizeof(http_request) * MAX_AUTH_SESSION_COUNT);
        for(int j = 0; j < MAX_AUTH_SESSION_COUNT; j++)
        {
            this->request_pool_[i].push(request_block_[i] + (j * sizeof(http_request)));
        }
    }
}


void game_service::on_started()
{
    this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
    this->check_timeout_timer.async_wait(std::bind(&game_service::on_check_timeout_clients, this, std::placeholders::_1));
}


void game_service::on_stop()
{
    //停止对登录超时的检测
    this->check_timeout_timer.cancel();

    //清理临时登录用户的内存资源
    {
        __lock__(this->clients_mutex_, "game_service::on_stop::client_mutex");

        for (game_client_map::iterator curr_client = this->clients_.begin();
                curr_client != this->clients_.end(); ++curr_client)
        {
            game_client* client = (*curr_client);
            client->disconnect(service_error::SERVER_CLOSEING);
        }
    }

    {
        __lock__(this->plugins_mutex_, "game_service::on_stop::plugins_mutex");
        //停止所有的游戏逻辑
        for (game_plugin_list::iterator curr_game = this->plugin_list_.begin();
                curr_game != this->plugin_list_.end(); ++curr_game)
        {
            (*curr_game)->unload();
        }
    }

    //boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //停止所有的游戏区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        (*curr_zone).second->unload();
    }
}

void game_service::on_stoped()
{
    on_destroy_clients_in_destroy_list(true);
}


void game_service::dispatch_bye(game_client* client)
{
    this->on_client_leave(client, client->error_code());
}


void game_service::on_client_command(game_client* client, command* command)
{
    if (this->is_running_ == false)
        return;

    if (client->can_active() == false)
    {
        client->disconnect(service_error::CONSTANT_REQUEST);
        return;
    }

    command_dispatcher::on_client_command(client, command);
}



tcp_client* game_service::on_create_client()
{
    //std::cout << "创建game_client at MAIN." << std::endl;
    void* client_mem = memory_pool_malloc<game_client>();//boost::singleton_pool<game_client, sizeof(game_client)>::malloc();
    return new(client_mem) game_client(this->get_io_service());
    //return new game_client(this->get_io_service());
}


void game_service::on_client_connected(tcp_client* t_client)
{
    game_client* client = (game_client*)t_client;
    client->available_ = true;
    //设置命令的监听者为当前service
    client->set_command_dispatcher(this);

    {
        __lock__(this->clients_mutex_, "game_service::on_client_join");
        //在登录用户组中注册
        this->clients_.insert(client);
    }//对用户组上锁

    client->read_from_client();
    client->active();

    //printf("game_client connected.\n");
}



void game_service::on_client_leave(game_client* client, int leave_code)
{
    printf("{%s} leave game_service. code={%d}\n", client->id(), leave_code);

    client->set_command_dispatcher(NULL);
    {
        __lock__(this->clients_mutex_, "game_service::on_client_leave::clients_mutex");
        this->clients_.erase(client);
    }

    {
        __lock__(client->send_buffer_lock_, "game_service::on_client_leave::send_buffer_lock_");
        if(client->read_pos_ == -1)
        {
            this->on_destroy_client(client);
            return;
        }
    }

    {
        __lock__(this->destroy_list_mutex_, "game_service::on_client_leave::destroy_list_mutext");
        this->client_list_for_destroy_.push_back(client);
        printf("INSERT client to destroy list.\n");
    }
}


void game_service::on_destroy_client(tcp_client* t_client)
{
    game_client* client = (game_client*)t_client;
    //std::cout << "destroy client at MAIN." << std::endl;
    client->~game_client();
    memory_pool_free<game_client>(client);
    //boost::singleton_pool<game_client, sizeof(game_client)>::free(client);
}

void game_service::on_destroy_clients_in_destroy_list(bool force_destroy)
{
    __lock__(this->destroy_list_mutex_, "game_service::on_destroy_clients_in_destroy_list");

    for(list<game_client*>::iterator e = this->client_list_for_destroy_.begin(); e != this->client_list_for_destroy_.end(); )
    {
        game_client* client = *e;

        std::cout << "destroy one client in destroy list." << std::endl;
        if(force_destroy)
        {
            this->on_destroy_client(client);
            this->client_list_for_destroy_.erase(e++);
        }
        else
        {
            __lock__(client->send_buffer_lock_, "game_service::on_destroy_clients_in_destroy_list::send_buffer_lock");
            if(client->read_pos_ == -1)
            {
                this->on_destroy_client(client);
                this->client_list_for_destroy_.erase(e++);
                continue;
            }
            ++e;
        }
    }
}


//使用了定时器
void game_service::on_check_timeout_clients(const boost::system::error_code &error)
{
    if (!error && this->is_running_)
    {
        //如果禁用超时检测，请注释return;
        if(this->clients_.size() > 0)
        {
            __lock__(this->clients_mutex_, "game_service::on_check_timeout_clients::clients_mutex");
            //对所有用户的active时间进行对比，超过20秒还没有登录动作的用户被装进临时数组
            for (game_client_map::iterator curr_client = this->clients_.begin();
                    curr_client != this->clients_.end(); ++curr_client)
            {
                game_client* client = (*curr_client);
                if (client->get_actived() > 10 * 1000)
                {
                    int ret = service_error::TIME_OUT;
                    this->post_handle_to_another_thread(std::bind(static_cast<void(game_client::*)(int)>(&game_client::disconnect), client, ret));
                }
            }
        }

        static tick_count timeout_count;

        if (timeout_count.elapsed() > 60 * 1000)
        {
            __lock__(this->plugins_mutex_, "game_service::on_check_timeout_clients::plugins_mutex");
            for (game_plugin_list::iterator curr_game = this->get_plugins()->begin();
                    curr_game != this->get_plugins()->end();
                    ++curr_game)
            {
                (*curr_game)->on_update_timeout_clients();
            }

            timeout_count.restart();
        }

        this->on_destroy_clients_in_destroy_list(false);

        if (tcp_client::LOG_IO_DATA)
        {
            char io_buffer_msg[64] = { 0 };
            sprintf(io_buffer_msg, "I/O : %ld / %ld\r", tcp_client::CURR_RECE_TOTAL / 5, tcp_client::CURR_SEND_TOTAL / 5);
            printf(io_buffer_msg);

            tcp_client::CURR_RECE_TOTAL = 0;
            tcp_client::CURR_SEND_TOTAL = 0;
        }
        //如果服务没有停止， 那么继续下一次计时
        if (this->is_running_)
        {
            this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
            this->check_timeout_timer.async_wait(std::bind(&game_service::on_check_timeout_clients, this, std::placeholders::_1));
        }
    }
}

bool game_service::queue_http_request(const char* host, const char* path, req_callback callback)
{
    //std::cout << "SO: queue" << std::endl;
    void* req_block = this->malloc_http_request(1);
    if(req_block != NULL)
    {
        http_request* request = new(req_block) http_request(this->get_io_service(),
                host,
                path,
                std::bind(&game_service::queue_http_request_handle, this, std::placeholders::_1, std::placeholders::_2, (http_request*)req_block, callback));
        return true;
    }
    else
    {
        void* task_block = memory_pool_malloc<http_request_task>();
        http_request_task* req_task = new(task_block) http_request_task(host, path, callback);
        ___lock___(this->task_queue_mutex_, "game_servcie::queue_http_request::task_queue_mutex_");
        this->task_queue_.push(req_task);
    }
}

void game_service::queue_http_request_handle(const boost::system::error_code& err, const int status_code, http_request* req, req_callback callback)
{
    //std::cout << err << std::endl;
    //std::cout << "SO queue handle" << std::endl;
    string response_content;
    req->read_response_content(response_content);
    callback(err, status_code, response_content);
    req->~http_request();
    this->free_http_request(1, req);

    ___lock___(this->task_queue_mutex_, "game_servcie::queue_http_request_handle::task_queue_mutex_");
    while(this->task_queue_.empty() == false)
    {
        void* req_block = this->malloc_http_request(1);
        if(req_block != NULL)
        {
            http_request_task* req_task = this->task_queue_.front();

            http_request* request = new(req_block) http_request(this->get_io_service(),
                    req_task->request_host.c_str(),
                    req_task->request_path.c_str(),
                    std::bind(&game_service::queue_http_request_handle, this, std::placeholders::_1, std::placeholders::_2, (http_request*)req_block, req_task->callback));

            this->task_queue_.pop();
            req_task->~http_request_task();
            memory_pool_free<http_request_task>(req_task);
            continue;
        }
        break;
    }
}


bool game_service::start_http_request(const char* host, const char* path, req_callback callback)
{
    //std::cout << "SO start" << std::endl;
    void* req_block = this->malloc_http_request(0);

    if(req_block == NULL)
        return false;

    http_request* request = new(req_block) http_request(this->get_io_service(),
            host,
            path,
            std::bind(&game_service::http_request_handle, this, std::placeholders::_1, std::placeholders::_2, (http_request*)req_block, callback));

    return true;
}

void game_service::http_request_handle(const boost::system::error_code& err, const int status_code, http_request* req, req_callback callback)
{
    //std::cout << "SO start handle" << std::endl;
    string response_content;
    req->read_response_content(response_content);
    callback(err, status_code, response_content);
    req->~http_request();
    this->free_http_request(0, req);
}


void* game_service::malloc_http_request(int pool_index)
{
    ___lock___(this->request_pool_mutex_[pool_index], "game_service::mallock_http_request::request_pool_mutex_");

    if(this->request_pool_[pool_index].empty())
        return NULL;

    void* req = this->request_pool_[pool_index].front();
    this->request_pool_[pool_index].pop();

    return req;
}

void game_service::free_http_request(int pool_index, void* request)
{
    ___lock___(this->request_pool_mutex_[pool_index], "game_servcie::free_http_request::request_pool_mutex_");
    this->request_pool_[pool_index].push(request);
}

void game_service::begin_auth(game_plugin* plugin, game_client* client, command* cmd)
{
    const char* game_id = plugin->game_id();
    req_callback f = std::bind(&game_service::end_auth, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, game_id, client);
    bool ret = this->start_http_request("127.0.0.1", "/", f);

    if(ret == false)
    {
        client->disconnect(service_error::LOGIN_CONCURENCY_LIMIT);
    }
}


void game_service::end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, const char* plugin_id, game_client* client)
{
    if (this->is_running_ == false)
        return;

    int ret = service_error::NO_ERROR;

    {
        __lock__(this->clients_mutex_, "game_service::end_auth::clients_mutex");
        //如果clients_中没有找到client，那么返回，说明client已经销毁了
        if(this->clients_.erase(client) <= 0)
            return;
    }

    if (!code && status_code == 200)
    {
        //检查client是不是已经在过程中断开
        ___lock___(client->commander_mutex_, "end_auth::commander_mutex");
        {
            if(client->available() == false)
                return;
            //上锁检查，因为plugin有可能已经被卸载
            ___lock___(this->plugins_mutex_, "game_service::end_auth::plugin_mutex");
            {
                //检查当前要登录的plugin是否还有效
                game_plugin_map::iterator finder = this->plugins_.find(plugin_id);
                if(finder != this->plugins_.end())
                {
                    ret = (*finder).second->auth_client(client, response_string);
                }
                else
                {
                    ret = service_error::GAME_NOT_EXISTED;
                }
            }
        }
    }
    else
    {
        ret = service_error::LOGIN_SERVICE_ERROR;
    }

    if (ret != service_error::NO_ERROR)
    {
        client->disconnect(ret);
        printf("authentication failed, error code is: %d\n", ret);
    }
}

void game_service::post_handle_to_another_thread(std::function<void(void)> handle)
{
    this->io_service_.post(handle);
}


void game_service::client_login_handle(game_client* client, command* command)
{
    if (command->param_size() != 2)
    {
        client->disconnect(service_error::COMMAND_ERROR);
        return;
    }

    /*检查该玩家是否已经发送过LOG命令， 防止在begin_auth异步去做其他事情的时候，该client又发起LOG操作*/
    /*至于位什么不用上锁， 是因为，本质上，单个用户的命令是串行的，不会产生并发冲突*/
    if(client->plugin_addr_ != 0)
    {
        client->disconnect(service_error::CLIENT_HAS_LOGINED);
        return;
    }

    ___lock___(this->plugins_mutex_, "game_service::client_login_handle::plugins_mutex");
    //查找是否存在对应玩家想加入的的game_plugin
    game_plugin_map::iterator it = this->plugins_.find(command->params(0));
    if (it != this->plugins_.end())
    {
        client->fill(command->params(1), command->params(1), NULL);
        game_plugin* plugin = (*it).second;
        //设定plugin_addr_地址，防止用户重复发送LOG命令
        client->plugin_addr_ = reinterpret_cast<int>(plugin);
        //把目标plugin的id带上，在回调函数中要再次检查plugin是否还在
        this->begin_auth(plugin, client, command);
        //在认证期间用户发送的命令将被忽略
        //client->set_command_dispatcher(NULL);
    }
    else
    {
        client->disconnect(service_error::GAME_NOT_EXISTED);
    }
}


void game_service::robot_login_handle(game_client* client, command* command)
{
}


game_service::~game_service()
{
    if (this->is_running_ == true)
    {
        this->stop();
    }

    on_destroy_clients_in_destroy_list(true);

    this->plugins_.clear();
    //销毁所有的游戏区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        delete (*curr_zone).second;
    }
    this->zones_.clear();


    for (thread_status_map::iterator pos_thread_status_pair = this->threads_status_.begin();
            pos_thread_status_pair != this->threads_status_.end();
            ++pos_thread_status_pair)
    {
        delete (*pos_thread_status_pair).second;
    }
    this->threads_status_.clear();

    //清理未返回的http_request对象
//    __lock__(this->http_request_mutex_, "~game_servcie::http_request_mutex");
//    for (std::set<http_request*>::iterator pos_http_request = this->http_request_working_.begin();
//            pos_http_request != this->http_request_working_.end();)
//    {
//        //这里一定要注意写法，因为这里是边遍历、边删除
//        //pos_http_request++返回一个临时对象， 对临时对象删除，并自动向下指向
//        this->free_http_request((*pos_http_request++));
//    }

    free(this->request_block_[0]);
    free(this->request_block_[1]);
//    boost::singleton_pool<game_client, sizeof(game_client)>::purge_memory();
//    boost::singleton_pool<http_request, sizeof(http_request)>::purge_memory();
//    boost::singleton_pool<timer, sizeof(timer)>::purge_memory();
//    boost::singleton_pool<buffer_stream, sizeof(buffer_stream)>::purge_memory();
}
}
}
