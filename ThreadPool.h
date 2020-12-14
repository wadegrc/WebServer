#pragma once
#include<pthread.h>
#include<queue>
#include<cstdio>
#include"locker.h"
#include<vector>
#include<exception>
#include<memory>
/* *
 * 此处为线程池头文件
 * 实现:
 * 通过预先创建指定数量的线程，并使用条件变量进行阻塞，
 * 再创建任务队列来存储任务和分配任务。
 * */
using std::vector;
using std::queue;
using std::exception;
static int MAX_THREAD_NUM = 8;
static int MAX_REQUESTS = 10000;
/*代码复用采用模板类*/
template< typename T >
class threadpool{
private:
    /*工作线程数量*/
    int m_thread_number;
    /*最大请求数量*/
    int m_max_requests;
    /*线程数组*/
    vector<pthread_t>threads;
    /*工作队列*/
    queue<std::shared_ptr<T>>taskqueue;
    /*互斥锁*/
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    /*条件变量*/
    pthread_cond_t notify = PTHREAD_COND_INITIALIZER;
    /*是否关闭线程池*/
    bool m_stop;
public:
    threadpool( int thread_number, int max_requests );
    ~threadpool();
    int thread_destroy();
    bool thread_add(std::shared_ptr<T> request);
    static void* worker( void* arg );
    void run();
};

template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) :
    threads(thread_number),m_stop(false),
    m_thread_number(thread_number),m_max_requests(max_requests)
{
    if((thread_number<=0)||(thread_number>MAX_THREAD_NUM)||
       (max_requests<=0)||(max_requests>MAX_REQUESTS)){
        threads.resize(MAX_THREAD_NUM);
        m_thread_number = thread_number;
        m_max_requests = max_requests;
    }
    /*创建thread_number个线程，并将他们都设置为脱离线程*/
    /*脱离线程:执行完后自动回收资源*/
    for(int i = 0; i < m_thread_number; i++)
    {
        printf("create the %dth thread\n", i );
        if( pthread_create( &threads[i], NULL, worker, this) !=0 )
        {
            threads.clear();
            throw exception();
        }
        /*脱离线程*/
        if( pthread_detach( threads[i] ) )
        {
            threads.clear();
            throw exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool()
{
    threads.clear();
    m_stop = true;
}


template< typename T >
bool threadpool<T>::thread_add( std::shared_ptr<T> request )
{
    pthread_mutex_lock(&lock);
    if( taskqueue.size() > m_max_requests )
    {
        pthread_mutex_unlock(&lock);
        return false;
    }
    taskqueue.push( request );
    
    if(pthread_cond_signal(&notify)!=0)
    {
        pthread_mutex_unlock(&lock);
        return false;
    }
    pthread_mutex_unlock(&lock);
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run()
{
    /*因为为脱离线程，当m_stop为true时，函数执行完毕后会自动回收资源*/
    while( ! m_stop )
    {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&notify,&lock);
        if( taskqueue.empty() )
        {
            pthread_mutex_lock(&lock);
            continue;
        }
        T* request = taskqueue.front();
        taskqueue.pop();
        pthread_mutex_unlock(&lock);
        if( !request )
        {
            continue;
        }
        request->process();
    }
}

