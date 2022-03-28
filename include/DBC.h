#ifndef MYSQLMysqlQuery_H
#define MYSQLMysqlQuery_H

#include "mysql.h"
#include <iostream>
#include <cstring>


class MysqlCon
{
public:
    MysqlCon();
    MysqlCon(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port = 3306);
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

    bool query(const std::string& sqlstr, MysqlCon& conn);

    char* getfieldname(int index);
    bool nextline();
    inline u_int64_t rowNum() { return row_num; }
    inline u_int32_t fieldNum() { return field_num; }
    inline MYSQL_ROW& getRow() { return row; };

private:
    u_int64_t row_num;
    u_int32_t field_num;
    MYSQL_RES *res;
    MYSQL_ROW row;
    MYSQL_FIELD *field;

};





#endif
