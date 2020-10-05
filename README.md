# 学习安安

## 目标

学习安安，享受生活！

## 编译运行

1. 操作系统windowns7或者windows10
2. 在windows上安装[夜神模拟器](https://www.yeshen.com/)，其他模拟器会出现一些小问题，不推荐使用
3. 在模拟器的安卓系统上安装学习强国App(v2.15.0)和[ADBKeyboard](bin)
4. 打开学习强国，登录
5. 直接双击`make_run.bat`编译运行，主菜单如下
   - 1 - 积分39
   - 2 - 积分39 + 点点通33
   - T - 题库训练
   - Q - 退出
6. 因怕被请喝茶，把`third_party`加密了，所以正常状态下少了`third_party`是无法编译的，需要的联系我

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












