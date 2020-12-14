#include"treatRequest.h"
using std::string;
/*定义http响应的一些状态信息*/
const string ok_200_title = "OK";
const string error_400_title = "Bad Request";
const string error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const string error_403_title = " Forbidden";
const string error_403_form = "You do not have permission to get file from this server.\n";
const string error_404_title = "Not Found";
const string error_404_form = "The requested file was not found on this server.\n";
const string error_500_title = "Internal Error";
const string error_500_form = "There was an unusual problem serving the requested file.\n";

/*网站根目录*/
const string doc_root = "/var/www/html";

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn( bool real_close )
{
    if( real_close && ( m_sockfd != -1 ) )
    {
        Epoll::epoll_del( m_sockfd );
        m_sockfd = -1;
        m_user_count--;/*关闭一个连接时，客户数量减一*/
        /*关闭定时器*/
        if(timer.lock())//尝试提升为shared_ptr
        {
            shared_ptr<TimerNode>my_timer(timer.lock());
            my_timer->clearReq();
            timer.reset();
        }
    }
}
void http_conn::linkTimer(shared_ptr<TimerNode>mtimer)
{
    timer = mtimer;
}
void http_conn::seperateTimer()
{
    if(timer.lock())
    {
        shared_ptr<TimerNode>my_timer(timer.lock());
        my_timer->clearReq();
        timer.reset();
    }
}
void http_conn::init( int sockfd )
{
    m_sockfd = sockfd;
}
void http_conn::init( int sockfd, const sockaddr_in& addr )
{
    m_sockfd = sockfd;
    m_address = addr;

    m_user_count++;
    
    init();
}
void http_conn::init()
{

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = "";
    m_version = "";
    m_host = "";
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset( m_read_buf, '\0', READ_BUFFER_SIZE );
    memset( m_write_buf, '\0', WRITE_BUFFER_SIZE );
}

/*从状态*/
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx )
    {
        temp = m_read_buf[ m_checked_idx ];
        request_line.push_back(temp);
        if( temp == '\r' )
        {
            if( ( m_checked_idx + 1 ) == m_read_idx )
            {
                return LINE_OPEN;
            }
            else if( m_read_buf[ m_checked_idx + 1 ] == '\n' )
            {
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if( temp == '\n' ){
            if( (m_checked_idx > 1 ) && ( m_read_buf[ m_checked_idx - 1 ] == '\r' ) )
            {
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
/*循环读取客户数据，直到无数据刻度或者对方关闭连接*/
bool http_conn::read()
{
    if( m_read_idx >= READ_BUFFER_SIZE )/*读缓存已满*/
    {
        return false;
    }
    int bytes_read = 0;
    while( true )
    {
        bytes_read = recv( m_sockfd, m_read_buf + m_read_idx,
                READ_BUFFER_SIZE - m_read_idx, 0 );
        if( bytes_read == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                break;
            }
            return false;
        }
        else if( bytes_read == 0 )
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

/*解析HTTP请求行，获得请求方法，目标URL，以及HTTP版本号*/
http_conn::HTTP_CODE http_conn::parse_request_line()
{
    int pos = request_line.find("POST");
    if( pos < 0 )
    {
        pos = request_line.find("GET");
        if( pos < 0 )
        {
            return BAD_REQUEST;
        }
        else{
            m_method = GET;
        }
    }
    else{
        m_method = POST;
    }
    pos = request_line.find(" ");
    int _pos = request_line.find(" ", pos );
    m_url = request_line.substr(pos+1, _pos-pos-1 );
    pos = request_line.find("HTTP/1.1");
    if( pos < 0 )
    {
        pos = request_line.find("HTTP/1.0");
        if( pos < 0 )
        {
            return BAD_REQUEST;
        }
        else
        {
           m_version = "HTTP/1.0";
        }
    }
    else 
    {
        m_version = "HTTP/1.1";
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*解析HTTP请求的一个头部信息*/
http_conn::HTTP_CODE http_conn::parse_headers()
{
    /*遇到空行表示头部字段解析完毕*/
    if( request_line[0] == '\0' )
    {
        /*如果HTTP请求还有消息体，则还需要读取m_content_length字节的消息体
         * 状态机转移到CHECK_STATE_CONTENT状态*/
        if( m_content_length != 0 )
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        /*否则说明我们已经得到了一个完整的HTTP请求*/
        return GET_REQUEST;
    }
    /*处理Connection头部字段*/
    else if( request_line.find("Connection:") >= 0 )
    {
        string head_right = request_line.substr(11);
        if(head_right[ head_right.size() - 2 ] == '\r' )/*去除换行符*/
        {
            head_right[ head_right.size() - 2 ] = '\0';
        }
        request_head["Connection"] = head_right;
        if( head_right == "keep-alive " )
        {
            m_linger  = true;
        }
    }
    /*处理Content-Length字段*/
    else if( request_line.find("Content-Length:", 15 ) >= 0 )
    {
        m_content_length = stoi(request_line, (std::size_t*)15);/*将从15位置开始的字符串转整型*/
    }
    /*处理HOST字段*/
    else if( request_line.find("Host:") >= 0 )
    {
        m_host = request_line.substr(5);
    }
    else
    {
        printf( "oop! unknow header %s\n", request_line.c_str()  );
    }

    return NO_REQUEST;
}

/*我们没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了*/
http_conn::HTTP_CODE http_conn::parse_content()
{
    if( m_read_idx >= ( m_content_length + m_checked_idx ) )
    {
        request_line[ m_content_length ] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
/*主状态机*/
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    while( ((m_check_state == CHECK_STATE_CONTENT ) && ( line_status == LINE_OK ) )
            || (( line_status = parse_line() ) == LINE_OK ) )
    {
        switch ( m_check_state )
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line();
                request_line = "";
                if( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers();
                request_line = "";
                if( ret == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                else if( ret == GET_REQUEST )
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content();
                if( ret == GET_REQUEST )
                {
                    request_line = "";
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}
/*当得到一个完整的/正确的HTTP请求时，我们就分析目标文件的属性，如果目标文件存在、对所有用户可读
 * 且不是目录。则使用mmap将其映射到内存地址m_file_address处，并告诉调用者获取文件成功*/

http_conn::HTTP_CODE http_conn::do_request()
{
    string curr_file_name = doc_root + m_url;
    const char*m_real_file = curr_file_name.c_str();
    if( stat( m_real_file, &m_file_stat ) < 0 )
    {
        return NO_REQUEST;
    }

    if( ! ( m_file_stat.st_mode & S_IROTH ) )
    {
        return FORBIDDEN_REQUEST;
    }

    if( S_ISDIR( m_file_stat.st_mode ) )
    {
        return BAD_REQUEST;
    }
    int fd = open( m_real_file, O_RDONLY );

    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ,
                                    MAP_PRIVATE, fd, 0 );
    close(fd);
    return FILE_REQUEST;
}

/*对内存映射区执行那个munmap操作*/
void http_conn::unmap()
{
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

/*写HTTP响应*/
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if( bytes_to_send == 0 )
    {
        Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLIN );
        init();
        return true;
    }

    while(1)
    {
        temp = writev( m_sockfd, m_iv, m_iv_count );
        if( temp <= -1 )
        {
            /*如果TCP写缓存没有空间，则等待下一轮EPOLLOUT事件。虽然在此期间，
             * 服务器无法立即收到同一个客户的下一个请求，但这可以保证连接的完整性*/
            if( errno == EAGAIN )
            {
                Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLOUT );    
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if( bytes_to_send <= bytes_have_send )
        {
            /*发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接*/
            unmap();
            if( m_linger )
            {
                init();
                Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLIN );
                return true;
            }
            else
            {
                Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLIN );
                
                return false;
            }
        }
    }
}

/*往写缓存中写入待发送的数据*/
bool http_conn::add_response( const char* format, ... )
{
    if( m_write_idx >=  WRITE_BUFFER_SIZE )
    {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,
                         format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) )
    {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool http_conn::add_status_line( int status, string title )
{
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title.c_str() );
}

bool http_conn::add_headers( int content_len )
{
    add_content_length( content_len );
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length( int content_len )
{
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", (m_linger == true ) ?
                         "keep-alive" : "close" );
}

bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool http_conn::add_content( string content )
{
    return add_response( "%s", content.c_str() );
}

/*根据服务器处理HTTP请求的结果，决定返回给客户端的内容*/
bool http_conn::process_write( HTTP_CODE ret )
{
    switch ( ret )
    {
    case INTERNAL_ERROR:
        {
            add_status_line( 500, error_500_title );
            add_headers(  error_500_form.size() );
            if( ! add_content( error_500_form ) )
            {
                return false;
            }
            break;
        }
    case BAD_REQUEST:
        {
            add_status_line( 400, error_400_title );
            add_headers( error_400_form.size() );
            if( !add_content( error_400_form ) )
            {
                return false;
            }
            break;
        }
    case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_headers( error_404_form.size() );
            if( ! add_content( error_404_form ) )
            {
                return false;
            }
            break;
        }
    case FORBIDDEN_REQUEST:
        {
            add_status_line( 403, error_403_title );
            add_headers( error_403_form.size() );
            if( !add_content( error_403_form ) )
            {
                return false;
            }
            break;
        }
    case FILE_REQUEST:
        {
            add_status_line( 200, ok_200_title );
            if( m_file_stat.st_size != 0 )
            {
                add_headers( m_file_stat.st_size );
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body>OK!</body></html>";
                add_headers( strlen( ok_string ) );
                if( ! add_content( ok_string ) )
                {
                    return false;
                }
            }
        }
    default:
        {
            return false;
        }
    }
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/*由线程池中的工作线程调用，这是处理HTTP请求的入口函数*/
void http_conn::process()
{   
    if(!read())
    {
        close_conn();
        return ;
    }
    HTTP_CODE read_ret = process_read();
    if( read_ret == NO_REQUEST )
    {
        Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLIN );
        return;
    }

    bool write_ret = process_write( read_ret );
    if( !write_ret )
    {
        close_conn();
        return ;
    }
    Epoll::add_Timer( shared_from_this(),500);
    int ret=Epoll::epoll_mod( m_sockfd,shared_from_this(), EPOLLIN );
    if(ret<0)
    {
        return;
    }
}
