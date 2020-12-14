#include <iostream>
#include<sys/time.h>
#include<string>
#include"timer.h"
#include"epoll.h"
using namespace std;
TimerNode::TimerNode(SP_request _request, int timeout):deleted(false),request(_request)
{
    struct timeval now;
    gettimeofday(&now,NULL);
    //以毫秒记
    expired_time = ((now.tv_sec*1000)+(now.tv_usec/1000)) + timeout;
}

TimerNode::~TimerNode()
{
    /*临时变量request*/
    if(request)
    {
        Epoll::epoll_del(request->getFd());
    }
}

void TimerNode::update( int timeout )
{
    struct timeval now;
    gettimeofday(&now,NULL);
    expired_time = ((now.tv_sec*1000)+(now.tv_usec/1000)) + timeout;  
}
/*判断定时器是否超时*/
bool TimerNode::isvalid()
{
    struct timeval now;
    gettimeofday(&now,NULL);
    size_t curr = ((now.tv_sec*1000)+(now.tv_usec/1000));
    if(curr<expired_time)
    {
        return true;
    }
    else
    {
        this->setDeleted();
        return false;
    }
}
/*清理请求*/
void TimerNode::clearReq()
{
    request->close_conn(true);
    this->setDeleted();
}

inline void TimerNode::setDeleted()
{
    deleted = true;
}

inline bool TimerNode::isDeleted() const
{
    return deleted;
}

inline size_t TimerNode::getExpiredTime()const
{
    return expired_time;    
}

TimerManager::TimerManager()
{

}
TimerManager::~TimerManager()
{

}

void TimerManager::addTimer(SP_request _request, int timeout)
{
    SP_TimerNode new_node(new TimerNode(_request,timeout));
    {
        MutexLockGuard locker(lock);
        TimerHeap.push(new_node);
    }
    _request->linkTimer(new_node);//TODO
}

void TimerManager::addTimer(SP_TimerNode timer_node)
{

}

void TimerManager::hanlde_expired_event()
{
    MutexLockGuard locker(lock);
    while(!TimerHeap.empty())
    {
        SP_TimerNode curr_node = TimerHeap.top();
        if(curr_node->isDeleted())
        {
            TimerHeap.pop();
        }
        else if(curr_node->isvalid()==false)
        {
            TimerHeap.pop();
        }
        else{
            break;
        }
    }
}

