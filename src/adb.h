#ifndef ADB_H_
#define ADB_H_

class adb {
private:
    pugi::xml_document ui;
    int width = 0;
    int height = 0;
    int back_x = 0;
    int back_y = 0;
    int exit_x = 0;
    int exit_y = 0;
    std::string my_name;

public:
    adb();
    virtual ~adb();

    // 学习前准备
    void connect(const std::string &ip, unsigned short port);
    void init();

    // 各种学习
    void read(bool is_ppp = true);
    void listen(bool is_ppp = true);
    void daily(bool is_training = false);
    void challenge(bool is_ppp = true, bool is_training = false);
    void race2();
    void race4(int count = 31);
    void local();

    // 导航
    void back(int64_t delay = 2, bool is_pull = true);
    void score();
    void store();
    void repair();

    // 测试
    void test();
private:

    // 人工操作
    void tap(int x, int y, int64_t delay = 2, bool is_pull = true);
    void tap(const pugi::xml_node &node, int64_t delay = 2, bool is_pull = true);
    void swipe_up(int64_t delay = 2, bool is_pull = true);
    void swipe_down(int64_t delay = 2, bool is_pull = true);
    void swipe_left(int64_t delay = 2, bool is_pull = true);
    void swipe_right(int64_t delay = 2, bool is_pull = true);

    // adb底层命令
    void pull();
    void input_text(const std::string &msg);
    std::string exec(const std::string &cmd);

    // 辅助函数
    void getxy(int &x, int &y, const std::string &bounds, int dx1 = 1, int dx2 = 1, int dy1 = 1, int dy2 = 1);
    pugi::xml_node select_with_text(const std::string &text);
    bool exist_with_text(const std::string &text);
    pugi::xml_node select(const std::string &xpath);
    bool exist(const std::string &xpath);
    std::string get_text(const pugi::xml_node &node);

};

#endif /* ADB_H_ */
