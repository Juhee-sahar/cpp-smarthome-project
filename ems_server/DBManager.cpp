#include "DBManager.h"
#include <iostream>
#include <sstream>

DBManager::DBManager()
    : m_conn(nullptr)
{
}

DBManager::~DBManager()
{
    if (m_conn)
    {
        mysql_close(m_conn);
    }
}

bool DBManager::connect(const std::string& host,
                        const std::string& user,
                        const std::string& password,
                        const std::string& db,
                        unsigned int port)
{
    m_conn = mysql_init(nullptr);
    if (!m_conn)
    {
        std::cerr << "MySQL init 실패" << std::endl;
        return false;
    }

    if (!mysql_real_connect(m_conn, host.c_str(), user.c_str(), password.c_str(),
                            db.c_str(), port, nullptr, 0))
    {
        std::cerr << "MySQL 연결 실패: " << mysql_error(m_conn) << std::endl;
        return false;
    }

    std::cout << "MySQL 연결 성공" << std::endl;
    return true;
}

void DBManager::insertHomeData(float temperature, float humidity, float illumination)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream oss;
    oss << "INSERT INTO home_env (temperature, humidity, illumination, home_id) VALUES ("
        << temperature << ", " << humidity << ", " << illumination << ", 1);";

    if (mysql_query(m_conn, oss.str().c_str()))
    {
        std::cerr << "insertHomeData 실패: " << mysql_error(m_conn) << std::endl;
    }
}

void DBManager::insertFireData(const std::string& fireState, int fireData,
                               const std::string& gasState, float gasData)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream oss;
    oss << "INSERT INTO fire_events (fire_level, fire_status, level, level_status, home_id) VALUES ("
        << fireData << ", '" << fireState << "', "
        << gasData << ", '" << gasState << "', 1);";

    if (mysql_query(m_conn, oss.str().c_str()))
    {
        std::cerr << "insertFireData 실패: " << mysql_error(m_conn) << std::endl;
    }
}

void DBManager::insertPetData(const std::string& foodData,
                              const std::string& waterData,
                              const std::string& toiletState)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream oss;
    oss << "INSERT INTO pet_status (food, water, toilet, home_id) VALUES ('"
        << foodData << "', '" << waterData << "', '"
        << toiletState << "', 1);";

    if (mysql_query(m_conn, oss.str().c_str()))
    {
        std::cerr << "insertPetData 실패: " << mysql_error(m_conn) << std::endl;
    }
}

void DBManager::insertPlantData(float soilData, float tempData, float humiData, float lightData)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream oss;
    oss << "INSERT INTO plant_env (temperature, soil_moisture, illumination, humidity, home_id) VALUES ("
        << tempData << ", " << soilData << ", " << lightData << ", " << humiData << ", 1);";

    if (mysql_query(m_conn, oss.str().c_str()))
    {
        std::cerr << "insertPlantData 실패: " << mysql_error(m_conn) << std::endl;
    }
}
