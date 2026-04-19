#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <string>

class HttpRequest
{
  private:
    CURL* curl;
    std::stringstream ss;
    long http_code;

  public:
    HttpRequest();
    ~HttpRequest();
    
    std::string Get(const std::string& url);
    std::string Post(const std::string& url, const std::string& data);
    
    long GetHttpCode();

  private:
    static size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp);
    size_t Write(void* buffer, size_t size, size_t nmemb);
};

#endif // HTTP_REQUEST_H