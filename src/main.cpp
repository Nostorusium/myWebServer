#include "server/WebServer.hpp"

int main(){
    WebServer server(12345,1,60000,6);
    server.start();
    return 0;
}