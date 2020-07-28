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
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "1" << " - " << "积分37" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "2" << " - " << "积分37 + 点点通33" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "T" << " - " << "题库训练" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "Q" << " - " << "退出" << std::endl;
            std::string arg = user_input("请选择(1/2/T/Q): ");
            if (arg == "1") {
                auto time_start = clock();
                adb.init();
                adb.read(false);
                adb.listen(false);
                adb.daily(false);
                adb.challenge(false);
                auto time_end = clock();
                logger->info("用时 {:.1f} 分钟。学习安安，享受生活！", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
            } else if (arg == "2") {
                auto time_start = clock();
                adb.init();
                adb.read(true);
                adb.listen(true);
                adb.daily(false);
                adb.challenge(true);
                auto time_end = clock();
                logger->info("用时 {:.1f} 分钟。学习安安，享受生活！", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
            } else if (arg == "T" || arg == "t") {
                logger->info("[题库训练]");
                adb.init();
                adb.daily(true);
                break;
            } else if (arg == "Q" || arg == "q") {
                logger->info("退出");
                break;
            } else {
                logger->warn("无效输入: {}", arg);
                continue;
            }
        } catch (const std::exception &ex) {
            logger->error("{}", ex.what());
        }
        system("pause");
    }
    return 0;
}
