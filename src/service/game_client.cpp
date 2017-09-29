#include "game_client.h"

namespace dooqu_service
{
namespace service
{
game_client::game_client(io_service& ios) :
    tcp_client(ios),
    game_info_(NULL),
    cmd_dispatcher_(NULL),
    error_code_(dooqu_service::net::service_error::CLIENT_NET_ERROR),
    plugin_addr_(0),
    curr_dispatcher_thread_id_(NULL)
{
    this->id_[0] = 0;
    this->name_[0] = 0;
    this->retry_update_times_ = UP_RE_TIMES;
    this->active();
    this->message_monitor_.init(5, 4000);
    this->active_monitor_.init(30, 60 * 1000);
}


game_client::~game_client()
{
}

/*
该函数是触发接受逻辑和离开逻辑的顶层函数
1、先调用client->active，响应客户端的心跳逻辑，有数据响应即算为有心跳数据，如果考虑严谨性，可将active的调用放置在
on_command中，判定command有效后，再进行active，但考虑到dispatcher可以客户定制化，对逻辑的底层封装过于暴露，默认
在代码的on_received中；
2、将逻辑处理交付给command_dispatcher::on_client_data_received;

*/
void game_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
{
    //std::cout << this->id() << ".on_data_received:" << "bytes_received" << bytes_received << ",result:" << error <<  "thread_id:" << std::this_thread::get_id() << std::endl;
    if (!error)
    {
        assert(this->cmd_dispatcher_ != NULL);
        if (this->cmd_dispatcher_ == NULL )
        {
            return;
        }
        this->active();
        int next_receive_buffer_pos = 0;
        int error_result = this->cmd_dispatcher_->on_client_data_received(this, bytes_received, &next_receive_buffer_pos);
        //在on_client_data_received中，有两种意外情况；
        //第一种、用户的数据违规，直接返回error_code;
        //第二种，用户逻辑违规，直接在on_command_handler中调用了disconnect;
        if(error_result != dooqu_service::net::service_error::NO_ERROR)
        {
            this->disconnect(error_result);
        }
        this->tcp_client::on_data_received(error, bytes_received);
        this->buffer_pos = next_receive_buffer_pos;
        this->p_buffer = &this->buffer[this->buffer_pos];
        this->read_from_client();
    }
    else
    {
        ___lock___(this->commander_mutex_, "game_client::on_data_received.commander_mutex_");
        this->available_ = false;
        this->on_error(service_error::CLIENT_NET_ERROR);
    }
}


//on_error的主要功能是将用户的离开和逻辑错误动作传递给command_dispatcher对象进行依次处理。
//error_code_的初始默认值为CLIENT_NET_ERROR；
//如果这个值被改动，说明在on_error之前、调用过disconnect、并传递过断开的原因；
//这样在tcp_client的断开处理中、即使传递0，也不会被赋值；
void game_client::on_error(const int error)
{
    if (this->error_code_ == service_error::CLIENT_NET_ERROR)
    {
        this->error_code_ = error;
    }

    if (this->cmd_dispatcher_ != NULL)
    {
        this->ios.post(std::bind(&command_dispatcher::dispatch_bye, this->cmd_dispatcher_, this));
    }
}


void game_client::fill(char* id, char* name, char* profile)
{
    int id_len = std::strlen(id);
    int name_len = std::strlen(name);
    int pro_len = (profile == NULL) ? 0 : std::strlen(profile);

    int min_id_len = std::min((ID_LEN - 1), id_len);
    int min_name_len = std::min((NAME_LEN - 1), name_len);

    strncpy(this->id_, id, min_id_len);
    strncpy(this->name_, name, min_name_len);

    this->id_[min_id_len] = 0;
    this->name_[min_name_len] = 0;
}


void game_client::dispatch_data(char* command_data)
{
    assert(this->cmd_dispatcher_ != NULL);

    if (this->cmd_dispatcher_ == NULL)
        return;

    int command_data_len = strlen(command_data);

    assert((command_data_len + 1) <= tcp_client::MAX_BUFFER_SIZE);

    bool locked = this->commander_mutex_.try_lock();
    if(locked && this->curr_dispatcher_thread_id_ == &std::this_thread::get_id())
    {
        char command_data_clone[tcp_client::MAX_BUFFER_SIZE];
        command_data_clone[sprintf(command_data_clone, command_data, command_data_len)] = 0;
        this->cmd_dispatcher_->simulate_client_data(this, command_data_clone);
        this->commander_mutex_.unlock();
    }
    else
    {
        assert(this->curr_dispatcher_thread_id_ != &std::this_thread::get_id());

        char* command_data_clone = (char*)malloc(command_data_len + 1);//new char[command_data_len + 1];
        std::strcpy(command_data_clone, command_data);
        command_data_clone[std::strlen(command_data)] = 0;
        this->commander_mutex_.unlock();
        this->ios.post(std::bind(&game_client::simulate_command_process, this, command_data_clone));
    }
}

void game_client::simulate_command_process(char* command_data)
{
    if(this->cmd_dispatcher_ != NULL)
    {
        this->cmd_dispatcher_->simulate_client_data(this, command_data);
    }
    //delete [] command_data;
    free(command_data);
}


void game_client::set_command_dispatcher(command_dispatcher* dispatcher)
{
    this->cmd_dispatcher_ = dispatcher;
}

void game_client::disconnect()
{
    __lock__(this->commander_mutex_, "game_client::disconnect.commander_mutex");

    if (this->available())
    {
        this->available_ = false;
        boost::system::error_code err_code;
        this->t_socket.shutdown(boost::asio::socket_base::shutdown_receive, err_code);
    }
}


void game_client::disconnect(int code)
{
    __lock__(this->commander_mutex_, "game_client::disconnect_int.commander_mutex_");

    if (this->available())
    {
        this->error_code_ = code;
        //this->write("ERR %d%c", this->error_code_, NULL);        //this->disconnect_when_io_end();

        this->disconnect();
    }
}
}
}
