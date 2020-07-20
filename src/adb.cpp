#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <list>
#include <json/value.h>
#include <pugixml.hpp>

#include "database.h"
#include "log.h"
#include "common.h"

#include "adb.h"

char i2c(int i) {
    return (char) ('A' + i);
}

int c2i(char c) {
    return c - 'A';
}

adb::adb() {
    system("adb disconnect 1>nul");
}

adb::~adb() {
}

// 连接安卓模拟器
void adb::connect(const std::string &ip, unsigned short port) {
    std::ostringstream oss;
    oss << "adb connect " << ip << ":" << port;
    auto result = exec(oss.str());
    if (result.find("connected to") == std::string::npos)
        throw std::runtime_error(result);
    logger->debug(result);
}

// 初始化
void adb::init() {
    logger->info("[初始化]");
    pull();
    int x, y;
    getxy(x, y, select("//node").attribute("bounds").value());
    width = 2 * x;
    height = 2 * y;
    logger->debug("分辨率：{}×{}", width, height);

    auto result = exec("adb shell ime set com.android.adbkeyboard/.AdbIME");
    if (result.find("Error:") != std::string::npos)
        throw std::runtime_error("未安装[ADBKeyboard.apk]");

    pugi::xml_node node;
    try {
        node = select("//node[@resource-id='cn.xuexi.android:id/comm_head_title']");
    } catch (const std::exception &ex) {
        throw std::runtime_error("请打开学习强国首页");
    }
    getxy(back_x, back_y, node.attribute("bounds").value(), 33, 10);
    logger->debug("返回按钮：({},{})", back_x, back_y);
}

// 学习文章
void adb::read() {
    int score1, score3;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    std::list<std::string> titles;

    logger->info("[学习]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    logger->info("[推荐]");
    tap(select_with_text("推荐"));
    logger->info("[刷新]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    for (;;) {
        // 积分界面
        score();

        // 阅读文章
        node = select("//node[@class='android.widget.ListView']/node[@index='1']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
            throw std::runtime_error("找不到[ 阅读文章 ]");
        score1 = atoi(sm[1].str().c_str());
        logger->info("[阅读文章]：已获{}分/每日上限6分", score1);

        // 文章学习时长
        node = select("//node[@class='android.widget.ListView']/node[@index='3']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
            throw std::runtime_error("找不到[ 文章学习时长 ]");
        score3 = atoi(sm[1].str().c_str());
        logger->info("[文章学习时长]：已获{}分/每日上限6分", score3);

        if (score1 >= 6 && score3 >= 6) {
            logger->info("[返回]");
            back();
            back();
            return;
        }

        // 计算平均每篇文章所需时长
        int delay = 20 * (6 - score3);
        if (!delay)
            delay = 20;

        // 首页
        logger->info("[返回]");
        back();
        back();

        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@resource-id='cn.xuexi.android:id/general_card_title_id']");
            // xpath_nodes 中选择不重复的为 valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                int x, y1, y2;
                getxy(x, y1, bounds, 1, 1, 1, 0);
                getxy(x, y2, bounds, 1, 1, 0, 1);
                return std::find(titles.begin(), titles.end(), this->get_text(node)) == titles.end() && y2 - y1 > 3;
            });
            logger->info("[学习文章]：发现 {} 篇文章", valid_xpath_nodes.size());
            bool is_left = false;
            for (auto &xpath_node : valid_xpath_nodes) {
                if (c == 6) {
                    is_left = true;
                    break;
                }
                auto node = xpath_node.node();
                text = get_text(node);
                logger->info("[学习文章]：{}. {}({}秒)", titles.size() + 1, text, delay);
                tap(node, 2, false);
                for (int i = 0; i < delay / 20; i++) {
                    if (rand() % 2)
                        swipe_up(0, false);
                    else
                        swipe_down(0, false);
                    std::this_thread::sleep_for(std::chrono::seconds(20 + std::rand() % 3));
                }
                titles.push_back(text);
                c++;
                back();
            }
            if (exist_with_text("你已经看到我的底线了") || (valid_xpath_nodes.size() && titles.size() % 12 == 0)) {
                swipe_left();
                logger->info("[刷新]");
                tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
                continue;
            }
            if (!is_left)
                swipe_up();
        }
    } //for(;;)
} //read()

// 视听学习
void adb::listen() {
    int score2, score4;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    std::list<std::string> titles;

    logger->info("[电视台]");
    tap(select_with_text("电视台"));
    logger->info("[联播频道]");
    tap(select_with_text("联播频道"));
    logger->info("[刷新]");
    tap(select_with_text("电视台"));
    for (;;) {
        // 积分界面
        score();

        // 视听学习
        node = select("//node[@class='android.widget.ListView']/node[@index='2']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
            throw std::runtime_error("找不到[ 视听学习 ]");
        score2 = atoi(sm[1].str().c_str());
        logger->info("[视听学习]：已获{}分/每日上限6分", score2);

        // 视听学习时长
        node = select("//node[@class='android.widget.ListView']/node[@index='4']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
            throw std::runtime_error("找不到[ 视听学习时长 ]");
        score4 = atoi(sm[1].str().c_str());
        logger->info("[视听学习时长]：已获{}分/每日上限6分", score4);

        if (score2 >= 6 && score4 >= 6) {
            logger->info("[返回]");
            back();
            back();
            return;
        }

        // 计算平均每个视听所需时长
        int delay = 30 * (6 - score4);
        if (!delay)
            delay = 30;

        // 首页
        logger->info("[返回]");
        back();
        back();

        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@resource-id='cn.xuexi.android:id/general_card_title_id']");
            // xpath_nodes 中选择不重复的为 valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                int x, y1, y2;
                getxy(x, y1, bounds, 1, 1, 1, 0);
                getxy(x, y2, bounds, 1, 1, 0, 1);
                return std::find(titles.begin(), titles.end(), this->get_text(node)) == titles.end() && y2 - y1 > 3;
            });
            logger->info("[视听学习]：发现 {} 个视听", valid_xpath_nodes.size());
            bool is_left = false;
            for (auto &xpath_node : valid_xpath_nodes) {
                if (c == 6) {
                    is_left = true;
                    break;
                }
                auto node = xpath_node.node();
                text = get_text(node);
                logger->info("[视听学习]：{}. {}({}秒)", titles.size() + 1, text, delay);
                tap(node, 2, false);
                std::this_thread::sleep_for(std::chrono::seconds(delay + (delay / 30) * (std::rand() % 4)));
                titles.push_back(text);
                c++;
                back();
            }
            if (exist_with_text("你已经看到我的底线了") || (valid_xpath_nodes.size() && titles.size() % 12 == 0)) {
                swipe_right();
                logger->info("[刷新]");
                tap(select_with_text("电视台"));
                continue;
            }
            if (!is_left)
                swipe_up();
        }

    } //for(;;)
} //listen()

// 每日答题
void adb::daily() {
    int score5;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 每日答题
    node = select("//node[@class='android.widget.ListView']/node[@index='5']/node[@index='2']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
        throw std::runtime_error("找不到[ 每日答题 ]");
    score5 = atoi(sm[1].str().c_str());
    logger->info("[每日答题]：已获{}分/每日上限6分", score5);

    if (score5 >= 6) {
        logger->info("[返回]");
        back();
        back();
        return;
    }

    // 去答题
    for (;;) {
        node = select("//node[@class='android.widget.ListView']/node[@index='5']/node[@index='3']");
        std::string bounds = node.attribute("bounds").value();
        int x, y1, y2;
        getxy(x, y1, bounds, 1, 1, 1, 0);
        getxy(x, y2, bounds, 1, 1, 0, 1);
        if (y2 - y1 > 3) {
            logger->info("[去答题]");
            tap(node, 10);
            pull();
            break;
        }
        swipe_up();
    }

    for (int i = 1;; i++) {
        auto type = get_text(select("//node[@text='填空题' or @text='单选题' or @text='多选题' or @content-desc='填空题' or @content-desc='单选题' or @content-desc='多选题']"));
        //ui.save_file(("log/MuMU-" + std::to_string(i) + "-" + type + ".xml").c_str());
        if (type == "填空题") {
            // 华为真机、360真机
            auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/../../node");
            std::string content_utf;
            for (auto &xpath_node : xpath_nodes) {
                content_utf += xpath_node.node().attribute("text").value();
                content_utf += xpath_node.node().attribute("content-desc").value();
            }
            // MuMu模拟器
            if (content_utf.find(std::to_string((i - 1) % 10 + 1) + " /10") != std::string::npos) {
                xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/../node");
                content_utf.clear();
                for (auto &xpath_node : xpath_nodes) {
                    content_utf += xpath_node.node().attribute("text").value();
                    content_utf += xpath_node.node().attribute("content-desc").value();
                }
            }
            std::cout << std::endl;
            logger->info("[{}] {}. {}", type, i, utf2gbk(content_utf));

            xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/following-sibling::node[1]");
            std::list<int> answer_indexs;
            for (unsigned answer_index = 0; answer_index < xpath_nodes.size(); answer_index++)
                answer_indexs.push_back(answer_index);
            for (int answer_index : answer_indexs) {
                for (;;) {
                    auto node = xpath_nodes[answer_index].node();
                    int x, y1, y2;
                    getxy(x, y1, node.attribute("bounds").value(), 1, 1, 1, 0);
                    getxy(x, y2, node.attribute("bounds").value(), 1, 1, 0, 1);
                    if (y2 - y1 > 3) {
                        tap(node, 0, false);
                        input_text("不忘初心牢记使命");
                        logger->info("[默认答案]：不忘初心牢记使命");
                        break;
                    }
                    logger->warn("Can't see any blank!");
                    swipe_up();
                    xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/following-sibling::node[1]");
                }
            }
        } else if (type == "单选题" || type == "多选题") {
            std::cout << std::endl;
            auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node[1]");
            std::string content_utf;
            for (auto &xpath_node : xpath_nodes) {
                content_utf += xpath_node.node().attribute("text").value();
                content_utf += xpath_node.node().attribute("content-desc").value();
                // 华为真机和360真机的界面xml，8个0xc20xa0前面多个空格0x20，而MuMu模拟器的界面xml，只有8个0xc20xa0，前面没有空格\x20
                // 参考[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
                if (std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")))
                    logger->warn("More SPACE(0x20) before NO-BREAK SPACE(0xC2A0)!");
                //content_utf = std::regex_replace(content_utf, std::regex("\x20((\\xc2\\xa0){8})"), "$1");
                //content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
                content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
            }
            logger->info("[{}] {}. {}", type, i, utf2gbk(content_utf));

            xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/node//node[@index='2']");
            std::string options_utf;
            char mark = 'A';
            for (auto &xpath_node : xpath_nodes) {
                std::string option_utf;
                option_utf += mark++;
                option_utf += ". ";
                option_utf += xpath_node.node().attribute("text").value();
                option_utf += xpath_node.node().attribute("content-desc").value();
                logger->info(utf2gbk(option_utf));
                //options_utf += option_utf + "'||CHAR(13)||CHAR(10)||'";
                options_utf += option_utf + "\r\n";
            }
            //options_utf = options_utf.substr(0, options_utf.size() - 24);
            options_utf = options_utf.substr(0, options_utf.size() - 2);

            std::list<int> answer_indexs;
            if (type == "单选题") {
                auto result = db.get_answer("challenge", content_utf, options_utf);
                logger->info("[答案提示]：{}", dump(result));
                auto answer = result["answer"].asString();
                auto notanswer = result["notanswer"].asString();
                if (answer.size()) {
                    logger->info("[自动答案]：{}", answer);
                    answer_indexs.push_back(c2i(answer[0]));
                } else {
                    if (notanswer.size() >= xpath_nodes.size())
                        throw std::runtime_error("数据库非答案有误");
                    int answer_index;
                    do {
                        answer_index = rand() % xpath_nodes.size();
                    } while (notanswer.find(i2c(answer_index)) != std::string::npos);
                    logger->info("[随机答案]：{}", i2c(answer_index));
                    answer_indexs.push_back(answer_index);
                }
            } else if (type == "多选题") {
                logger->info("[默认答案]：全选");
                for (unsigned answer_index = 0; answer_index < xpath_nodes.size(); answer_index++)
                    answer_indexs.push_back(answer_index);
            }

            for (int answer_index : answer_indexs) {
                for (;;) {
                    auto node = xpath_nodes[answer_index].node();
                    int x, y1, y2;
                    getxy(x, y1, node.attribute("bounds").value(), 1, 1, 1, 0);
                    getxy(x, y2, node.attribute("bounds").value(), 1, 1, 0, 1);
                    if (y2 - y1 > 3) {
                        tap(node, 0, false);
                        break;
                    }
                    swipe_up();
                    xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/node//node[@index='2']");
                }
            }
        } else
            throw std::runtime_error("[每日答题] 未知题型：" + type);

        pull();
        tap(select_with_text("确定"), 1, false);
        tap(select_with_text("确定"));

        if (i % 10 > 0)
            continue;
        if (exist_with_text("领取奖励已达今日上限")) {
            logger->info("[领取奖励已达今日上限]");
            logger->info("[返回]");
            back();
            back();
            back();
            return;
        }
        logger->info("[再来一组]");
        tap(select_with_text("再来一组"));
    }
}

void adb::challenge(int max) {
    int score8;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 挑战答题
    node = select("//node[@class='android.widget.ListView']/node[@index='8']/node[@index='2']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限6分")))
        throw std::runtime_error("找不到[ 挑战答题 ]");
    score8 = atoi(sm[1].str().c_str());
    logger->info("[挑战答题]：已获{}分/每日上限6分", score8);

    if (score8 >= 6) {
        logger->info("[返回]");
        back();
        back();
        return;
    }

    // 去看看
    for (;;) {
        node = select("//node[@class='android.widget.ListView']/node[@index='8']/node[@index='3']");
        std::string bounds = node.attribute("bounds").value();
        int x, y1, y2;
        getxy(x, y1, bounds, 1, 1, 1, 0);
        getxy(x, y2, bounds, 1, 1, 0, 1);
        if (y2 - y1 > 3) {
            logger->info("[去看看]");
            tap(node, 10);
            pull();
            break;
        }
        swipe_up();
    }

    for (int i = 1;; i++) {
        std::cout << std::endl;
        auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node[1]");

        // 题干
        std::string content_utf;
        for (auto &xpath_node : xpath_nodes) {
            content_utf += xpath_node.node().attribute("text").value();
            content_utf += xpath_node.node().attribute("content-desc").value();
            // 华为真机和360真机的界面xml，8个0xc20xa0前面多个空格0x20，而MuMu模拟器的界面xml，只有8个0xc20xa0，前面没有空格\x20
            // 参考[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
            if (std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")))
                logger->warn("More SPACE(0x20) before NO-BREAK SPACE(0xC2A0)!");
            //content_utf = std::regex_replace(content_utf, std::regex("\x20((\\xc2\\xa0){8})"), "$1");
            //content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
            content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
        }

        logger->info("[挑战答题] {}. {}", i, utf2gbk(content_utf));

        // 选项
        xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/node//node[@index='1']");
        std::string options_utf;
        char mark = 'A';
        for (auto &xpath_node : xpath_nodes) {
            std::string option_utf;
            option_utf += mark++;
            option_utf += ". ";
            option_utf += xpath_node.node().attribute("text").value();
            option_utf += xpath_node.node().attribute("content-desc").value();
            logger->info(utf2gbk(option_utf));
            //options_utf += option_utf + "'||CHAR(13)||CHAR(10)||'";
            options_utf += option_utf + "\r\n";
        }
        //options_utf = options_utf.substr(0, options_utf.size() - 24);
        options_utf = options_utf.substr(0, options_utf.size() - 2);

        auto result = db.get_answer(i <= max ? "challenge" : "max", content_utf, options_utf);
        logger->info("[答案提示]：{}", dump(result));
        auto answer = result["answer"].asString();
        auto notanswer = result["notanswer"].asString();
        int answer_index;
        if (answer.size()) {
            logger->info("[自动答案]：{}", answer);
            answer_index = c2i(answer[0]);
        } else {
            if (notanswer.size() >= xpath_nodes.size())
                throw std::runtime_error("数据库非答案有误");
            do {
                answer_index = rand() % xpath_nodes.size();
            } while (notanswer.find(i2c(answer_index)) != std::string::npos);
            logger->info("[随机答案]：{}", i2c(answer_index));
        }
        for (;;) {
            auto node = xpath_nodes[answer_index].node();
            int x, y1, y2;
            getxy(x, y1, node.attribute("bounds").value(), 1, 1, 1, 0);
            getxy(x, y2, node.attribute("bounds").value(), 1, 1, 0, 1);
            if (y2 - y1 > 3) {
                tap(node, 3);
                break;
            }
            swipe_up();
            xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/node//node[@index='1']");
        }

        // 如果因为字体太大点空了，则自动休息30秒，等待自行结束
        std::ostringstream oss;
        oss << i << " /10";
        if (exist_with_text(oss.str())) {
            logger->warn("No answer response!");
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }

        if (exist_with_text("挑战结束")) {
            node = select("//node[@text='挑战结束' or @content-desc='挑战结束']/following-sibling::node[1]");
            text = get_text(node);
            if (!std::regex_search(text, sm, std::regex("本次答对 (\\d+) 题")))
                throw std::runtime_error("找不到[ 本次答对 (\\d+) 题 ]");
            score8 = atoi(sm[1].str().c_str());
            logger->info("[挑战答题]：本次答对 {} 题", score8);
            logger->info("[结束本局]");
            tap(select_with_text("结束本局"));

            if (score8 >= max) {
                logger->info("[返回]");
                back();
                back();
                back();
                return;
            }

            logger->info("[再来一局]");
            tap(select_with_text("再来一局"));
            i = 0;
            continue;
        }
    }
}

// 回退
void adb::back(int64_t delay, bool is_pull) {
    tap(back_x, back_y, delay, is_pull);
}

// 首页 -> 学习积分
void adb::score() {
    logger->info("[我的]");
    tap(select_with_text("我的"));

    for (int i = 0; i < 3; i++) {
        logger->info("[学习积分]");
        tap(select_with_text("学习积分"), 10);
        if (exist_with_text("积分明细")) {
            pull();
            return;
        }
        back();
    }
    throw std::runtime_error("点不动[学习积分]");
}

// 点击坐标
void adb::tap(int x, int y, int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input tap " << x << " " << y;
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// 点击node
void adb::tap(const pugi::xml_node &node, int64_t delay, bool is_pull) {
    int x, y;
    getxy(x, y, node.attribute("bounds").value());
    tap(x, y, delay, is_pull);
}

// 上滑
void adb::swipe_up(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 65) / 100.0) << " " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 25) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// 下滑
void adb::swipe_down(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 25) / 100.0) << " " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 65) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// 左滑
void adb::swipe_left(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 9 + 80) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (width * (std::rand() % 10 + 10) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// 右滑
void adb::swipe_right(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 10) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (width * (std::rand() % 9 + 80) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// 获取界面xml
void adb::pull() {
    auto result = exec("adb shell uiautomator dump /sdcard/ui.xml");
    if (result.size() && result.find("UI hierchary dumped to") == std::string::npos)
        throw std::runtime_error(result);
    logger->debug(result);

    result = exec("adb pull /sdcard/ui.xml log/ui.xml");
    if (result.find("1 file pulled") == std::string::npos)
        throw std::runtime_error(result);
    logger->debug(result);

    auto result1 = ui.load_file("log/ui.xml");
    if (!result1)
        throw std::runtime_error(result1.description());
}

// 键盘输入，支持中文
void adb::input_text(const std::string &msg) {
    std::ostringstream oss;
    oss << "adb shell am broadcast -a ADB_INPUT_TEXT --es msg \"" << msg << "\"";
    exec(oss.str());
}

// 执行底层命令
std::string adb::exec(const std::string &cmd) {
    logger->debug("> " + cmd);
    system((cmd + " 1>log/adb.txt 2>log/err.txt").c_str());
    auto err = utf2gbk(trim_copy(read_file("log/err.txt")));
    if (err.length())
        throw std::runtime_error(err);
    return utf2gbk(trim_copy(read_file("log/adb.txt")));
}

// bounds = "[x1,y1][x2,y2]"
// x = (x1 * dx1 + x2 * dx2) / (dx1 + dx2)
// y = (y1 * dy1 + y2 * dy2) / (dy1 + dy2)
void adb::getxy(int &x, int &y, const std::string &bounds, int dx1, int dx2, int dy1, int dy2) {
    std::regex rgx_x("\\[(\\d+),"), rgx_y(",(\\d+)\\]");
    x = 0;
    y = 0;
    auto it_x = std::sregex_iterator(bounds.begin(), bounds.end(), rgx_x);
    x += dx1 * atoi((*it_x++)[1].str().c_str());
    x += dx2 * atoi((*it_x++)[1].str().c_str());
    x /= (dx1 + dx2);
    auto it_y = std::sregex_iterator(bounds.begin(), bounds.end(), rgx_y);
    y += dy1 * atoi((*it_y++)[1].str().c_str());
    y += dy2 * atoi((*it_y++)[1].str().c_str());
    y /= (dy1 + dy2);
}

// 选取特定文本的节点
pugi::xml_node adb::select_with_text(const std::string &text) {
    std::ostringstream oss;
    oss << "//node[@text='" << text << "' or @content-desc='" << text << "']";
    return select(oss.str());
}

// 判断特定文本的节点是否存在
bool adb::exist_with_text(const std::string &text) {
    try {
        select_with_text(text);
    } catch (...) {
        return false;
    }
    return true;
}

// 选取xpath节点
pugi::xml_node adb::select(const std::string &xpath) {
    auto xpath_utf = gbk2utf(xpath);
    auto node = ui.select_node(xpath_utf.c_str()).node();
    if (node.empty())
        throw std::runtime_error("找不到[ " + xpath + " ]");
    return node;
}

// 判断xpath节点是否存在
bool adb::exist(const std::string &xpath) {
    try {
        select(xpath);
    } catch (...) {
        return false;
    }
    return true;
}

// 获取界面文本
std::string adb::get_text(const pugi::xml_node &node) {
    std::string text;
    text += node.attribute("text").value();
    text += node.attribute("content-desc").value();
    return utf2gbk(text);
}
