#ifndef DATABASE_H_
#define DATABASE_H_

class database {
public:
    database();
    ~database(void);

    Json::Value groupby_type();
    Json::Value get_answer(const std::string &type, const std::string &content, const std::string &options);
    int insert_or_update_answer(const std::string &type, const std::string &content, const std::string &options, const std::string &answer, const std::string &notanswer);

    // execute
    void execute(const std::string &sql);

    // get_records and get_record
    Json::Value get_items(const std::string &sql);
    Json::Value get_item(const std::string &sql);

private:
    static int callback(void *p, int argc, char **argv, char **azColName);
    struct sqlite3 *db;
};

extern database db;

#endif /* DATABASE_H_ */
