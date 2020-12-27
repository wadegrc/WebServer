#include"ThreadPool.h"
#include<memory>

static int MAX_THREAD_NUM = 8;
static int MAX_REQUESTS = 10000;
template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) :
    m_thread_number(thread_number),
    m_max_requests(max_requests),threads(thread_number),m_stop(false)
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
bool threadpool<T>::thread_add(T request )
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
        T request = taskqueue.front();
        taskqueue.pop();
        pthread_mutex_unlock(&lock);
        if( !request )
        {
            continue;
        }
        request->process();
    }
}
template class threadpool<shared_ptr<http_conn>>;
