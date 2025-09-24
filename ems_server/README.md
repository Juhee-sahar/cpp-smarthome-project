# Smart Home Sensor Data Server with TCP/IP Control

## 프로젝트 개요

이 프로젝트는 아두이노 기반 센서 데이터를 블루투스(HC-05/HC-06)를 통해 수집하고, C++ 서버에서 MySQL 데이터베이스에 저장하는 시스템입니다.  
또한 TCP/IP 통신을 통해 클라이언트에서 스마트 홈 디바이스를 원격 제어할 수 있습니다.

**주요 기능:**
- 🔄 **양방향 블루투스 통신**: 센서 데이터 수신 + 제어 명령 송신
- 📊 **실시간 데이터 저장**: MySQL 데이터베이스에 센서 데이터 저장
- 🌐 **TCP/IP 서버**: 클라이언트 애플리케이션과의 통신
- 🏠 **스마트 홈 제어**: 창문, 조명, 문 등 원격 제어
- ⚡ **Non-blocking I/O**: 센서 데이터 수신과 제어 명령이 독립적으로 동작

## 시스템 아키텍처

```
[클라이언트 앱] <--TCP/IP--> [C++ 서버] <--Bluetooth--> [Arduino 모듈들]
                                |
                            [MySQL DB]
```

## 지원 센서 모듈

| 모듈        | 데이터 형식 (수신)                                           | 제어 명령 (송신)              | DB 저장 테이블       |
|------------|----------------------------------------------------------|---------------------------|-------------------|
| Fire       | `iot01_fire_<fireData>_<gasData>`                        | -                         | `fire_events`     |
| Pet        | `iot01_pet_<foodData>_<waterData>_<toiletState>`         | -                         | `pet_status`      |
| Plant      | `iot01_plant_<soilData>_<lightData>_<tempData>_<humiData>` | -                       | `plant_env`, `home_env` |
| Home       | -                                                        | `CMD_WINDOW_OPEN/CLOSE`   | -                 |
| Light      | -                                                        | `CMD_LIGHT_ON/OFF`        | -                 |
| Door       | -                                                        | `CMD_DOOR_OPEN/CLOSE`     | -                 |

## TCP/IP 명령어

클라이언트에서 서버로 전송할 수 있는 명령어:

| TCP 명령어      | 블루투스 변환       | 설명           |
|----------------|------------------|---------------|
| `window_open`  | `CMD_WINDOW_OPEN` | 창문 열기      |
| `window_close` | `CMD_WINDOW_CLOSE`| 창문 닫기      |
| `light_on`     | `CMD_LIGHT_ON`    | 조명 켜기      |
| `light_off`    | `CMD_LIGHT_OFF`   | 조명 끄기      |
| `door_open`    | `CMD_DOOR_OPEN`   | 문 열기        |
| `door_close`   | `CMD_DOOR_CLOSE`  | 문 닫기        |

## 프로젝트 구조

```
smart_home_server/
├── main.cpp                 # 메인 프로그램 (멀티스레딩)
├── BluetoothManager.h       # 블루투스 송수신 헤더
├── BluetoothManager.cpp     # 블루투스 송수신 구현
├── DBManager.h              # 데이터베이스 관리 헤더
├── DBManager.cpp            # 데이터베이스 관리 구현
├── TCPServer.h              # TCP 서버 헤더
├── TCPServer.cpp            # TCP 서버 구현
├── CMakeLists.txt           # 빌드 설정
├── README.md                # 프로젝트 설명서
├── client_test.py           # 클라이언트 테스트 프로그램
└── arduino_sketch.ino       # 아두이노 테스트 스케치
```

## 빌드 및 실행

### 1. 의존성 설치 (Ubuntu)

```bash
# MySQL 개발 라이브러리
sudo apt-get update
sudo apt-get install libmysqlclient-dev

# 기본 개발 도구
sudo apt-get install build-essential cmake pkg-config
```

### 2. 프로젝트 빌드

```bash
mkdir build
cd build
cmake ..
make
```

### 3. 블루투스 디바이스 페어링

```bash
# 블루투스 디바이스 연결 (HC-05/06)
sudo bluetoothctl
> scan on
> pair XX:XX:XX:XX:XX:XX
> trust XX:XX:XX:XX:XX:XX
> connect XX:XX:XX:XX:XX:XX

# RFCOMM 채널 바인딩
sudo rfcomm bind /dev/rfcomm0 XX:XX:XX:XX:XX:XX
sudo rfcomm bind /dev/rfcomm1 XX:XX:XX:XX:XX:XX
sudo rfcomm bind /dev/rfcomm2 XX:XX:XX:XX:XX:XX
sudo rfcomm bind /dev/rfcomm3 XX:XX:XX:XX:XX:XX
```

### 4. MySQL 데이터베이스 설정

```sql
-- 데이터베이스 연결 정보는 main.cpp에서 수정 가능
-- 기본값: localhost:3306, user1/1234, database: hometer
```

### 5. 서버 실행

```bash
./Server
```

출력 예시:
```
MySQL 연결 성공
fireModule (/dev/rfcomm0) 포트 열림
petModule (/dev/rfcomm1) 포트 열림
plantModule (/dev/rfcomm2) 포트 열림
homeModule (/dev/rfcomm3) 포트 열림
TCP 서버 시작됨 - 포트: 8080
스마트 홈 서버가 시작되었습니다.
TCP 포트: 8080
블루투스 데이터 수신 중...
종료하려면 Ctrl+C를 누르세요.
```

## 클라이언트 테스트

### 1. Python 기본 테스트 클라이언트

```bash
python3 client_test.py
```

### 2. Smart Window 전용 테스트 클라이언트

```bash
python3 smart_window_test.py
```
- 자동 테스트 시퀀스: 창문 열기/닫기/상태확인/각도설정 자동 테스트
- 대화형 모드: 수동으로 창문 제어

### 3. 수동 테스트 (telnet)

```bash
telnet 127.0.0.1 8080
window_open
window_status
set_open_angle=150
window_close
```

### Smart Window 응답 예시

```
클라이언트 → 서버: window_open
서버 → Arduino: OPEN
Arduino 응답: ACK OPEN
Arduino 응답: EVT OPENED
Arduino 상태: {"pose":"OPEN","angle":100}
```

## 아두이노 스케치 예시

```cpp
#include <SoftwareSerial.h>
SoftwareSerial BT(10, 11); // RX, TX

void setup() {
    BT.begin(9600);
    Serial.begin(9600);
}

void loop() {
    // 명령 수신
    if (BT.available()) {
        String cmd = BT.readString();
        cmd.trim();
        handleCommand(cmd);
    }
    
    // 센서 데이터 전송 (5분마다)
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 300000) {
        BT.println("iot01_fire_150_650.5");
        lastTime = millis();
    }
}

void handleCommand(String command) {
    if (command == "CMD_WINDOW_OPEN") {
        // 창문 열기 로직
        digitalWrite(WINDOW_PIN, HIGH);
    }
    // ... 기타 명령 처리
}
```

## 주요 특징

### 1. 비동기 처리
- **센서 데이터 수신**: Non-blocking I/O로 여러 디바이스 동시 처리
- **TCP 서버**: 멀티스레드로 여러 클라이언트 동시 연결 가능
- **데이터베이스**: Thread-safe한 singleton 패턴 적용

### 2. 확장성
- 새로운 센서 모듈 추가 용이
- 새로운 제어 명령어 쉽게 추가 가능
- 데이터베이스 스키마 확장 지원

### 3. 안정성
- 연결 오류 처리 및 자동 복구
- 데이터 파싱 오류 방지
- 메모리 누수 방지

## 데이터 흐름

### 센서 데이터 수집 흐름
1. Arduino → Bluetooth → C++ Server → MySQL DB

### 제어 명령 흐름  
1. Client App → TCP/IP → C++ Server → Bluetooth → Arduino

## 문제 해결

### 블루투스 연결 문제
```bash
# RFCOMM 채널 확인
sudo rfcomm show

# 블루투스 서비스 재시작
sudo systemctl restart bluetooth
```

### 포트 권한 문제
```bash
# 사용자를 dialout 그룹에 추가
sudo usermod -a -G dialout $USER
```

### TCP 포트 충돌
```cpp
// main.cpp에서 포트 번호 변경
TCPServer* tcpServer = new TCPServer(8081); // 8080 → 8081
```

## 향후 개선사항

- [ ] 웹 기반 클라이언트 인터페이스
- [ ] 실시간 데이터 시각화
- [ ] 모바일 앱 연동
- [ ] 보안 인증 기능 추가
- [ ] 로그 시스템 구축
- [ ] 설정 파일 기반 구성