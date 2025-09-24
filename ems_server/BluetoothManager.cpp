#include "BluetoothManager.h"
#include "DBManager.h"

#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/select.h>
#include <map>

// 디바이스별 데이터 버퍼
std::map<std::string, std::string> deviceBuffers;

// 디바이스 등록
void BluetoothManager::addDevice(const std::string& name, const std::string& path)
{
    devices[name] = path;
    deviceBuffers[name] = ""; // 버퍼 초기화
}

// 포트 초기화
bool BluetoothManager::initializeDevices()
{
    for (auto& it : devices)
    {
        // 읽기는 Non-blocking, 쓰기는 Blocking으로 설정
        int fd = open(it.second.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0)
        {
            perror(("블루투스 포트 열기 실패: " + it.second).c_str());
            return false;
        }

        // 읽기용 Non-blocking 설정
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        deviceFds[it.first] = fd;
        std::cout << it.first << " (" << it.second << ") 포트 열림" << std::endl;
    }
    return true;
}

// Non-blocking 데이터 수신 루프 - 버퍼링 추가
void BluetoothManager::processDataLoop()
{
    char buf[1024];

    while (true)
    {
        fd_set readfds;
        FD_ZERO(&readfds);

        int maxFd = 0;
        for (auto& it : deviceFds)
        {
            FD_SET(it.second, &readfds);
            if (it.second > maxFd)
                maxFd = it.second;
        }

        struct timeval tv;
        tv.tv_sec = 1;   // 1초 타임아웃
        tv.tv_usec = 0;

        int ret = select(maxFd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0)
        {
            perror("select 오류");
            continue;
        }
        else if (ret == 0)
        {
            // 타임아웃: 데이터 없음
            continue;
        }

        // 읽을 수 있는 fd 처리
        for (auto& it : deviceFds)
        {
            int fd = it.second;
            std::string deviceName = it.first;
            
            if (FD_ISSET(fd, &readfds))
            {
                int bytesRead = read(fd, buf, sizeof(buf)-1);
                if (bytesRead > 0)
                {
                    buf[bytesRead] = '\0';
                    
                    // 디바이스별 버퍼에 데이터 추가
                    deviceBuffers[deviceName] += std::string(buf);
                    
                    // 완전한 줄(개행문자 포함) 검사 및 처리
                    processCompleteLines(deviceName);
                }
            }
        }
    }
}

// 완전한 줄을 찾아서 처리하는 함수
void BluetoothManager::processCompleteLines(const std::string& deviceName)
{
    std::string& buffer = deviceBuffers[deviceName];
    size_t pos = 0;
    
    while ((pos = buffer.find('\n')) != std::string::npos)
    {
        // 완전한 한 줄 추출
        std::string completeLine = buffer.substr(0, pos);
        
        // \r 문자 제거 (Windows 스타일 줄바꿈 대응)
        if (!completeLine.empty() && completeLine.back() == '\r')
        {
            completeLine.pop_back();
        }
        
        // 빈 줄이 아니면 처리
        if (!completeLine.empty())
        {
            std::cout << "[" << deviceName << "] received: " << completeLine << std::endl;
            handleData(completeLine);
        }
        
        // 처리된 줄을 버퍼에서 제거
        buffer.erase(0, pos + 1);
    }
    
    // 버퍼가 너무 크면 일부 제거 (메모리 보호)
    if (buffer.length() > 2048)
    {
        buffer.clear();
        std::cout << "[" << deviceName << "] 버퍼 오버플로우 - 초기화" << std::endl;
    }
}

// 특정 디바이스에 명령 전송
bool BluetoothManager::sendCommand(const std::string& deviceName, const std::string& command)
{
    std::lock_guard<std::mutex> lock(sendMutex);
    
    auto it = deviceFds.find(deviceName);
    if (it == deviceFds.end())
    {
        std::cerr << "디바이스를 찾을 수 없음: " << deviceName << std::endl;
        return false;
    }

    int fd = it->second;
    std::string fullCommand = command + "\n";  // 개행 문자 추가
    
    ssize_t bytesWritten = write(fd, fullCommand.c_str(), fullCommand.length());
    if (bytesWritten < 0)
    {
        perror(("블루투스 전송 실패: " + deviceName).c_str());
        return false;
    }

    std::cout << "[" << deviceName << "] sent: " << command << std::endl;
    return true;
}

// 모든 디바이스에 명령 전송
bool BluetoothManager::sendToAllDevices(const std::string& command)
{
    bool allSuccess = true;
    for (auto& it : deviceFds)
    {
        if (!sendCommand(it.first, command))
        {
            allSuccess = false;
        }
    }
    return allSuccess;
}

// TCP 명령을 블루투스 명령으로 변환하여 전송
void BluetoothManager::handleTCPCommand(const std::string& tcpCommand)
{
    std::string bluetoothCommand = convertTCPToBluetoothCommand(tcpCommand);
    
    if (!bluetoothCommand.empty())
    {
        // 명령에 따라 특정 디바이스나 모든 디바이스에 전송
        if (tcpCommand.find("window") != std::string::npos)
        {
            // 창문 관련 명령은 창문 모듈(Smart Window)에 전송
            sendCommand("windowModule", bluetoothCommand);
            std::cout << "[TCP->BT] Window command sent: " << bluetoothCommand << std::endl;
        }
        else if (tcpCommand.find("light") != std::string::npos)
        {
            // 조명 관련 명령은 조명 제어 모듈에 전송
            sendCommand("lightModule", bluetoothCommand);
            std::cout << "[TCP->BT] Light command sent: " << bluetoothCommand << std::endl;
        }
        else if (tcpCommand.find("door") != std::string::npos)
        {
            // 문 관련 명령은 문 제어 모듈에 전송
            sendCommand("doorModule", bluetoothCommand);
            std::cout << "[TCP->BT] Door command sent: " << bluetoothCommand << std::endl;
        }
        else if (tcpCommand.find("set_") != std::string::npos)
        {
            // 설정 명령은 해당 창문 모듈에 전송
            sendCommand("windowModule", bluetoothCommand);
            std::cout << "[TCP->BT] Setting command sent: " << bluetoothCommand << std::endl;
        }
        else
        {
            // 기타 명령은 모든 디바이스에 전송
            sendToAllDevices(bluetoothCommand);
            std::cout << "[TCP->BT] Broadcast command sent: " << bluetoothCommand << std::endl;
        }
    }
    else
    {
        std::cout << "[TCP->BT] Unknown command: " << tcpCommand << std::endl;
    }
}

// TCP 명령을 블루투스 프로토콜로 변환
std::string BluetoothManager::convertTCPToBluetoothCommand(const std::string& tcpCommand)
{
    // TCP 명령을 아두이노가 이해할 수 있는 형식으로 변환
    if (tcpCommand == "window_open")
        return "OPEN";
    else if (tcpCommand == "window_close")
        return "CLOSE";
    else if (tcpCommand == "light_on")
        return "CMD_LIGHT_ON";
    else if (tcpCommand == "light_off")
        return "CMD_LIGHT_OFF";
    else if (tcpCommand == "door_open")
        return "CMD_DOOR_OPEN";
    else if (tcpCommand == "door_close")
        return "CMD_DOOR_CLOSE";
    
    // 기타 명령어들 추가 가능
    return "";
}

// 문자열 파싱
std::vector<std::string> BluetoothManager::split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string temp;
    while (std::getline(ss, temp, delimiter))
        tokens.push_back(temp);
    return tokens;
}

// 데이터 처리 후 DB 저장 (기존 코드 유지)
void BluetoothManager::handleData(const std::string& rawData)
{
    auto tokens = split(rawData, '_');
    if (tokens.size() < 2)
        return;

    std::string type = tokens[1];

    if (type == "fire" && tokens.size() == 4)
    {
        int fireData = std::stoi(tokens[2]);
        float gasData = std::stof(tokens[3]);

        // 조건에 따라 상태값 설정
        std::string fireState = (fireData >= 150) ? "정상" : "화재";
        std::string gasState  = (gasData >= 700.0f) ? "위험" : "정상";

        // DB 저장
        DBManager::instance().insertFireData(fireState, fireData, gasState, gasData);
    }
    else if (type == "pet" && tokens.size() == 5)
    {
        int foodVal = std::stoi(tokens[2]);
        int waterVal = std::stoi(tokens[3]);
        int toiletVal = std::stoi(tokens[4]);

        // 조건에 따라 상태 문자열 변환
        std::string foodData   = (foodVal == 1) ? "충분" : "부족";
        std::string waterData  = (waterVal == 1) ? "충분" : "부족";
        std::string toiletState = (toiletVal == 0) ? "깨끗함" : "청소 필요";

        // DB 저장
        DBManager::instance().insertPetData(foodData, waterData, toiletState);
    }
    else if (type == "plant" && tokens.size() == 6)
    {
        float soilData = std::stof(tokens[2]);
        float lightData = std::stof(tokens[3]);
        float tempData = std::stof(tokens[4]);
        float humiData = std::stof(tokens[5]);
        DBManager::instance().insertPlantData(soilData, tempData, humiData, lightData);
        DBManager::instance().insertHomeData(tempData, humiData, lightData);
    }
}