# WebServer学习版

本项目的初衷是写一个能在局域网上运作的HTTP服务器，这样我就可以在电脑上放电影躺床上用手机观看了。
本项目的原型是github上非常经典的WebServer项目: 
https://github.com/qinguoyi/TinyWebServer
https://github.com/markparticle/WebServer
主要采用下面这个更精简的版本
Reactor+线程池以及自定义的Buffer


我抄了不少内容，或者说整个框架基本都源于此，但也做了一些删改，至少改了几个能更符合我直觉的函数名。
因为这东西只运行在自己家里，所以完全没有数据库、日志、登录等繁琐业务逻辑。
因为没有日志，所以输出全靠cout

## 主要的改进

为了支持我看电影的需求，我在原WebServer基础上增加了对Range的支持。
播放媒体如电影/音频时，由于进度条的存在浏览器发出的请求报文会插入Range: bytes 114-514
表示请求这个文件的某个部分。响应报文需要返回代码206，并给出Content-Range: bytes 114-514/1919810
并只需要发送这一部分文件即可。

在提供了这一支持后，页面所播放的视频已经可以随意自由地拖动进度条，且每次拖动都不需要等待一整个文件重新发送了。

## 注意事项

编译没什么好说的全塞一起就行，资源目录应设置为执行文件同目录下的/resourses
可能有未知bug存在，在过往调试的过程中有服务器崩溃的情况发生，也许已经更改，也许没有。
因为对linux服务器开发不甚了解，所以本项目非常粗糙，内部包含在阅读原项目时留下的大量混乱注释与猜想。
