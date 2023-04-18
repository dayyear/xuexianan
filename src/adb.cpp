#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <list>
#include <json/writer.h>
#include <json/reader.h>
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

std::list<std::string> get_titles(const std::string &my_name) {
    Json::Value titles_json;
    std::ifstream ifs("log/" + my_name + ".json", std::ios::binary | std::ios::in);
    if (ifs.is_open())
        ifs >> titles_json;
    ifs.close();

    std::list<std::string> titles;
    for (const auto &title : titles_json)
        titles.push_back(utf2gbk(title.asString()));
    return titles;
}

void save_titles(const std::string &my_name, const std::list<std::string> &titles) {
    Json::Value titles_json;
    for (const auto &title : titles)
        titles_json.append(gbk2utf(title));

    Json::Value temp;
    while (titles_json.size() > 1024)
        titles_json.removeIndex(0, &temp);

    std::ofstream ofs("log/" + my_name + ".json", std::ios::binary | std::ios::out);
    ofs << titles_json;
    ofs.close();
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
    getxy(exit_x, exit_y, select("//node").attribute("bounds").value(), 17, 10, 7, 9);
    logger->debug("退出按钮：({},{})", exit_x, exit_y);

    // [{"c":"1575","type":"blank"},{"c":"1065","type":"challenge"},{"c":"1100","type":"multiple"},{"c":"921","type":"single"}]
    auto groupby_type = db.groupby_type();
    logger->info("[题库统计]：单选题 {}；多选题 {}；填空题 {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
}

// 我要选读文章
void adb::read(bool is_ppp) {
    int score1, score11, score12;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    int title_index = 0;

    logger->info("[学习]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    logger->info("[思想]");
    tap(select_with_text("思想"));
    logger->info("[推荐]");
    tap(select_with_text("推荐"));
    logger->info("[刷新]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    for (;;) {
        // 积分界面
        score();

        // 我要选读文章
        node = select("//node[@class='android.widget.ListView']/node[@index='1']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d+)")))
            throw std::runtime_error("找不到[ 我要选读文章 ]");
        score1 = atoi(sm[1].str().c_str());
        logger->info("[我要选读文章]：已获{}分/每日上限12分", score1);

        // 分享
        /*node = select("//node[@class='android.widget.ListView']/node[@index='11']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("已获(\\d)分/每日上限1分")))
            throw std::runtime_error("找不到[ 分享 ]");
        score11 = atoi(sm[1].str().c_str());
        logger->info("[分享]：已获{}分/每日上限1分", score11);*/
        score11 = 1;

        // 发表观点
        node = select("//node[@class='android.widget.ListView']/node[@index='10']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("找不到[ 发表观点 ]");
        score12 = atoi(sm[1].str().c_str());
        logger->info("[发表观点]：已获{}分/每日上限1分", score12);

        if (score1 >= 12 && score11 >= 1) {
            if (is_ppp) {
                store();
                logger->info("[点点通明细]");
                tap(select("//node[@text='点点通明细' or @content-desc='点点通明细']/following-sibling::node[1]"), 5);
                pull();
                auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='有效浏览' or @content-desc='有效浏览']/following-sibling::node[1]").c_str());
                int ppp = 0;
                for (auto &ppp_xpath : ppps_xpath) {
                    auto text = get_text(ppp_xpath.node());
                    if (!std::regex_search(text, sm, std::regex("\\+(\\d)点")))
                        throw std::runtime_error("找不到[ 有效浏览 ]");
                    ppp += atoi(sm[1].str().c_str());
                }
                logger->info("[有效浏览]：+{}点/每日上限12点", ppp);
                if (ppp >= 12) {
                    logger->info("[返回]");
                    back(1, false);
                    back(1, false);
                    back(1, false);
                    back();
                    return;
                } else {
                    back(1, false);
                    back();
                }
            } else {
                logger->info("[返回]");
                back(1, false);
                back();
                return;
            }
        }

        // 计算平均每篇文章所需时长
        int delay = 5 * (12 - score1);
        if (delay < 15)
            delay = 15;

        // 首页
        logger->info("[返回]");
        back();
        back();

        std::list<std::string> titles = get_titles(my_name);
        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@class='android.widget.ListView']//node[@class='android.widget.TextView']");
            // xpath_nodes 中选择不重复的为 valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                std::string resource_id = node.attribute("resource-id").value();
                std::string text = this->get_text(node);
                int x1, x2, y1, y2;
                getxy(x1, y1, bounds, 1, 0, 1, 0);
                getxy(x2, y2, bounds, 0, 1, 0, 1);
                // 1. 不重复；2. 不在水下；3. 字数足够；4. 足够宽或general_card_title_id
                    return std::find(titles.begin(), titles.end(), "[我要选读文章]：" + text) == titles.end() && y2 - y1 > 3 && text.size() > 5 && (x2 - x1 > 0.8 * this->width || resource_id == "cn.xuexi.android:id/general_card_title_id");
                });
            logger->info("[我要选读文章]：发现 {} 篇文章", valid_xpath_nodes.size());
            bool is_left = false;
            for (auto &xpath_node : valid_xpath_nodes) {
                if (c == 6) {
                    is_left = true;
                    break;
                }
                auto node = xpath_node.node();
                text = get_text(node);
                logger->info("[我要选读文章]：{}. {}({}秒)", ++title_index, text, delay);
                tap(node, 2, false);
                for (int i = 0; i < delay / 15; i++) {
                    if (rand() % 2)
                        swipe_up(0, false);
                    else
                        swipe_down(0, false);
                    std::this_thread::sleep_for(std::chrono::seconds(15 + std::rand() % 3));
                }

                // 分享
                if (score11 < 1)
                    try {
                        pull();
                        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[4]");
                        tap(node);
                        logger->info("[分享]");
                        tap(select_with_text("分享到学习强国"));
                        back();
                    } catch (...) {
                    }

                // 发表观点
                if (score12 < 1)
                    try {
                        pull();
                        //auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]/node[1]/node[2]");
                        //int comment_count = atoi(get_text(node).c_str());
                        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]");
                        int comment_count = 1;
                        if (comment_count > 0) {
                            tap(node);
                            if (!exist_with_text("删除")) {
                                node = select("//node[@class='android.support.v7.widget.RecyclerView' or @class='androidx.recyclerview.widget.RecyclerView']/node[3]/node[2]/node[1]");
                                auto comment = get_text(node);
                                tap(select_with_text("欢迎发表你的观点"));
                                input_text(comment);
                                pull();
                                int x, y;
                                getxy(x, y, select_with_text("发布").attribute("bounds").value());
                                logger->info("[发表观点]：{}", comment);
                                // 有时候这个，有时候那个，哎呀，全点了吧
                                tap(x, y - 0 * height / 40, 2, false);
                                tap(x, y - 1 * height / 40, 2, false);
                            }
                        }
                    } catch (...) {
                    }

                titles.push_back("[我要选读文章]：" + text);
                save_titles(my_name, titles);
                c++;
                back(1, false);
                back();
                if (exist_with_text("加入书架"))
                    tap(select_with_text("取消"));
            }
            if (exist_with_text("你已经看到我的底线了") || exist_with_text("内容持续更新中") || (valid_xpath_nodes.size() && title_index % 24 == 0)) {
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
void adb::listen(bool is_ppp) {
    int score2, score4;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    int title_index = 0;

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
        node = select("//node[@class='android.widget.ListView']/node[@index='2']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("找不到[ 视听学习 ]");
        score2 = atoi(sm[1].str().c_str());
        logger->info("[视听学习]：已获{}分/每日上限6分", score2);

        // 视听学习时长
        node = select("//node[@class='android.widget.ListView']/node[@index='3']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("找不到[ 视听学习时长 ]");
        score4 = atoi(sm[1].str().c_str());
        logger->info("[视听学习时长]：已获{}分/每日上限6分", score4);

        if (score2 >= 6 && score4 >= 6) {
            if (is_ppp) {
                store();
                logger->info("[点点通明细]");
                tap(select("//node[@text='点点通明细' or @content-desc='点点通明细']/following-sibling::node[1]"), 5);
                pull();
                auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='有效视听' or @content-desc='有效视听']/following-sibling::node[1]").c_str());
                int ppp = 0;
                for (auto &ppp_xpath : ppps_xpath) {
                    auto text = get_text(ppp_xpath.node());
                    if (!std::regex_search(text, sm, std::regex("\\+(\\d)点")))
                        throw std::runtime_error("找不到[ 有效视听 ]");
                    ppp += atoi(sm[1].str().c_str());
                }
                logger->info("[有效视听]：+{}点/每日上限12点", ppp);
                if (ppp >= 12) {
                    logger->info("[返回]");
                    back(1, false);
                    back(1, false);
                    back(1, false);
                    back();
                    return;
                } else {
                    back(1, false);
                    back();
                }
            } else {
                logger->info("[返回]");
                back(1, false);
                back();
                return;
            }
        }

        // 计算平均每个视听所需时长
        int delay = 10 * (6 - score4);
        if (delay < 15)
            delay = 15;

        // 首页
        logger->info("[返回]");
        back();
        back();

        std::list<std::string> titles = get_titles(my_name);
        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@class='android.widget.ListView']//node[@class='android.widget.TextView']");
            // xpath_nodes 中选择不重复的为 valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                std::string resource_id = node.attribute("resource-id").value();
                std::string text = this->get_text(node);
                int x1, x2, y1, y2;
                getxy(x1, y1, bounds, 1, 0, 1, 0);
                getxy(x2, y2, bounds, 0, 1, 0, 1);
                // 1. 不重复；2. 不在水下；3. 字数足够；4. 足够宽或general_card_title_id
                    return std::find(titles.begin(), titles.end(), "[视听学习]：" + text) == titles.end() && y2 - y1 > 3 && text.size() > 5 && (x2 - x1 > 0.8 * this->width || resource_id == "cn.xuexi.android:id/general_card_title_id");
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
                if (text.find("新闻联播") != std::string::npos) {
                    logger->info("[视听学习]：{}. {}({}秒)", ++title_index, text, delay * 4);
                    tap(node, 2, false);
                    std::this_thread::sleep_for(std::chrono::seconds(delay * 4));
                    delay /= 2;
                    if (delay < 15)
                        delay = 15;
                } else {
                    logger->info("[视听学习]：{}. {}({}秒)", ++title_index, text, delay);
                    tap(node, 2, false);
                    std::this_thread::sleep_for(std::chrono::seconds(delay + (delay / 30) * (std::rand() % 4)));
                }
                titles.push_back("[视听学习]：" + text);
                save_titles(my_name, titles);
                c++;
                back();
            }
            if (exist_with_text("你已经看到我的底线了") || exist_with_text("内容持续更新中") || (valid_xpath_nodes.size() && title_index % 24 == 0)) {
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
void adb::daily(bool is_training) {
    int score4;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 每日答题
    node = select("//node[@class='android.widget.ListView']/node[@index='4']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("找不到[ 每日答题 ]");
    score4 = atoi(sm[1].str().c_str());
    logger->info("[每日答题]：已获{}分/每日上限5分", score4);

    if (is_training) {
        back();
        logger->info("[我要答题]");
        tap(select_with_text("我要答题"), 10);
        pull();
        if (exist_with_text("下一步"))
            tap(select_with_text("下一步"));
        if (exist_with_text("下一步"))
            tap(select_with_text("下一步"));
        if (exist_with_text("知道了"))
            tap(select_with_text("知道了"));
        if (exist_with_text("立即答题"))
            tap(select_with_text("立即答题"));
        logger->info("[每日答题]");
        tap(select_with_text("每日答题"), 10);
    } else {
        if (score4 >= 5) {
            logger->info("[返回]");
            back();
            back();
            return;
        }
        for (;;) {
            node = select("//node[@class='android.widget.ListView']/node[@index='4']/node[@index='4']");
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
    }

    for (int i = 1;; i++) {
        auto type = get_text(select("//node[@text='填空题' or @text='单选题' or @text='多选题' or @content-desc='填空题' or @content-desc='单选题' or @content-desc='多选题']"));
        //ui.save_file(("log/每日答题/夜神-" + std::to_string(i) + "-" + type + ".xml").c_str());
        std::string content_utf, options_utf, answer;
        if (type == "填空题") {
            std::cout << std::endl;
            // 华为真机、360真机、夜神模拟器
            auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/../..//node[@class='android.view.View'][count(*)=0]");
            for (auto &xpath_node : xpath_nodes) {
                std::string text;
                text += xpath_node.node().attribute("text").value();
                text += xpath_node.node().attribute("content-desc").value();
                text = std::regex_replace(text, std::regex("\\s"), "");
                if (text.size())
                    content_utf += text;
                else
                    content_utf += "__";
            }
            // MuMu模拟器
            if (content_utf.find(gbk2utf(type) + std::to_string((i - 1) % 10 + 1) + "/10") != std::string::npos) {
                logger->warn("MuMu模拟器！");
                xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/..//node[@class='android.view.View'][count(*)=0]");
                content_utf.clear();
                for (auto &xpath_node : xpath_nodes) {
                    std::string text;
                    text += xpath_node.node().attribute("text").value();
                    text += xpath_node.node().attribute("content-desc").value();
                    text = std::regex_replace(text, std::regex("\\s"), "");
                    if (text.size())
                        content_utf += text;
                    else
                        content_utf += "__";
                }
            }
            logger->info("[{}] {}. {}", type, i, utf2gbk(content_utf));

            auto result = db.get_answer("blank", content_utf, options_utf);
            auto answer_db = result["answer"].asString();
            answer = answer_db.size() ? utf2gbk(answer_db) : "不忘初心牢记使命敢于担当";

            xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/following-sibling::node[1]");
            std::list<int> answer_indexs;
            for (unsigned answer_index = 0; answer_index < xpath_nodes.size(); answer_index++)
                answer_indexs.push_back(answer_index);
            int substr_index = 0;
            for (int answer_index : answer_indexs) {
                for (;;) {
                    auto node = xpath_nodes[answer_index].node();
                    int x, y1, y2;
                    getxy(x, y1, node.attribute("bounds").value(), 1, 1, 1, 0);
                    getxy(x, y2, node.attribute("bounds").value(), 1, 1, 0, 1);
                    if (y2 - y1 > 3 && y1 < 0.9 * height) {
                        tap(node, 0, false);
                        int substr_size = 0, word_index = 0;
                        do {
                            if (answer[substr_index + substr_size] & 0x80)
                                substr_size++;
                            substr_size++;
                            word_index++;
                            std::string xpath = "(//node[@class='android.widget.EditText'])[" + std::to_string(answer_index + 1) + "]/following-sibling::node[" + std::to_string(word_index + 1) + "]";
                            node = ui.select_node(xpath.c_str()).node();
                        } while (!node.empty() && !get_text(node).size());
                        logger->debug("substr_index = {}, substr_size = {}, word_index = {}", substr_index, substr_size, word_index);
                        input_text(answer.substr(substr_index, substr_size));
                        logger->info("[提交答案]：{}", answer.substr(substr_index, substr_size));
                        substr_index += substr_size;
                        break;
                    }
                    logger->warn("Can't see any blank!");
                    swipe_up();
                    xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/following-sibling::node[1]");
                }
            }
        } else if (type == "单选题" || type == "多选题") {
            std::cout << std::endl;
            auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node");
            std::list<pugi::xpath_node> reverse_xpath_nodes;
            std::copy(xpath_nodes.begin(), xpath_nodes.end(), std::front_inserter(reverse_xpath_nodes));
            for (auto &xpath_node : reverse_xpath_nodes) {
                content_utf += xpath_node.node().attribute("text").value();
                content_utf += xpath_node.node().attribute("content-desc").value();
                // 华为真机、360真机、夜神模拟器的界面xml，8个0xc20xa0前面多个空格0x20，而MuMu模拟器的界面xml，只有8个0xc20xa0，前面没有空格\x20
                // 参考[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
                if (!std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")) && std::regex_search(content_utf, std::regex("(\\xc2\\xa0){8}")))
                    logger->warn("MuMu模拟器！");
                content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
                content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
            }
            logger->info("[{}] {}. {}", type, i, utf2gbk(content_utf));

            xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/node//node[@index='2']");
            char mark = 'A';
            for (auto &xpath_node : xpath_nodes) {
                std::string option_utf;
                option_utf += mark++;
                option_utf += ". ";
                option_utf += xpath_node.node().attribute("text").value();
                option_utf += xpath_node.node().attribute("content-desc").value();
                logger->info(utf2gbk(option_utf));
                options_utf += option_utf + "\r\n";
            }
            options_utf = options_utf.substr(0, options_utf.size() - 2);

            std::list<int> answer_indexs;
            if (type == "单选题") {
                auto result = db.get_answer("single", content_utf, options_utf);
                logger->info("[答案提示]：{}", dump(result));
                auto answer_db = result["answer"].asString();
                if (answer_db.size()) {
                    logger->info("[提交答案]：{}", answer_db);
                    answer_indexs.push_back(c2i(answer_db[0]));
                } else {
                    int answer_index = rand() % xpath_nodes.size();
                    logger->info("[提交答案]：{}", i2c(answer_index));
                    answer_indexs.push_back(answer_index);
                }
            } else if (type == "多选题") {
                auto result = db.get_answer("multiple", content_utf, options_utf);
                logger->info("[答案提示]：{}", dump(result));
                auto answer_db = result["answer"].asString();
                if (answer_db.size()) {
                    logger->info("[提交答案]：{}", answer_db);
                    for (auto c : answer_db)
                        answer_indexs.push_back(c2i(c));
                } else {
                    logger->info("[提交答案]：全选");
                    for (unsigned answer_index = 0; answer_index < xpath_nodes.size(); answer_index++)
                        answer_indexs.push_back(answer_index);
                }
            }

            for (int answer_index : answer_indexs) {
                answer += i2c(answer_index);
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

        // 正确答案
        pull();
        tap(select_with_text("确定"));
        auto xpath_answer_correct = "//node[starts-with(@text,'正确答案：') or starts-with(@content-desc,'正确答案：')]";
        std::string answer_correct_utf;
        if (!exist(xpath_answer_correct))
            answer_correct_utf = gbk2utf(answer);
        else {
            answer_correct_utf = get_text(select(xpath_answer_correct));
            answer_correct_utf = answer_correct_utf.substr(10);
            answer_correct_utf = gbk2utf(answer_correct_utf);
            trim(answer_correct_utf);
            logger->info("[正确答案]：{}", utf2gbk(answer_correct_utf));
        }

        // 保存答案
        if (type == "填空题")
            type = "blank";
        else if (type == "单选题")
            type = "single";
        else if (type == "多选题")
            type = "multiple";
        else
            throw std::runtime_error("[每日答题] 未知题型：" + type);
        if (is_training && (content_utf.size() + options_utf.size()))
            if (db.insert_or_update_answer(type, content_utf, options_utf, answer_correct_utf, "")) {
                logger->info("[保存答案至题库]");
                auto groupby_type = db.groupby_type();
                logger->info("[题库统计]：单选题 {}；多选题 {}；填空题 {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
            }

        // 下一题或完成
        if (exist(xpath_answer_correct))
            tap(select("//node[@text='下一题' or @text='完成' or @content-desc='下一题' or @content-desc='完成']"));

        // 再来一组或返回
        if (i % 5 > 0)
            continue;

        logger->info("[本次答对题目数]：{}", get_text(select("//node[@text='本次答对题目数' or @content-desc='本次答对题目数']/following-sibling::node[1]")));
        if (!is_training && exist_with_text("领取奖励已达今日上限")) {
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

void adb::challenge(bool is_ppp, bool is_training) {
    int score7;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 挑战答题
    node = select("//node[@class='android.widget.ListView']/node[@index='6']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("找不到[ 挑战答题 ]");
    score7 = atoi(sm[1].str().c_str());
    logger->info("[挑战答题]：已获{}分/每日上限5分", score7);

    int five;
    if (is_training)
        five = INT_MAX / 5;
    else if (score7 >= 5) {
        if (is_ppp) {
            store();
            logger->info("[点点通明细]");
            tap(select("//node[@text='点点通明细' or @content-desc='点点通明细']/following-sibling::node[1]"), 5);
            pull();
            auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='挑战答题' or @content-desc='挑战答题']/following-sibling::node[1]").c_str());
            int ppp = 0;
            for (auto &ppp_xpath : ppps_xpath) {
                auto text = get_text(ppp_xpath.node());
                if (!std::regex_search(text, sm, std::regex("\\+(\\d)点")))
                    throw std::runtime_error("找不到[ 挑战答题 ]");
                ppp += atoi(sm[1].str().c_str());
            }
            logger->info("[挑战答题]：+{}点/每日上限9点", ppp);
            if (ppp >= 9) {
                logger->info("[返回]");
                back(1, false);
                back(1, false);
                back(1, false);
                back();
                return;
            } else {
                back(1, false);
                back();
                five = (9 - ppp) / 3;
            }
        } else {
            logger->info("[返回]");
            back(1, false);
            back();
            return;
        }
    } else {
        five = is_ppp ? 3 : 1;
    }
    logger->debug("five: {}", five);

    back();
    logger->info("[我要答题]");
    tap(select_with_text("我要答题"), 10);
    pull();
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("知道了"))
        tap(select_with_text("知道了"));
    if (exist_with_text("立即答题"))
        tap(select_with_text("立即答题"));
    logger->info("[挑战答题]");
    tap(select("//node[@text='排行榜' or @content-desc='排行榜']/following-sibling::node[3]"), 10);
    logger->info("[时事政治]");
    tap(select_with_text("时事政治"));

    for (int i = 1; i <= 1000; i++) {
        std::cout << std::endl;
        auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node[1]");

        // 题干
        std::string content_utf;
        for (auto &xpath_node : xpath_nodes) {
            content_utf += xpath_node.node().attribute("text").value();
            content_utf += xpath_node.node().attribute("content-desc").value();
            content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
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
            options_utf += option_utf + "\r\n";
        }
        options_utf = options_utf.substr(0, options_utf.size() - 2);

        auto result = db.get_answer(i <= (five * 5) ? "single" : "max", content_utf, options_utf);
        logger->info("[答案提示]：{}", dump(result));
        auto answer_db = result["answer"].asString();
        auto notanswer_db = result["notanswer"].asString();
        int answer_index;
        if (answer_db.size()) {
            logger->info("[提交答案]：{}", answer_db);
            answer_index = c2i(answer_db[0]);
        } else {
            if (notanswer_db.size() >= xpath_nodes.size())
                throw std::runtime_error("数据库无效notanswer");
            do {
                answer_index = rand() % xpath_nodes.size();
            } while (notanswer_db.find(i2c(answer_index)) != std::string::npos);
            logger->info("[提交随机答案]：{}", i2c(answer_index));
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

        if (exist_with_text("挑战结束")) {
            if (is_training && (content_utf.size() + options_utf.size()))
                if (db.insert_or_update_answer("single", content_utf, options_utf, "", notanswer_db + i2c(answer_index))) {
                    logger->info("[保存答案至题库]");
                    auto groupby_type = db.groupby_type();
                    logger->info("[题库统计]：单选题 {}；多选题 {}；填空题 {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
                }

            text = get_text(select("//node[@text='挑战结束' or @content-desc='挑战结束']/following-sibling::node[1]"));
            if (!std::regex_search(text, sm, std::regex("本次答对 (\\d+) 题")))
                throw std::runtime_error("找不到[ 本次答对 (\\d+) 题 ]");
            score7 = atoi(sm[1].str().c_str());
            five -= score7 / 5;
            logger->debug("five: {}", five);
            logger->info("[挑战答题]：本次答对 {} 题", score7);
            logger->info("[结束本局]");
            tap(select_with_text("结束本局"));

            if (five <= 0) {
                logger->info("[返回]");
                back(1, false);
                back(1, false);
                back(1, false);
                back();
                return;
            }

            logger->info("[再来一局]");
            tap(select_with_text("再来一局"));
            i = 0;
            continue;
        } else if (is_training && (content_utf.size() + options_utf.size())) {
            if (db.insert_or_update_answer("single", content_utf, options_utf, std::string(1, i2c(answer_index)), "")) {
                logger->info("[保存答案至题库]");
                auto groupby_type = db.groupby_type();
                logger->info("[题库统计]：单选题 {}；多选题 {}；填空题 {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
            }
        }
    }
    this->repair();
}

void adb::race2() {
    int score9;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 争上游答题
    node = select("//node[@class='android.widget.ListView']/node[@index='8']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("找不到[ 双人对战 ]");
    score9 = atoi(sm[1].str().c_str());
    logger->info("[双人对战]：已获{}分/每日上限2分", score9);

    back();
    logger->info("[我要答题]");
    tap(select_with_text("我要答题"), 10);
    pull();
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("知道了"))
        tap(select_with_text("知道了"));
    if (exist_with_text("立即答题"))
        tap(select_with_text("立即答题"));
    logger->info("[双人对战]");
    tap(select("//node[@text='排行榜' or @content-desc='排行榜']/following-sibling::node[2]"), 10);

    for (auto i1 = 0; i1 < 50; i1++) {
        if (!exist_with_text("今日积分奖励局：1/1")) {
            logger->info("[返回]");
            back(1);
            tap(select_with_text("退出"), 1, false);
            back(1, false);
            back();
            break;
        }
        logger->info("[随机匹配]");
        tap(select("//node[@text='随机匹配' or @content-desc='随机匹配']/preceding-sibling::node[1]"), 2, false);
        logger->info("[开始对战]");
        tap(select_with_text("开始对战"), 10, false);
        for (auto i2 = 0; i2 < 50; i2++) {
            std::string content_utf, options_utf;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                logger->debug("pull");
                pull();

                if (exist_with_text("继续挑战")) {
                    logger->info("[返回]");
                    back();
                    break;
                }

                // 题干
                // auto xpath_nodes = ui.select_nodes(gbk2utf("//node[starts-with(@text,'距离答题结束00') or starts-with(@content-desc,'距离答题结束00')]/following-sibling::node[1]/node[1]/node[1]/node[1]/node[1]/node[1]").c_str());
                auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node");
                std::list<pugi::xpath_node> reverse_xpath_nodes;
                std::copy(xpath_nodes.begin(), xpath_nodes.end(), std::front_inserter(reverse_xpath_nodes));
                if (!xpath_nodes.size()) {
                    logger->warn("[not found content]");
                    continue;
                }
                for (auto &xpath_node : reverse_xpath_nodes) {
                    content_utf += xpath_node.node().attribute("text").value();
                    content_utf += xpath_node.node().attribute("content-desc").value();
                    content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
                    content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
                    content_utf = std::regex_replace(content_utf, std::regex("^\\d*\\."), "");
                }

                // 选项
                // xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]/node[1]/node[2]").c_str());
                xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]").c_str());
                if (!xpath_nodes.size()) {
                    logger->warn("[not found options]");
                    continue;
                }
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    options_utf += option_utf + "\r\n";
                }
                options_utf = options_utf.substr(0, options_utf.size() - 2);

                // log
                std::cout << std::endl;
                logger->info("[双人对战] {}", utf2gbk(content_utf));
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    logger->info(utf2gbk(option_utf));
                }

                // 答案提示
                auto result = db.get_answer("single", content_utf, options_utf);
                auto answer_db = result["answer"].asString();
                int answer_index;
                if (answer_db.size()) {
                    logger->info("[答案提示]：{}", dump(result));
                    logger->info("[提交答案]：{}", answer_db);
                    answer_index = c2i(answer_db[0]);
                } else {
                    logger->info("[答案提示]：{}", "null");
                    answer_index = rand() % xpath_nodes.size();
                    // answer_index = 0;
                    logger->info("[提交答案]：{}", i2c(answer_index));
                }
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
                    // xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]/node[1]/node[2]").c_str());
                    xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]").c_str());
                }

            } catch (const std::exception &ex) {
                logger->warn("{}", ex.what());
            }
        }
    }
}

void adb::race4(int count) {
    int score8;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 争上游答题
    node = select("//node[@class='android.widget.ListView']/node[@index='7']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("找不到[ 争上游答题 ]");
    score8 = atoi(sm[1].str().c_str());
    logger->info("[争上游答题]：已获{}分/每日上限5分", score8);

    back();
    logger->info("[我要答题]");
    tap(select_with_text("我要答题"), 10);
    pull();
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("下一步"))
        tap(select_with_text("下一步"));
    if (exist_with_text("知道了"))
        tap(select_with_text("知道了"));
    if (exist_with_text("立即答题"))
        tap(select_with_text("立即答题"));
    logger->info("[争上游答题]");
    tap(select("//node[@text='排行榜' or @content-desc='排行榜']/following-sibling::node[1]"), 10);

    for (auto i1 = 0; i1 < 50; i1++) {
        if (exist_with_text("今日积分奖励局" + std::to_string(count) + "/2")) {
            logger->info("[返回]");
            back(1, false);
            back(1, false);
            back();
            break;
        }
        logger->info("[开始比赛]");
        tap(select_with_text("开始比赛"), 10, false);
        for (auto i2 = 0; i2 < 50; i2++) {
            std::string content_utf, options_utf;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                logger->debug("pull");
                pull();

                if (exist_with_text("温故知新"))
                    back();
                if (exist_with_text("继续挑战")) {
                    logger->info("[返回]");
                    back();
                    break;
                }

                // 题干
                // auto xpath_nodes = ui.select_nodes(gbk2utf("//node[starts-with(@text,'距离答题结束00') or starts-with(@content-desc,'距离答题结束00')]/following-sibling::node[1]/node[1]/node[1]/node[1]/node[1]/node[1]").c_str());
                auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node");
                std::list<pugi::xpath_node> reverse_xpath_nodes;
                std::copy(xpath_nodes.begin(), xpath_nodes.end(), std::front_inserter(reverse_xpath_nodes));
                if (!xpath_nodes.size()) {
                    logger->warn("[not found content]");
                    continue;
                }
                for (auto &xpath_node : reverse_xpath_nodes) {
                    content_utf += xpath_node.node().attribute("text").value();
                    content_utf += xpath_node.node().attribute("content-desc").value();
                    content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
                    content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
                    content_utf = std::regex_replace(content_utf, std::regex("^\\d*\\."), "");
                }

                // 选项
                // xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]/node[1]/node[2]").c_str());
                xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]").c_str());
                if (!xpath_nodes.size()) {
                    logger->warn("[not found options]");
                    continue;
                }
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    options_utf += option_utf + "\r\n";
                }
                options_utf = options_utf.substr(0, options_utf.size() - 2);

                // log
                std::cout << std::endl;
                logger->info("[争上游答题] {}", utf2gbk(content_utf));
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    logger->info(utf2gbk(option_utf));
                }

                // 答案提示
                auto result = db.get_answer("single", content_utf, options_utf);
                auto answer_db = result["answer"].asString();
                int answer_index;
                if (answer_db.size()) {
                    logger->info("[答案提示]：{}", dump(result));
                    logger->info("[提交答案]：{}", answer_db);
                    answer_index = c2i(answer_db[0]);
                } else {
                    logger->info("[答案提示]：{}", "null");
                    answer_index = rand() % xpath_nodes.size();
                    // answer_index = 0;
                    logger->info("[提交答案]：{}", i2c(answer_index));
                }
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
                    // xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]/node[1]/node[2]").c_str());
                    xpath_nodes = ui.select_nodes(gbk2utf("//node[@class='android.widget.ListView']/node[@index]").c_str());
                }

            } catch (const std::exception &ex) {
                logger->warn("{}", ex.what());
            }
        }
    }
}

void adb::local() {
    int score13;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // 积分界面
    score();

    // 本地频道
    node = select("//node[@class='android.widget.ListView']/node[@index='11']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("找不到[ 本地频道 ]");
    score13 = atoi(sm[1].str().c_str());
    logger->info("[本地频道]：已获{}分/每日上限1分", score13);

    if (score13 >= 1) {
        logger->info("[返回]");
        back();
        back();
        return;
    }
    for (;;) {
        node = select("//node[@class='android.widget.ListView']/node[@index='11']/node[@index='4']");
        std::string bounds = node.attribute("bounds").value();
        int x, y1, y2;
        getxy(x, y1, bounds, 1, 1, 1, 0);
        getxy(x, y2, bounds, 1, 1, 0, 1);
        if (y2 - y1 > 3) {
            logger->info("[去看看]");
            tap(node);
            break;
        }
        swipe_up();
    }

    tap(select("//node[@class='android.support.v7.widget.RecyclerView' or @class='androidx.recyclerview.widget.RecyclerView']/node[1]"));
    back();
}

// 回退
void adb::back(int64_t delay, bool is_pull) {
    tap(back_x, back_y, delay, is_pull);
}

// 首页 -> 学习积分
void adb::score() {
    logger->info("[我的]");
    tap(select_with_text("我的"));
    my_name = get_text(select("//node[@resource-id='cn.xuexi.android:id/my_display_name']"));

    for (int i = 0; i < 3; i++) {
        logger->info("[学习积分]");
        tap(select_with_text("学习积分"), 10);
        if (exist_with_text("重启"))
            tap(select_with_text("重启"), 10);
        if (exist_with_text("积分规则")) {
            pull();
            if (exist_with_text("好的，知道了"))
                tap(select_with_text("好的，知道了"));
            return;
        }
        back();
    }
    throw std::runtime_error("点不动[学习积分]");
}

// 学习积分 -> 强国商城
void adb::store() {
    for (int i = 0; i < 3; i++) {
        logger->info("[强国商城]");
        tap(select_with_text("强国城兑福利"), 5);
        pull();
        if (exist_with_text("点点通明细"))
            return;
        back();
    }
    throw std::runtime_error("点不动[强国商城]");
}

void adb::repair() {
    for (;;) {
        try {
            back(2, false);
            tap(exit_x, exit_y, 2, false);
            back();
        } catch (...) {
        }
        if (exist("//node[@resource-id='cn.xuexi.android:id/comm_head_title']"))
            return;
    }
}

void adb::test() {
    std::string content_utf;
    pull();
    auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node");
    std::list<pugi::xpath_node> reverse_xpath_nodes;
    std::copy(xpath_nodes.begin(), xpath_nodes.end(), std::front_inserter(reverse_xpath_nodes));
    for (auto &xpath_node : reverse_xpath_nodes) {
        content_utf += xpath_node.node().attribute("text").value();
        content_utf += xpath_node.node().attribute("content-desc").value();
        // 华为真机、360真机、夜神模拟器的界面xml，8个0xc20xa0前面多个空格0x20，而MuMu模拟器的界面xml，只有8个0xc20xa0，前面没有空格\x20
        // 参考[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
        if (!std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")) && std::regex_search(content_utf, std::regex("(\\xc2\\xa0){8}")))
            logger->warn("MuMu模拟器！");
        content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
        content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
    }
    logger->info("{}", utf2gbk(content_utf));
    return;

    pull();
    int x, y;
    getxy(x, y, select("//node").attribute("bounds").value());
    width = 2 * x;
    height = 2 * y;
    logger->debug("分辨率：{}×{}", width, height);

    try {
        pull();
        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]/node[1]/node[2]");
        int comment_count = atoi(get_text(node).c_str());
        if (comment_count > 0) {
            tap(node);
            node = select("//node[@class='android.support.v7.widget.RecyclerView']/node[3]/node[2]/node[1]");
            auto comment = get_text(node);
            tap(select_with_text("欢迎发表你的观点"));
            input_text(comment);
            pull();
            int x, y;
            getxy(x, y, select_with_text("发布").attribute("bounds").value());
            tap(x, y - 0 * height / 40, 2, false);
        }
    } catch (...) {
    }
    try {
        pull();
        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[3]");
        tap(node, 2, false);
        tap(node, 2, false);
    } catch (...) {
    }
    try {
        pull();
        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[4]");
        tap(node);
        tap(select_with_text("分享到短信"));
    } catch (...) {
    }

    /*x = width / 2;
     y = 47 * height / 48;
     tap(x, y, 2, false);
     exec("adb shell input keyevent 01");
     input_text("不玩初心牢记使命");
     exec("adb shell input keyevent 111");

     x = 31 * width / 36;
     tap(x, y, 2, false);

     x = 17 * width / 18;
     tap(x, y, 2, false);
     exec("adb shell input keyevent 111");*/
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
    std::string result;
    try {
        result = exec("adb shell uiautomator dump /sdcard/ui.xml");
    } catch (const std::exception &ex) {
        std::string err_message(ex.what());
        if (err_message.find("java.io.FileNotFoundException") == std::string::npos)
            throw std::runtime_error(err_message);
    }
    //auto result = exec("adb shell uiautomator dump /sdcard/ui.xml");
    if (result.size() && result.find("UI hierchary dumped to") == std::string::npos)
        throw std::runtime_error(result);
    if (result.size())
        logger->debug(result);

    result = exec("adb pull /sdcard/ui.xml log/ui.xml");
    if (result.find("[100%] /sdcard/ui.xml") == std::string::npos)
        throw std::runtime_error(result);
    logger->debug(result);

    auto result1 = ui.load_file("log/ui.xml");
    if (!result1)
        throw std::runtime_error(result1.description());

    static int file_index = 0;
    ui.save_file(("log/ui-" + std::to_string(file_index) + ".xml").c_str());
    file_index = (file_index + 1) % 10;
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
    if (err.size())
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
