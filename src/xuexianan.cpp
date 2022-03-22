#include <iostream>
#include <iomanip>
#include <pugixml.hpp>

#include "adb.h"
#include "log.h"

// ��std::cin��ȡ��trim��ȷ���û����벻Ϊ��
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
            logger->info("-------- [ѧϰ������v2.15.0] --------");
            logger->info("[���˵�]");
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "1" << " - " << "����46" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "2" << " - " << "����46 + ���ͨ33" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "T" << " - " << "���ѵ��" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "Q" << " - " << "�˳�" << std::endl;
            std::string arg = user_input("��ѡ��(1/2/T/Q): ");
            if (arg == "1") {
                auto time_start = clock();
                adb.init();
                for (;;) {
                    try {
                        adb.local();
                        adb.daily(false);
                        adb.challenge(false);
                        adb.read(false);
                        adb.listen(false);
                        adb.race4();
                        adb.race2();
                        break;
                    } catch (const std::exception &ex) {
                        logger->error("{}", ex.what());
                        adb.repair();
                    }
                }
                auto time_end = clock();
                logger->info("��ʱ {:.1f} ���ӡ�ѧϰ�������������", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
            } else if (arg == "2") {
                auto time_start = clock();
                adb.init();
                for (;;) {
                    try {
                        adb.local();
                        adb.daily(false);
                        adb.challenge(true);
                        adb.read(true);
                        adb.listen(true);
                        adb.race4();
                        adb.race2();
                        break;
                    } catch (const std::exception &ex) {
                        logger->error("{}", ex.what());
                        adb.repair();
                    }
                }
                auto time_end = clock();
                logger->info("��ʱ {:.1f} ���ӡ�ѧϰ�������������", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
            } else if (arg == "S" || arg == "s") {
                auto time_start = clock();
                adb.init();
                for (;;) {
                    try {
                        adb.local();
                        adb.race2();
                        adb.daily(false);
                        adb.challenge(false);
                        adb.read(false);
                        adb.listen(false);
                        adb.race4(3);
                        break;
                    } catch (const std::exception &ex) {
                        logger->error("{}", ex.what());
                        adb.repair();
                    }
                }
                auto time_end = clock();
                logger->info("��ʱ {:.1f} ���ӡ�ѧϰ�������������", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
            } else if (arg == "T" || arg == "t") {
                logger->info("[���ѵ��]");
                adb.init();
                for (;;) {
                    try {
                        adb.challenge(false, true);
                        adb.daily(true);
                    } catch (const std::exception &ex) {
                        logger->error("{}", ex.what());
                        adb.repair();
                    }
                }
            } else if (arg == "test") {
                logger->info("[����]");
                for (;;) {
                    adb.test();
                    system("pause");
                }
            } else if (arg == "Q" || arg == "q") {
                logger->info("�˳�");
                break;
            } else {
                logger->warn("��Ч����: {}", arg);
                continue;
            }
        } catch (const std::exception &ex) {
            logger->error("{}", ex.what());
        }
        system("pause");
    }
    return 0;
}
