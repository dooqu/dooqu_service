#include "game_plugin.h"
#include "game_service.h"


namespace dooqu_service
{
namespace service
{
game_plugin::game_plugin(game_service* game_service, char* gameid, char* title, int capacity) :
    game_service_(game_service),
    update_timer_(this->game_service_->get_io_service()),
    capacity_(capacity),
    frequence_(2000)
{
    this->gameid_ = new char[std::strlen(gameid) + 1];
    std::strcpy(this->gameid_, gameid);

    this->title_ = new char[std::strlen(title) + 1];
    std::strcpy(this->title_, title);

    this->is_onlined_ = false;
}


char* game_plugin::game_id()
{
    return this->gameid_;
}


char* game_plugin::title()
{
    return this->title_;
}


bool game_plugin::is_onlined()
{
    return this->is_onlined_;
}


void game_plugin::on_load()
{
}


void game_plugin::on_unload()
{
}


void game_plugin::on_update()
{
}


void game_plugin::on_run()
{
    if (this->is_onlined() == false)
        return;

    this->on_update();

    this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
    this->update_timer_.async_wait(std::bind(&game_plugin::on_run, this));
}


int game_plugin::auth_client(game_client* client, const std::string& auth_response_string)
{
    int ret = this->on_auth_client(client, auth_response_string);

    if (ret == service_error::NO_ERROR)
    {
        return this->join_client(client);
    }

    return ret;
}
//对返回的认证内容进行分析，决定用户是否通过认证，同时在这里要做两件事：
//1、对用户的信息进行填充
//2、对game_info的实例进行填充
int game_plugin::on_auth_client(game_client* client, const  std::string& auth_response_string)
{
    //std::cout <<"http response:" <<  auth_response_string << std::endl;
    return service_error::NO_ERROR;
}


int game_plugin::on_befor_client_join(game_client* client)
{
    if(this->clients()->size() >= this->capacity_)
    {
        return service_error::GAME_IS_FULL;
    }

    if (std::strlen(client->id()) > 32 || std::strlen(client->name()) > 32)
    {
        return service_error::ARGUMENT_ERROR;
    }
    //防止用户重复登录
    game_client_map::iterator curr_client = this->clients_.find(client->id());

    if (curr_client == this->clients_.end())
    {
        return service_error::NO_ERROR;
    }
    else
    {
        return service_error::CLIENT_HAS_LOGINED;
    }
}


void game_plugin::on_client_join(game_client* client)
{
    printf("{%s} join plugin .\n", client->name());
}


void game_plugin::on_client_leave(game_client* client, int code)
{
    printf("{%s} leave plugin. ret={%d}.\n", client->name(), code);
}


game_zone* game_plugin::on_create_zone(char* zone_id)
{
    return new game_zone(this->game_service_, zone_id);
}


void game_plugin::dispatch_bye(game_client* client)
{
    this->remove_client(client);
}


void game_plugin::on_client_command(game_client* client, command* command)
{
    if (this->game_service_->is_running() == false)
        return;

    if (client->active_monitor_.can_active() == false)
    {
        client->disconnect(service_error::CONSTANT_REQUEST);
        return;
    }

    command_dispatcher::on_client_command(client, command);

    {
        using namespace dooqu_service::net;

        thread_status_map::iterator curr_thread_pair = this->game_service_->threads_status()->find(std::this_thread::get_id());

        if (curr_thread_pair != this->game_service_->threads_status()->end())
        {
            curr_thread_pair->second->restart();
        }
    }
}


void game_plugin::on_update_timeout_clients()
{
    __lock__(this->clients_lock_, "on_update_timeout_clients");

    for (game_client_map::iterator e = this->clients_.begin();
            e != this->clients_.end();
            ++e)
    {
        game_client* client = (*e).second;
        if (client->actived_time.elapsed() > 60 * 1000)
        {
            int code = dooqu_service::service::service_error::TIME_OUT;
            this->game_service_->get_io_service().post(std::bind(static_cast<void(game_client::*)(int)>(&game_client::disconnect), client, code));// client->disconnect(service_error::TIME_OUT);
        }
    }
}


void game_plugin::load()
{
    if (this->is_onlined_ == false)
    {
        this->is_onlined_ = true;
        this->on_load();

        //开始按frequence_进行on_run的调用
        this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
        this->update_timer_.async_wait(std::bind(&game_plugin::on_run, this));

        print_success_info("plugin {%s} loaded.", this->game_id());
    }
}


void game_plugin::unload()
{
    if (this->is_onlined_ == true)
    {
        //停止更新on_run(); 最外层其实已经调用了ios_stop();
        this->is_onlined_ = false;
        this->update_timer_.cancel();
        this->on_unload();

        {
            __lock__(this->clients_lock_, "game_plugin::unload");

            game_client* client = NULL;
            foreach_plugin_client(client, this)
            {
                int ret = service_error::SERVER_CLOSEING;
                this->game_service_->get_io_service().post(std::bind(static_cast<void(game_client::*)(int)>(&game_client::disconnect), client, ret));
                //client->disconnect(service_error::SERVER_CLOSEING);
            }
        }

        while(this->clients()->size() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        print_success_info("plugin {%s} unloaded.", this->game_id());
    }
}


int game_plugin::join_client(game_client* client)
{
    __lock__(this->clients_lock_, "game_plugin::join_client::clients_lock_");

    int ret = this->on_befor_client_join(client);

    if(client->available() == false)
    {
        return client->error_code();
    }

    if (ret == service_error::NO_ERROR)
    {
        client->set_command_dispatcher(this);
        //把当前玩家加入成员组
        this->clients_[client->id()] = client;
        //调用玩家进入成员组后的事件
        this->on_client_join(client);
    }
    //返回结果
    return ret;
}


void game_plugin::remove_client_from_plugin(game_client* client)
{
    {
        __lock__(this->clients_lock_, "game_plugin::remove_client_from_plugin");
        this->clients_.erase(client->id());
    }

    this->on_client_leave(client, client->error_code_);
    ((command_dispatcher*)this->game_service_)->dispatch_bye(client);
}


//负责处理玩家离开game_plugin后的所有逻辑
void game_plugin::remove_client(game_client* client)
{
    client->set_command_dispatcher(NULL);

    string server_url, path_url;

    if (this->need_update_offline_client(client, server_url, path_url))
    {
        this->begin_update_client(client, server_url, path_url);
    }
    else
    {
        this->remove_client_from_plugin(client);
        //甩给另外一个线程， 因为上游在command_dispatcher已经lock(client->status),这样会形成client->status到clients_->status的连续链， 二在其他地方会有先锁住集合，再锁住个体的顺序，会产生死锁；
        //this->game_service_->get_io_service().post(boost::bind(&game_plugin::remove_client_from_plugin, this, client));
    }
}


void game_plugin::begin_update_client(game_client* client, string& server_url, string& path_url)
{
    if (this->game_service_ != NULL)
    {
        using namespace std::placeholders;

        req_callback f = std::bind(&game_plugin::end_update_client, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, client);
        this->game_service_->queue_http_request(server_url.c_str(), path_url.c_str(), f);
    }
}


void game_plugin::end_update_client(const boost::system::error_code& err,
                                    const int status_code,
                                    const string& result, game_client* client)
{
    //this->game_service_->free_http_request(request);
    //std::cout << "end_update_client:err=" << err <<", code=" << status_code << std::endl;
    //如果http请求成功、同时http的返回码是200
    if (!err && status_code == 200)
    {
        printf("http update client successed.\n");
        this->on_update_offline_client(err, status_code, client);
    }
    else
    {
        printf("http update client failed,retry...\n");
        //如果http请求没有成功
        //利用game_zone再发起延时请求；
        if (this->zone_ != NULL && --client->retry_update_times_ > 0)
        {
            this->zone_->queue_task(std::bind(&game_plugin::remove_client, this, client), 3000);
            return;
        }
    }

    this->remove_client_from_plugin(client);
    //std::cout << "callback thread id is : " << std::this_thread::get_id() << std::endl;
}


//如果当game_client离开game_plugin需要调用http协议的外部地址来更新更新game_client的状态；
//那么请在此函数返回true，并且正确的赋值server_url和request_path的值；
bool game_plugin::need_update_offline_client(game_client* client, string& server_url, string& request_path)
{
    return false;
}


//如果get_offline_update_url返回true，请重写on_update_offline_client，并根据error_code的值来确定update操作的返回值。
void game_plugin::on_update_offline_client(const boost::system::error_code& err, const int status_code, game_client* client)
{

}


void game_plugin::broadcast(char* message, bool asynchronized)
{
    __lock__(this->clients_lock_, "game_plugin::broadcast");

    game_client* curr_client = NULL;
    foreach_plugin_client(curr_client, this)
    {
        curr_client->write(message);
    }
}


game_plugin::~game_plugin()
{
    if (this->gameid_ != NULL)
    {
        delete [] this->gameid_;
    }

    if (this->title_ != NULL)
    {
        delete [] this->title_;
    }
}
}
}

extern "C"
{
    void set_thread_status_instance(thread_status* instance)
    {
        thread_status::set_instance(instance);
    }
}
