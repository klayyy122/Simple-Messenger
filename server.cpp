#include <winsock2.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <set>
#pragma warning(default: 4996)
#pragma comment(lib, "ws2_32.lib")
#define MAX_CONNECTIONS 2
using namespace std;

SOCKET Connections[MAX_CONNECTIONS];                 // Массив для хранения подключений
int Counter = 0;                                      // Индекс подключений
mutex ConnectionMutex;                                // Мьютекс для защиты общих ресурсов
set<string> Logins;                                   // Множество для хранения уникальных логинов

enum Packet {
    P_ChatMessageAll,                                 // Для широковещательных сообщений
    P_LoginTaken,                                     // Для уведомления о занятом логине
    P_ServerMessage                                   // Для серверных уведомлений
};

bool ProcessPacket(int index, Packet packettype) {
    switch (packettype) {
    case P_ChatMessageAll:
    {
        int msg_size;                                  // Переменная для записи размера строки
        if (recv(Connections[index], (char*)&msg_size, sizeof(int), NULL) <= 0) {
            cout << "Error receiving message size from client " << index << endl;
            return false;
        }

        unique_ptr<char[]> msg(new char[msg_size + 1]);
        if (recv(Connections[index], msg.get(), msg_size, NULL) <= 0) {
            cout << "Error receiving message from client " << index << endl;
            return false;
        }
        msg[msg_size] = '\0';
        cout << "Received message from client " << index << ": " << msg.get() << endl;

        for (int i = 0; i < Counter; i++) {
            if (i == index) {
                continue;
            }

            Packet msgtype = P_ChatMessageAll;
            if (send(Connections[i], (char*)&msgtype, sizeof(Packet), NULL) <= 0 ||
                send(Connections[i], (char*)&msg_size, sizeof(int), NULL) <= 0 ||
                send(Connections[i], msg.get(), msg_size, NULL) <= 0) {
                cout << "Error sending message to client " << i << endl;
                return false;
            }
        }
        break;
    }
    default:
        cout << "Unrecognized packet: " << packettype << endl;
        break;
    }

    return true;
}

void ClientHandler(int index) {
    Packet packettype;
    while (true) {
        if (recv(Connections[index], (char*)&packettype, sizeof(Packet), NULL) <= 0) {
            cout << "Error receiving packet from client " << index << endl;
            break;
        }

        if (!ProcessPacket(index, packettype)) {
            break;
        }
    }

    ConnectionMutex.lock();                           // Блокировка мьютекса перед изменением общего ресурса
    closesocket(Connections[index]);
    Connections[index] = INVALID_SOCKET;
    ConnectionMutex.unlock();                         // Разблокировка мьютекса
}

int main(int argc, char* argv[]) {
    WSAData wsaData;                                   // Создание структуры
    WORD DLLVersion = MAKEWORD(2, 1);                  // Запрос версии библиотеки
    if (WSAStartup(DLLVersion, &wsaData) != 0) {
        cout << "Error loading DLL version" << endl;
        exit(1);
    }

    SOCKADDR_IN addr;                                  // Заполнение информации о сокете
    ZeroMemory(&addr, sizeof(addr));
    int sizeofaddr = sizeof(addr);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(1111);
    addr.sin_family = AF_INET;

    SOCKET sListen = socket(AF_INET, SOCK_STREAM, NULL);// Создание слушающего сокета
    if (sListen == INVALID_SOCKET) {
        cout << "Error creating socket" << endl;
        WSACleanup();
        return 1;
    }

    if (bind(sListen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "Error binding socket" << endl;
        closesocket(sListen);
        WSACleanup();
        return 1;
    }

    if (listen(sListen, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Error listening on socket" << endl;
        closesocket(sListen);
        WSACleanup();
        return 1;
    }

    SOCKET newConnection;                              // Объявление нового сокета для удержания соединения
    while (Counter < MAX_CONNECTIONS) {
        newConnection = accept(sListen, (SOCKADDR*)&addr, &sizeofaddr);

        if (newConnection == INVALID_SOCKET) {
            cout << "Error accepting new connection" << endl;
        }
        else {
            // Принимаем логин от клиента
            int login_size;
            if (recv(newConnection, (char*)&login_size, sizeof(int), NULL) <= 0) {
                cout << "Error receiving login size from client" << endl;
                closesocket(newConnection);
                continue;
            }

            unique_ptr<char[]> login(new char[login_size + 1]);
            if (recv(newConnection, login.get(), login_size, NULL) <= 0) {
                cout << "Error receiving login from client" << endl;
                closesocket(newConnection);
                continue;
            }
            login[login_size] = '\0';

            // Проверяем уникальность логина
            ConnectionMutex.lock();
            if (Logins.find(login.get()) != Logins.end()) {
                // Логин уже занят
                Packet msgtype = P_LoginTaken;
                send(newConnection, (char*)&msgtype, sizeof(Packet), NULL);
                closesocket(newConnection);
                ConnectionMutex.unlock();
                cout << "Client tried to connect with taken login: " << login.get() << endl;
                continue;
            }
            else {
                // Логин уникален, добавляем его в множество
                Logins.insert(login.get());
                ConnectionMutex.unlock();
            }

            cout << "Client Connected with login: " << login.get() << endl;
            string msg = "###SERVER MESSAGE###: You connected to the server!";
            int msg_size = msg.size();
            Packet msgtype = P_ServerMessage;  // Используем новый тип для серверных сообщений
            if (send(newConnection, (char*)&msgtype, sizeof(Packet), NULL) <= 0 ||
                send(newConnection, (char*)&msg_size, sizeof(int), NULL) <= 0 ||
                send(newConnection, msg.c_str(), msg_size, NULL) <= 0) {
                cout << "Error sending welcome message to client" << endl;
                closesocket(newConnection);
                continue;
            }

            ConnectionMutex.lock();                    // Блокировка мьютекса перед изменением общего ресурса
            Connections[Counter++] = newConnection;    // Запись соединения в массив
            ConnectionMutex.unlock();                  // Разблокировка мьютекса

            thread clientThread(ClientHandler, Counter - 1);
            clientThread.detach();                     // Отсоединение потока для выполнения в фоновом режиме
        }
    }

    closesocket(sListen);
    WSACleanup();
    return 0;
}