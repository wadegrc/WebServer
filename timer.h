#pragma once
#ifndef TIMER_H
/**
 * 计时器的实现
 * 通过优先队列构造小根堆计时器，每次循环完毕检查检查并删除超时链接
 * TimerNode：封装请求和时间
 * 1、构造节点
 * 2、删除节点
 * 3、设置删除标记，待超时后统一删除
 * 4、更新时间：每接受一次响应，都应该更新一次节点信息
 * TimerManager:管理时间节点,响应超时事件*/
#include"treatRequest.h"
#include<queue>
#include<deque>
#include<unistd.h>
#include<memory>
#include<pthread.h>
#include"locker.h"
using std::priority_queue;
using std::deque;
using std::shared_ptr;
class http_conn;
class TimerNode{
    typedef shared_ptr<http_conn> SP_request;
private:
    /*删除标记*/
    bool deleted;
    /*请求连接*/
    SP_request request;
    /*超时时间*/
    size_t expired_time;
public:
    inline bool isDeleted() const;
    bool isvalid();
    inline void setDeleted();
    inline size_t getExpiredTime() const;
    TimerNode(SP_request _request, int _expiredtime );
    ~TimerNode();
    void update(int _time);
    void clearReq();
};
struct timerCmp
{
    bool operator()(shared_ptr<TimerNode>&a, shared_ptr<TimerNode>&b)
    {
        return a->getExpiredTime() > b->getExpiredTime();
    }
};
class TimerManager
{
    typedef shared_ptr<http_conn> SP_request;
    typedef shared_ptr<TimerNode> SP_TimerNode;
private:
    priority_queue<SP_TimerNode,deque<SP_TimerNode>,timerCmp> TimerHeap;
    MutexLock lock;
public:
    TimerManager();
    ~TimerManager();
    void addTimer(SP_request request, int timeout);
    void addTimer(SP_TimerNode timernode);
    void hanlde_expired_event();
};
#endif
