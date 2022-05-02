#ifndef MYSQLMysqlQuery_H
#define MYSQLMysqlQuery_H

#include "mysql.h"
#include <iostream>
#include <cstring>
#include <queue>
#include <future>

namespace zwdbc
{
    constexpr u_int32_t MAXCONNUM = 10;

    class MysqlCon
    {
    public:
        MysqlCon();
        MysqlCon(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port = 3306);
        MysqlCon(MYSQL* conn);
        ~MysqlCon();
        bool Connect(const char* host, const char* user, const char* pwd, const char* dbname, unsigned int port = 3306);
        inline MYSQL* GetCon() { return conn; }

    private:
        MYSQL* conn;
    };

    class MysqlQuery
    {
    public:
        MysqlQuery();

       // MysqlQuery& operator= (MysqlQuery &&);

        bool query(const std::string& sqlstr, MYSQL* conn);

        char* getfieldname(int index);
        bool nextline();

        inline u_int64_t rowNum() { return row_num; }
        inline u_int32_t fieldNum() { return field_num; }
        inline MYSQL_ROW& getRow() { return row; };
        inline bool getState() { return state; }

    private:
        u_int64_t row_num;
        u_int32_t field_num;
        MYSQL_RES* res;
        MYSQL_ROW row;
        MYSQL_FIELD* field;
        bool state;
    };

    class Connectpool
    {
    public:
        Connectpool(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port = 3306, int num = 4);
        Connectpool() = default;
        ~Connectpool();

        void Init(int num = 4);
        MysqlQuery query(const std::string& sqlstr);

        const char* host;
        const  char* user;
        const  char* pwd;
        const char* dbname;
        unsigned int port = 3306;
    private:
        MYSQL* getCon();
        void addCon(MYSQL* conn);
        MYSQL* Connect(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port = 3306);

        std::queue<MYSQL*>pool;
        std::mutex _m;
        std::atomic<bool> run;
        std::condition_variable cv;
    };
}


#endif
