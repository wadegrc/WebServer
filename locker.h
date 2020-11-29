#pragma once
#ifndef LOCKER_H
#define LOCKER_H
#include<boost/noncopyable.hpp>
#include<pthread.h>
class MutexLock : boost::noncopyable
{
private:
    pthread_mutex_t mutex_;
public:
    MutexLock(){
        pthread_mutex_init(&mutex_, NULL);
    }
    ~MutexLock(){
        pthread_mutex_destroy(&mutex_);
    }
    void lock()
    {
        pthread_mutex_lock(&mutex_);
    }
    void unlock(){
        pthread_mutex_unlock(&mutex_);
    }

    pthread_mutex_t* getPthreadMutex()
    {
        return &mutex_;
    }
};

class MutexLockGuard : boost::noncopyable
{
public:
    explicit MutexLockGuard(MutexLock& mutex) : mutex_(mutex)
    {
        mutex_.lock();
    }
    ~MutexLockGuard()
    {
        mutex_.unlock();
    }
private: 
    MutexLock& mutex_;
};
#endif

