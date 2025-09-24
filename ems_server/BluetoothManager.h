#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>

class BluetoothManager
{
public:
    // 디바이스 경로와 이름 매핑
    void addDevice(const std::string& name, const std::string& path);

    // 포트 초기화 (open + non-blocking for read, blocking for write)
    bool initializeDevices();

    // 데이터 수신 처리 (Non-blocking)
    void processDataLoop();

    // 데이터 송신 기능 추가
    bool sendCommand(const std::string& deviceName, const std::string& command);
    bool sendToAllDevices(const std::string& command);

    // TCP 명령을 블루투스 명령으로 변환하여 전송
    void handleTCPCommand(const std::string& tcpCommand);

private:
    std::map<std::string, std::string> devices;       // 이름 -> 시리얼 경로
    std::map<std::string, int> deviceFds;            // 이름 -> fd
    std::mutex sendMutex;                            // 송신용 뮤텍스

    std::vector<std::string> split(const std::string& str, char delimiter);
    void handleData(const std::string& rawData);
    std::string convertTCPToBluetoothCommand(const std::string& tcpCommand);

    void processCompleteLines(const std::string& deviceName);
};

#endif // BLUETOOTHMANAGER_H