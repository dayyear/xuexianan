# 学习安安

## 目标

学习安安，享受生活！

## 编译运行

1. 操作系统windowns7或者windows10

2. 两种连接方式：连接安卓手机或者连接模拟器

   - 连接安卓手机

     用USB线连接安卓手机，记得打开安卓系统的USB调试模式

   - 连接模拟器

     [夜神模拟器](https://www.yeshen.com/)，安装[夜神模拟器](https://www.yeshen.com/)后，记得将目录[Nox\bin]下的文件[adb.exe]和[nox_adb.exe]用本项目的[[adb.exe](bin)]替换，以保持版本一致

     [MuMu模拟器](http://mumu.163.com/)，现阶段发现一些问题，不建议使用

3. 在安卓系统中安装[ADBKeyboard](bin)，用于中文输入

4. 在安卓系统中安装学习强国App，当前版本为v2.14.1

5. 打开学习强国，登录

6. 直接双击`make_run.bat`编译运行

## 所需库

1. pugixml-1.10

   用于解析安卓ui

2. jsoncpp-1.8.4

   不解释

3. spdlog-1.4.0

   用于产生日志

4. iconv

   字符编码转换

5. sqlite

   题目答案数据库

## eclipse配置

### C/C++ Builder

- GCC C++ Compiler > Dialect
  -std=c++11

- GCC C++ Compiler > Preprocessor

  SPDLOG_COMPILED_LIB

- GCC C++ Compiler > Includes

  1. ../third_party/pugixml-1.10/include
  2. ../third_party/jsoncpp-1.8.4/include
  3. ../third_party/spdlog-1.4.0/include
  4. ../mingw32-7.3.0/opt/include

- MinGW C++ Linker > Libraries > Library search path (-L)

  1. ../third_party/pugixml-1.10/lib
  2. ../third_party/jsoncpp-1.8.4/lib
  3. ../third_party/spdlog-1.4.0/lib
  4. ../mingw32-7.3.0/opt/lib

- MinGW C++ Linker > Libraries > Libraries (-l)

  1. pugixml
  2. jsoncpp
  3. spdlog
  4. iconv
  5. sqlite3

- MinGW C++ Linker > Miscellaneous

  -static

### parallel (-j)

- C/C++ Builder > Behavior

  √ Enable parallel build












