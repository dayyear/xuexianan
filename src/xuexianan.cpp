#include <iostream>
#include <iomanip>
#include <pugixml.hpp>

#include "adb.h"
#include "log.h"

// 从std::cin读取，trim，确保用户输入不为空
std::string user_input(const std::string &message) {
    std::string s;
    std::cout << message;
    for (;;) {
        std::getline(std::cin, s);
        // left trim
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        // right trim
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), s.end());
        if (s.size())
            break;
    }
    return s;
}

int main() {
    std::srand(std::time(NULL));
    for (;;) {
        try {
            adb adb;
            std::cout << std::endl;
            logger->info("-------- [学习安安：v2.15.0] --------");
            logger->info("[主菜单]");
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "1" << " - " << "连接安卓手机" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* 用USB线连接安卓手机，记得打开安卓系统的USB调试模式" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "2" << " - " << "连接MuMu模拟器" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* 需要设置最小字体，字体太大时不稳定" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "3" << " - " << "连接夜神模拟器" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* 安装夜神模拟器后，记得将目录[Nox\\bin]下的文件[adb.exe]和[nox_adb.exe]用本项目的[adb.exe]替换，以保持版本一致" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "Q" << " - " << "退出" << std::endl;
            std::string arg = user_input("请选择(1-3/Q): ");
            if (arg == "1") {
            } else if (arg == "2") {
                adb.connect("127.0.0.1", 7555);
            } else if (arg == "3") {
                adb.connect("127.0.0.1", 62001);
            } else if (arg == "Q" || arg == "q") {
                logger->info("退出");
                break;
            } else if (arg == "T" || arg == "t") {
                adb.connect("127.0.0.1", 62001);
                logger->info("[题库训练]");
                adb.init();
                adb.daily(1);
                break;
            } else {
                logger->warn("无效输入: {}", arg);
                continue;
            }
            auto time_start = clock();
            adb.init();
            adb.read();
            adb.listen();
            adb.daily();
            adb.challenge();
            adb.score();
            auto time_end = clock();
            logger->info("用时 {:.1f} 分钟。学习安安，享受生活！", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
        } catch (const std::exception &ex) {
            logger->error("{}", ex.what());
        }
        system("pause");
    }
    return 0;
}
