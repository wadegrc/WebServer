#pragma once
#include"treatRequest.h"
#include"timer.h"
#include<vector>
#include<unordered_map>
#include<sys/epoll.h>
#include<memory>
#include"ThreadPool.h"
using std::vector;
using std::shared_ptr;
using std::unordered_map;
class http_conn;
class TimerManager;
class Epoll
{
public:
    typedef shared_ptr<http_conn> SP_ReqData;
public:
    Epoll() {}
    ~Epoll() {}
public:
    static int epoll_init( int maxevents,int listen_num );
    static int epoll_add(int fd, SP_ReqData request,int ev );
    static int epoll_mod(int fd, SP_ReqData request, int ev );
    static int epoll_del( int fd );
    static void my_epoll_wait(int listen_fd, int max_events, int timeout ,
                              std::shared_ptr<threadpool<http_conn>>thread_pool);
    static void acceptConn(int listen_fd );
    static vector<SP_ReqData>getEventRequest( int listen_fd, int events_num );
    static void add_Timer(std::shared_ptr<http_conn> request_data, int timeout);
private:
    static epoll_event* events;
    static unordered_map<int,SP_ReqData>fd2req;
    static int epoll_fd;
    static TimerManager timer_manager;
};

