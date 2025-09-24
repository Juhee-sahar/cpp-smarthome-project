cmake_minimum_required(VERSION 3.10)
project(Server)

set(CMAKE_CXX_STANDARD 17)

# MySQL 개발 라이브러리 찾기 (Ubuntu)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

# pthread 라이브러리 찾기 (멀티스레딩용)
find_package(Threads REQUIRED)

add_executable(Server
    main.cpp
    BluetoothManager.cpp
    DBManager.cpp
    TCPServer.cpp
)

# include 경로와 링크 라이브러리 설정
target_include_directories(Server PRIVATE ${MYSQL_INCLUDE_DIRS})
target_link_libraries(Server PRIVATE 
    ${MYSQL_LIBRARIES} 
    Threads::Threads
)