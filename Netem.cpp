#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>

using namespace std;

wstring hostname2ips(string &hostname)
{
    struct addrinfo * _addrinfo;
    struct addrinfo * _res;
    char _address[INET6_ADDRSTRLEN];
    int err = 0;
    wstring ips;

    err = getaddrinfo(hostname.c_str(), nullptr, nullptr, &_addrinfo);

    if(err != 0)
    {
        printf("error: %s\n", gai_strerror(err));
        return L"";
    }

    for(_res = _addrinfo; _res != nullptr; _res = _res->ai_next)
    {
        if(_res->ai_family == AF_INET)
        {

            if(inet_ntop(AF_INET, &((struct sockaddr_in *)_res->ai_addr)->sin_addr, _address, sizeof(_address)) == nullptr)
            {
                return L"";
            }
            string ws(_address);
            wstring loginfo = L"\nIPv4 - ";
            wstring ip(ws.begin(), ws.end());
            loginfo += ip;
            ips += ip + L",";
        }
        else if(_res->ai_family == AF_INET6)
        {
            if(inet_ntop(AF_INET6, &((struct sockaddr_in *)_res->ai_addr)->sin_addr, _address, sizeof(_address)) == nullptr)
            {
                return L"";
            }
            string ws(_address);
            wstring loginfo = L"\nIPv6 - ";
            wstring ip(ws.begin(), ws.end());
            loginfo += ip;
            ips += ip + L",";
        }
    }
}

