#ifndef __HTTP_REQUEST__
#define __HTTP_REQUEST__

#include<string>
#include<regex>
#include<unordered_map>
#include<unordered_set>

#include "../buffer/Buffer.hpp"

class HttpRequest{
    friend class HttpConnection;

public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };

    // HTTP的状态码
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
private:

    // 请求头: 方法 路径 版本
    // GET /562f25980001b1b106000338.jpg HTTP/1.1
    std::string _method;
    std::string _path;
    std::string _version;

    // 若干行header
    std::unordered_map<std::string,std::string> _headers;
    // 请求体 POST用
    std::string _body;
    std::unordered_map<std::string,std::string> _post;

    // html页面
    static const std::unordered_set<std::string> DEFAULT_HTML;

    // TAG标签 用于对某些页面做特殊处理
    // 比如根据所请求path的对应tag来决定是否需要跳转登录页面等.
    static const std::unordered_map<std::string,int> DEFAULT_HTML_TAG;

    // 报文解析
    PARSE_STATE _parse_state;
    bool _parse_request_line(const std::string& line);
    void _parse_header(const std::string& line);
    void _parse_body(const std::string& line);
    void _parse_path();
    void _parse_post();
    void _parsefrom_url_encoded();

    static int conver_hex(char c);

public:
    HttpRequest();
    ~HttpRequest() = default;

    void init();

    bool parse(Buffer& buff);
    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    // 表示是否复用TCP连接
    bool is_keepalive() const;
};

#endif // __HTTP_REQUEST__