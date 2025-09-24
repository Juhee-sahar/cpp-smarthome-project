#include "TCPServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

TCPServer::TCPServer(int port) 
    : m_port(port), m_serverSocket(-1), m_running(false)
{
}

TCPServer::~TCPServer()
{
    stop();
}

bool TCPServer::start()
{
    // 소켓 생성
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0)
    {
        std::cerr << "소켓 생성 실패" << std::endl;
        return false;
    }

    // 소켓 옵션 설정 (재사용 가능)
    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        std::cerr << "소켓 옵션 설정 실패" << std::endl;
        close(m_serverSocket);
        return false;
    }

    // 주소 설정
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    // 바인드
    if (bind(m_serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        std::cerr << "바인드 실패: 포트 " << m_port << std::endl;
        close(m_serverSocket);
        return false;
    }

    // 리슨
    if (listen(m_serverSocket, 5) < 0)
    {
        std::cerr << "리슨 실패" << std::endl;
        close(m_serverSocket);
        return false;
    }

    m_running = true;
    m_serverThread = std::thread(&TCPServer::serverLoop, this);

    std::cout << "TCP 서버 시작됨 - 포트: " << m_port << std::endl;
    return true;
}

void TCPServer::stop()
{
    if (m_running)
    {
        m_running = false;
        
        if (m_serverSocket >= 0)
        {
            close(m_serverSocket);
            m_serverSocket = -1;
        }

        if (m_serverThread.joinable())
        {
            m_serverThread.join();
        }

        std::cout << "TCP 서버 종료됨" << std::endl;
    }
}

void TCPServer::setCommandCallback(std::function<void(const std::string&)> callback)
{
    m_commandCallback = callback;
}

void TCPServer::serverLoop()
{
    while (m_running)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0)
        {
            if (m_running) // 정상 종료가 아닌 경우만 에러 출력
            {
                std::cerr << "클라이언트 연결 수락 실패" << std::endl;
            }
            continue;
        }

        std::cout << "클라이언트 연결됨: " << inet_ntoa(clientAddr.sin_addr) 
                  << ":" << ntohs(clientAddr.sin_port) << std::endl;

        // 각 클라이언트를 별도 스레드에서 처리
        std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
        clientThread.detach(); // 독립적으로 실행
    }
}

void TCPServer::handleClient(int clientSocket)
{
    char buffer[1024];
    
    while (m_running)
    {
        memset(buffer, 0, sizeof(buffer));
        
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived <= 0)
        {
            // 클라이언트 연결 종료
            break;
        }

        std::string command(buffer);
        std::cout << "[TCP] 클라이언트 명령: " << command << std::endl;

        // 명령 처리
        std::string response = processCommand(command);

        // 응답 전송
        if (!response.empty())
        {
            send(clientSocket, response.c_str(), response.length(), 0);
        }

        // 콜백 함수 호출 (블루투스 전송용)
        if (m_commandCallback)
        {
            m_commandCallback(command);
        }
    }

    close(clientSocket);
    std::cout << "클라이언트 연결 종료" << std::endl;
}

std::string TCPServer::processCommand(const std::string& command)
{
    // 명령어 파싱 및 응답 생성
    std::stringstream ss(command);
    std::string action;
    ss >> action;
    
    // Smart Window 명령어들
    if (action == "window_open")
        return "OK_WINDOW_OPENING\n";
    else if (action == "window_close")
        return "OK_WINDOW_CLOSING\n";
    else if (action == "window_status")
        return "OK_STATUS_REQUESTED\n";
    
    // Smart Window 각도 설정 명령어들
    // else if (action.find("set_open_angle=") == 0)
    // {
    //     std::string angle = action.substr(15);
    //     return "OK_OPEN_ANGLE_SET_TO_" + angle + "\n";
    // }
    // else if (action.find("set_close_angle=") == 0)
    // {
    //     std::string angle = action.substr(16);
    //     return "OK_CLOSE_ANGLE_SET_TO_" + angle + "\n";
    // }
    
    // 기타 명령어들 (기존)
    // else if (action == "light_on")
    //     return "OK_LIGHT_ON\n";
    // else if (action == "light_off")
    //     return "OK_LIGHT_OFF\n";
    // else if (action == "door_open")
    //     return "OK_DOOR_OPEN\n";
    // else if (action == "door_close")
    //     return "OK_DOOR_CLOSE\n";

    return "OK_COMMAND_RECEIVED\n";
}