#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

class TCPServer
{
public:
    TCPServer(int port);
    ~TCPServer();

    // 서버 시작/중지
    bool start();
    void stop();

    // 콜백 함수 설정 (클라이언트 명령 처리용)
    void setCommandCallback(std::function<void(const std::string&)> callback);

private:
    int m_port;
    int m_serverSocket;
    std::atomic<bool> m_running;
    std::thread m_serverThread;
    
    std::function<void(const std::string&)> m_commandCallback;

    void serverLoop();
    void handleClient(int clientSocket);
    std::string processCommand(const std::string& command);
};

#endif // TCPSERVER_H