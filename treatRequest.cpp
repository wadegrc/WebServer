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
const string doc_root = "/var/www";

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn( bool real_close )
{
    if( real_close && ( m_sockfd != -1 ) )
    {
        removefd( m_epollfd, m_sockfd );
        m_sockfd = -1;
        m_user_count--;/*关闭一个连接时，客户数量减一*/
    }
}

void http_conn::init( int sockfd, const sockaddr_in& addr )
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd( m_epollfd, sockfd, true );
    m_user_count++;
    
    init();
}

void http_conn::init()
{

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
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
        request_line.emplace_back(temp);
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

