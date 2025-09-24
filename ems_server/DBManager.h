#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <mysql/mysql.h>
#include <string>
#include <mutex>

class DBManager
{
public:
    static DBManager& instance()
    {
        static DBManager instance;
        return instance;
    }

    bool connect(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& db,
                 unsigned int port);

    void insertHomeData(float temperature, float humidity, float illumination);
    void insertFireData(const std::string& fireState, int fireData,
                        const std::string& gasState, float gasData);
    void insertPetData(const std::string& foodData,
                       const std::string& waterData,
                       const std::string& toiletState);
    void insertPlantData(float soilData, float tempData, float humiData, float lightData);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    MYSQL* m_conn;
    std::mutex mtx;
};

#endif // DBMANAGER_H
