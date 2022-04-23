#include "DBC.h"

namespace zwdbc
{
    void SQLQueryError(const std::string& sqlstr, MYSQL* sqlcon)
    {
        std::cerr << "sql query error=================================================>" << std::endl;
        std::cerr << "sql :" << sqlstr << std::endl;
        std::cerr << "sqlcon :" << mysql_ping(sqlcon) << std::endl;
        std::cerr << mysql_error(sqlcon) << std::endl;
        std::cerr << "sql query error=================================================>" << std::endl;
    }

    MysqlCon::MysqlCon()
    {
        conn = new MYSQL;
        mysql_init(conn);
        if (!conn) { std::cerr << "init error! \n"; }
    }

    //  create a db connection
    MysqlCon::MysqlCon(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port)
    {
        conn = new MYSQL;
        mysql_init(conn);
        if (!conn) { std::cerr << "init error! \n"; }
        conn = mysql_real_connect(conn, host, user, pwd, dbname, port, NULL, 0);
        if (!conn) { std::cerr << "connect error! \n"; }
        mysql_query(conn, "set names utf8");
    }

    MysqlCon::MysqlCon(MYSQL* conn)
    {
        this->conn = conn;
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
        state = false;
    }

    //MysqlQuery& MysqlQuery::operator=(MysqlQuery&& src)
    //{
    //    if (this != &src)
    //    {
    //        row_num = src.row_num;
    //        field_num = src.field_num;
    //        res = src.res;
    //        src.res = nullptr;
    //        field = src.field;
    //        src.field = nullptr;
    //        state = src.state;
    //    }
    //    return *this;
    //}

    //  do a sql query
    //  result save in private field 'res'
    //  use getrow() to get one row of result
    bool MysqlQuery::query(const std::string& sqlstr, MYSQL* conn)
    {
        const char* s = sqlstr.data();
        if (mysql_query(conn, s))
        {
            SQLQueryError(sqlstr, conn);
            return state = false;
        }
        state = true;
        res = mysql_store_result(conn);
        if (!res)
        {
            row_num = field_num = 0;
            return true;
        }

        row_num = mysql_num_rows(res);
        field_num = mysql_num_fields(res);
        return true;
    }

    // get the name of your select field at index
    char* MysqlQuery::getfieldname(int index)
    {
        field = mysql_fetch_field_direct(res, index);
        return field->name;
    }

    //  get the next row of result
    bool MysqlQuery::nextline()
    {
        row = mysql_fetch_row(res);
        if (!row) return false;
        return true;
    }

    Connectpool::Connectpool(const char* host, const char* user, const char* pwd, const char* dbname, unsigned int port, int num)
        :host(host), user(user), pwd(pwd), dbname(dbname), port(port)
    {
        for (int i = 0; i < num; i++)
        {
            pool.emplace(Connect(host, user, pwd, dbname, port));
        }
        run = true;
    }

    Connectpool::~Connectpool()
    {
        std::unique_lock<std::mutex> lock{ _m };
        while (pool.size())
        {
            mysql_close(pool.front());
            pool.pop();
        }
        run = false;
    }

    MysqlQuery Connectpool::query(const std::string& sqlstr)
    {
        //std::cout << "\nsql : " << sqlstr << "\n";
        MysqlQuery res;
        MYSQL* conn = getCon();
        if (mysql_ping(conn)) conn = Connect(host, user, pwd, dbname, port);
        mysql_query(conn, "set names utf8mb4");
        res.query(sqlstr, conn);
        addCon(conn);
        return res;
    }

    MYSQL* Connectpool::getCon()
    {
        MYSQL* res;
        std::unique_lock<std::mutex> lock{ _m };
        cv.wait(lock, [this]() {return !run || !pool.empty();});
        if(pool.empty()) return nullptr;
        res = pool.front();
        pool.pop();

        return res;
    }

    void Connectpool::addCon(MYSQL* conn)
    {
        std::unique_lock<std::mutex> lock{ _m };
        if(pool.size() < MAXCONNUM) pool.emplace(conn);
    }

    MYSQL* Connectpool::Connect(const char* host, const  char* user, const  char* pwd, const char* dbname, unsigned int port)
    {
        MYSQL* conn = new MYSQL;
        mysql_init(conn);
        if (!conn) { std::cerr << "init error! \n"; }
        conn = mysql_real_connect(conn, host, user, pwd, dbname, port, NULL, 0);
        if (!conn) { std::cerr << "connect error! \n"; }
        mysql_query(conn, "set names utf8");
        if (!conn)
        {
            std::cerr << "dbc pool init failed !\n";
            return nullptr;
        }
        return conn;
    }

}

