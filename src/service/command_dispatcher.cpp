#include "command_dispatcher.h"
#include "game_client.h"

namespace dooqu_service
{
namespace service
{
command_dispatcher::~command_dispatcher()
{
    this->unregist_all_handles();
}


bool command_dispatcher::regist_handle(const char* cmd_name, command_handler handler)
{
    std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(cmd_name);

    if (handle_pair == this->handles.end())
    {
        this->handles[cmd_name] = handler;
        return true;
    }
    else
    {
        return false;
    }
}


int command_dispatcher::on_client_data_received(game_client* client, size_t bytes_received, int* next_receive_buffer_pos)
{
    client->buffer_pos += bytes_received;
    //防止意外，正常逻辑来说，收的字节数是可以控制的，不会产生缓冲区溢出
    if (client->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
    {
        return service_error::DATA_OUT_OF_BUFFER;
    }

    for (int i = 0, cmd_start_pos = 0; i < client->buffer_pos; ++i)
    {
        if (client->buffer[i] == 0)
        {
            this->on_client_data(client, &client->buffer[cmd_start_pos]);
            //如果on_data 中如需要对用户进行离线处理，那么只需调用disconnect
            //在disconnect中，设置available = false，并关闭接收，关闭socket。
            //那么在之后的检查中，判断需要对用户的离开进行处理，调用on_error进行清理，并离开大逻辑循环。
            if (client->available() == false)
            {
                return client->error_code();
            }
            //i此时是'\0'，预先将下一个cmd的开始位置进行重置;
            if (i < (client->buffer_pos - 1))
            {
                cmd_start_pos = i + 1;
            }
        }

        //如果是最后一个有效的命令字符
        if (i == (client->buffer_pos - 1))
        {
            //如果最后一个字符，不是命令的结束符号，那么当前命令还需要继续接收才完整
            //那么为了让接收的空间更富裕，如果当前命令是从半截开始，把这个命令向前串，拷贝到缓冲区的头部
            if (client->buffer[i] != 0)
            {
                //如果命令从0开始
                if(cmd_start_pos == 0)
                {
                    //如果命令从头开始，那就没有拷贝的必要了,但要检查命令是否会越界
                    //如果当前的命令从0开始，同时buffer_pos已经越界，那么关掉用的链接
                    if(client->buffer_pos == tcp_client::MAX_BUFFER_SIZE)
                    {
                        return service_error::DATA_OUT_OF_BUFFER;
                    }
                }
                else
                {
                    //如果命令不是从0开始， 那么为了给继续接受其余的数据疼出空间，要做一次向前串的拷贝
                    std::copy(client->buffer + cmd_start_pos, client->buffer + client->buffer_pos, client->buffer);
                    *next_receive_buffer_pos = client->buffer_pos -= cmd_start_pos;
                }
            }
            else
            {
                //设定bufferPos = 0;表示buffer可以从头开用了。
                *next_receive_buffer_pos = client->buffer_pos = 0;
            }
        }
    }

    return dooqu_service::net::service_error::NO_ERROR;
}


void command_dispatcher::on_client_data(game_client* client, char* data)
{
    ___lock___(client->commander_mutex_, "command_dispatcher::on_client_data::commander_mutex_");

    if(client->available_ == false)
    {
        return;
    }
    client->curr_dispatcher_thread_id_ = &std::this_thread::get_id();
    client->commander_.reset(data);
    if (client->commander_.is_correct())
    {
        this->on_client_command(client, &client->commander_);
    }
    client->curr_dispatcher_thread_id_ = NULL;
}


void command_dispatcher::simulate_client_data(game_client* client, char* data)
{
    this->on_client_data(client, data);
}


void command_dispatcher::on_client_command(game_client* client, command* command)
{
    std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(command->name());

    if (handle_pair != this->handles.end())
    {
        command_handler* handle = &handle_pair->second;
        (this->**handle)(client, command);
    }
}


void command_dispatcher::unregist_handle(char* cmd_name)
{
    this->handles.erase(cmd_name);
}


void command_dispatcher::unregist_all_handles()
{
    this->handles.clear();
}
}
}
