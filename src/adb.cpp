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

// ���Ӱ�׿ģ����
void adb::connect(const std::string &ip, unsigned short port) {
    std::ostringstream oss;
    oss << "adb connect " << ip << ":" << port;
    auto result = exec(oss.str());
    if (result.find("connected to") == std::string::npos)
        throw std::runtime_error(result);
    logger->debug(result);
}

// ��ʼ��
void adb::init() {
    logger->info("[��ʼ��]");
    pull();
    int x, y;
    getxy(x, y, select("//node").attribute("bounds").value());
    width = 2 * x;
    height = 2 * y;
    logger->debug("�ֱ��ʣ�{}��{}", width, height);

    auto result = exec("adb shell ime set com.android.adbkeyboard/.AdbIME");
    if (result.find("Error:") != std::string::npos)
        throw std::runtime_error("δ��װ[ADBKeyboard.apk]");

    pugi::xml_node node;
    try {
        node = select("//node[@resource-id='cn.xuexi.android:id/comm_head_title']");
    } catch (const std::exception &ex) {
        throw std::runtime_error("���ѧϰǿ����ҳ");
    }
    getxy(back_x, back_y, node.attribute("bounds").value(), 33, 10);
    logger->debug("���ذ�ť��({},{})", back_x, back_y);
    getxy(exit_x, exit_y, select("//node").attribute("bounds").value(), 17, 10, 7, 9);
    logger->debug("�˳���ť��({},{})", exit_x, exit_y);

    // [{"c":"1575","type":"blank"},{"c":"1065","type":"challenge"},{"c":"1100","type":"multiple"},{"c":"921","type":"single"}]
    auto groupby_type = db.groupby_type();
    logger->info("[���ͳ��]����ѡ�� {}����ѡ�� {}������� {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
}

// ��Ҫѡ������
void adb::read(bool is_ppp) {
    int score1, score11, score12;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    int title_index = 0;

    logger->info("[ѧϰ]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    logger->info("[˼��]");
    tap(select_with_text("˼��"));
    logger->info("[�Ƽ�]");
    tap(select_with_text("�Ƽ�"));
    logger->info("[ˢ��]");
    tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
    for (;;) {
        // ���ֽ���
        score();

        // ��Ҫѡ������
        node = select("//node[@class='android.widget.ListView']/node[@index='1']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d+)")))
            throw std::runtime_error("�Ҳ���[ ��Ҫѡ������ ]");
        score1 = atoi(sm[1].str().c_str());
        logger->info("[��Ҫѡ������]���ѻ�{}��/ÿ������12��", score1);

        // ����
        /*node = select("//node[@class='android.widget.ListView']/node[@index='11']/node[@index='2']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("�ѻ�(\\d)��/ÿ������1��")))
            throw std::runtime_error("�Ҳ���[ ���� ]");
        score11 = atoi(sm[1].str().c_str());
        logger->info("[����]���ѻ�{}��/ÿ������1��", score11);*/
        score11 = 1;

        // ����۵�
        node = select("//node[@class='android.widget.ListView']/node[@index='10']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("�Ҳ���[ ����۵� ]");
        score12 = atoi(sm[1].str().c_str());
        logger->info("[����۵�]���ѻ�{}��/ÿ������1��", score12);

        if (score1 >= 12 && score11 >= 1) {
            if (is_ppp) {
                store();
                logger->info("[���ͨ��ϸ]");
                tap(select("//node[@text='���ͨ��ϸ' or @content-desc='���ͨ��ϸ']/following-sibling::node[1]"), 5);
                pull();
                auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='��Ч���' or @content-desc='��Ч���']/following-sibling::node[1]").c_str());
                int ppp = 0;
                for (auto &ppp_xpath : ppps_xpath) {
                    auto text = get_text(ppp_xpath.node());
                    if (!std::regex_search(text, sm, std::regex("\\+(\\d)��")))
                        throw std::runtime_error("�Ҳ���[ ��Ч��� ]");
                    ppp += atoi(sm[1].str().c_str());
                }
                logger->info("[��Ч���]��+{}��/ÿ������12��", ppp);
                if (ppp >= 12) {
                    logger->info("[����]");
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
                logger->info("[����]");
                back(1, false);
                back();
                return;
            }
        }

        // ����ƽ��ÿƪ��������ʱ��
        int delay = 5 * (12 - score1);
        if (delay < 15)
            delay = 15;

        // ��ҳ
        logger->info("[����]");
        back();
        back();

        std::list<std::string> titles = get_titles(my_name);
        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@class='android.widget.ListView']//node[@class='android.widget.TextView']");
            // xpath_nodes ��ѡ���ظ���Ϊ valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                std::string resource_id = node.attribute("resource-id").value();
                std::string text = this->get_text(node);
                int x1, x2, y1, y2;
                getxy(x1, y1, bounds, 1, 0, 1, 0);
                getxy(x2, y2, bounds, 0, 1, 0, 1);
                // 1. ���ظ���2. ����ˮ�£�3. �����㹻��4. �㹻���general_card_title_id
                    return std::find(titles.begin(), titles.end(), "[��Ҫѡ������]��" + text) == titles.end() && y2 - y1 > 3 && text.size() > 5 && (x2 - x1 > 0.8 * this->width || resource_id == "cn.xuexi.android:id/general_card_title_id");
                });
            logger->info("[��Ҫѡ������]������ {} ƪ����", valid_xpath_nodes.size());
            bool is_left = false;
            for (auto &xpath_node : valid_xpath_nodes) {
                if (c == 6) {
                    is_left = true;
                    break;
                }
                auto node = xpath_node.node();
                text = get_text(node);
                logger->info("[��Ҫѡ������]��{}. {}({}��)", ++title_index, text, delay);
                tap(node, 2, false);
                for (int i = 0; i < delay / 15; i++) {
                    if (rand() % 2)
                        swipe_up(0, false);
                    else
                        swipe_down(0, false);
                    std::this_thread::sleep_for(std::chrono::seconds(15 + std::rand() % 3));
                }

                // ����
                if (score11 < 1)
                    try {
                        pull();
                        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[4]");
                        tap(node);
                        logger->info("[����]");
                        tap(select_with_text("����ѧϰǿ��"));
                        back();
                    } catch (...) {
                    }

                // ����۵�
                if (score12 < 1)
                    try {
                        pull();
                        //auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]/node[1]/node[2]");
                        //int comment_count = atoi(get_text(node).c_str());
                        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]");
                        int comment_count = 1;
                        if (comment_count > 0) {
                            tap(node);
                            if (!exist_with_text("ɾ��")) {
                                node = select("//node[@class='android.support.v7.widget.RecyclerView' or @class='androidx.recyclerview.widget.RecyclerView']/node[3]/node[2]/node[1]");
                                auto comment = get_text(node);
                                tap(select_with_text("��ӭ������Ĺ۵�"));
                                input_text(comment);
                                pull();
                                int x, y;
                                getxy(x, y, select_with_text("����").attribute("bounds").value());
                                logger->info("[����۵�]��{}", comment);
                                // ��ʱ���������ʱ���Ǹ�����ѽ��ȫ���˰�
                                tap(x, y - 0 * height / 40, 2, false);
                                tap(x, y - 1 * height / 40, 2, false);
                            }
                        }
                    } catch (...) {
                    }

                titles.push_back("[��Ҫѡ������]��" + text);
                save_titles(my_name, titles);
                c++;
                back(1, false);
                back();
                if (exist_with_text("�������"))
                    tap(select_with_text("ȡ��"));
            }
            if (exist_with_text("���Ѿ������ҵĵ�����") || exist_with_text("���ݳ���������") || (valid_xpath_nodes.size() && title_index % 24 == 0)) {
                swipe_left();
                logger->info("[ˢ��]");
                tap(select("//node[@resource-id='cn.xuexi.android:id/home_bottom_tab_icon_large']"));
                continue;
            }
            if (!is_left)
                swipe_up();
        }
    } //for(;;)
} //read()

// ����ѧϰ
void adb::listen(bool is_ppp) {
    int score2, score4;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;
    int title_index = 0;

    logger->info("[����̨]");
    tap(select_with_text("����̨"));
    logger->info("[����Ƶ��]");
    tap(select_with_text("����Ƶ��"));
    logger->info("[ˢ��]");
    tap(select_with_text("����̨"));
    for (;;) {
        // ���ֽ���
        score();

        // ����ѧϰ
        node = select("//node[@class='android.widget.ListView']/node[@index='2']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("�Ҳ���[ ����ѧϰ ]");
        score2 = atoi(sm[1].str().c_str());
        logger->info("[����ѧϰ]���ѻ�{}��/ÿ������6��", score2);

        // ����ѧϰʱ��
        node = select("//node[@class='android.widget.ListView']/node[@index='3']/node[@index='3']/node[@index='0']");
        text = get_text(node);
        if (!std::regex_search(text, sm, std::regex("(\\d)")))
            throw std::runtime_error("�Ҳ���[ ����ѧϰʱ�� ]");
        score4 = atoi(sm[1].str().c_str());
        logger->info("[����ѧϰʱ��]���ѻ�{}��/ÿ������6��", score4);

        if (score2 >= 6 && score4 >= 6) {
            if (is_ppp) {
                store();
                logger->info("[���ͨ��ϸ]");
                tap(select("//node[@text='���ͨ��ϸ' or @content-desc='���ͨ��ϸ']/following-sibling::node[1]"), 5);
                pull();
                auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='��Ч����' or @content-desc='��Ч����']/following-sibling::node[1]").c_str());
                int ppp = 0;
                for (auto &ppp_xpath : ppps_xpath) {
                    auto text = get_text(ppp_xpath.node());
                    if (!std::regex_search(text, sm, std::regex("\\+(\\d)��")))
                        throw std::runtime_error("�Ҳ���[ ��Ч���� ]");
                    ppp += atoi(sm[1].str().c_str());
                }
                logger->info("[��Ч����]��+{}��/ÿ������12��", ppp);
                if (ppp >= 12) {
                    logger->info("[����]");
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
                logger->info("[����]");
                back(1, false);
                back();
                return;
            }
        }

        // ����ƽ��ÿ����������ʱ��
        int delay = 10 * (6 - score4);
        if (delay < 15)
            delay = 15;

        // ��ҳ
        logger->info("[����]");
        back();
        back();

        std::list<std::string> titles = get_titles(my_name);
        for (int c = 0; c < 6;) {
            pugi::xml_document ui1;
            ui1.append_copy(ui.child("hierarchy"));
            auto xpath_nodes = ui1.select_nodes("//node[@class='android.widget.ListView']//node[@class='android.widget.TextView']");
            // xpath_nodes ��ѡ���ظ���Ϊ valid_xpath_nodes
            std::list<pugi::xpath_node> valid_xpath_nodes;
            std::copy_if(xpath_nodes.begin(), xpath_nodes.end(), std::back_inserter(valid_xpath_nodes), [this, &titles](const pugi::xpath_node &xpath_node) {
                auto node = xpath_node.node();
                std::string bounds = node.attribute("bounds").value();
                std::string resource_id = node.attribute("resource-id").value();
                std::string text = this->get_text(node);
                int x1, x2, y1, y2;
                getxy(x1, y1, bounds, 1, 0, 1, 0);
                getxy(x2, y2, bounds, 0, 1, 0, 1);
                // 1. ���ظ���2. ����ˮ�£�3. �����㹻��4. �㹻���general_card_title_id
                    return std::find(titles.begin(), titles.end(), "[����ѧϰ]��" + text) == titles.end() && y2 - y1 > 3 && text.size() > 5 && (x2 - x1 > 0.8 * this->width || resource_id == "cn.xuexi.android:id/general_card_title_id");
                });
            logger->info("[����ѧϰ]������ {} ������", valid_xpath_nodes.size());
            bool is_left = false;
            for (auto &xpath_node : valid_xpath_nodes) {
                if (c == 6) {
                    is_left = true;
                    break;
                }
                auto node = xpath_node.node();
                text = get_text(node);
                if (text.find("��������") != std::string::npos) {
                    logger->info("[����ѧϰ]��{}. {}({}��)", ++title_index, text, delay * 4);
                    tap(node, 2, false);
                    std::this_thread::sleep_for(std::chrono::seconds(delay * 4));
                    delay /= 2;
                    if (delay < 15)
                        delay = 15;
                } else {
                    logger->info("[����ѧϰ]��{}. {}({}��)", ++title_index, text, delay);
                    tap(node, 2, false);
                    std::this_thread::sleep_for(std::chrono::seconds(delay + (delay / 30) * (std::rand() % 4)));
                }
                titles.push_back("[����ѧϰ]��" + text);
                save_titles(my_name, titles);
                c++;
                back();
            }
            if (exist_with_text("���Ѿ������ҵĵ�����") || exist_with_text("���ݳ���������") || (valid_xpath_nodes.size() && title_index % 24 == 0)) {
                swipe_right();
                logger->info("[ˢ��]");
                tap(select_with_text("����̨"));
                continue;
            }
            if (!is_left)
                swipe_up();
        }

    } //for(;;)
} //listen()

// ÿ�մ���
void adb::daily(bool is_training) {
    int score4;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // ���ֽ���
    score();

    // ÿ�մ���
    node = select("//node[@class='android.widget.ListView']/node[@index='4']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("�Ҳ���[ ÿ�մ��� ]");
    score4 = atoi(sm[1].str().c_str());
    logger->info("[ÿ�մ���]���ѻ�{}��/ÿ������5��", score4);

    if (is_training) {
        back();
        logger->info("[��Ҫ����]");
        tap(select_with_text("��Ҫ����"), 10);
        pull();
        if (exist_with_text("��һ��"))
            tap(select_with_text("��һ��"));
        if (exist_with_text("��һ��"))
            tap(select_with_text("��һ��"));
        if (exist_with_text("֪����"))
            tap(select_with_text("֪����"));
        if (exist_with_text("��������"))
            tap(select_with_text("��������"));
        logger->info("[ÿ�մ���]");
        tap(select_with_text("ÿ�մ���"), 10);
    } else {
        if (score4 >= 5) {
            logger->info("[����]");
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
                logger->info("[ȥ����]");
                tap(node, 10);
                pull();
                break;
            }
            swipe_up();
        }
    }

    for (int i = 1;; i++) {
        auto type = get_text(select("//node[@text='�����' or @text='��ѡ��' or @text='��ѡ��' or @content-desc='�����' or @content-desc='��ѡ��' or @content-desc='��ѡ��']"));
        //ui.save_file(("log/ÿ�մ���/ҹ��-" + std::to_string(i) + "-" + type + ".xml").c_str());
        std::string content_utf, options_utf, answer;
        if (type == "�����") {
            std::cout << std::endl;
            // ��Ϊ�����360�����ҹ��ģ����
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
            // MuMuģ����
            if (content_utf.find(gbk2utf(type) + std::to_string((i - 1) % 10 + 1) + "/10") != std::string::npos) {
                logger->warn("MuMuģ������");
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
            answer = answer_db.size() ? utf2gbk(answer_db) : "���������μ�ʹ�����ڵ���";

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
                        logger->info("[�ύ��]��{}", answer.substr(substr_index, substr_size));
                        substr_index += substr_size;
                        break;
                    }
                    logger->warn("Can't see any blank!");
                    swipe_up();
                    xpath_nodes = ui.select_nodes("//node[@class='android.widget.EditText']/following-sibling::node[1]");
                }
            }
        } else if (type == "��ѡ��" || type == "��ѡ��") {
            std::cout << std::endl;
            auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node");
            std::list<pugi::xpath_node> reverse_xpath_nodes;
            std::copy(xpath_nodes.begin(), xpath_nodes.end(), std::front_inserter(reverse_xpath_nodes));
            for (auto &xpath_node : reverse_xpath_nodes) {
                content_utf += xpath_node.node().attribute("text").value();
                content_utf += xpath_node.node().attribute("content-desc").value();
                // ��Ϊ�����360�����ҹ��ģ�����Ľ���xml��8��0xc20xa0ǰ�����ո�0x20����MuMuģ�����Ľ���xml��ֻ��8��0xc20xa0��ǰ��û�пո�\x20
                // �ο�[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
                if (!std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")) && std::regex_search(content_utf, std::regex("(\\xc2\\xa0){8}")))
                    logger->warn("MuMuģ������");
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
            if (type == "��ѡ��") {
                auto result = db.get_answer("single", content_utf, options_utf);
                logger->info("[����ʾ]��{}", dump(result));
                auto answer_db = result["answer"].asString();
                if (answer_db.size()) {
                    logger->info("[�ύ��]��{}", answer_db);
                    answer_indexs.push_back(c2i(answer_db[0]));
                } else {
                    int answer_index = rand() % xpath_nodes.size();
                    logger->info("[�ύ��]��{}", i2c(answer_index));
                    answer_indexs.push_back(answer_index);
                }
            } else if (type == "��ѡ��") {
                auto result = db.get_answer("multiple", content_utf, options_utf);
                logger->info("[����ʾ]��{}", dump(result));
                auto answer_db = result["answer"].asString();
                if (answer_db.size()) {
                    logger->info("[�ύ��]��{}", answer_db);
                    for (auto c : answer_db)
                        answer_indexs.push_back(c2i(c));
                } else {
                    logger->info("[�ύ��]��ȫѡ");
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
            throw std::runtime_error("[ÿ�մ���] δ֪���ͣ�" + type);

        // ��ȷ��
        pull();
        tap(select_with_text("ȷ��"));
        auto xpath_answer_correct = "//node[starts-with(@text,'��ȷ�𰸣�') or starts-with(@content-desc,'��ȷ�𰸣�')]";
        std::string answer_correct_utf;
        if (!exist(xpath_answer_correct))
            answer_correct_utf = gbk2utf(answer);
        else {
            answer_correct_utf = get_text(select(xpath_answer_correct));
            answer_correct_utf = answer_correct_utf.substr(10);
            answer_correct_utf = gbk2utf(answer_correct_utf);
            trim(answer_correct_utf);
            logger->info("[��ȷ��]��{}", utf2gbk(answer_correct_utf));
        }

        // �����
        if (type == "�����")
            type = "blank";
        else if (type == "��ѡ��")
            type = "single";
        else if (type == "��ѡ��")
            type = "multiple";
        else
            throw std::runtime_error("[ÿ�մ���] δ֪���ͣ�" + type);
        if (is_training && (content_utf.size() + options_utf.size()))
            if (db.insert_or_update_answer(type, content_utf, options_utf, answer_correct_utf, "")) {
                logger->info("[����������]");
                auto groupby_type = db.groupby_type();
                logger->info("[���ͳ��]����ѡ�� {}����ѡ�� {}������� {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
            }

        // ��һ������
        if (exist(xpath_answer_correct))
            tap(select("//node[@text='��һ��' or @text='���' or @content-desc='��һ��' or @content-desc='���']"));

        // ����һ��򷵻�
        if (i % 5 > 0)
            continue;

        logger->info("[���δ����Ŀ��]��{}", get_text(select("//node[@text='���δ����Ŀ��' or @content-desc='���δ����Ŀ��']/following-sibling::node[1]")));
        if (!is_training && exist_with_text("��ȡ�����Ѵ��������")) {
            logger->info("[��ȡ�����Ѵ��������]");
            logger->info("[����]");
            back();
            back();
            back();
            return;
        }
        logger->info("[����һ��]");
        tap(select_with_text("����һ��"));
    }
}

void adb::challenge(bool is_ppp, bool is_training) {
    int score7;
    std::smatch sm;
    pugi::xml_node node;
    std::string text;

    // ���ֽ���
    score();

    // ��ս����
    node = select("//node[@class='android.widget.ListView']/node[@index='6']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("�Ҳ���[ ��ս���� ]");
    score7 = atoi(sm[1].str().c_str());
    logger->info("[��ս����]���ѻ�{}��/ÿ������5��", score7);

    int five;
    if (is_training)
        five = INT_MAX / 5;
    else if (score7 >= 5) {
        if (is_ppp) {
            store();
            logger->info("[���ͨ��ϸ]");
            tap(select("//node[@text='���ͨ��ϸ' or @content-desc='���ͨ��ϸ']/following-sibling::node[1]"), 5);
            pull();
            auto ppps_xpath = ui.select_nodes(gbk2utf("//node[@text='��ս����' or @content-desc='��ս����']/following-sibling::node[1]").c_str());
            int ppp = 0;
            for (auto &ppp_xpath : ppps_xpath) {
                auto text = get_text(ppp_xpath.node());
                if (!std::regex_search(text, sm, std::regex("\\+(\\d)��")))
                    throw std::runtime_error("�Ҳ���[ ��ս���� ]");
                ppp += atoi(sm[1].str().c_str());
            }
            logger->info("[��ս����]��+{}��/ÿ������9��", ppp);
            if (ppp >= 9) {
                logger->info("[����]");
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
            logger->info("[����]");
            back(1, false);
            back();
            return;
        }
    } else {
        five = is_ppp ? 3 : 1;
    }
    logger->debug("five: {}", five);

    back();
    logger->info("[��Ҫ����]");
    tap(select_with_text("��Ҫ����"), 10);
    pull();
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("֪����"))
        tap(select_with_text("֪����"));
    if (exist_with_text("��������"))
        tap(select_with_text("��������"));
    logger->info("[��ս����]");
    tap(select("//node[@text='���а�' or @content-desc='���а�']/following-sibling::node[3]"), 10);
    logger->info("[ʱ������]");
    tap(select_with_text("ʱ������"));

    for (int i = 1; i <= 1000; i++) {
        std::cout << std::endl;
        auto xpath_nodes = ui.select_nodes("//node[@class='android.widget.ListView']/preceding-sibling::node[1]");

        // ���
        std::string content_utf;
        for (auto &xpath_node : xpath_nodes) {
            content_utf += xpath_node.node().attribute("text").value();
            content_utf += xpath_node.node().attribute("content-desc").value();
            content_utf = std::regex_replace(content_utf, std::regex("\\xc2\\xa0"), "_");
            content_utf = std::regex_replace(content_utf, std::regex("\\s"), "");
        }

        logger->info("[��ս����] {}. {}", i, utf2gbk(content_utf));

        // ѡ��
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
        logger->info("[����ʾ]��{}", dump(result));
        auto answer_db = result["answer"].asString();
        auto notanswer_db = result["notanswer"].asString();
        int answer_index;
        if (answer_db.size()) {
            logger->info("[�ύ��]��{}", answer_db);
            answer_index = c2i(answer_db[0]);
        } else {
            if (notanswer_db.size() >= xpath_nodes.size())
                throw std::runtime_error("���ݿ���Чnotanswer");
            do {
                answer_index = rand() % xpath_nodes.size();
            } while (notanswer_db.find(i2c(answer_index)) != std::string::npos);
            logger->info("[�ύ�����]��{}", i2c(answer_index));
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

        if (exist_with_text("��ս����")) {
            if (is_training && (content_utf.size() + options_utf.size()))
                if (db.insert_or_update_answer("single", content_utf, options_utf, "", notanswer_db + i2c(answer_index))) {
                    logger->info("[����������]");
                    auto groupby_type = db.groupby_type();
                    logger->info("[���ͳ��]����ѡ�� {}����ѡ�� {}������� {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
                }

            text = get_text(select("//node[@text='��ս����' or @content-desc='��ս����']/following-sibling::node[1]"));
            if (!std::regex_search(text, sm, std::regex("���δ�� (\\d+) ��")))
                throw std::runtime_error("�Ҳ���[ ���δ�� (\\d+) �� ]");
            score7 = atoi(sm[1].str().c_str());
            five -= score7 / 5;
            logger->debug("five: {}", five);
            logger->info("[��ս����]�����δ�� {} ��", score7);
            logger->info("[��������]");
            tap(select_with_text("��������"));

            if (five <= 0) {
                logger->info("[����]");
                back(1, false);
                back(1, false);
                back(1, false);
                back();
                return;
            }

            logger->info("[����һ��]");
            tap(select_with_text("����һ��"));
            i = 0;
            continue;
        } else if (is_training && (content_utf.size() + options_utf.size())) {
            if (db.insert_or_update_answer("single", content_utf, options_utf, std::string(1, i2c(answer_index)), "")) {
                logger->info("[����������]");
                auto groupby_type = db.groupby_type();
                logger->info("[���ͳ��]����ѡ�� {}����ѡ�� {}������� {}", groupby_type[3]["c"].asString(), groupby_type[2]["c"].asString(), groupby_type[0]["c"].asString());
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

    // ���ֽ���
    score();

    // �����δ���
    node = select("//node[@class='android.widget.ListView']/node[@index='8']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("�Ҳ���[ ˫�˶�ս ]");
    score9 = atoi(sm[1].str().c_str());
    logger->info("[˫�˶�ս]���ѻ�{}��/ÿ������2��", score9);

    back();
    logger->info("[��Ҫ����]");
    tap(select_with_text("��Ҫ����"), 10);
    pull();
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("֪����"))
        tap(select_with_text("֪����"));
    if (exist_with_text("��������"))
        tap(select_with_text("��������"));
    logger->info("[˫�˶�ս]");
    tap(select("//node[@text='���а�' or @content-desc='���а�']/following-sibling::node[2]"), 10);

    for (auto i1 = 0; i1 < 50; i1++) {
        if (!exist_with_text("���ջ��ֽ����֣�1/1")) {
            logger->info("[����]");
            back(1);
            tap(select_with_text("�˳�"), 1, false);
            back(1, false);
            back();
            break;
        }
        logger->info("[���ƥ��]");
        tap(select("//node[@text='���ƥ��' or @content-desc='���ƥ��']/preceding-sibling::node[1]"), 2, false);
        logger->info("[��ʼ��ս]");
        tap(select_with_text("��ʼ��ս"), 10, false);
        for (auto i2 = 0; i2 < 50; i2++) {
            std::string content_utf, options_utf;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                logger->debug("pull");
                pull();

                if (exist_with_text("������ս")) {
                    logger->info("[����]");
                    back();
                    break;
                }

                // ���
                // auto xpath_nodes = ui.select_nodes(gbk2utf("//node[starts-with(@text,'����������00') or starts-with(@content-desc,'����������00')]/following-sibling::node[1]/node[1]/node[1]/node[1]/node[1]/node[1]").c_str());
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

                // ѡ��
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
                logger->info("[˫�˶�ս] {}", utf2gbk(content_utf));
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    logger->info(utf2gbk(option_utf));
                }

                // ����ʾ
                auto result = db.get_answer("single", content_utf, options_utf);
                auto answer_db = result["answer"].asString();
                int answer_index;
                if (answer_db.size()) {
                    logger->info("[����ʾ]��{}", dump(result));
                    logger->info("[�ύ��]��{}", answer_db);
                    answer_index = c2i(answer_db[0]);
                } else {
                    logger->info("[����ʾ]��{}", "null");
                    answer_index = rand() % xpath_nodes.size();
                    // answer_index = 0;
                    logger->info("[�ύ��]��{}", i2c(answer_index));
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

    // ���ֽ���
    score();

    // �����δ���
    node = select("//node[@class='android.widget.ListView']/node[@index='7']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("�Ҳ���[ �����δ��� ]");
    score8 = atoi(sm[1].str().c_str());
    logger->info("[�����δ���]���ѻ�{}��/ÿ������5��", score8);

    back();
    logger->info("[��Ҫ����]");
    tap(select_with_text("��Ҫ����"), 10);
    pull();
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("��һ��"))
        tap(select_with_text("��һ��"));
    if (exist_with_text("֪����"))
        tap(select_with_text("֪����"));
    if (exist_with_text("��������"))
        tap(select_with_text("��������"));
    logger->info("[�����δ���]");
    tap(select("//node[@text='���а�' or @content-desc='���а�']/following-sibling::node[1]"), 10);

    for (auto i1 = 0; i1 < 50; i1++) {
        if (exist_with_text("���ջ��ֽ�����" + std::to_string(count) + "/2")) {
            logger->info("[����]");
            back(1, false);
            back(1, false);
            back();
            break;
        }
        logger->info("[��ʼ����]");
        tap(select_with_text("��ʼ����"), 10, false);
        for (auto i2 = 0; i2 < 50; i2++) {
            std::string content_utf, options_utf;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            try {
                logger->debug("pull");
                pull();

                if (exist_with_text("�¹�֪��"))
                    back();
                if (exist_with_text("������ս")) {
                    logger->info("[����]");
                    back();
                    break;
                }

                // ���
                // auto xpath_nodes = ui.select_nodes(gbk2utf("//node[starts-with(@text,'����������00') or starts-with(@content-desc,'����������00')]/following-sibling::node[1]/node[1]/node[1]/node[1]/node[1]/node[1]").c_str());
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

                // ѡ��
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
                logger->info("[�����δ���] {}", utf2gbk(content_utf));
                for (auto &xpath_node : xpath_nodes) {
                    std::string option_utf;
                    option_utf += xpath_node.node().attribute("text").value();
                    option_utf += xpath_node.node().attribute("content-desc").value();
                    logger->info(utf2gbk(option_utf));
                }

                // ����ʾ
                auto result = db.get_answer("single", content_utf, options_utf);
                auto answer_db = result["answer"].asString();
                int answer_index;
                if (answer_db.size()) {
                    logger->info("[����ʾ]��{}", dump(result));
                    logger->info("[�ύ��]��{}", answer_db);
                    answer_index = c2i(answer_db[0]);
                } else {
                    logger->info("[����ʾ]��{}", "null");
                    answer_index = rand() % xpath_nodes.size();
                    // answer_index = 0;
                    logger->info("[�ύ��]��{}", i2c(answer_index));
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

    // ���ֽ���
    score();

    // ����Ƶ��
    node = select("//node[@class='android.widget.ListView']/node[@index='11']/node[@index='3']/node[@index='0']");
    text = get_text(node);
    if (!std::regex_search(text, sm, std::regex("(\\d)")))
        throw std::runtime_error("�Ҳ���[ ����Ƶ�� ]");
    score13 = atoi(sm[1].str().c_str());
    logger->info("[����Ƶ��]���ѻ�{}��/ÿ������1��", score13);

    if (score13 >= 1) {
        logger->info("[����]");
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
            logger->info("[ȥ����]");
            tap(node);
            break;
        }
        swipe_up();
    }

    tap(select("//node[@class='android.support.v7.widget.RecyclerView' or @class='androidx.recyclerview.widget.RecyclerView']/node[1]"));
    back();
}

// ����
void adb::back(int64_t delay, bool is_pull) {
    tap(back_x, back_y, delay, is_pull);
}

// ��ҳ -> ѧϰ����
void adb::score() {
    logger->info("[�ҵ�]");
    tap(select_with_text("�ҵ�"));
    my_name = get_text(select("//node[@resource-id='cn.xuexi.android:id/my_display_name']"));

    for (int i = 0; i < 3; i++) {
        logger->info("[ѧϰ����]");
        tap(select_with_text("ѧϰ����"), 10);
        if (exist_with_text("����"))
            tap(select_with_text("����"), 10);
        if (exist_with_text("���ֹ���")) {
            pull();
            if (exist_with_text("�õģ�֪����"))
                tap(select_with_text("�õģ�֪����"));
            return;
        }
        back();
    }
    throw std::runtime_error("�㲻��[ѧϰ����]");
}

// ѧϰ���� -> ǿ���̳�
void adb::store() {
    for (int i = 0; i < 3; i++) {
        logger->info("[ǿ���̳�]");
        tap(select_with_text("ǿ���ǶҸ���"), 5);
        pull();
        if (exist_with_text("���ͨ��ϸ"))
            return;
        back();
    }
    throw std::runtime_error("�㲻��[ǿ���̳�]");
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
        // ��Ϊ�����360�����ҹ��ģ�����Ľ���xml��8��0xc20xa0ǰ�����ո�0x20����MuMuģ�����Ľ���xml��ֻ��8��0xc20xa0��ǰ��û�пո�\x20
        // �ο�[https://www.utf8-chartable.de/unicode-utf8-table.pl?utf8=dec]: U+00A0 194(0xc2) 160(0xa0) NO-BREAK SPACE
        if (!std::regex_search(content_utf, std::regex("\x20((\\xc2\\xa0){8})")) && std::regex_search(content_utf, std::regex("(\\xc2\\xa0){8}")))
            logger->warn("MuMuģ������");
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
    logger->debug("�ֱ��ʣ�{}��{}", width, height);

    try {
        pull();
        auto node = select("//node[@resource-id='cn.xuexi.android:id/BOTTOM_LAYER_VIEW_ID']/node[2]/node[1]/node[2]");
        int comment_count = atoi(get_text(node).c_str());
        if (comment_count > 0) {
            tap(node);
            node = select("//node[@class='android.support.v7.widget.RecyclerView']/node[3]/node[2]/node[1]");
            auto comment = get_text(node);
            tap(select_with_text("��ӭ������Ĺ۵�"));
            input_text(comment);
            pull();
            int x, y;
            getxy(x, y, select_with_text("����").attribute("bounds").value());
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
        tap(select_with_text("��������"));
    } catch (...) {
    }

    /*x = width / 2;
     y = 47 * height / 48;
     tap(x, y, 2, false);
     exec("adb shell input keyevent 01");
     input_text("��������μ�ʹ��");
     exec("adb shell input keyevent 111");

     x = 31 * width / 36;
     tap(x, y, 2, false);

     x = 17 * width / 18;
     tap(x, y, 2, false);
     exec("adb shell input keyevent 111");*/
}

// �������
void adb::tap(int x, int y, int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input tap " << x << " " << y;
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// ���node
void adb::tap(const pugi::xml_node &node, int64_t delay, bool is_pull) {
    int x, y;
    getxy(x, y, node.attribute("bounds").value());
    tap(x, y, delay, is_pull);
}

// �ϻ�
void adb::swipe_up(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 65) / 100.0) << " " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 25) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// �»�
void adb::swipe_down(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 25) / 100.0) << " " << (width * (std::rand() % 10 + 55) / 100.0) << " " << (height * (std::rand() % 10 + 65) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// ��
void adb::swipe_left(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 9 + 80) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (width * (std::rand() % 10 + 10) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// �һ�
void adb::swipe_right(int64_t delay, bool is_pull) {
    std::ostringstream oss;
    oss << "adb shell input swipe " << (width * (std::rand() % 10 + 10) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (width * (std::rand() % 9 + 80) / 100.0) << " " << (height * (std::rand() % 14 + 75) / 100.0) << " " << (std::rand() % 400 + 800);
    exec(oss.str());
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    if (is_pull)
        pull();
}

// ��ȡ����xml
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

// �������룬֧������
void adb::input_text(const std::string &msg) {
    std::ostringstream oss;
    oss << "adb shell am broadcast -a ADB_INPUT_TEXT --es msg \"" << msg << "\"";
    exec(oss.str());
}

// ִ�еײ�����
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

// ѡȡ�ض��ı��Ľڵ�
pugi::xml_node adb::select_with_text(const std::string &text) {
    std::ostringstream oss;
    oss << "//node[@text='" << text << "' or @content-desc='" << text << "']";
    return select(oss.str());
}

// �ж��ض��ı��Ľڵ��Ƿ����
bool adb::exist_with_text(const std::string &text) {
    try {
        select_with_text(text);
    } catch (...) {
        return false;
    }
    return true;
}

// ѡȡxpath�ڵ�
pugi::xml_node adb::select(const std::string &xpath) {
    auto xpath_utf = gbk2utf(xpath);
    auto node = ui.select_node(xpath_utf.c_str()).node();
    if (node.empty())
        throw std::runtime_error("�Ҳ���[ " + xpath + " ]");
    return node;
}

// �ж�xpath�ڵ��Ƿ����
bool adb::exist(const std::string &xpath) {
    try {
        select(xpath);
    } catch (...) {
        return false;
    }
    return true;
}

// ��ȡ�����ı�
std::string adb::get_text(const pugi::xml_node &node) {
    std::string text;
    text += node.attribute("text").value();
    text += node.attribute("content-desc").value();
    return utf2gbk(text);
}
