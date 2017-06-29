#include "tcp_server.h"

namespace dooqu_service
{
	namespace net
	{
		tcp_server::tcp_server(unsigned int port) :
			acceptor(io_service_),
			port_(port),
			is_running_(false),
			is_accepting_(false)
		{

		}


		void tcp_server::create_worker_thread()
		{
			std::thread* worker_thread = new std::thread(std::bind(static_cast<size_t(io_service::*)()>(&io_service::run), &this->io_service_));

			this->threads_status_[worker_thread->get_id()] = new tick_count();

			worker_threads_.push_back(worker_thread);

			dooqu_service::util::print_success_info("service worker thread create ok, thead id is:{%d}", worker_thread->get_id());
		}


		void tcp_server::start_accept()
		{
			//提前把game_client变量准备好。该game_client实例如果监听失败，在on_accept的error分支中delete。
			tcp_client* client = this->on_create_client();

			//投递accept监听
			this->acceptor.async_accept(client->socket(), std::bind(&tcp_server::accept_handle, this, std::placeholders::_1, client));

			this->is_accepting_ = true;
		}


		void tcp_server::start()
		{
			if (this->is_running_ == false)
			{
				this->work_mode_ = new io_service::work(this->io_service_);

				//注意on_init中，isrunning = false;
				this->on_init();

				this->is_running_ = true;

				if(acceptor.is_open() == false)
				{
                    tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), this->port_);
                    acceptor.open(endpoint.protocol());
                    //acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
                    acceptor.bind(endpoint);
                    acceptor.listen();
				}

				//投递 {MAX_ACCEPTION_NUM} 个accept动作
				for (int i = 0; i < MAX_ACCEPTION_NUM; i++)
				{
					this->start_accept();
				}

				//开始工作者线程
				for (int i = 0; i < std::thread::hardware_concurrency() + 1; i++)
				{
					this->create_worker_thread();
				}

				this->on_start();
			}
		}


		void tcp_server::stop_accept()
		{
			if (this->is_accepting_ == true)
			{
				boost::system::error_code error;
				this->acceptor.cancel(error);
				this->acceptor.close(error);

				this->is_accepting_ = false;
			}
		}


		void tcp_server::stop()
		{
			if (this->is_running_ == true)
			{
				this->is_running_ = false;

				//不接受新的client
				this->stop_accept();

				//on_stop很重要，它的目的是让信息以加速度缓慢，直至静止的方式，让plugin、zone中的消息停止下来；
				//不要求信号量精，但on_stop之后，相当于在plugin和zone中放置了一个信号量， 各个有惯性的事件，通过读取
				//信号量，逐步的将事件停止下来， so，要在后面1、delete work；2、thread.join; 这样join阻塞完成之时，其实就是
				//所有线程上的事件都已经执行完毕;
				this->on_stop();

				//让所有工作者线程不再空等
				delete this->work_mode_;

				for (int i = 0; i < this->worker_threads_.size(); i++)
				{
                    printf("waiting thread {%d} exiting...\n");
					this->worker_threads_.at(i)->join();

					dooqu_service::util::print_success_info("worker thread has exited, id={%d}.\n", this->worker_threads_.at(i)->get_id());
				}

				this->worker_threads_.clear();
				//所有线程上的事件都已经执行完毕后， 安全的停止io_service;
				this->io_service_.stop();
				//重置io_service，以备后续可能的tcp_server.start()的再次调用。
				this->io_service_.reset();

				dooqu_service::util::print_success_info("main service has been stoped successfully.");
			}
		}


		void tcp_server::on_init()
		{
		}


		void tcp_server::on_start()
		{
		}


		void tcp_server::on_stop()
		{
		}


		void tcp_server::accept_handle(const boost::system::error_code& error, tcp_client* client)
		{
			if (!error)
			{
				if (this->is_running_)
				{
                    //处理新加入的game_client对象，这个时候game_client的available已经是true;
					this->on_client_connected(client);
					//处理当前的game_client对象，继续投递一个新的接收请求；
					start_accept();
				}
				else
				{
					//ios is running
					this->on_destroy_client(client);
				}
				return;
			}
			else
			{
				//如果走到这个分支，说明game_server已经调用了stop，停止了服务；
				//但因为start_accept，已经提前投递了N个game_client
				//所以这里返回要对game_client进行销毁处理
				//printf("tcp_server.accept_handle canceled:%s\n", error.message().c_str());
				this->on_destroy_client(client);
			}
		}


		tcp_server::~tcp_server()
		{
            //boost::singleton_pool<buffer_stream, sizeof(buffer_stream)>::release_memory();
			for (int i = 0; i < worker_threads_.size(); i++)
			{
				delete worker_threads_.at(i);
			}
		}
	}
}
