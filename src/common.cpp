#include <iconv.h> // iconv_open, iconv, iconv_close
#include <cstring> // strlen
#include <sstream>
#include <fstream>
#include <algorithm>
#include <json/reader.h>
#include <json/writer.h>

#include "common.h"

std::string conv_between(const char *text, const std::string &to_encoding, const std::string &from_encoding) {
    const int BUFFER_SIZE = 1024;
    /* iconv_open */
    iconv_t cd = iconv_open(to_encoding.c_str(), from_encoding.c_str());
    if (cd == (iconv_t) - 1) {
        perror("iconv_open");
        throw std::runtime_error("iconv_open");
    }
    /* buffer */
    char buffer[BUFFER_SIZE];
    /* four parameters for iconv */
    char *inbuf = (char*) text;
    size_t inbytesleft = strlen(text);
    char *outbuf;
    size_t outbytesleft;

    /* If all input from the input buffer is successfully converted and stored in the output buffer, the
     * function returns the number of non-reversible conversions performed. In all other cases the
     * return value is (size_t) -1 and errno is set appropriately. In such cases the value pointed to by
     * inbytesleft is nonzero.
     * - E2BIG The conversion stopped because it ran out of space in the output buffer. */
    std::ostringstream oss;
    while (inbytesleft) {
        outbuf = buffer;
        outbytesleft = BUFFER_SIZE;
        size_t r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        if (r != (size_t) -1)
            oss.write(buffer, BUFFER_SIZE - outbytesleft);
        else if (errno == E2BIG)
            oss.write(buffer, BUFFER_SIZE - outbytesleft);
        else if (errno == EILSEQ) {
            oss.write(buffer, BUFFER_SIZE - outbytesleft);
            ++inbuf;
            --inbytesleft;
        } else if (errno == EINVAL) {
            oss.write(buffer, BUFFER_SIZE - outbytesleft);
            break;
        } else {
            iconv_close(cd);
            perror("iconv");
            throw std::runtime_error("iconv");
        }
    }
    /* iconv_close */
    iconv_close(cd);
    return oss.str();
}

std::string utf2gbk(const char *s) {
    return conv_between(s, "gbk", "utf-8");
}
std::string utf2gbk(const std::string &s) {
    return utf2gbk(s.c_str());
}
std::string gbk2utf(const char *s) {
    return conv_between(s, "utf-8", "gbk");
}
std::string gbk2utf(const std::string &s) {
    return gbk2utf(s.c_str());
}

std::string read_file(const std::string& file_name) {
    std::ifstream file(file_name, std::ios::binary | std::ios::in);
    std::string s((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return s;
}

void write_file(const std::string& file_name, const std::string& s) {
    std::ofstream file(file_name, std::ios::binary | std::ios::out);
    file << s;
    file.close();
}

void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

Json::Value parse(const std::string &s) {
    Json::Value json;
    std::istringstream(s) >> json;
    return json;
}

std::string dump(const Json::Value &value){
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    builder["commentStyle"] = "None";
    return Json::writeString(builder, value);
}
