#pragma once
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>
int setnonblocking( int fd );
