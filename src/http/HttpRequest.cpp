#include "HttpRequest.hpp"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index","/welcome","/video"
};

const unordered_map<string,int> HttpRequest::DEFAULT_HTML_TAG{
};

HttpRequest::HttpRequest(){
    init();
}

void HttpRequest::init(){
    _method = _path = _version = _body = "";
    _parse_state = REQUEST_LINE;
    _headers.clear();
    _post.clear();
}

/*
    unordered_map为哈希表，元素可重复
*/
bool HttpRequest::is_keepalive() const{
    // 1.1才支持复用链接
    if(_headers.count("Connection") == 1){
        return (_headers.find("Connection")->second == "keep-alive") && (_version == "1.1");
    }
    return false;
}

/*
    parse自动机：
    1. 先解析第一行的请求行 request line
    2. 开始解析接下来若干行headers
        若缓冲区已无数据，说明没有后面的body，结束
    3. 若解析完headers还有数据，说明有body
        解析body，然后FINISH
*/

bool HttpRequest::parse(Buffer& buff){
    // 请求报文里面的那条空行
    const char CRLF[] = "\r\n";

    if(buff.get_readable_bytes()<=0){
        return false;
    }

    // 自动机
    while(buff.get_readable_bytes() && _parse_state!=FINISH){
        // std::search:寻找 seq2 在 seq1 中第一次出现的位置
        // 寻找匹配char数组CRLF所对应的\r\n出现的首位置
        // 即从开头到当前行结尾CRLF
        const char* line_end= search(buff.peek(),buff.peek()+buff.get_readable_bytes(),CRLF,CRLF+2);
        std::string currline(buff.peek(),line_end);

        switch (_parse_state){
        case REQUEST_LINE:
            //如果第一行都失败 直接寄
            if(!_parse_request_line(currline)){
                return false;
            }
            // 解析成功，把path拿了
            _parse_path();
            break;
        case HEADERS:
            _parse_header(currline);
            // 如果<=2 说明后面的所有行最多是一个CRLF 结束
            if(buff.get_readable_bytes()<=2){
                _parse_state = FINISH;
            }
            break;
        case BODY:
            _parse_body(currline);
            break;
        default:
            break;
        }

        // 如果已经读完
        // 并不会实际执行到这一条
        if(line_end == buff.peek()+buff.get_readable_bytes()){
            break;
        }
        // buff内部削减到CRLF+2 即跳过\r\n到下一行
        buff.retrieve_until(line_end+2);
    }
    return true;
}

// 正则表达式解析请求行
bool HttpRequest::_parse_request_line(const std::string& line){
    // [^ ]表示非空格字符
    // 翻译为： 行首的非空格字符串 空格 非空格字符串 HTTP/末尾非空格字符串
    // example:
    // GET /562f25980001b1b106000338.jpg HTTP/1.1
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch sub_match;
    if(regex_match(line,sub_match,patten)){
        _method = sub_match[1];
        _path = sub_match[2];
        _version = sub_match[3];
        // 请求行结束，进入headers解析
        _parse_state = HEADERS;
        return true;
    }
    return false;
}

// 当读到空行CRLF时匹配失败 进入BODY
void HttpRequest::_parse_header(const std::string& line){
    // ^前插表达式表示在行首 $尾插表示在行尾
    // [^:] 表示非:字符，[^:]*表示任意非:字符
    //     ?表示可以出现的空格
    // (.*)表示任意长度的任意字符
    
    // 例：
    // Connection: keep-alive
    regex patten("^([^:]*): ?(.*)$");
    smatch sub_match;
    if(regex_match(line,sub_match,patten)){
        _headers[sub_match[1]] = sub_match[2];
    }else{
        // 若匹配失败，说明header解析完毕 该解析BODY了
        _parse_state = BODY;
    }
}

// 有body则进行post解析
void HttpRequest::_parse_body(const std::string& line){
    // body内容自成一整行，直到遇到最后面表文结束时跟着的CRLF
    _body = line;
    _parse_post();
    _parse_state = FINISH;
}

// 在解析请求行时正则表达式匹配到_path 可能带后缀也可能不带
// 对于默认的html页面，后面也许没加html 给加上
// 如果路径没带.html这样的后缀，给加上
void HttpRequest::_parse_path(){
    // 默认走/index.html
    if(_path == "/"){
        _path = "/index.html";
    }else{
        for(auto& htmlpage:DEFAULT_HTML){
            if(htmlpage == _path){
                _path += ".html";
                break;
            }
        }
    }
}

// 编码格式会在header Content-Type中给出
void HttpRequest::_parse_post(){
    if(_method == "POST" && _headers["Content-Type"] == "application/x-www-form-urlencoded"){
        _parsefrom_url_encoded();
        if(DEFAULT_HTML_TAG.count(_path)){
            //... 此处可以做tag处理
        }
    }
    // 如果不是POST那啥也不干
}

// 编码转换 主要改body内容
// url编码
void HttpRequest::_parsefrom_url_encoded(){
    if(_body.size()==0){
        return;
    }
    string key,value;
    int num=0;
    int bodysize = _body.size();
    int i=0,j=0;
    for(; i < bodysize; i++) {
        char ch = _body[i];
        switch (ch) {
        case '=':
            key = _body.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            _body[i] = ' ';
            break;
        case '%':
            num = conver_hex(_body[i + 1]) * 16 + conver_hex(_body[i + 2]);
            _body[i] = num;
            // 如#在服务端为%23 理论上解析为十进制35 对应字符#
            // 去掉后两个
            _body.erase(_body.begin()+i+1);
            _body.erase(_body.begin()+i+1);
            bodysize-=2;
            // _body[i + 2] = num % 10 + '0';
            // _body[i + 1] = num / 10 + '0';
            // i += 2;
            break;
        case '&':
            value = _body.substr(j, i - j);
            j = i + 1;
            _post[key] = value;
            break;
        default:
            break;
        }
    }

    if(_post.count(key) == 0 && j < i) {
        value = _body.substr(j, i - j);
        _post[key] = value;
    }
}

// 16转10
int HttpRequest::conver_hex(char ch){
    unsigned int y;
    if (ch >= 'A' && ch <= 'F') {
    y = ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'f') {
    y = ch - 'a' + 10;
    } else if (ch >= '0' && ch <= '9') {
    y = ch - '0';
    }else{
        perror("conver_hex");
    }
    return y;
}

// public get methods:

std::string HttpRequest::path() const{
    return _path;
}
std::string& HttpRequest::path(){
    return _path;
}
std::string HttpRequest::method() const{
    return _method;
}
std::string HttpRequest::version() const{
    return _version;
}
std::string HttpRequest::GetPost(const std::string& key) const{
    if(key == ""){
        return "";
    }
    if(_post.count(key) == 1){
        return _post.find(key)->second;
    }
    return "";
}
std::string HttpRequest::GetPost(const char* key) const{
    if(key == ""){
        return "";
    }
    if(_post.count(key) == 1){
        return _post.find(key)->second;
    }
    return "";
}