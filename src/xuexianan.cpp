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
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "1" << " - " << "���Ӱ�׿�ֻ�" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* ��USB�����Ӱ�׿�ֻ����ǵô򿪰�׿ϵͳ��USB����ģʽ" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "2" << " - " << "����MuMuģ����" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* ��Ҫ������С���壬����̫��ʱ���ȶ�" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "3" << " - " << "����ҹ��ģ����" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(8) << " " << "* ��װҹ��ģ�����󣬼ǵý�Ŀ¼[Nox\\bin]�µ��ļ�[adb.exe]��[nox_adb.exe]�ñ���Ŀ��[adb.exe]�滻���Ա��ְ汾һ��" << std::endl;
            std::cout << setiosflags(std::ios::right) << std::setw(3) << "Q" << " - " << "�˳�" << std::endl;
            std::string arg = user_input("��ѡ��(1-3/Q): ");
            if (arg == "1") {
            } else if (arg == "2") {
                adb.connect("127.0.0.1", 7555);
            } else if (arg == "3") {
                adb.connect("127.0.0.1", 62001);
            } else if (arg == "Q" || arg == "q") {
                logger->info("�˳�");
                break;
            } else if (arg == "T" || arg == "t") {
                adb.connect("127.0.0.1", 62001);
                logger->info("[���ѵ��]");
                adb.init();
                adb.daily(1);
                break;
            } else {
                logger->warn("��Ч����: {}", arg);
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
            logger->info("��ʱ {:.1f} ���ӡ�ѧϰ�������������", (float) (time_end - time_start) / CLOCKS_PER_SEC / 60);
        } catch (const std::exception &ex) {
            logger->error("{}", ex.what());
        }
        system("pause");
    }
    return 0;
}
