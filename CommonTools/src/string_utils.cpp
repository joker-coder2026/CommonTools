#include "string_utils.h"

#include <windows.h>
#include <stdexcept>
#include <cstdarg>
#include <sstream>
#include <cctype>

namespace string_utils
{
	std::vector<std::string> split(const std::string& str, char delimiter, int pad_number)
	{
		std::stringstream ss(str);
		std::vector<std::string> tokens;
		std::string token;
		while (std::getline(ss, token, delimiter))
		{
			tokens.push_back(std::move(token));
		}
		if (pad_number > 0)
		{
			while (tokens.size() < pad_number)
			{
				tokens.push_back("");
			}
		}
		return tokens;
	}

	std::vector<std::string> split(const std::string& str, std::string delimiters, int pad_number)
	{
		std::vector<std::string> tokens;
		if (!str.empty() && !delimiters.empty()) // 解决str为空时无法按pad_number填充
		{
			size_t start = 0;
			size_t end = str.find_first_of(delimiters);

			while (end != std::string::npos)
			{
				if (end != start)
				{
					tokens.push_back(str.substr(start, end - start));
				}
				start = end + 1;
				end = str.find_first_of(delimiters, start);
			}

			if (start < str.length())
			{
				tokens.push_back(str.substr(start));
			}
		}

		if (pad_number > 0)
		{
			while (tokens.size() < pad_number)
			{
				tokens.push_back("");
			}
		}
		return tokens;
	}

	std::string merge(const std::vector<std::string>& list, char delimiter)
	{
		return merge(list, std::string(1, delimiter));
	}

	std::string merge(const std::vector<std::string>& list, std::string delimiters)
	{
		std::string merge;
		for (int i = 0; i < list.size(); ++i)
		{
			merge.append(list.at(i));
			if (i < list.size() - 1)
				merge.append(delimiters);
		}
		return merge;
	}

	std::string format(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		size_t size = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)) + 1;
		va_end(args);

		std::string result;
		result.resize(size);
		va_start(args, format);
		vsnprintf(&result[0], size, format, args);
		va_end(args);
		if (size > 0)
			result.resize(size - 1); // 去除末尾的'\0'
		return result; // 直接返回string,避免野指针  
	}

	void format(std::string& out, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		size_t size = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)) + 1;
		va_end(args);

		out.clear();
		out.resize(size);
		va_start(args, format);
		vsnprintf(&out[0], size, format, args);
		va_end(args);
		if (size > 0)
			out.resize(size - 1); // 去除末尾的'\0'
	}

	std::string GBKToUTF8(const std::string& gbk)
	{
#ifdef _WIN32
		std::string gbk_str(gbk);
		int utf8_len = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, nullptr, 0);
		if (utf8_len <= 0)
			throw std::runtime_error("GBK to WideChar failed");

		std::wstring wstr(utf8_len, 0);
		MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, &wstr[0], utf8_len);

		int gbk_len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("WideChar to UTF8 failed");

		std::string utf8_str(gbk_len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], gbk_len, nullptr, nullptr);
		utf8_str.pop_back(); // 移除末尾的\0
		return utf8_str;
#else
		throw std::runtime_error("Windows API unavailable on this platform");
#endif
	}

	std::string UTF8ToGBK(const std::string& utf8)
	{
#ifdef _WIN32
		std::string utf8_str(utf8);
		int utf8_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
		if (utf8_len <= 0)
			throw std::runtime_error("UTF8 to WideChar failed");

		std::wstring wstr(utf8_len, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wstr[0], utf8_len);

		int gbk_len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("WideChar to GBK failed");

		std::string gbk_str(gbk_len, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &gbk_str[0], gbk_len, nullptr, nullptr);
		gbk_str.pop_back();
		return gbk_str;
#else
		throw std::runtime_error("Windows API unavailable on this platform");
#endif
	}

	std::string trim_left(const std::string& str)
	{
		size_t start = str.find_first_not_of(" \t\n\r\v\f");
		return (start == std::string::npos) ? str : str.substr(start);
	}

	std::string trim_right(const std::string& str)
	{
		size_t end = str.find_last_not_of(" \t\n\r\v\f");
		return (end == std::string::npos) ? str : str.substr(0, end + 1);
	}

	std::string trim(const std::string& str)
	{
		return trim_left(trim_right(str));
	}

	std::string trim(const std::string& str, const std::string& chars)
	{
		size_t start = str.find_first_not_of(chars);
		std::string temp = (start == std::string::npos) ? str : str.substr(start);
		size_t end = temp.find_last_not_of(chars);
		return (end == std::string::npos) ? temp : temp.substr(0, end + 1);
	}

	std::string to_upper(const std::string& str)
	{
		std::string res = str;
		for (char& c : res)
		{
			c = static_cast<char>(toupper(static_cast<unsigned char>(c))); // c = toupper(c);
		}
		return res;
	}

	std::string to_lower(const std::string& str)
	{
		std::string res = str;
		for (char& c : res)
		{
			c = static_cast<char>(tolower(static_cast<unsigned char>(c))); // c = tolower(c);
		}
		return res;
	}

	std::string replace(const std::string& str, const std::string& old_str, const std::string& new_str)
	{
		std::string tmp(str);
		std::string::size_type pos = 0;
		while ((pos = tmp.find(old_str)) != std::string::npos)
		{
			tmp.replace(pos, old_str.length(), new_str);
			pos += new_str.length();
		}
		return tmp;
	}
}
