#include <iostream>
#include"treatRequest.h"
#include"epoll.h"
#include"ThreadPool.h"
#include"util.h"

#include<sys/epoll.h>
#include<queue>
#include<sys/time.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdio.h>
#include<string.h>
#include<cstdlib>
#include<iostream>
#include<vector>
#include<unistd.h>
#include<memory>
using namespace std;


static const int MAXEVENTS = 5000;
static const int LISTENQ = 1024;
const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65535;

const int PORT = 8888;
const int TIMER_TIME_OUT = 500;
/*创建监听socket*/
int socket_bind_listen(int port)
{
    //检查port值，取正确的区间范围
    if(port < 1024 || port > 65535)
        return -1;

    //创建socket(IPV4+TCP)，返回监听描述符
    int listen_fd = 0;
    if((listen_fd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        return -1;
    }

    //消除bind时Address already int use 错误
    int optval = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, 
                  sizeof(optval))==-1)
        return -1;
    //设置服务器IP和Port，和监听符绑定
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        return -1;
    }

    //开始监听，最大等待队列为LISTENQ
    if(listen(listen_fd, LISTENQ) == -1)
        return -1;
    //无效监听描述符
    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

void addsig( int sig, void( handler )(int), bool restart = true )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
}
int main()
{
#ifdef _PTHREADS 
    printf("_PTHREADS is not defined!\n");
#endif
    addsig( SIGPIPE, SIG_IGN );
    if(Epoll::epoll_init(MAXEVENTS, LISTENQ)<0)
    {
        perror("epoll init failed");
        return 1;
    }
    /*创建线程池*/
    threadpool<http_conn>* pool =NULL;
    try
    {
        pool = new threadpool< http_conn >(THREADPOOL_THREAD_NUM,
                                           QUEUE_SIZE);
    }
    catch(...)
    {
        return 1;
    }
    
    int listen_fd = socket_bind_listen(PORT);

    if(listen_fd < 0)
    {
        perror("socket bind failed");
        return 1;
    }
    
    if(setnonblocking(listen_fd)<0)
    {
        perror("set socket non block failed");
        return 1;
    }

    shared_ptr<http_conn>request(new http_conn());

    request->init(listen_fd);

    if(Epoll::epoll_add(listen_fd,request,EPOLLIN | EPOLLET)<0)
    {
        perror("epoll add error");
        return 1;
    }

    while(true)
    {
        Epoll::my_epoll_wait(listen_fd,MAXEVENTS,-1);
    }
    return 0;
}
