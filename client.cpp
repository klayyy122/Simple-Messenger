#pragma warning(default: 4996)
#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <iostream>
#include <string>
#include <cmath>
#include <ctime>

int Key;

std::string Function(std::string& message, const int& key) {
    time_t t = time(NULL);
    struct tm* now = localtime(&t);
    char str[20];
    strftime(str, sizeof(str), "%H:%M:%S", now);
    if ((pow(key, 3) >= 65535) or (key < 0)) {
        return "Key is not correct";
    }
    else if (key == 0) {
        return message + "\n" + "Was sent in " + str;
    }
    else {
        int len = message.size();
        int* num = new int[len];
        std::string res = "";
        for (int i = 0; i < len; i++) {
            num[i] = int(message[i]);
        }
        int i = 0;
        while (i < len) {
            int j = min(i + 4, len);
            int st = 0;
            for (int ii = i; ii < j; ii++) {
                int k = pow(key, st);
                char el = char(num[ii] xor k);
                res = res + el;
                st++;
            }
            i += 4;
        }
        int sec = int(str[7]) - 48 + 10 * (int(str[6]) - 48);
        int hour = int(str[1]) - 48 + 10 * (int(str[0]) - 48);
        i = 0;
        while (i < len) {
            int j = min(i + 3, len);
            int st = 0;
            for (int ii = i; ii < j; ii++) {
                int k = pow(hour, st) + sec;
                char el = char(res[ii] xor k);
                res[ii] = el;
                st++;
            }
            i += 3;
        }
        res = res + "\n" + "Was sent in " + str;
        return res;
    }
}

std::string DeFunction(const std::string& message, const int& key) {
    if (message == "###SERVER MESSAGE###: You connected to the server!") {
        return message;
    }
    if ((pow(key, 3) >= 65535) or (key < 0)) {
        return "Key is not correct";
    }
    else if (key == 0) {
        return message;
    }
    else {
        int sec = int(message[message.length() - 1] - 48) + 10 * int(message[message.length() - 2] - 48);
        int hour = int(message[message.length() - 7] - 48) + 10 * int(message[message.length() - 8] - 48);
        int len = message.size();
        int* num = new int[len];
        std::string res = "";
        int i = 0;
        while (i < len - 21) {
            int j = min(i + 3, len - 21);
            int st = 0;
            for (int ii = i; ii < j; ii++) {
                int k = pow(hour, st) + sec;
                char el = char(message[ii] xor k);
                res = res + el;
                st++;
            }
            i += 3;
        }
        for (int i = 0; i < len - 21; i++) {
            num[i] = int(res[i]);
        }
        i = 0;
        while (i < len - 21) {
            int j = min(i + 4, len - 21);
            int st = 0;
            for (int ii = i; ii < j; ii++) {
                int k = pow(key, st);
                res[ii] = char(int(num[ii]) xor k);
                st++;
            }
            i += 4;
        }
        for (int i = len - 21; i < len; i++) {
            res = res + message[i];
        }
        return res;
    }
}

SOCKET Connection;   // Создание нового сокета, для присоединения к серверу

enum Packet {
    P_ChatMessage,   // Тип пакета
    P_LoginTaken,    // Логин занят
    P_ServerMessage  // Серверное сообщение
};

bool ProcessPacket(Packet packettype) {
    switch (packettype) {
    case P_ChatMessage:
    {
        int msg_size;
        recv(Connection, (char*)&msg_size, sizeof(int), NULL);
        char* msg = new char[msg_size + 1];
        msg[msg_size] = '\0';
        recv(Connection, msg, msg_size, NULL);
        std::string message(msg);
        delete[] msg;
        message = DeFunction(message, Key);
        std::cout << message << std::endl;
        break;
    }
    case P_ServerMessage:
    {
        int msg_size;
        recv(Connection, (char*)&msg_size, sizeof(int), NULL);
        char* msg = new char[msg_size + 1];
        msg[msg_size] = '\0';
        recv(Connection, msg, msg_size, NULL);
        std::cout << msg << std::endl;
        delete[] msg;
        break;
    }
    default:
        std::cout << "Unrecognized packet: " << packettype << std::endl;
        break;
    }
    return true;
}

void ClientHandler() {
    Packet packettype;
    while (true) {
        if (recv(Connection, (char*)&packettype, sizeof(Packet), NULL) <= 0) {
            std::cout << "Connection lost!" << std::endl;
            break;
        }
        if (!ProcessPacket(packettype)) {
            break;
        }
    }
    closesocket(Connection);
}

int main(int argc, char* argv[]) {
    std::cout << "Hello, Client! Go to registration!" << std::endl;
    std::string login, trash;
    std::cout << "Enter your Login:" << std::endl;
    std::cin >> login;
    login = "[" + login + "]: ";
    std::cout << "Enter Crypto key: ";
    std::cin >> Key;
    std::getline(std::cin, trash);

    WSAData wsaData;
    WORD DLLVersion = MAKEWORD(2, 1);
    if (WSAStartup(DLLVersion, &wsaData) != 0) {
        std::cout << "Error" << std::endl;
        exit(1);
    }

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    int sizeofaddr = sizeof(addr);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(1111);
    addr.sin_family = AF_INET;

    Connection = socket(AF_INET, SOCK_STREAM, NULL);
    if (connect(Connection, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
        std::cout << "Error: failed connect to server." << std::endl;
        return 1;
    }

    // Отправляем логин серверу
    int login_size = login.size();
    send(Connection, (char*)&login_size, sizeof(int), NULL);
    send(Connection, login.c_str(), login_size, NULL);

    // Проверяем ответ сервера
    Packet serverResponse;
    if (recv(Connection, (char*)&serverResponse, sizeof(Packet), NULL) <= 0) {
        std::cout << "Connection error" << std::endl;
        closesocket(Connection);
        return 1;
    }

    // Если это ответ о занятом логине
    if (serverResponse == P_LoginTaken) {
        std::cout << "Login is taken!" << std::endl;
        closesocket(Connection);
        system("pause");
        return 1;
    }

    // Если это серверное сообщение (приветствие)
    if (serverResponse == P_ServerMessage) {
        int msg_size;
        recv(Connection, (char*)&msg_size, sizeof(int), NULL);
        char* msg = new char[msg_size + 1];
        recv(Connection, msg, msg_size, NULL);
        msg[msg_size] = '\0';
        std::cout << DeFunction(std::string(msg), Key) << std::endl;
        delete[] msg;
    }

    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientHandler, NULL, NULL, NULL);

    std::string msg1;
    while (true) {
        std::getline(std::cin, msg1);
        if (msg1 == "CLOSE_PROGRAMM") {
            std::cout << "Exiting..." << std::endl;
            closesocket(Connection);
            WSACleanup();
            exit(0);
        }
        msg1 = login + msg1;
        msg1 = Function(msg1, Key);

        int msg_size = msg1.size();
        Packet packettype = P_ChatMessage;
        send(Connection, (char*)&packettype, sizeof(Packet), NULL);
        send(Connection, (char*)&msg_size, sizeof(int), NULL);
        send(Connection, msg1.c_str(), msg_size, NULL);
    }

    return 0;
}