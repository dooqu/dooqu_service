#include "tcp_client.h"
#include "tcp_server.h"


namespace dooqu_service
{
namespace net
{
bool tcp_client::LOG_IO_DATA = false;
long tcp_client::CURR_RECE_TOTAL = 0;
long tcp_client::CURR_SEND_TOTAL = 0;

tcp_client::tcp_client(io_service& ios)
    : ios(ios),
      t_socket(ios),
      //send_buffer_sequence_(MAX_BUFFER_SEQUENCE_SIZE / 2),
      read_pos_(-1), write_pos_(0),
      buffer_pos(0),
      available_(false)
{
    this->p_buffer = &this->buffer[0];
    memset(this->buffer, 0, sizeof(char) * MAX_BUFFER_SIZE);
}


void tcp_client::read_from_client()
{
    this->t_socket.async_read_some(boost::asio::buffer(this->p_buffer, tcp_client::MAX_BUFFER_SIZE - this->buffer_pos),
                                   std::bind(&tcp_client::on_data_received, this, std::placeholders::_1, std::placeholders::_2));
}


//注意，send_buffer_sequence默认构造了一定的量，正常情况下是够用的，如果不够用，最大可创建
//MAX_BUFFER_SEQUENCE_SIZE个，但MAX_BUFFER_SEQUENCE_SIZE-8个，要引发复制构造函数，效率低下
bool tcp_client::alloc_available_buffer(buffer_stream** buffer_alloc)
{
    //如果当前的写入位置的指针指向存在一个可用的send_buffer，那么直接取这个集合；
    if (write_pos_ < this->send_buffer_sequence_.size())
    {
        *buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);
        this->write_pos_++;
        return true;
    }
    else
    {
        //如果指向的位置不存在，需要新申请对象；
        //如果已经存在了超过16个对象，说明网络异常那么断开用户；不再新申请数据;
        if (this->send_buffer_sequence_.size() >= MAX_BUFFER_SEQUENCE_SIZE)
        {
            *buffer_alloc = NULL;
            return false;
        }

        void* buffer_mem = memory_pool_malloc<buffer_stream>();//boost::singleton_pool<buffer_stream, sizeof(buffer_stream)>::malloc();
        buffer_stream* curr_buffer = new(buffer_mem) buffer_stream(MAX_BUFFER_SIZE);
        this->send_buffer_sequence_.push_back(curr_buffer);

        //std::cout << "创建buffer_stream at MAIN." << std::endl;
        //将写入指针指向下一个预置位置
        this->write_pos_ = this->send_buffer_sequence_.size() - 1;
        //得到当前的send_buffer/
        *buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);
        this->write_pos_++;

        return true;
    }
}


void tcp_client::write(char* data)
{
    this->write("%s%c", data, NULL);
}


void tcp_client::write(const char* format, ...)
{
    __lock__(this->send_buffer_lock_, "tcp_client::write::send_buffer_lock_");

    buffer_stream* curr_buffer = NULL;
    if (this->alloc_available_buffer(&curr_buffer) == false)
    {
        return;
    }

    for(;;)
    {
        va_list arg_ptr;
        va_start(arg_ptr, format);
        int bytes_writed = curr_buffer->write(format, arg_ptr);
        va_end(arg_ptr);

        if(bytes_writed == -1)
        {
            curr_buffer->double_size();
            continue;
        }
        break;
    }


    if (read_pos_ == -1)
    {
        //只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
        read_pos_++;
        boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
                                 std::bind(&tcp_client::send_handle, this, std::placeholders::_1));
        this->is_data_sending_ = true;
    }
}

///在发送完毕后，对发送队列中的数据进行处理；
///发送数据是靠async_send 来实现的， 它的内部是使用多次async_send_same来实现数据全部发送；
///所以，如果想保障数据的有序、正确到达，一定不要在第一次async_send结束之前发起第二次async_send；
///实现采用一个发送的数据报队列，按数据报进行依次的发送,靠回调来驱动循环发送，直到当次队列中的数据全部发送完毕；
void tcp_client::send_handle(const boost::system::error_code& error)
{
    if (error)
    {
        __lock__(this->send_buffer_lock_, "tcp_client::send_handle::send_buffer_lock_");
        read_pos_ = -1;
        write_pos_ = 0;
        this->is_data_sending_ = false;
        return;
    }

    __lock__(this->send_buffer_lock_, "tcp_client::send_handle::send_buffer_lock_");

    buffer_stream* curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);
    if (read_pos_ >= (write_pos_ - 1))
    {
        read_pos_ = -1;
        write_pos_ = 0;
        this->is_data_sending_ = false;
    }
    else
    {
        read_pos_++;
        buffer_stream* curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);
        boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
                                 std::bind(&tcp_client::send_handle, this, std::placeholders::_1));
    }
}


void tcp_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
{
}


tcp_client::~tcp_client()
{
    //std::cout << "~tcp_client at MAIN." << std::endl;
    boost::system::error_code err_code;
    this->t_socket.close(err_code);
    {
        __lock__(this->send_buffer_lock_, "tcp_client::~tcp_client::lock");
        int size = this->send_buffer_sequence_.size();

        for(int i = 0; i < size ; i++)
        {
            buffer_stream* curr_buffer = this->send_buffer_sequence_.at(i);
            curr_buffer->~buffer_stream();
            memory_pool_free<buffer_stream>(curr_buffer);
        }
        this->send_buffer_sequence_.clear();
    }
}
}
}


