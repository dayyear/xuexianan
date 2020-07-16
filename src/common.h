#ifndef COMMON_H_
#define COMMON_H_

// ×Ö·û±àÂë×ª»»
std::string utf2gbk(const char *s);
std::string utf2gbk(const std::string &s);
std::string gbk2utf(const char *s);
std::string gbk2utf(const std::string &s);

// ÎÄ±¾ÎÄ¼ş¶ÁĞ´
std::string read_file(const std::string& file_name);
void write_file(const std::string& file_name, const std::string& s);

// trim
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::string ltrim_copy(std::string s);
std::string rtrim_copy(std::string s);
std::string trim_copy(std::string s);

// json¸¨Öú
Json::Value parse(const std::string &s);
std::string dump(const Json::Value &value);

#endif /* COMMON_H_ */
