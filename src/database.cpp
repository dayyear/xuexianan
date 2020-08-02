#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <json/writer.h>
#include <json/reader.h>

#include "database.h"

database::database() {
    if (sqlite3_open_v2(".\\xuexi.sqlite", &db, SQLITE_OPEN_READWRITE, NULL) == 0)
        return;
    if (sqlite3_open_v2("..\\xuexi.sqlite", &db, SQLITE_OPEN_READWRITE, NULL) == 0)
        return;
    std::cerr << sqlite3_errmsg(db) << std::endl;
    exit(1);
} //database::database

database::~database(void) {
    sqlite3_close(db);
} //database::~database

Json::Value database::groupby_type() {
    return get_items("select type, count(*) c from quiz group by type order by type");
}

void write_file(const std::string &file_name, const std::string &s);
Json::Value database::get_answer(const std::string &type, const std::string &content, const std::string &options) {
    auto sql = sqlite3_mprintf("select answer, notanswer from quiz where type='%q' and content='%q' and options='%q'", type.c_str(), content.c_str(), options.c_str());
    write_file("log/sql.txt", sql);
    auto result = get_items(sql)[0];
    sqlite3_free(sql);
    return result;
}

int database::insert_or_update_answer(const std::string &type, const std::string &content, const std::string &options, const std::string &answer) {
    // https://www.runoob.com/cprogramming/c-function-strftime.html
    time_t rawtime;
    char buffer[80];
    time(&rawtime);
    std::strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", std::localtime(&rawtime));

    auto result = get_answer(type, content, options);
    std::string sql;
    if (result.isNull())
        sql = sqlite3_mprintf("insert into quiz(type, content, options, answer, time) values('%q', '%q', '%q', '%q', '%q')", type.c_str(), content.c_str(), options.c_str(), answer.c_str(), buffer);
    else if (result["answer"].asString() != answer)
        sql = sqlite3_mprintf("update quiz set answer='%q', time='%q' where type='%q' and content='%q' and options='%q'", answer.c_str(), buffer, type.c_str(), content.c_str(), options.c_str());
    if (sql.size()) {
        execute(sql);
        return 1;
    }
    return 0;
}

void database::execute(const std::string &sql) {
    char *zErrMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg)) {
        sqlite3_free(zErrMsg);
        throw std::runtime_error(std::string("SQL error, ") + zErrMsg);
    }
}

Json::Value database::get_items(const std::string &sql) {
    Json::Value items;
    char *zErrMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), &database::callback, static_cast<void*>(&items), &zErrMsg)) {
        sqlite3_free(zErrMsg);
        throw std::runtime_error(std::string("SQL error, ") + zErrMsg);
    }
    return items;
}

Json::Value database::get_item(const std::string &sql) {
    Json::Value items = get_items(sql);
    if (!items.size())
        throw std::runtime_error(std::string("Find no record with sql: ") + sql);
    if (items.size() > 1)
        throw std::runtime_error(std::string("Find too many records with sql: ") + sql);
    return items[0];
}

int database::callback(void *p, int argc, char **argv, char **azColName) {
    auto pitems = static_cast<Json::Value*>(p);
    Json::Value item;
    for (auto i = 0; i < argc; i++)
        item[azColName[i]] = (argv[i]) ? (argv[i]) : "";
    pitems->append(item);
    return 0;
}

database db;
