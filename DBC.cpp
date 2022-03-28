#include "DBC.h"

MysqlCon::MysqlCon()
{
    conn = new MYSQL;
    mysql_init(conn);
    if (!conn) { std::cerr << "init error! \n"; }
}

MysqlCon::MysqlCon(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port)
{
    mysql_init(conn);
    if (!conn) { std::cerr << "init error! \n"; }
    conn = mysql_real_connect(conn, host, user, pwd, dbname, port, NULL, 0);
    if (!conn) { std::cerr << "connect error! \n"; }
    mysql_query(conn, "set names utf8");
}

bool MysqlCon::Connect(const char* host, const char* user, const char* pwd, const char* dbname, unsigned int port)
{
    conn = mysql_real_connect(conn, host, user, pwd, dbname, port, NULL, 0);
    if (!conn) return false;
    mysql_query(conn, "set names utf8");
    return true;
}

MysqlCon::~MysqlCon()
{
    std::cout << "sqlcon discontruct\n";
    mysql_close(conn);
}



MysqlQuery::MysqlQuery()
{
    field_num = row_num = 0;
}

bool MysqlQuery::query(const std::string &sqlstr, MysqlCon& conn)
{
    const char* s = sqlstr.data();
    if (mysql_query(conn.GetCon(), s)) return false;
    res = mysql_store_result(conn.GetCon());
    if (!res)
    {
        row_num = field_num = 0;
        return true;
    }

    row_num = mysql_num_rows(res);
    field_num = mysql_num_fields(res);
    return true;
}

char* MysqlQuery::getfieldname(int index)
{
    field = mysql_fetch_field_direct(res, index);
    return field->name;
}

bool MysqlQuery::nextline()
{
    row = mysql_fetch_row(res);
    if (!row) return false;
    return true;
}
