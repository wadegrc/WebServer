#include <iostream>
#include"epoll.h"
#include"util.h"
#include"ThreadPool.h"
#include<sys/epoll.h>
#include<errno.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<deque>
using std::vector;
using std::unordered_map;
using std::shared_ptr;

int TIMER_TIME_OUT = 500;

epoll_event*Epoll::events;
unordered_map<int,shared_ptr<http_conn>>Epoll::fd2req;
int Epoll::epoll_fd = 0;

TimerManager Epoll::timer_manager;

int Epoll::epoll_init(int maxevents, int listen_num)
{
    epoll_fd = epoll_create(listen_num+1);
    if(epoll_fd == -1)
    {
        return -1;
    }
    events = new epoll_event[maxevents];
    return 0;
}

//注册新描述符
int Epoll::epoll_add(int fd, SP_ReqData request, int events)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events;

    if(epoll_ctl( epoll_fd,EPOLL_CTL_ADD,fd,&event )<0)
    {
        perror("epoll_add error!");
        return -1;
    }
    fd2req[fd]=request;
    return 0;
}

//修改描述符状态
int Epoll::epoll_mod( int fd, SP_ReqData request, int events )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    if( epoll_ctl( epoll_fd, EPOLL_CTL_MOD, fd, &event ) < 0 )
    {
        perror("epoll_mod error!");
        return -1;
    }
    fd2req[fd] = request;
    return 0;
}

//从epoll中删除文件描述符
int Epoll::epoll_del( int fd )
{
    if(epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, 0 ) < 0 )
    {
        perror("epoll_del error!");
        return -1;
    }
    if(fd2req.count(fd)!=0)
    {
        fd2req.erase(fd);
    }
    close(fd);
    return 0;
}

void Epoll::my_epoll_wait( int listenfd, int max_events, int timeout ,
                           std::shared_ptr<threadpool<http_conn>>thread_pool)
{
    int event_count = epoll_wait(epoll_fd, events, max_events, timeout);
    if(event_count<0)
    {
        perror("epoll wait error!");
    }
    vector<SP_ReqData>req_data = getEventRequest(listenfd, event_count);
    if(req_data.size() > 0)
    {
        for(auto &req:req_data)
        {
            if(thread_pool->thread_add(req)<0){
                break;
            }
        }
    }
}

//接受链接
void Epoll::acceptConn(int listen_fd)
{
    printf("acceptConn\n");
    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(struct sockaddr_in));

    socklen_t client_addr_len = sizeof(client_addr);

    int accept_fd = 0;
    while((accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr,&client_addr_len))>0)
    {
        printf("%s\n", inet_ntoa(client_addr.sin_addr));
        printf("%d\n", ntohs(client_addr.sin_port));
        printf("%d\n",accept_fd);
        //设为非阻塞模式
        int ret = setnonblocking(accept_fd);
        if(ret<0)
        {
            perror("Set nonblocking failed!");
            return ;
        }
        SP_ReqData req_info(new http_conn());
        req_info->init(accept_fd,client_addr);
        int _epo_event = EPOLLIN|EPOLLET|EPOLLONESHOT;
        Epoll::epoll_add(accept_fd, req_info, _epo_event);
        //加上时间信息
        timer_manager.addTimer(req_info,TIMER_TIME_OUT);
    }
}

//获取响应fd
vector<std::shared_ptr<http_conn>>Epoll::getEventRequest(int listen_fd, int events_num)
{
    printf("getEventRequest\n");
    std::vector<SP_ReqData>req_data;
    for(int i=0;i<events_num;++i)
    {
        //获取有事件产生的描述符
        int fd = events[i].data.fd;

        //若描述符为监听描述符
        if(fd==listen_fd)
        {
            acceptConn(listen_fd);
        }
        else if(fd<3)//标准输入输出
        {
            break;
        }
        else
        {
            if((events[i].events&EPOLLERR)||(events[i].events&EPOLLHUP)
               ||(!(events[i].events&EPOLLIN)))
            {
                if(fd2req.count(fd)!=0)
                {
                    fd2req.erase(fd);
                }
                continue;
            }

             /**
             * 将请求加入线程
             * 加入线程之前将计时器与request分离，处理完后加入新的timer*/
             SP_ReqData cur_req(fd2req[fd]);
             cur_req->seperateTimer();
             req_data.push_back(cur_req);
             if(fd2req.count(fd)!=0)
             {   
                 fd2req.erase(fd);
             }
             
         }
    }
    return req_data;

}

void Epoll::add_Timer(std::shared_ptr<http_conn>request_data, int timeout)
{
    timer_manager.addTimer(request_data,timeout);
}
