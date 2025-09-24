#include <iostream>
#include <thread>
#include <signal.h>
#include "BluetoothManager.h"
#include "DBManager.h"
#include "TCPServer.h"

// 전역 변수로 서버 인스턴스 관리
TCPServer* tcpServer = nullptr;
std::atomic<bool> running(true);

// 시그널 핸들러
void signalHandler(int signal)
{
    std::cout << "\n종료 신호 받음..." << std::endl;
    running = false;
    
    if (tcpServer)
    {
        tcpServer->stop();
    }
}

int main()
{
    // 시그널 핸들러 등록
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 1. DB 연결
    if (!DBManager::instance().connect("127.0.0.1", "user1", "1234", "hometer", 3306))
    {
        std::cerr << "DB 연결 실패" << std::endl;
        return 1;
    }

    // 2. 블루투스 매니저 초기화
    BluetoothManager btManager;
    btManager.addDevice("fireModule",   "/dev/rfcomm0");
    // btManager.addDevice("petModule",    "/dev/rfcomm1");
    // btManager.addDevice("plantModule",  "/dev/rfcomm2");
    // btManager.addDevice("windowModule", "/dev/rfcomm3");  // Smart Window 모듈
    // btManager.addDevice("lightModule",  "/dev/rfcomm4");  // 조명 제어 모듈 (필요시)
    // btManager.addDevice("doorModule",   "/dev/rfcomm5");  // 문 제어 모듈 (필요시)

    if (!btManager.initializeDevices())
    {
        std::cerr << "블루투스 포트 초기화 실패" << std::endl;
        return 1;
    }

    // 3. TCP 서버 초기화 및 시작
    tcpServer = new TCPServer(8080);
    
    // TCP 명령을 블루투스로 전달하는 콜백 설정
    tcpServer->setCommandCallback([&btManager](const std::string& command) {
        btManager.handleTCPCommand(command);
    });

    if (!tcpServer->start())
    {
        std::cerr << "TCP 서버 시작 실패" << std::endl;
        delete tcpServer;
        return 1;
    }

    // 4. 블루투스 데이터 수신을 별도 스레드에서 실행
    std::thread bluetoothThread([&btManager]() {
        btManager.processDataLoop();
    });

    // 5. 메인 스레드는 종료 신호 대기
    std::cout << "스마트 홈 서버가 시작되었습니다." << std::endl;
    std::cout << "TCP 포트: 8080" << std::endl;
    std::cout << "블루투스 데이터 수신 중..." << std::endl;
    std::cout << "종료하려면 Ctrl+C를 누르세요." << std::endl;

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 6. 정리
    std::cout << "서버 종료 중..." << std::endl;
    
    if (tcpServer)
    {
        tcpServer->stop();
        delete tcpServer;
    }

    // 블루투스 스레드는 무한루프라서 강제 종료
    // 실제 운영환경에서는 더 우아한 종료 방법 필요
    if (bluetoothThread.joinable())
    {
        bluetoothThread.detach();
    }

    std::cout << "서버가 종료되었습니다." << std::endl;
    return 0;
}