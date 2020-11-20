#ifndef TREATREQUEST_H
#define TREATREQUEST_H
/*
 *主要实现对Http请求的分析
 *通过自动机来处理请求
 * */
#include<unordered_map>
#include<unistd.h>
#include<string>

class http_conn
{
public:
    enum METHOD{ /*请求方法*/
        GET = 0, POST, HEAD, PUT, DELETE,
        TRACE, OPTIONS, CONNECT, PATCH };
    enum CHECK_STATE {/*解析客户请求时，主状态机所处的状态*/
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT };
    enum HTTP_CODE {/*服务器处理HTTP请求的可能结果*/
        NO_REQUEST, GET_REQUEST, BAD_REQUEST,
        NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
        INTERNAL_ERROR, CLOSED_CONNECTION };
    enum LINE_STATUS {/*行的读取状态*/
        LINE_OK = 0, LINE_BAD, LINE_OPEN };
    static const int READ_BUFFER_SIZE = 2048;/*读缓冲区大小*/
    static const int WRITE_BUFFER_SIZE = 1024;/*写缓冲区大小*/

public:
    http_conn(){};
    ~http_conn(){};
public:
    /*初始化新接受的连接*/
    void init( int sockfd, const sockaddr_in& addr );
    /*关闭连接*/
    void close_conn( bool real_close = true );
    /*处理客户请求*/
    void process();
    /*非阻塞写操作*/
    void wirte();
    /*非阻塞读操作*/
    void read();
private:
    /*初始化连接*/
    void init();
    /*分析请求的函数*/
    void process_read();
    /*请求行*/
    HTTP_CODE parse_request_line();
    /*读取报文中的没一行*/
    LINE_STATUS parse_line();
    /*分析头部*/
    HTTP_CODE parse_header();
    /*分析正文*/
    HTTP_CODE parse_content();
    /*处理请求*/
    HTTP_CODE do_request();

    /*做出应答*/
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();
public:
    /*所有的socket事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的*/
    static int m_epollfd;
    /*统计用户数量*/
    static int m_user_count;
private:
    /*该HTTP连接的socket和对方的socket地址*/
    int m_sockfd;
    sockaddr_in m_address;

    /*读缓冲区*/
    char m_read_buf[ READ_BUFFER_SIZE ];
    /*标识该缓冲区已经读入的客户数据的最后一个字节的下一个位置*/
    int m_read_idx;
    /*当前正在分析的字符在读缓冲区的位置*/
    int m_checked_idx;
    /*当前正在解析的行的起始位置*/
    int m_start_line;
    /*写缓冲区*/
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    /*写缓冲区中待发送的字节数*/
    int m_write_idx;

    /*主状态机所处的状态*/
    CHECK_STATE m_check_state;
    /*请求方法*/
    METHOD m_method;
    std::string file_name;/*用户请求的文件名*/
    std::unordered_map<std::string,std::string>request_head;/*请求头部内容*/
    std::string request_line = "";/*报文中的每一行*/
    std::string version;
    /*主机名*/
    std::string m_host;
    /*HTTP请求的消息体*/
    std::string content;

    /*HTTP请求是否要求保持连接*/
    bool m_linger;

    /*客户请求的目标文件被mmap到内存中的起始位置*/
    char* m_file_address;

    /*目标文件的状态。是否存在，是否为目录，是否可读，并获取文件大小等消息*/
    struct stat m_file_stat;
    
    /*我们将采用writev来执行写操作，其中m_iv_count表示被写内存块的数量*/
    struct iovec m_iv[2];
    int m_iv_count;
};
#endif
