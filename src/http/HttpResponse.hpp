#ifndef __HTTP_RESPONSE__
#define __HTTP_RESPONSE__

#include"../buffer/Buffer.hpp"

#include <unordered_map>
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/stat.h>   // stat
#include <sys/mman.h>   // mmap,munmap

/*
    响应报文:
    1. 状态行 包括版本，状态码，状态信息
    2. headers 和请求报文一样
    3. 空行
    4. 响应正文 返回数据

eg:
    HTTP/1.1 200 OK
    Date: ....
    Content_Type: text/html;charset = ISO-8859-1
    Content-Length: 122
    \s\n
    ...
*/

class HttpResponse{
friend class HttpConnection;
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& src_dir,std::string& path,
                bool is_keepalive = false,int code = -1);
                
    void check_code(Buffer& buff,bool is_range =false);

    void unmap_file();
    char* file();
    size_t file_length() const;

    // 如果出现错误 构造一个临时的小html作为错误页面
    // 可能给400 BAD_ERQUEST
    void error_content(Buffer&buff,std::string message);

private:
    void _add_stateline(Buffer& buff);
    void _add_basic_header(Buffer& buff);
    void _add_range_header(Buffer& buff,size_t start,size_t end);
    bool _load_content(Buffer& buff);
    void _add_contentlen_header(Buffer& buff,size_t size);
    void _add_endline(Buffer& buff);

    void _checkcode_errorhtml();
    std::string _getfile_type();

    // 响应码
    int _code;
    bool _is_keepalive;

    std::string _path;
    std::string _srcdir;

    // file loaded into the memory
    // here is the address
    char* _mmfile;
    // state: st_mode和st_size
    struct stat _file_state;

    // 后缀与目录对应
    static const std::unordered_map<std::string,std::string> SUFFIX_TYPE;
    // 状态码与含义
    static const std::unordered_map<int,std::string> CODE_STATUS;
    // 状态码与相应页面
    static const std::unordered_map<int,std::string> CODE_PATH;
};

#endif  //__HTTP_RESPONSE__