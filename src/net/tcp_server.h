#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include "tcp_client.h"
#include "../util/tick_count.h"
#include "../util/utility.h"


namespace dooqu_service
{
namespace net
{
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace dooqu_service::util;


typedef std::map<std::thread::id, tick_count*> thread_status_map;
typedef std::vector<std::thread*> worker_threads;

class tcp_server : boost::noncopyable
{

protected:
    friend class tcp_client;

    enum { MAX_ACCEPTION_NUM = 4 };
    io_service io_service_;
    io_service::work* work_mode_;
    tcp::acceptor acceptor;
    unsigned int port_;
    bool is_running_;
    bool is_accepting_;
    thread_status_map threads_status_;
    worker_threads worker_threads_;
    std::thread::id service_thread_id_;

    void create_worker_thread();
    void start_accept();
    void accept_handle(const boost::system::error_code& error, tcp_client* client);

    virtual tcp_client* on_create_client() = 0;
    virtual void on_client_connected(tcp_client* client) = 0;
    virtual void on_destroy_client(tcp_client*) = 0;

    virtual void on_start();
    virtual void on_started();
    virtual void on_stop();
    virtual void on_stoped();

public:

    tcp_server(unsigned int port);
    io_service& get_io_service()
    {
        return this->io_service_;
    };
    thread_status_map* threads_status()
    {
        return &this->threads_status_;
    }
    void tick_count_threads()
    {
        for (int i = 0; i < this->worker_threads_.size(); i++)
        {
            this->io_service_.post(std::bind(&tcp_server::update_curr_thread, this));
        }
    }

    void update_curr_thread()
    {
        thread_status_map::iterator curr_thread_pair = this->threads_status()->find(std::this_thread::get_id());

        if (curr_thread_pair != this->threads_status()->end())
        {
            curr_thread_pair->second->restart();
            std::cout << "post update at thread: " << std::this_thread::get_id() << std::endl;
        }
    }
    void start();
    void stop();
    void stop_accept();
    inline bool is_running()
    {
        return this->is_running_;
    }
    virtual ~tcp_server();

};
}
}
#endif
