#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <ctime>
#include <cmath>
using namespace std;
const int DEFAULT_PORT = 12345;
const int BUFFER_SIZE = 1024;
inline const char *IP_DEFAULT = "127.0.0.1";
extern string ADMIN_PASSWORD;
enum MessageType
{
    BROADCAST,
    PRIVATE, 
    COMMAND,
    ADMIN_COMMAND
};

class Client
{
public:
    SOCKET socket;
    string name;

    Client(SOCKET sock = INVALID_SOCKET, const string &n = "") : socket(sock), name(n) {}
};

class Admin : public Client
{
public:
    Admin(SOCKET sock = INVALID_SOCKET, const string &n = "") : Client(sock, n) {}

    static bool Authenticate(const string &password)
    {
        return password == ADMIN_PASSWORD;
    }
};

inline string Function(const string &message, int key)
{
    if (key <= 0 || pow(key, 3) >= 65535)
        return message;

    string result;
    for (size_t i = 0; i < message.size(); i++)
    {
        int k = pow(key, i % 4 + 1); 
        result += message[i] ^ k;
    }
    return result;
}
inline string DeFunction(const string &message, int key)
{
    if (key <= 0 || pow(key, 3) >= 65535)
        return message;

    string result;
    for (size_t i = 0; i < message.size(); i++)
    {
        int k = pow(key, i % 4 + 1);
        result += message[i] ^ k;
    }
    return result;
}
class Clients
{
private:
    mutex cout_mutex;

    void safe_print(const string &message);

    void show_help(bool is_admin);

public:
    void run_client();
};
