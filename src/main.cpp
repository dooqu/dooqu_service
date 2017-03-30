//#include <cstdio>
//#include <iostream>
//#include <thread>
//#include "service/game_service.h"
//#include "service/game_zone.h"
//#include "ddz/ddz_plugin.h"
//
//#define SERVER_STATUS_DEBUG
//#define SERVER_DEBUG_MODE
//
///*
//1、main thread: 1
//
//2、io_service 线程: cpu + 1
//
//3、io_service's timer: 1
//
//4、game_zone's io_service thread; zone num * 1;
//5、game_zone's io_service timer zone_num * 1;
//
//6、http_request 会动态启动线程，无法控制 += 2
//*/
//
//
//inline void enable_mem_leak_check()
//{
//#ifdef WIN32
//	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
//#endif
//}
//
//int main(int argc, char* argv[])
//{
//	enable_mem_leak_check();
//
//	{
//		dooqu_server::service::game_service service(8000);
//
//		for (int i = 0, j = 0, zone_id = 0; i < 30; i++)
//		{
//			char curr_game_id[30];
//			curr_game_id[sprintf(curr_game_id, "ddz_%d", i) + 1] = 0;
//
//			char curr_zone_id[30];
//			curr_zone_id[sprintf(curr_zone_id, "ddz_zone_%d", zone_id) + 1] = 0;
//
//			if ((++j % 60) == 0)
//			{
//				j = 0;
//				zone_id++;
//			}
//			dooqu_server::ddz::ddz_plugin* curr_plugin = new dooqu_server::ddz::ddz_plugin(&service, curr_game_id, curr_game_id, 2000, 100, 15, 21000);
//			service.create_plugin(curr_plugin, curr_zone_id);
//		}
//
//		//dooqu_server::plugins::ask_plugin* ask_plugin = new dooqu_server::plugins::ask_plugin(&service, "ask_0", "开心辞典");
//		//service.create_plugin(ask_plugin, "ask_zone_0");
//
//		service.start();
//		printf("please input the service command:\n");
//		string read_line;
//		std::getline(std::cin, read_line);
//
//
//		while (std::strcmp(read_line.c_str(), "exit") != 0)
//		{
//			if (std::strcmp(read_line.c_str(), "onlines") == 0)
//			{
//				printf("current onlines is:\n");
//				const std::map<const char*, dooqu_server::service::game_plugin*, dooqu_server::service::char_key_op>* plugins = service.get_plugins();
//
//				std::map<const char*, dooqu_server::service::game_plugin*, dooqu_server::service::char_key_op>::const_iterator curr_plugin = plugins->begin();
//
//				int online_total = 0;
//				while (curr_plugin != plugins->end())
//				{
//					online_total += curr_plugin->second->clients()->size();
//					printf("current game {%s}, onlines=%d\n", curr_plugin->second->game_id(), curr_plugin->second->clients()->size());
//					++curr_plugin;
//				}
//				printf("all plugins online total=%d\n", online_total);
//			}
//			else if (std::strcmp(read_line.c_str(), "io") == 0)
//			{
//				printf("press any key to exit i/o monitor mode:\n");
//				tcp_client::LOG_IO_DATA = true;
//				getline(std::cin, read_line);
//				tcp_client::LOG_IO_DATA = false;
//				printf("i/o monitor is stoped.\n");
//			}
//			else if (std::strcmp(read_line.c_str(), "timers") == 0)
//			{
//				printf("press any key to exit timers monitor mode:\n");
//				dooqu_server::service::game_zone::LOG_TIMERS_INFO = true;
//				getline(std::cin, read_line);
//				dooqu_server::service::game_zone::LOG_TIMERS_INFO = false;
//				printf("timers monitor is stoped.\n");
//			}
//			else if (std::strcmp(read_line.c_str(), "thread") == 0)
//			{
//				for (dooqu_server::net::thread_status_map::iterator curr_thread_pair = service.threads_status()->begin();
//					curr_thread_pair != service.threads_status()->end();
//					++curr_thread_pair)
//				{
//					std::cout << "thread id: "<< (*curr_thread_pair).first << " " <<   (*curr_thread_pair).second->elapsed() << std::endl;
//				}
//			}
//			else if (std::strcmp(read_line.c_str(), "stop") == 0)
//			{
//				service.stop();
//			}
//			else if (std::strcmp(read_line.c_str(), "unload") == 0)
//			{
//				service.stop();
//			}
//			else if (std::strcmp(read_line.c_str(), "update") == 0)
//			{
//				service.tick_count_threads();
//
//				std::this_thread::sleep_for(std::chrono::milliseconds(10));
//
//				for (dooqu_server::net::thread_status_map::iterator curr_thread_pair = service.threads_status()->begin();
//					curr_thread_pair != service.threads_status()->end();
//					++curr_thread_pair)
//				{
//					std::cout << "thread id " <<  (*curr_thread_pair).first << " " <<  (*curr_thread_pair).second->elapsed()<< std::endl;
//				}
//
//				for (std::map<std::thread::id, char*>::iterator e = thread_status::messages.begin();
//					e != thread_status::messages.end();
//					++e)
//				{
//					std::cout << "thread id " << (*e).first << " " << (*e).second<< std::endl;
//				}
//			}
//			cin.clear();
//			printf("please input the service command:\n");
//			std::getline(std::cin, read_line);
//		}
//	}
//
//	printf("server object is recyled, press any key to exit.");
//	getchar();
//
//	return 1;
//}
//
//
//
