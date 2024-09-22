#include"HttpResponse.hpp"
#include <string>

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css"},
    { ".js",    "text/javascript"},
    { ".mp4",   "video/mp4"}
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 206, "Partial Content"}
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};


HttpResponse::HttpResponse(){
    _code = -1;
    _path = "";
    _srcdir = "";
    _mmfile = nullptr;
    _file_state = {0};
}

HttpResponse::~HttpResponse(){
    unmap_file();
}

// 给定请求目录，路径
void HttpResponse::init(const std::string& srcdir,std::string& path,
                bool is_keepalive,int code){
    // 如果已载入 清掉换新的
    if(_mmfile){
        unmap_file();
    }
    _code = code;
    _is_keepalive = is_keepalive;
    _path = path;
    _srcdir = srcdir;
    _mmfile = nullptr;
    _file_state = {0};
}

// 加入状态行
void HttpResponse::_add_stateline(Buffer& buff){
    string status;
    // 根据当前代码构造status
    if(CODE_STATUS.count(_code) == 1){
        status = CODE_STATUS.find(_code)->second;
    }else{
    // 不然直接400错误
        _code = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.append("HTTP/1.1 "+to_string(_code)+" "+status+"\r\n");
}

// 加入基础的header
void HttpResponse::_add_basic_header(Buffer& buff){
    // 先处理一下持久化连接的header
    buff.append("Connection: ");
    if(_is_keepalive){
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6,timeout=120\r\n");
    }else{
        buff.append("close\r\n");
    }
    // 类型信息
    buff.append("Content-type: " + _getfile_type() + "\r\n");
    buff.append("Accept-Ranges: bytes\r\n");
}

// 加个range header 顺便改响应码为206
// 偏移量-偏移量/总量 如: 0-1023/1024
void HttpResponse::_add_range_header(Buffer& buff,size_t start,size_t end){
    size_t total = _file_state.st_size;
    std::string content = "Content-Range: bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(total) +"\r\n";
    // like: COntent-Range: bytes 114-514/1919810\r\n
    buff.append(content);
}

void HttpResponse::_add_contentlen_header(Buffer& buff,size_t size){
    buff.append("Content-length: " + to_string(size) + "\r\n");
}

void HttpResponse::_add_endline(Buffer& buff){
    buff.append("\r\n");
}

// 添加内容，把file内容载入内存，位置为_mmfile
bool HttpResponse::_load_content(Buffer& buff){
    std::cout<<"HttpResponse:: _load_content"<<"\n";
    int src_fd = open((_srcdir+_path).data(),O_RDONLY);
    std::cout<<"    load content :src_fd: "<<src_fd<<"\n";
    std::cout<<"    path: "<<_path<<"\n";
    if(src_fd<0){
        
        // 直接给一个错误信息
        error_content(buff,"file notfound");
        return false;
    }
    // mmap:把该fd从0~size映射到内存
    // 返回映射到内存的位置
    // 此处st_size不能为零 不然会报段错误
    int size = _file_state.st_size;
    std::cout<<"    filesize:"<<size<<"\n";
    if(size == 0){
        error_content(buff,"file is empty");
        close(src_fd);
        return false;
    }
    void* mm_return = mmap(0,_file_state.st_size,PROT_READ,MAP_PRIVATE,src_fd,0);
    if(mm_return == MAP_FAILED){
        error_content(buff,"file notfound");
        close(src_fd);
        return false;
    }
    // 转化为字节*
    _mmfile = (char*)mm_return;
    close(src_fd);
    return true;
}

// 根据后缀判断文件类型
string HttpResponse::_getfile_type(){
    std::cout<<"HttpResponse::get filetype\n";
    std::cout<<"    path: "<<_path<<"\n";
    size_t pos = _path.find_last_of('.');
    // 如果没找到. 认为是text/plain
    if(pos == string::npos){
        return "text/plain";
    }
    string suffix = _path.substr(pos);
    if(SUFFIX_TYPE.count(suffix) == 1){
        return SUFFIX_TYPE.find(suffix)->second;
    }
    // 不可识别的后缀
    return "text/plain";
}

// 如果载入中途出错 此时文件不载入内存 mmfile为nullptr
void HttpResponse::error_content(Buffer&buff,std::string message){
    // 构造一个临时的小html作为错误页面
    string html_body;
    string status;
    html_body += "<html><title>Error</title>";
    html_body += "<body bgcolor=\"ffffff\">";

    if(CODE_STATUS.count(_code) == 1){
        status = CODE_STATUS.find(_code)->second;
    }else{
        status = "Bad Request";
    }

    html_body += to_string(_code) + " : " + status  + "\n";
    html_body += "<p>" + message + "</p>";
    html_body += "<hr><em>TinyWebServer</em></body></html>";

    buff.append("Content-length: " + to_string(html_body.size()) + "\r\n\r\n");
    buff.append(html_body);
}

// 设置错误页面 重新设置path和state
void HttpResponse::_checkcode_errorhtml(){
    if(CODE_PATH.count(_code) == 1){
        _path = CODE_PATH.find(_code)->second;
        stat((_srcdir + _path).data(), &_file_state);
    }
}

// 往buff里写入响应报文
void HttpResponse::check_code(Buffer& buff,bool is_range){
    std::cout<<"HttpResponse::make response\n";
    std::cout<<"    srcdir + path = "<<_srcdir<<_path<<"\n";
    if(stat((_srcdir+_path).data(),&_file_state)<0){
        std::cout<<"<0\n";
        _code = 404;    //NOT FOUND
    }else if(S_ISDIR(_file_state.st_mode)){
        std::cout<<"is dir\n";
        _code = 404;    //NOT FOUND
    }else if(is_range){
        _code = 206;    // range
    }else if(_code == -1){
        _code = 200;    // ok
    }

    // std::cout<< "code: "<<_code<<"\n";
    // std::cout<< "size: "<<_file_state.st_size;
    // std::cout<<"HttpResponse:: check errorhtml"<<"\n";
    // _check_errorhtml(); // reset filepath and htmlpage if error
    // std::cout<<"HttpResponse:: add stateline"<<"\n";
    // _add_stateline(buff);
    
    // _add_basic_header(buff);
    // std::cout<<"HttpResponse:: load content"<<"\n";
    // _load_content(buff); //载入file到内存 若出错则给一个error html作为body
    
    // _add_content_len(buff,_file_state.st_size);
    // //buff.append("Content-length: " + to_string(_file_state.st_size) + "\r\n\r\n");
    // _add_endline(buff);
}

void HttpResponse::unmap_file(){
    if(_mmfile){
        munmap(_mmfile,_file_state.st_size);
        _mmfile = nullptr;
    }
}

char* HttpResponse::file(){
    return _mmfile;
}

size_t HttpResponse::file_length() const{
    return _file_state.st_size;
}