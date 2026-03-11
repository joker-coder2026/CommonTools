//#include "pch.h"

#include "CommonTools.h"

#include <fstream>
#include <cctype>
#include <algorithm>
#include <direct.h>   // for _mkdir, _rmdir
#include <corecrt_io.h> // for _access
#include <regex>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include "Windows.h"

#include "tinyxml/tinyxml.h"
#include "json/json.h"
#include "sqlite3.h"
#include "XTrace.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/common.h"

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")


namespace CommonTools
{
#pragma region BitTools
	// ========== 32/64位通用实现 ==========
	template <int BitWidth>
	void BitTools<BitWidth>::Set(ValueType& value, int bit_idx, bool bit_value)
	{
		CheckBitIndex(bit_idx);
		const UnsignedType mask = ShiftBase << static_cast<UnsignedType>(bit_idx);
		// 合并清零+置位：仅1次内存写操作，编译器可优化为单条指令
		value = (value & ~static_cast<ValueType>(mask)) | (bit_value ? static_cast<ValueType>(mask) : 0);
	}

	template <int BitWidth>
	bool BitTools<BitWidth>::Get(ValueType value, int bit_idx)
	{
		CheckBitIndex(bit_idx);
		// 无符号转换避免符号扩展，移位后与1比较
		const UnsignedType unsigned_val = static_cast<UnsignedType>(value);
		return (unsigned_val >> static_cast<UnsignedType>(bit_idx)) & ShiftBase;
	}

	template <int BitWidth>
	void BitTools<BitWidth>::Toggle(ValueType& value, int bit_idx)
	{
		CheckBitIndex(bit_idx);
		const UnsignedType mask = ShiftBase << static_cast<UnsignedType>(bit_idx);
		value ^= static_cast<ValueType>(mask);
	}

	template <int BitWidth>
	int BitTools<BitWidth>::CountSetBits(ValueType value) noexcept
	{
		const UnsignedType unsigned_val = static_cast<UnsignedType>(value);
		UnsignedType val = unsigned_val;
		auto count = 0;
		// Brian Kernighan算法：循环次数=置1位数，32/64位通用且最优
		while (val != 0)
		{
			val &= val - 1; // 清除最低位的1
			++count;
		}
		return count;
	}

	// ========== 显式模板实例化（必须，否则链接失败） ==========
	template class BitTools<32>;
	template class BitTools<64>;
#pragma endregion

#pragma region StringUtils
	template <typename T>
	std::string StringUtils::ToString(const T& value, int precision)
	{
		std::stringstream oss;
		if (typeid(value) == typeid(float) || typeid(value) == typeid(double) || typeid(value) == typeid(long double))
		{
			if (precision > -1)
			{
				oss.precision(precision);
				oss << std::fixed << value;
			}
			else
				oss << value;
		}
		else
			oss << value;
		return oss.str();
	}

	std::vector<std::string> StringUtils::Split(const std::string& str, const char& sep, int max_split_num)
	{
		return Split(str, std::string(1, sep), max_split_num);
	}

	std::vector<std::string> StringUtils::Split(const std::string& str, const std::string& seps, int max_split_num)
	{
		std::vector<std::string> tokens;
		if (str.empty() || seps.empty())
			return tokens;

		size_t sep_count = std::count_if(str.begin(), str.end(), [&](char c)
		{
			return seps.find(c) != std::string::npos;
		});
		tokens.reserve((max_split_num > 0 && max_split_num < sep_count + 1) ? max_split_num : sep_count + 1);

		size_t start = 0;
		size_t end = str.find_first_of(seps);

		while (end != std::string::npos)
		{
			tokens.emplace_back(str, start, end - start);
			if (max_split_num > 0 && tokens.size() >= max_split_num - 1)
				break;

			start = end + 1;
			end = str.find_first_of(seps, start);
		}

		if ((max_split_num <= 0 || tokens.size() < static_cast<size_t>(max_split_num) - 1) && start <= str.size())
			tokens.emplace_back(str, start);

		if (max_split_num > 0 && tokens.size() < static_cast<size_t>(max_split_num))
			tokens.resize(max_split_num, "");

		return tokens;
	}

	std::string StringUtils::Merge(const std::vector<std::string>& list, const char& sep)
	{
		return Merge(list, std::string(1, sep));
	}

	std::string StringUtils::Merge(const std::vector<std::string>& list, const std::string& seps)
	{
		if (list.empty())
			return {};

		const size_t list_size = list.size();
		const size_t sep_size = seps.size();

		size_t totalSize = 0;
		const auto* p = list.data();
		const auto* end = p + list_size;
		while (p != end)
		{
			totalSize += p->size();
			++p;
		}
		totalSize += sep_size * (list_size - 1);

		std::string merge;
		merge.resize(totalSize);

		char* dst = &merge[0];
		const auto* src_list = list.data();
		for (size_t i = 0; i < list_size; ++i)
		{
			if (i > 0 && sep_size > 0)
			{
				std::memcpy(dst, seps.data(), sep_size);
				dst += sep_size;
			}

			const auto& s = src_list[i];
			std::memcpy(dst, s.data(), s.size());
			dst += s.size();
		}

		return merge;
	}

	std::string StringUtils::Format(const char* format, ...)
	{
		if (!format)
			throw std::invalid_argument("StringUtils::Format: format string is null");

		va_list args;
		va_start(args, format);
		const int len = vsnprintf(nullptr, 0, format, args);
		va_end(args);

		if (len < 0)
			throw std::runtime_error("StringUtils::Format: failed to calculate format length");

		std::string result;
		if (len > 0)
		{
			result.reserve(static_cast<size_t>(len) + 1);
			result.resize(static_cast<size_t>(len) + 1);

			va_start(args, format);
			const int ret = vsnprintf(&result[0], result.size(), format, args);
			va_end(args);

			if (ret < 0 || static_cast<size_t>(ret) != static_cast<size_t>(len))
				throw std::runtime_error(
					"StringUtils::Format: failed to format string (ret=" + std::to_string(ret) + ")");

			result.resize(static_cast<size_t>(len));
		}

		return result;
	}

	int StringUtils::Format(std::string& out, const char* format, ...)
	{
		if (!format)
		{
			out.clear();
			throw std::invalid_argument("StringUtils::Format: format string is null");
		}

		va_list args_len;
		va_start(args_len, format);
		const int len = vsnprintf(nullptr, 0, format, args_len);
		va_end(args_len);

		if (len < 0)
		{
			out.clear();
			throw std::runtime_error(
				"StringUtils::Format: failed to calculate format length (error=" + std::to_string(len) + ")");
		}

		out.clear();
		auto result = 0;
		if (len > 0)
		{
			const size_t required_size = static_cast<size_t>(len) + 1;
			if (out.capacity() < required_size)
				out.reserve(static_cast<size_t>(required_size * 1.5));
			out.resize(required_size);

			va_list args_format;
			va_start(args_format, format);
			const int ret = vsnprintf(&out[0], out.size(), format, args_format);
			va_end(args_format);

			if (ret < 0 || static_cast<size_t>(ret) != static_cast<size_t>(len))
			{
				out.clear();
				throw std::runtime_error(
					"StringUtils::Format: failed to format string (ret=" + std::to_string(ret) + ", expected=" +
					std::to_string(len) + ")");
			}

			out.resize(static_cast<size_t>(len));
			result = len;
		}

		// 返回值：成功格式化的字符数（空字符串返回0）
		return result;
	}

	std::string StringUtils::G2U(const std::string& gbk)
	{
#ifndef _WIN32
		throw std::runtime_error("StringUtils::G2U: Windows API unavailable on this platform");
#else
		if (gbk.empty()) return {};

		const auto src_len = static_cast<int>(gbk.size());
		const int w_len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, gbk.c_str(), src_len, nullptr, 0);
		if (w_len <= 0)
			throw std::runtime_error("G2U: GBK to WideChar failed (error=" + std::to_string(GetLastError()) + ")");

		std::wstring w_str;
		w_str.reserve(static_cast<size_t>(w_len));
		w_str.resize(static_cast<size_t>(w_len));
		if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, gbk.c_str(), src_len, &w_str[0], w_len) <= 0)
			throw std::runtime_error(
				"G2U: GBK to WideChar convert failed (error=" + std::to_string(GetLastError()) + ")");

		const int u8_len = WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), w_len, nullptr, 0, nullptr, nullptr);
		if (u8_len <= 0)
			throw std::runtime_error("G2U: WideChar to UTF8 failed (error=" + std::to_string(GetLastError()) + ")");

		std::string utf8_str;
		utf8_str.reserve(static_cast<size_t>(u8_len));
		utf8_str.resize(static_cast<size_t>(u8_len));
		if (WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), w_len, &utf8_str[0], u8_len, nullptr, nullptr) <= 0)
			throw std::runtime_error(
				"G2U: WideChar to UTF8 convert failed (error=" + std::to_string(GetLastError()) + ")");

		utf8_str.resize(static_cast<size_t>(u8_len));
		return utf8_str; // NRVO优化生效
#endif
	}

	std::string StringUtils::U2G(const std::string& utf8)
	{
#ifndef _WIN32
		throw std::runtime_error("StringUtils::U2G: Windows API unavailable on this platform");
#else
		if (utf8.empty()) return {};

		const auto src_len = static_cast<int>(utf8.size());
		const int w_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), src_len, nullptr, 0);
		if (w_len <= 0)
			throw std::runtime_error("U2G: UTF8 to WideChar failed (error=" + std::to_string(GetLastError()) + ")");

		std::wstring w_str;
		w_str.reserve(static_cast<size_t>(w_len));
		w_str.resize(static_cast<size_t>(w_len));
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), src_len, &w_str[0], w_len) <= 0)
			throw std::runtime_error(
				"U2G: UTF8 to WideChar convert failed (error=" + std::to_string(GetLastError()) + ")");

		const int gbk_len = WideCharToMultiByte(CP_ACP, 0, w_str.c_str(), w_len, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("U2G: WideChar to GBK failed (error=" + std::to_string(GetLastError()) + ")");

		std::string gbk_str;
		gbk_str.reserve(static_cast<size_t>(gbk_len));
		gbk_str.resize(static_cast<size_t>(gbk_len));
		if (WideCharToMultiByte(CP_ACP, 0, w_str.c_str(), w_len, &gbk_str[0], gbk_len, nullptr, nullptr) <= 0)
			throw std::runtime_error(
				"U2G: WideChar to GBK convert failed (error=" + std::to_string(GetLastError()) + ")");

		gbk_str.resize(static_cast<size_t>(gbk_len));
		return gbk_str;
#endif
	}

	std::string StringUtils::TrimLeft(const std::string& str)
	{
		if (str.empty())
			return str;

		static const auto whitespace = " \t\n\r\v\f";
		const size_t start = str.find_first_not_of(whitespace);

		return (start == std::string::npos) ? std::string() : str.substr(start);
	}

	std::string StringUtils::TrimRight(const std::string& str)
	{
		if (str.empty())
			return str;

		static const auto whitespace = " \t\n\r\v\f";
		const size_t end = str.find_last_not_of(whitespace);

		return (end == std::string::npos) ? std::string() : str.substr(0, end + 1);
	}

	std::string StringUtils::Trim(const std::string& str)
	{
		return TrimRight(TrimLeft(str));
	}

	std::string StringUtils::Trim(const std::string& str, const std::string& chars)
	{
		if (str.empty() || chars.empty()) return str;

		const size_t start = str.find_first_not_of(chars);
		if (start == std::string::npos)
			return std::string();

		const size_t end = str.find_last_not_of(chars);
		if (end == std::string::npos)
			return std::string();

		if (start > end)
			return std::string();

		return str.substr(start, end - start + 1);
	}

	std::string StringUtils::ToUpper(const std::string& str)
	{
		if (str.empty()) return str;

		std::string res = str;
		char* ptr = &res[0];
		const size_t len = res.size();

		for (size_t i = 0; i < len; ++i)
			ptr[i] = static_cast<char>(toupper(static_cast<unsigned char>(ptr[i])));
		return res;
	}

	std::string StringUtils::ToLower(const std::string& str)
	{
		if (str.empty()) return str;

		std::string res = str;
		char* ptr = &res[0];
		const size_t len = res.size();

		for (size_t i = 0; i < len; ++i)
			ptr[i] = static_cast<char>(tolower(static_cast<unsigned char>(ptr[i])));
		return res;
	}
#pragma endregion

#pragma region TimePoint
	TimePoint::TimePoint(std::time_t timestamp)
	{
		time_point_ = (timestamp == 0)
			              ? std::chrono::system_clock::now()
			              : std::chrono::system_clock::from_time_t(timestamp);
	}

	TimePoint::TimePoint(const std::chrono::system_clock::time_point& tp) : time_point_(tp)
	{
	}

	int64_t TimePoint::ToTimeStamp()
	{
		// 修复：显式指定 duration_cast 模板参数，避免类型推导错误
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			time_point_.time_since_epoch()
		).count();
	}

	std::string TimePoint::ToString(const std::string& format)
	{
		if (format.empty())
			throw std::invalid_argument("format empty");

		const auto sec_ts = std::chrono::system_clock::to_time_t(time_point_);
		const auto ms = static_cast<int>(ToTimeStamp() % 1000);
		char ms_buf[4] = {0};
		FormatMs(ms, ms_buf);

		const size_t ms_pos = format.find("%f") != std::string::npos
			                      ? format.find("%f")
			                      : format.find("%F");

		std::tm tm{};
		if (localtime_s(&tm, &sec_ts) != 0)
			throw std::runtime_error("localtime_s failed");

		std::ostringstream oss;
		if (ms_pos != std::string::npos)
		{
			oss << std::put_time(&tm, format.substr(0, ms_pos).c_str())
				<< ms_buf
				<< format.substr(ms_pos + 2);
		}
		else
			oss << std::put_time(&tm, format.c_str());

		return oss.str();
	}

	TimePoint TimePoint::Now()
	{
		return TimePoint(std::chrono::system_clock::now());
	}

	std::string TimePoint::ToString(int64_t timestamp, const std::string& format)
	{
		if (timestamp < 0)
			throw std::invalid_argument("timestamp < 0");

		const auto sec = timestamp / 1000;
		const auto ms = std::chrono::milliseconds(static_cast<int>(timestamp % 1000));
		return TimePoint(std::chrono::system_clock::from_time_t(sec) + ms).ToString(format);
	}

	int64_t TimePoint::ToTimestamp(const std::string& timeStr, const std::string& format)
	{
		if (timeStr.empty() || format.empty())
			throw std::invalid_argument("empty input");

		std::string fmt = format;
		const size_t ms_pos = fmt.find("%f") != std::string::npos ? fmt.find("%f") : fmt.find("%F");
		if (ms_pos != std::string::npos)
			fmt.erase(ms_pos, 2);

		// 提取毫秒
		auto ms = 0;
		size_t sep = timeStr.size();
		while (sep > 0 && isdigit(static_cast<unsigned char>(timeStr[sep - 1])))
			--sep;

		std::string base_time = timeStr;
		if (sep < timeStr.size())
		{
			std::string ms_str = timeStr.substr(sep, 3);
			for (char c : ms_str)
				if (isdigit(static_cast<unsigned char>(c))) ms = ms * 10 + (c - '0');
			ms = ClampInt(ms, 0, 999);
			base_time = timeStr.substr(0, sep);
		}

		std::tm tm{};
		std::istringstream iss(base_time);
		iss >> std::get_time(&tm, fmt.c_str());
		if (iss.fail() || !iss.eof())
			throw std::invalid_argument("parse time failed");

		const std::time_t t = mktime(&tm);
		if (t == -1)
			throw std::invalid_argument("mktime failed");

		auto tp = std::chrono::system_clock::from_time_t(t) + std::chrono::milliseconds(ms);
		return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
	}

	void TimePoint::FormatMs(int ms, char* buf)
	{
		std::snprintf(buf, 4, "%03d", ms < 0 ? 0 : (ms > 999 ? 999 : ms));
	}

	int TimePoint::ClampInt(int val, int min_val, int max_val)
	{
		if (val < min_val) return min_val;
		if (val > max_val) return max_val;
		return val;
	}
#pragma endregion

#pragma region FileSystem
	bool FileSystem::Exists(const std::string& path)
	{
		return _access(path.c_str(), 0) == 0;
	}

	bool FileSystem::IsFile(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
			return false;
		return (info.st_mode & S_IFMT) == S_IFREG;
	}

	bool FileSystem::CreateFile(const std::string& path)
	{
		std::ofstream file(path);
		if (!file)
			return false;
		file.close();
		return true;
	}

	bool FileSystem::RenameFile(const std::string& srcpath, const std::string& dstpath)
	{
		std::ifstream file(srcpath);
		if (!file)
			return false;
		file.close();

		if (std::rename(srcpath.c_str(), dstpath.c_str()) != 0)
		{
			if (::MoveFileEx(srcpath.c_str(), dstpath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
				return false;
		}
		return true;
	}

	bool FileSystem::CopyFile(const std::string& srcpath, const std::string& dstpath)
	{
		if (!IsFile(srcpath))
			return false;

		std::ifstream in(srcpath, std::ios::binary);
		if (!in.is_open())
			return false;

		std::ofstream out(dstpath, std::ios::binary);
		if (!out.is_open())
		{
			in.close();
			return false;
		}

		out << in.rdbuf();

		in.close();
		out.close();
		return true;
	}

	bool FileSystem::MoveFile(const std::string& srcpath, const std::string& dstpath)
	{
		if (!IsFile(srcpath))
			return false;

		if (rename(srcpath.c_str(), dstpath.c_str()) != 0)
		{
			if (::MoveFileEx(srcpath.c_str(), dstpath.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
				return false;
		}
		return true;
	}

	bool FileSystem::DeleteFile(const std::string& path)
	{
		if (!IsFile(path))
			return false;
		return remove(path.c_str()) == 0;
	}

	std::vector<std::string> FileSystem::GetFiles(const std::string& path, const std::string& extension)
	{
		std::vector<std::string> files;
		if (!IsDirectory(path))
			return files;

		WIN32_FIND_DATA findData;
		HANDLE hFind = ::FindFirstFile((path + "\\*").c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE) return files;

		do
		{
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string filename(findData.cFileName);
				if (extension.empty() || filename.rfind(extension) == filename.length() - extension.length())
					files.push_back(path + "\\" + filename);
			}
		}
		while (::FindNextFile(hFind, &findData));
		FindClose(hFind);

		return files;
	}

	size_t FileSystem::GetFileSize(const std::string& path)
	{
		if (!IsFile(path))
			return 0;

		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open())
			return 0;

		std::streampos size = in.tellg();
		in.close();
		return size;
	}

	std::time_t FileSystem::GetFileCreateTime(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
			return 0;
		return info.st_ctime;
	}

	std::time_t FileSystem::GetFileModifiedTime(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
			return 0;
		return info.st_mtime;
	}

	std::string FileSystem::GetFileName(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
			return path;
		return path.substr(pos + 1);
	}

	std::string FileSystem::GetFilePath(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
			return "";
		return path.substr(0, pos);
	}

	std::string FileSystem::GetFileExtensionName(const std::string& path)
	{
		size_t pos = path.find_last_of('.');
		if (pos == std::string::npos)
			return "";
		return path.substr(pos + 1);
	}

	bool FileSystem::IsDirectory(const std::string& path)
	{
		std::string _path(path);
		while (!_path.empty() && (_path.back() == '\\' || _path.back() == '/'))
			_path.pop_back();
		if (_path.length() == 2 && _path[1] == ':')
			return false;

		struct stat info;
		if (stat(_path.c_str(), &info) != 0)
			return false;
		return (info.st_mode & S_IFMT) == S_IFDIR;
	}

	bool FileSystem::CreateDirectorys(const std::string& path)
	{
		if (Exists(path))
			return IsDirectory(path);

		// 递归创建目录
		size_t pos = 0;
		do
		{
			pos = path.find_first_of("\\/", pos + 1);
			std::string subdir = path.substr(0, pos);
			if (!Exists(subdir) && _mkdir(subdir.c_str()) != 0 && errno != EEXIST)
				return false;
		}
		while (pos != std::string::npos);

		return true;
	}

	bool FileSystem::DeleteDirectorys(const std::string& path)
	{
		if (!IsDirectory(path))
			return false;

		std::string _path(path);
		while (!_path.empty() && (_path.back() == '\\' || _path.back() == '/'))
			_path.pop_back();
		if (_path.length() == 2 && _path[1] == ':')
			return false;

		TCHAR dir[MAX_PATH + 1] = {0};
		strcpy_s(dir, MAX_PATH, _path.c_str());
		strcpy_s(dir, MAX_PATH, "\\*");
		//_tcscpy_s(dir, MAX_PATH, _path.c_str());
		//_tcscat_s(dir, MAX_PATH, "\\*");

		WIN32_FIND_DATA findData;
		HANDLE hFind = ::FindFirstFile(dir, &findData);

		if (hFind == INVALID_HANDLE_VALUE)
			return false;

		do
		{
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
				//if (_tcscmp(findData.cFileName, ".") == 0 || _tcscmp(findData.cFileName, "..") == 0)
				continue;
			TCHAR filePath[MAX_PATH] = {0};
			sprintf_s(filePath, MAX_PATH, "%s\\%s", _path.c_str(), findData.cFileName);

			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!DeleteDirectorys(filePath))
				{
					FindClose(hFind);
					return false;
				}
			}
			else
			{
				if (!FileSystem::DeleteFile(filePath))
				{
					FindClose(hFind);
					return false;
				}
			}
		}
		while (::FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		return ::RemoveDirectory(path.c_str()) != 0;
	}

	std::vector<std::string> FileSystem::GetDirectorys(const std::string& path)
	{
		std::vector<std::string> dirs;
		if (!IsDirectory(path))
			return dirs;

		WIN32_FIND_DATA findData;
		HANDLE hFind = ::FindFirstFile((path + "\\*").c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return dirs;

		do
		{
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strcmp(findData.cFileName, ".") != 0 &&
				strcmp(findData.cFileName, "..") != 0)
				dirs.push_back(findData.cFileName);
		}
		while (::FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		return dirs;
	}

	std::string FileSystem::GetCurrentWorkDirectory()
	{
		char buffer[MAX_PATH];
		::GetCurrentDirectory(MAX_PATH, buffer);
		return buffer;
	}

	bool FileSystem::SetCurrentWorkDirectory(const std::string& path)
	{
		return ::SetCurrentDirectory(path.c_str()) != 0;
	}

	std::string FileSystem::ReadAllText(const std::string& path)
	{
		if (!IsFile(path))
			return "";

		std::ifstream in(path);
		if (!in.is_open())
			return "";

		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		in.close();

		return content;
	}

	bool FileSystem::WriteAllText(const std::string& path, const std::string& text)
	{
		std::ofstream out(path);
		if (!out.is_open())
			return false;

		out << text;
		out.close();
		return true;
	}
#pragma endregion

#pragma region IniManager
	bool ContainsDriveLetter(const std::string& path)
	{
		if (path.length() >= 2 && std::isalpha(path[0]) && path[1] == ':')
			return true; // 例如 "C:"
		return false;
	}

	bool ContainsPathSeparator(const std::string& path)
	{
		if (path.find('\\') != std::string::npos)
			return true;
		if (path.find('/') != std::string::npos)
			return true;
		return false;
	}

	std::string GetExecutablePath()
	{
		char buffer[MAX_PATH];
		::GetModuleFileName(nullptr, buffer, MAX_PATH);

		std::string path(buffer);
		size_t lastPos = path.find_last_of("\\/");
		if (lastPos != std::string::npos)
			path = path.substr(0, lastPos);
		return path;
	}

	std::string IniManager::Convert::ToString(int value)
	{
		char buffer[32];
		sprintf_s(buffer, "%d", value);
		return buffer;
	}

	std::string IniManager::Convert::ToString(bool value)
	{
		return value ? "true" : "false";
	}

	std::string IniManager::Convert::ToString(double value)
	{
		char buffer[64];
		sprintf_s(buffer, "%.15g", value);
		return buffer;
	}

	int IniManager::Convert::ToInt(const std::string& value, int defaultValue)
	{
		if (value.empty()) return defaultValue;

		auto result = 0;
		try
		{
			result = atoi(value.c_str());
		}
		catch (...)
		{
			return defaultValue;
		}
		return result;
	}

	bool IniManager::Convert::ToBool(const std::string& value, bool defaultValue)
	{
		if (value.empty()) return defaultValue;

		std::string lowerValue = value;
		std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), tolower);

		if (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes")
			return true;
		if (lowerValue == "false" || lowerValue == "0" || lowerValue == "no")
			return false;
		return defaultValue;
	}

	double IniManager::Convert::ToDouble(const std::string& value, double defaultValue)
	{
		if (value.empty()) return defaultValue;

		auto result = 0.0;
		try
		{
			result = atof(value.c_str());
		}
		catch (...)
		{
			return defaultValue;
		}
		return result;
	}

	IniManager::IniManager(const std::string& filePath)
	{
		SetLastError("");
		m_filePath = "";

		if (ContainsDriveLetter(filePath) && ContainsPathSeparator(filePath))
			m_filePath = filePath;
		else
		{
			std::string currentPath = GetExecutablePath();
			m_filePath = currentPath + "\\" + filePath;
		}
	}

	bool IniManager::WriteValue(const std::string& section, const std::string& key, const std::string& value)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), m_filePath.c_str());
		if (!result)
			SetLastError("Failed to write value to INI file");
		return result != 0;
	}

	std::string IniManager::ReadValue(const std::string& section, const std::string& key,
	                                  const std::string& defaultValue)
	{
		SetLastError("");

		char buffer[256] = {0};
		DWORD size = ::GetPrivateProfileString(section.c_str(), key.c_str(), defaultValue.c_str(), buffer,
		                                       sizeof(buffer), m_filePath.c_str());
		if (size == 0 && !SectionExists(section))
			SetLastError("Section not found in INI file");
		return buffer;
	}

	bool IniManager::WriteInt(const std::string& section, const std::string& key, int value)
	{
		return WriteValue(section, key, Convert::ToString(value));
	}

	bool IniManager::WriteBool(const std::string& section, const std::string& key, bool value)
	{
		return WriteValue(section, key, Convert::ToString(value));
	}

	bool IniManager::WriteDouble(const std::string& section, const std::string& key, double value)
	{
		return WriteValue(section, key, Convert::ToString(value));
	}

	int IniManager::ReadInt(const std::string& section, const std::string& key, int defaultValue)
	{
		return Convert::ToInt(ReadValue(section, key, ""), defaultValue);
	}

	bool IniManager::ReadBool(const std::string& section, const std::string& key, bool defaultValue)
	{
		return Convert::ToBool(ReadValue(section, key, ""), defaultValue);
	}

	double IniManager::ReadDouble(const std::string& section, const std::string& key, double defaultValue)
	{
		return Convert::ToDouble(ReadValue(section, key, ""), defaultValue);
	}

	std::map<std::string, std::string> IniManager::ReadSection(const std::string& section)
	{
		SetLastError("");

		std::map<std::string, std::string> result;

		if (!SectionExists(section))
		{
			SetLastError("Section not found in INI file");
			return result;
		}

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), m_filePath.c_str());

		if (size == 0) return result;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				std::string key = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				result[key] = value;
			}
			p += strlen(p) + 1;
		}

		return result;
	}

	bool IniManager::WriteSection(const std::string& section, const std::map<std::string, std::string>& keyValues)
	{
		SetLastError("");

		// 先删除现有 section
		if (!DeleteSection(section))
		{
			SetLastError("Failed to clear existing section");
			return false;
		}

		// 写入新的键值对
		for (auto it = keyValues.begin(); it != keyValues.end(); ++it)
		{
			if (!WriteValue(section, it->first, it->second))
			{
				SetLastError("Failed to write key-value pair");
				return false;
			}
		}

		return true;
	}

	bool IniManager::DeleteKey(const std::string& section, const std::string& key)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), nullptr, m_filePath.c_str());
		if (!result)
			SetLastError("Failed to delete key from INI file");
		return result != 0;
	}

	bool IniManager::DeleteSection(const std::string& section)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), nullptr, nullptr, m_filePath.c_str());
		if (!result)
			SetLastError("Failed to delete section from INI file");
		return result != 0;
	}

	bool IniManager::FileExists()
	{
		SetLastError("");

		std::ifstream file(m_filePath);
		bool exist = file.good();
		if (file.is_open()) file.close();
		return exist;
	}

	bool IniManager::BackupFile(const std::string& backupPath)
	{
		SetLastError("");

		std::string filePath(m_filePath);

		std::string actualBackupPath = backupPath;
		if (actualBackupPath.empty())
		{
			size_t dotPos = filePath.rfind('.');
			if (dotPos != std::string::npos)
				actualBackupPath = filePath.substr(0, dotPos) + ".bak" + filePath.substr(dotPos);
			else
				actualBackupPath = filePath + ".bak";
		}

		BOOL result = ::CopyFile(filePath.c_str(), actualBackupPath.c_str(), FALSE); // 覆盖已存在的文件
		if (!result)
			SetLastError("Failed to backup INI file");

		return result != 0;
	}

	bool IniManager::SectionExists(const std::string& section)
	{
		SetLastError("");

		std::vector<std::string> sections = GetSectionNames();
		return std::find(sections.begin(), sections.end(), section) != sections.end();
	}

	bool IniManager::KeyExists(const std::string& section, const std::string& key)
	{
		SetLastError("");

		std::vector<std::string> keys = GetKeyNames(section);
		return std::find(keys.begin(), keys.end(), key) != keys.end();
	}

	std::vector<std::string> IniManager::GetSectionNames()
	{
		SetLastError("");

		std::vector<std::string> sections;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSectionNames(buffer, sizeof(buffer), m_filePath.c_str());

		if (size == 0) return sections;

		char* p = buffer;
		while (*p)
		{
			sections.push_back(std::string(p));
			p += strlen(p) + 1;
		}

		return sections;
	}

	std::vector<std::string> IniManager::GetKeyNames(const std::string& section)
	{
		SetLastError("");

		std::vector<std::string> keys;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), m_filePath.c_str());

		if (size == 0) return keys;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
				keys.push_back(line.substr(0, pos));
			p += strlen(p) + 1;
		}

		return keys;
	}

	std::string IniManager::GetLastError()
	{
		return m_lastError;
	}

	void IniManager::SetLastError(const std::string& error)
	{
		m_lastError = error;
	}
#pragma endregion

#pragma region XmlManager
	XmlManager::XmlManager()
	{
	}

	XmlManager::~XmlManager()
	{
	}

#pragma endregion

#pragma region JsonManager
	JsonManager::JsonManager()
	{
	}

	JsonManager::~JsonManager()
	{
	}

#pragma endregion

#pragma region SQLServerManager

#pragma endregion

#pragma region SqliteManager

	SqliteManager::SqliteManager() noexcept = default;

	SqliteManager::~SqliteManager()
	{
		Close();
	}

	SqliteManager::SqliteManager(const std::string& fileName)
	{
		Open(fileName);
	}

	SqliteManager::SqliteManager(SqliteManager&& other) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(other.m_mutex);
		m_db = other.m_db;
		m_lastErrMsg = std::move(other.m_lastErrMsg);
		m_fileName = std::move(other.m_fileName);
		m_maxStmtCacheCount = other.m_maxStmtCacheCount;
		m_stmtCache = std::move(other.m_stmtCache);

		other.m_db = nullptr;
		other.m_stmtCache.clear();
	}

	SqliteManager& SqliteManager::operator=(SqliteManager&& other) noexcept
	{
		if (this != &other)
		{
			std::lock_guard<std::recursive_mutex> lockSelf(m_mutex);
			std::lock_guard<std::recursive_mutex> lockOther(other.m_mutex);

			Close();
			m_db = other.m_db;
			m_lastErrMsg = std::move(other.m_lastErrMsg);
			m_fileName = std::move(other.m_fileName);
			m_maxStmtCacheCount = other.m_maxStmtCacheCount;
			m_stmtCache = std::move(other.m_stmtCache);

			other.m_db = nullptr;
			other.m_stmtCache.clear();
		}
		return *this;
	}

	bool SqliteManager::Open(const std::string& fileName)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		m_fileName = fileName;
		if (m_db)
		{
			if (CheckConnection())
			{
				m_lastErrMsg = "Database already open";
				return false;
			}
			Close();
		}

		int rc = sqlite3_open_v2(m_fileName.c_str(), &m_db,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			sqlite3_close(m_db);
			m_db = nullptr;
			return false;
		}

		return true;
	}

	void SqliteManager::Close()
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (m_db)
		{
			ClearStmtCache();
			sqlite3_close(m_db);
			m_db = nullptr;
		}
	}

	bool SqliteManager::IsOpen() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		bool connected = CheckConnection();
		return m_db != nullptr && connected == true;
	}

	void SqliteManager::SetMaxStmtCacheCount(size_t count) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		m_maxStmtCacheCount = count;
		// 超过新上限则清理缓存
		while (m_stmtCache.size() > m_maxStmtCacheCount && !m_stmtCache.empty())
		{
			auto it = m_stmtCache.begin();
			sqlite3_finalize(it->second);
			m_stmtCache.erase(it);
		}
	}

	bool SqliteManager::ExecuteNonQuery(const std::string& sql, const ParamsList& params)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db)
		{
			m_lastErrMsg = "Database not open";
			return false;
		}

		// 获取缓存语句
		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool isCached = (stmt != nullptr);
		auto ret = false;

		if (!isCached)
		{
			// 预处理新语句
			int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			int rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE)
			{
				ret = true;
				m_lastErrMsg.clear();
				if (!isCached)
				{
					AddStmtCache(sql, stmt);
					stmt = nullptr; // 避免后续释放缓存语句
				}
			}
			else
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				// 缓存语句执行失败，移除缓存
				if (isCached)
				{
					sqlite3_finalize(stmt);
					stmt = nullptr;
					m_stmtCache.erase(sql);
				}
			}
		}

		if (!isCached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
			stmt = nullptr;
		}

		return ret;
	}

	bool SqliteManager::ExecuteBatchNonQuery(const std::string& sql, const BatchParamsList& paramsList)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db || paramsList.empty())
		{
			m_lastErrMsg = "Database not open or empty batch params";
			return false;
		}

		if (!BeginTransaction())
			return false;

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool isCached = (stmt != nullptr);
		auto allOk = true;

		if (!isCached)
		{
			int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				RollbackTransaction();
				return false;
			}
		}

		for (const auto& params : paramsList)
		{
			sqlite3_reset(stmt);
			if (!BindParams(stmt, params))
			{
				allOk = false;
				break;
			}

			int rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				allOk = false;
				break;
			}
		}

		if (allOk)
		{
			CommitTransaction();
			if (!isCached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}
		else
		{
			RollbackTransaction();
			if (isCached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				m_stmtCache.erase(sql);
			}
		}

		if (!isCached && stmt != nullptr)
			sqlite3_finalize(stmt);

		return allOk;
	}

	bool SqliteManager::ExecuteQuery(const std::string& sql, const ParamsList& params, UMapList& result)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db)
		{
			m_lastErrMsg = "Database not open";
			return false;
		}

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool isCached = (stmt != nullptr);
		auto ret = false;

		if (!isCached)
		{
			int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			ret = ExecutePreparedQuery(stmt, result);
			if (!ret && isCached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				m_stmtCache.erase(sql); // 缓存语句执行失败，移除
			}

			if (ret && !isCached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}

		if (!isCached && stmt != nullptr)
			sqlite3_finalize(stmt);

		return ret;
	}

	bool SqliteManager::ExecuteQueryPage(const std::string& sql, const ParamsList& params, int currentPage,
	                                     int pageSize, UMapList& data, int& totalCount, int& totalPages)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db || currentPage < 1 || pageSize < 1)
		{
			m_lastErrMsg = "Invalid pagination parameters";
			return false;
		}

		data.clear();
		totalCount = 0;
		totalPages = 0;

		// 查询总记录数
		std::string countSql = "SELECT COUNT(*) AS total FROM (" + sql + ") AS t_count;";
		UMapList countResult;
		if (!ExecuteQuery(countSql, params, countResult))
			return false;

		if (!countResult.empty())
			totalCount = std::stoi(countResult[0]["total"]);

		// 计算分页参数
		totalPages = (totalCount + pageSize - 1) / pageSize;
		int offset = (currentPage - 1) * pageSize;

		// 查询当前页
		std::string pageSql = sql + " LIMIT ? OFFSET ?;";
		ParamsList allParams = params;
		allParams.emplace_back(pageSize);
		allParams.emplace_back(offset);

		return ExecuteQuery(pageSql, allParams, data);
	}

	bool SqliteManager::ReadBlobChunk(const std::string& tableName, const std::string& colName, int64_t rowId,
	                                  size_t offset, size_t chunkSize, std::vector<char>& chunkData)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db)
		{
			m_lastErrMsg = "Database not open";
			return false;
		}

		chunkData.clear();
		sqlite3_blob* pBlob = nullptr;

		int rc = sqlite3_blob_open(m_db, "main", tableName.c_str(), colName.c_str(), rowId, 0, &pBlob);
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			return false;
		}

		int blobTotalLen = sqlite3_blob_bytes(pBlob);
		if (offset >= static_cast<size_t>(blobTotalLen))
		{
			m_lastErrMsg = "Offset exceeds blob length";
			sqlite3_blob_close(pBlob);
			pBlob = nullptr;
			return true; // 偏移超限，返回空数据
		}

		// 计算实际读取长度
		size_t readLen = min(chunkSize, static_cast<size_t>(blobTotalLen) - offset);
		chunkData.resize(readLen);

		// 分块读取
		rc = sqlite3_blob_read(pBlob, chunkData.data(), static_cast<int>(readLen), static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			sqlite3_blob_close(pBlob);
			pBlob = nullptr;
			return false;
		}

		sqlite3_blob_close(pBlob);
		pBlob = nullptr;
		return true;
	}

	bool SqliteManager::WriteBlobChunk(const std::string& tableName, const std::string& colName, int64_t rowId,
	                                   size_t offset, const std::vector<char>& chunkData)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (!m_db || chunkData.empty())
		{
			m_lastErrMsg = "DB not open or empty chunk data";
			return false;
		}

		sqlite3_blob* pBlob = nullptr;

		int rc = sqlite3_blob_open(m_db, "main", tableName.c_str(), colName.c_str(), rowId, 1, &pBlob);
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			return false;
		}

		// 检查偏移
		int blobTotalLen = sqlite3_blob_bytes(pBlob);
		if (offset > static_cast<size_t>(blobTotalLen))
		{
			m_lastErrMsg = "Offset exceeds BLOB length (write)";
			sqlite3_blob_close(pBlob);
			pBlob = nullptr;
			return false;
		}

		// 分块写入
		rc = sqlite3_blob_write(pBlob, chunkData.data(), static_cast<int>(chunkData.size()), static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			sqlite3_blob_close(pBlob);
			pBlob = nullptr;
			return false;
		}

		sqlite3_blob_close(pBlob);
		pBlob = nullptr;
		return true;
	}

	bool SqliteManager::BeginTransaction()
	{
		return ExecuteNonQuery("BEGIN TRANSACTION;");
	}

	bool SqliteManager::CommitTransaction()
	{
		return ExecuteNonQuery("COMMIT;");
	}

	bool SqliteManager::RollbackTransaction()
	{
		return ExecuteNonQuery("ROLLBACK;");
	}


	int64_t SqliteManager::GetLastInsertId() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
	}

	std::string SqliteManager::GetLastErrorMsg() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		return m_lastErrMsg;
	}

	bool SqliteManager::CheckConnection() noexcept
	{
		m_lastErrMsg.clear();
		if (!m_db)
		{
			m_lastErrMsg = "Database is nullptr";
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, "SELECT 1;", -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			if (stmt) sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		sqlite3_finalize(stmt); // 手动释放
		return true;
	}

	bool SqliteManager::TryReconnect() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_lastErrMsg.clear();
		if (m_fileName.empty())
		{
			m_lastErrMsg = "No database path for reconnect";
			return false;
		}

		if (m_db)
			Close();

		// 重新打开
		int rc = sqlite3_open_v2(m_fileName.c_str(), &m_db,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			sqlite3_close(m_db);
			m_db = nullptr;
			return false;
		}

		// 重连后清空缓存
		ClearStmtCache();
		return true;
	}

	sqlite3_stmt* SqliteManager::GetStmtCache(const std::string& sql)
	{
		m_lastErrMsg.clear();
		auto it = m_stmtCache.find(sql);
		if (it != m_stmtCache.end() && it->second != nullptr)
		{
			sqlite3_reset(it->second); // 重置语句可复用
			return it->second;
		}
		return nullptr;
	}

	void SqliteManager::AddStmtCache(const std::string& sql, sqlite3_stmt* stmt)
	{
		m_lastErrMsg.clear();
		if (!stmt)
		{
			m_lastErrMsg = "Database is nullptr";
			return;
		}

		// 超过最大缓存数，移除最早的
		while (m_stmtCache.size() >= m_maxStmtCacheCount && !m_stmtCache.empty())
		{
			auto it = m_stmtCache.begin();
			sqlite3_finalize(it->second);
			m_stmtCache.erase(it);
		}

		m_stmtCache[sql] = stmt;
	}

	void SqliteManager::ClearStmtCache()
	{
		m_stmtCache.clear();
		for (auto& pair : m_stmtCache)
			sqlite3_finalize(pair.second);
	}

	bool SqliteManager::BindParams(sqlite3_stmt* stmt, const ParamsList& params)
	{
		m_lastErrMsg.clear();
		auto idx = 1;
		for (const auto& param : params)
		{
			auto rc = SQLITE_ERROR;
			switch (param.type)
			{
			case ParamType::Null:
				rc = sqlite3_bind_null(stmt, idx);
				break;
			case ParamType::Bool:
				rc = sqlite3_bind_int(stmt, idx, param.intVal);
				break;
			case ParamType::Int:
				rc = sqlite3_bind_int(stmt, idx, param.intVal);
				break;
			case ParamType::UInt:
				rc = sqlite3_bind_int64(stmt, idx, param.int64Val);
				break;
			case ParamType::Int64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64Val);
				break;
			case ParamType::UInt64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64Val);
				break;
			case ParamType::String:
				rc = sqlite3_bind_text(stmt, idx, param.strVal.c_str(), static_cast<int>(param.strVal.size()),
				                       SQLITE_TRANSIENT);
				break;
			case ParamType::Double:
				rc = sqlite3_bind_double(stmt, idx, param.doubleVal);
				break;
			case ParamType::Blob:
				if (param.blobVal.empty())
				{
					m_lastErrMsg = "Empty blob data";
					return false;
				}
				rc = sqlite3_bind_blob(stmt, idx, param.blobVal.data(), static_cast<int>(param.blobVal.size()),
				                       SQLITE_TRANSIENT);
				break;
			}

			if (rc != SQLITE_OK)
			{
				m_lastErrMsg = sqlite3_errmsg(m_db);
				return false;
			}
			idx++;
		}
		return true;
	}

	bool SqliteManager::ExecutePreparedQuery(sqlite3_stmt* stmt, UMapList& result)
	{
		m_lastErrMsg.clear();
		result.clear();
		int colCount = sqlite3_column_count(stmt);
		if (colCount == 0)
			return true;

		// 获取列名
		std::vector<std::string> colNames;
		for (auto i = 0; i < colCount; ++i)
			colNames.emplace_back(sqlite3_column_name(stmt, i));

		// 遍历结果行
		int rc;
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			std::unordered_map<std::string, std::string> row;
			for (auto i = 0; i < colCount; ++i)
			{
				if (sqlite3_column_type(stmt, i) == SQLITE_BLOB)
				{
					auto blobData = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
					int blobLen = sqlite3_column_bytes(stmt, i);
					std::string hexStr;
					for (auto j = 0; j < blobLen; ++j)
					{
						char buf[3];
						snprintf(buf, sizeof(buf), "%02X", blobData[j]);
						hexStr += buf;
					}
					row[colNames[i]] = hexStr;
				}
				else
				{
					auto val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
					row[colNames[i]] = val ? val : "";
				}
			}
			result.push_back(std::move(row));
		}

		if (rc != SQLITE_DONE)
		{
			m_lastErrMsg = sqlite3_errmsg(m_db);
			return false;
		}
		return true;
	}

#pragma endregion

#pragma region ThreadPool
	// 放在头文件实现
#pragma endregion

#pragma region CustomSettings

	class CSImpl
	{
	public:
		Json::Value m_jsonRoot; // JSON数据根节点
		std::string m_basePath; // 配置文件根目录
		std::string m_lastError; // 最后错误信息
		mutable std::mutex m_cs; // 临界区对象
		mutable std::mutex m_csLoadFile; // 临界区对象（仅读文件时用）
		mutable std::mutex m_csSaveFile; // 临界区对象（仅写文件时用）

		CSImpl() : m_basePath("d:/param/custom_settings/")
		{
			m_lastError = "";

			std::string path(m_basePath);
			std::replace(path.begin(), path.end(), '/', '\\');
			//SHCreateDirectoryEx(NULL, path.c_str(), NULL); // 确保路径存在，WinAPI必须用'\\'
			CreateDirectory(path.c_str(), nullptr); // 确保路径存在，WinAPI必须用'\\'


			//InitializeCriticalSection(&m_cs);
			//InitializeCriticalSection(&m_csLoadFile);
			//InitializeCriticalSection(&m_csSaveFile);
			LoadJsonFiles();
		}

		~CSImpl()
		{
			SaveJsonFiles();
			//DeleteCriticalSection(&m_cs);
			//DeleteCriticalSection(&m_csLoadFile);
			//DeleteCriticalSection(&m_csSaveFile);
		}

		static std::string U2G(const std::string& utf8)
		{
			int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
			if (wideLen <= 0)
				return "";

			std::wstring wstr;
			wstr.resize(wideLen);
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wstr[0], wideLen);

			int ansiLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0,
			                                  nullptr, nullptr);
			if (ansiLen <= 0)
				return "";

			std::string ansiStr;
			ansiStr.resize(ansiLen);
			WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), static_cast<int>(wstr.size()), &ansiStr[0], ansiLen, nullptr,
			                    nullptr);

			return ansiStr;
		}

		static std::string G2U(const std::string& gbk)
		{
			if (gbk.empty())
				return "";

			int wideLen = MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), static_cast<int>(gbk.size()), nullptr, 0);
			if (wideLen <= 0)
				return "";

			std::wstring wstr;
			wstr.resize(wideLen);
			MultiByteToWideChar(CP_ACP, 0, gbk.c_str(), static_cast<int>(gbk.size()), &wstr[0], wideLen);

			int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0,
			                                  nullptr, nullptr);
			if (utf8Len <= 0)
				return "";

			std::string utf8Str;
			utf8Str.resize(utf8Len);
			WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &utf8Str[0], utf8Len, nullptr,
			                    nullptr);

			return utf8Str;
		}

		static std::string EnsureTrailingSlash(const std::string& path)
		{
			if (!path.empty() && (path[path.size() - 1] != '\\' && path[path.size() - 1] != '/'))
				return path + "/";
			return path;
		}

		static CSType JudgeStringType(const std::string& str)
		{
			std::regex int_pattern("^[+-]?\\d+$");
			std::regex float_pattern("^[+-]?(\\d+\\.?\\d*|\\.?\\d+)$");

			if (std::regex_match(str, int_pattern))
				return CSType::Int;
			if (std::regex_match(str, float_pattern))
				return CSType::Double;
			if (!str.empty())
				return CSType::String;
			return CSType::Null;
		}

		void EnsureNodeExists(const std::string& fileName, const std::string& section, const std::string& key)
		{
			if (fileName.empty() || section.empty() || key.empty()) return; // 不能创建空名称""的节点
			if (!m_jsonRoot.isMember(fileName)) m_jsonRoot[fileName] = Json::Value(Json::objectValue);
			if (!m_jsonRoot[fileName].isMember(section)) m_jsonRoot[fileName][section] = Json::Value(Json::objectValue);
			if (!m_jsonRoot[fileName][section].isMember(key))
			{
				m_jsonRoot[fileName][section][key] = Json::Value(
					Json::objectValue);
			}
			if (!m_jsonRoot[fileName][section][key].isMember("value"))
			{
				m_jsonRoot[fileName][section][key]["value"] =
					Json::Value(Json::nullValue);
			}
			if (!m_jsonRoot[fileName][section][key].isMember("description"))
			{
				m_jsonRoot[fileName][section][key][
					"description"] = "";
			}
		}

		bool LoadJsonFile(const std::string& fileName)
		{
			std::lock_guard<std::mutex> locker(m_csLoadFile);

			m_lastError = "";
			std::string filePath = m_basePath + fileName;
			std::ifstream ifs(filePath, std::ios::in | std::ios::binary);

			if (!ifs.is_open())
			{
				m_lastError = "打开文件失败: " + filePath;
				return false;
			}

			Json::Reader reader;
			Json::Value jsonData;
			if (!reader.parse(ifs, jsonData))
			{
				m_lastError = "JSON解析失败: " + reader.getFormattedErrorMessages();
				ifs.close();
				return false;
			}

			m_jsonRoot[fileName] = jsonData;
			ifs.close();
			m_lastError = "加载文件成功: " + fileName;
			return true;
		}

		bool SaveJsonFile(const std::string& fileName, const bool& isNewFile = false)
		{
			std::lock_guard<std::mutex> locker(m_csSaveFile);

			m_lastError = "";
			std::string filePath = m_basePath + fileName;

			if (isNewFile)
			{
				DWORD attrib = GetFileAttributesA(filePath.c_str());
				bool exist = (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
				if (!exist && !m_jsonRoot.isMember(fileName))
				{
					std::ofstream ofs(filePath, std::ios::out | std::ios::binary);
					if (!ofs.is_open())
					{
						m_lastError = "创建新文件失败: " + filePath;
						return false;
					}

					Json::Value newObj(Json::objectValue);
					Json::StreamWriterBuilder writerBuilder;
					writerBuilder["enable_escaping_for_non_ascii"] = false; //核心：关闭中文转义
					writerBuilder["emitUTF8"] = true;

					//Json::StyledWriter writer;
					//ofs << writer.write(newObj);
					//ofs.close();
					std::string strJson = Json::writeString(writerBuilder, newObj);
					ofs << strJson;
					ofs.close();

					m_jsonRoot[fileName] = newObj;

					m_lastError = "创建新文件成功: " + fileName;
				}
				else
				{
					m_lastError = "文件已存在: " + fileName;
					return false;
				}
			}
			else
			{
				if (m_jsonRoot.isMember(fileName))
				{
					std::ofstream ofs(filePath);
					if (!ofs.is_open())
					{
						m_lastError = "保存文件失败: " + filePath;
						return false;
					}

					Json::StreamWriterBuilder writerBuilder;
					writerBuilder["enable_escaping_for_non_ascii"] = false; //核心：关闭中文转义
					writerBuilder["emitUTF8"] = true;

					//Json::StyledWriter writer;
					//ofs << writer.write(m_jsonRoot[fileName]);
					//ofs.close();
					std::string strJson = Json::writeString(writerBuilder, m_jsonRoot[fileName]);
					ofs << strJson;
					ofs.close();

					m_lastError = "保存文件成功: " + fileName;
				}
				else
				{
					m_lastError = "文件不存在: " + fileName;
					return false;
				}
			}

			return true;
		}

		bool DeleteJsonFile(const std::string& fileName)
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			std::string filePath = m_basePath + fileName;
			if (std::remove(filePath.c_str()) != 0)
			{
				m_lastError = "删除文件失败: " + filePath;
				return false;
			}
			m_jsonRoot.removeMember(fileName);
			m_lastError = "删除文件成功: " + fileName;
			return true;
		}

		bool RenameJsonFile(const std::string& oldFileName, const std::string& newFileName)
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			std::string oldPath = m_basePath + oldFileName;
			std::string newPath = m_basePath + newFileName;

			if (std::rename(oldPath.c_str(), newPath.c_str()) != 0)
			{
				m_lastError = "重命名文件失败: " + oldFileName + " -> " + newFileName;
				return false;
			}

			// 更新内存中的键名
			if (m_jsonRoot.isMember(oldFileName))
			{
				m_jsonRoot[newFileName] = m_jsonRoot[oldFileName];
				m_jsonRoot.removeMember(oldFileName);
			}
			m_lastError = "重命名文件成功: " + oldFileName + " -> " + newFileName;
			return true;
		}

		bool LoadJsonFiles()
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			WIN32_FIND_DATAA findData;
			std::string searchPath = m_basePath + "*.json";
			HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

			if (hFind == INVALID_HANDLE_VALUE)
			{
				m_lastError = "未找到JSON文件: " + searchPath;
				return false;
			}

			do
			{
				if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::string fileName = findData.cFileName;
					LoadJsonFile(fileName);
				}
			}
			while (FindNextFileA(hFind, &findData) != 0);

			DWORD lastError = GetLastError();
			if (lastError != ERROR_NO_MORE_FILES)
				m_lastError = "遍历文件失败";
			else
				m_lastError = "加载所有JSON文件完成";
			FindClose(hFind);
			return true;
		}

		bool SaveJsonFiles()
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			auto failCount = 0;
			Json::Value::Members members = m_jsonRoot.getMemberNames();
			for (size_t i = 0; i < members.size(); ++i)
			{
				std::string member = members[i];
				size_t pos = member.rfind('.');
				if (pos == std::string::npos)
					continue;

				std::string suffix = member.substr(pos);
				for (size_t i = 0; i < suffix.size(); i++)
					suffix[i] = tolower(suffix[i]);
				if (suffix.compare(".json") != 0)
					continue;

				if (!SaveJsonFile(member))
					failCount++;
			}

			if (failCount == 0)
			{
				m_lastError = "所有文件保存成功";
				return true;
			}
			m_lastError = "所有文件保存失败";
			return false;
		}

		bool JsonToVector(const std::string& fileName, std::vector<CustomSettings::Members>& currentFileData)
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			currentFileData.clear();

			if (fileName.empty())
			{
				m_lastError = "文件名为空";
				return false;
			}

			if (!m_jsonRoot.isObject())
			{
				m_lastError = "JSON根节点不是对象";
				return false;
			}

			const Json::Value& fileData = m_jsonRoot[fileName];
			if (!fileData.isObject())
			{
				m_lastError = "文件数据不是对象: " + fileName;
				return false;
			}

			Json::Value::Members sections = fileData.getMemberNames();
			for (size_t i = 0; i < sections.size(); ++i)
			{
				std::string section = sections[i];
				const Json::Value& secObj = fileData[section];
				if (!secObj.isObject()) continue;

				Json::Value::Members keys = secObj.getMemberNames();
				for (size_t j = 0; j < keys.size(); ++j)
				{
					std::string key = keys[j];
					const Json::Value& keyObj = secObj[key];
					if (!keyObj.isObject()) continue;

					CustomSettings::Members item;
					item.fileName = U2G(fileName);
					item.section = U2G(section);
					item.key = U2G(key);
					item.description = keyObj.isMember("description") ? U2G(keyObj["description"].asString()) : U2G("");

					const Json::Value& val = keyObj["value"];

					CSType type = JudgeStringType(val.asString());

					if (val.isDouble() && CSType::Double == type) // 浮点数默认保留3位小数
					{
						std::ostringstream ss;
						ss.precision(3);
						ss << std::fixed << val.asDouble();
						item.value = U2G(ss.str());
					}
					else
						item.value = U2G(val.asString());

					currentFileData.push_back(item);
				}
			}

			m_lastError = "JSON转Vector成功: " + fileName;
			return true;
		}

		bool VectorToJson(const std::string& fileName, const std::vector<CustomSettings::Members>& currentFileData)
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			m_jsonRoot.removeMember(fileName);
			m_jsonRoot[fileName] = Json::Value(Json::objectValue);
			for (size_t i = 0; i < currentFileData.size(); ++i)
			{
				const CustomSettings::Members& member = currentFileData[i];

				CustomSettings::Members item;
				item.fileName = G2U(member.fileName);
				item.section = G2U(member.section);
				item.key = G2U(member.key);
				item.description = G2U(member.description);
				item.value = G2U(member.value);

				Json::Value& keyObj = m_jsonRoot[item.fileName][item.section][item.key];

				keyObj["description"] = (item.description);

				CSType type = JudgeStringType(item.value);

				if (CSType::Int == type)
					keyObj["value"] = atoi(item.value.c_str());
				else if (CSType::Double == type)
					keyObj["value"] = atof(item.value.c_str());
				else if (CSType::String == type)
					keyObj["value"] = item.value;
				else
					keyObj["value"] = Json::nullValue;
			}

			m_lastError = "Vector转JSON成功: " + fileName;
			return true;
		}

		bool RemoveJsonObject(const std::string& objectPath)
		{
			std::vector<std::string> objects = StringUtils::Split(objectPath, '/', 3);
			if (objects.size() < 3)
				return false;

			CustomSettings::Members member;
			member.fileName = objects.at(0);
			member.section = objects.at(1);
			member.key = objects.at(2);

			if (member.key.length() > 0)
				m_jsonRoot[member.fileName][member.section].removeMember(member.key);
			else if (member.section.length() > 0)
				m_jsonRoot[member.fileName].removeMember(member.section);
			else if (member.fileName.length() > 0)
				m_jsonRoot.removeMember(member.fileName);
			return true;
		}

		std::vector<std::string> GetJsonFileList()
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			return m_jsonRoot.getMemberNames();
		}

		std::string JsonToString()
		{
			std::lock_guard<std::mutex> locker(m_cs);

			m_lastError = "";
			return m_jsonRoot.toStyledString();
		}

		std::string GetLastErrorMsg() const
		{
			std::lock_guard<std::mutex> locker(const_cast<std::mutex&>(m_cs)); // 常量对象加锁
			return m_lastError;
		}
	};


	// -------------------------- CSKey 实现 --------------------------

	CSKey::CSKey(CSImpl* impl, const std::string& fileName, const std::string& section, const std::string& key)
		: m_impl(impl),
		  m_fileName(StringUtils::ToLower(fileName)),
		  m_section(StringUtils::ToLower(section)),
		  m_key(StringUtils::ToLower(key))
	{
		m_impl->EnsureNodeExists(m_fileName, m_section, m_key);
	}

	CSKey::CSKey(CSImpl* impl, const std::string& fileName, const std::string& section)
		: m_impl(impl),
		  m_fileName(StringUtils::ToLower(fileName)),
		  m_section(StringUtils::ToLower(section)),
		  m_key("")
	{
	}

	CSKey CSKey::operator[](const std::string& key)
	{
		m_key = StringUtils::ToLower(key);
		m_impl->EnsureNodeExists(m_fileName, m_section, m_key); //不能移除，当key非空时自动创建节点
		return *this;
	}

	CSKey& CSKey::SetInt(const int& value)
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = value;
		return *this;
	}

	CSKey& CSKey::SetDouble(const double& value)
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = value;
		return *this;
	}

	CSKey& CSKey::SetString(const std::string& value)
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = CSImpl::G2U(value);
		return *this;
	}

	CSKey& CSKey::SetDescription(const std::string& value)
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		//if (m_impl->m_jsonRoot.isMember(m_fileName)
		//	&& m_impl->m_jsonRoot[m_fileName].isMember(m_section)
		//	&& m_impl->m_jsonRoot[m_fileName][m_section].isMember(m_key)
		//	&& m_impl->m_jsonRoot[m_fileName][m_section][m_key].isMember("value")
		//	&& m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue
		//	)
		{
			m_impl->m_jsonRoot[m_fileName][m_section][m_key]["description"] = CSImpl::G2U(value);
		}
		return *this;
	}

	int CSKey::GetInt(const int& defaultValue) const
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		int value = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("value", Json::Value::maxInt).asInt();
		if (m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue && value !=
			Json::Value::maxInt)
			return value;
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = defaultValue;
		return defaultValue;
	}

	double CSKey::GetDouble(const double& defaultValue) const
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		double value = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("value", Json::Value::maxUInt64AsDouble).
		                                                                asDouble();
		if (m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue && value !=
			Json::Value::maxUInt64AsDouble)
			return value;
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = defaultValue;
		return defaultValue;
	}

	std::string CSKey::GetString(const std::string& defaultValue) const
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		std::string value = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("value", "").asString();
		if (m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue && !value.empty())
			return CSImpl::U2G(value);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = CSImpl::G2U(defaultValue);
		return defaultValue;
	}

	std::string CSKey::GetDescription(const std::string& defaultValue) const
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		//if (m_impl->m_jsonRoot.isMember(m_fileName)
		//	&& m_impl->m_jsonRoot[m_fileName].isMember(m_section)
		//	&& m_impl->m_jsonRoot[m_fileName][m_section].isMember(m_key)
		//	&& m_impl->m_jsonRoot[m_fileName][m_section][m_key].isMember("value")
		//	&& m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue
		//	)
		{
			std::string value = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("description", "").asString();
			if (m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type() != Json::nullValue && !value.empty())
				return CSImpl::U2G(value);
			m_impl->m_jsonRoot[m_fileName][m_section][m_key]["description"] = CSImpl::G2U(defaultValue);
		}
		return defaultValue;
	}

	CSType CSKey::GetType() const
	{
		std::lock_guard<std::mutex> locker(m_impl->m_cs);
		if (m_impl->m_jsonRoot.isMember(m_fileName)
			&& m_impl->m_jsonRoot[m_fileName].isMember(m_section)
			&& m_impl->m_jsonRoot[m_fileName][m_section].isMember(m_key)
			&& m_impl->m_jsonRoot[m_fileName][m_section][m_key].isMember("value")
		)
		{
			Json::ValueType type = m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"].type();
			switch (type)
			{
			case Json::nullValue:
				return CSType::Null;
			case Json::booleanValue:
			case Json::intValue:
			case Json::uintValue:
				return CSType::Int;
			case Json::realValue:
				return CSType::Double;
			case Json::stringValue:
				return CSType::String;
			}
		}
		return CSType::Null;
	}

	// -------------------------- CSSection 实现 --------------------------

	CSSection::CSSection(CSImpl* impl, const std::string& fileName)
		: m_impl(impl),
		  m_fileName(StringUtils::ToLower(fileName))
	{
	}

	CSKey CSSection::operator[](const std::string& section)
	{
		return CSKey(m_impl, m_fileName, StringUtils::ToLower(section));
	}

	// -------------------------- CustomSettings 实现 --------------------------

	CustomSettings& CustomSettings::GetInstance()
	{
		static CustomSettings instance;
		return instance;
	}

	CSSection CustomSettings::operator[](const std::string& fileName)
	{
		return CSSection(m_impl,
		                 StringUtils::ToLower(fileName)
		);
	}

	CSKey CustomSettings::Access(const std::string& fileName, const std::string& section, const std::string& key)
	{
		return CSKey(m_impl,
		             StringUtils::ToLower(fileName),
		             StringUtils::ToLower(section),
		             StringUtils::ToLower(key)
		);
	}

	std::vector<std::string> CustomSettings::GetJsonFileList()
	{
		return m_impl->GetJsonFileList();
	}

	std::string CustomSettings::JsonToString() const
	{
		return m_impl->JsonToString();
	}

	std::string CustomSettings::GetLastErrorMsg() const
	{
		return m_impl->GetLastErrorMsg();
	}

	bool CustomSettings::LoadJsonFile(const std::string& fileName)
	{
		return m_impl->LoadJsonFile(fileName);
	}

	bool CustomSettings::SaveJsonFile(const std::string& fileName, const bool& isNewFile)
	{
		return m_impl->SaveJsonFile(fileName, isNewFile);
	}

	bool CustomSettings::DeleteJsonFile(const std::string& fileName)
	{
		return m_impl->DeleteJsonFile(fileName);
	}

	bool CustomSettings::RenameJsonFile(const std::string& oldFileName, const std::string& newFileName)
	{
		return m_impl->RenameJsonFile(oldFileName, newFileName);
	}

	bool CustomSettings::LoadJsonFiles()
	{
		return m_impl->LoadJsonFiles();
	}

	bool CustomSettings::SaveJsonFiles()
	{
		return m_impl->SaveJsonFiles();
	}

	bool CustomSettings::JsonToVector(const std::string& fileName, std::vector<Members>& currentFileData)
	{
		return m_impl->JsonToVector(fileName, currentFileData);
	}

	bool CustomSettings::VectorToJson(const std::string& fileName, const std::vector<Members>& currentFileData)
	{
		return m_impl->VectorToJson(fileName, currentFileData);
	}

	bool CustomSettings::RemoveJsonObject(const std::string& objectPath)
	{
		return m_impl->RemoveJsonObject(objectPath);
	}

	CustomSettings::CustomSettings() : m_impl(new CSImpl())
	{
		//XTRACE("调用 CustomSettings::CustomSettings()\n");
	}

	CustomSettings::~CustomSettings()
	{
		//XTRACE("调用 CustomSettings::~CustomSettings()\n");

		if (m_impl)
		{
			delete m_impl;
			m_impl = nullptr;
		}
	}

#pragma endregion

#pragma region LoggerManager

	class Logger::CustomSink : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit CustomSink(const std::function<void(const MetaMsg&)>& callback)
			: m_callback(callback)
		{
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			if (m_callback)
			{
				MetaMsg metaMsg;
				metaMsg.level = static_cast<LogLevel>(msg.level);
				metaMsg.file = msg.source.filename;
				metaMsg.line = msg.source.line;
				metaMsg.message = fmt::to_string(msg.payload);
				metaMsg.time = msg.time;
				m_callback(metaMsg);
			}
		}

		void flush_() override
		{
			// 自定义sink无需flush
		}

	private:
		std::function<void(const MetaMsg&)> m_callback; // 回调函数
	};


	Logger::Config::Config(const LogName& name) : logName(name)
	{
	}

	Logger::Logger() : m_config(LogName::DEFAULT), m_initialized(false)
	{
	}

	Logger::~Logger()
	{
		Shutdown();
	}

	Logger::Logger(const LogName& name) : m_config(name), m_initialized(false)
	{
	}

	Logger::Logger(const Config& config) : m_config(config), m_initialized(false)
	{
	}

	bool Logger::Initialize()
	{
		try
		{
			spdlog::set_level(static_cast<spdlog::level::level_enum>(m_config.level));

			static std::once_flag initFlag;
			std::call_once(initFlag, []() { spdlog::init_thread_pool(8192, 1); }); // 确保线程池只初始化一次（异步日志依赖此线程池）

			m_initialized = true;
			return true;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << ex.what() << std::endl;
			m_initialized = false;
			return false;
		}
	}

	bool Logger::IsInitialized() const
	{
		return m_initialized;
	}

	void Logger::Flush()
	{
		if (m_config.outputs != LogOutput::None && HasValidOutput(m_config.outputs))
		{
			auto logger = GetCachedLogger(m_config.outputs);
			if (logger)
				logger->flush();
		}
	}

	void Logger::Shutdown()
	{
		m_initialized = false;
		// 注意：不在这里清理spdlog的全局状态，因为可能有其他日志器在使用
	}

	Logger::Config& Logger::GetConfig()
	{
		return m_config;
	}

	void Logger::SetOutputCallback(LogOutput type, const std::function<void(const MetaMsg&)>& callback)
	{
		m_customCallbacks[type] = callback;
	}

	void Logger::Trace(const char* format, ...)
	{
		if (LogLevel::Trace < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Trace, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Debug(const char* format, ...)
	{
		if (LogLevel::Debug < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Debug, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Info(const char* format, ...)
	{
		if (LogLevel::Info < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Info, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Warn(const char* format, ...)
	{
		if (LogLevel::Warn < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Warn, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Error(const char* format, ...)
	{
		if (LogLevel::Error < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Error, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Critical(const char* format, ...)
	{
		if (LogLevel::Critical < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Critical, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::LogRecord(LogLevel level, const char* format, ...)
	{
		if (level < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(level, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Trace(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Trace < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Trace, file, line, function, buffer.get());
	}

	void Logger::Debug(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Debug < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Debug, file, line, function, buffer.get());
	}

	void Logger::Info(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Info < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Info, file, line, function, buffer.get());
	}

	void Logger::Warn(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Warn < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Warn, file, line, function, buffer.get());
	}

	void Logger::Error(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Error < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Error, file, line, function, buffer.get());
	}

	void Logger::Critical(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Critical < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(LogLevel::Critical, file, line, function, buffer.get());
	}

	void Logger::LogRecord(const char* file, const int line, const char* function, LogLevel level, const char* format,
	                       ...)
	{
		if (level < m_config.level)
			return;

		va_list args;
		va_start(args, format);
		int length = vsnprintf(nullptr, 0, format, args);
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		Log(level, file, line, function, buffer.get());
	}

	std::shared_ptr<spdlog::logger> Logger::GetCachedLogger(LogOutput outputs)
	{
		std::lock_guard<std::mutex> lock(m_cacheMutex);

		// 定期清理过期缓存（每小时一次）
		constexpr auto cleanupInterval = std::chrono::hours(1);
		auto now = std::chrono::steady_clock::now();
		if (now - m_lastCleanupTime > cleanupInterval)
		{
			m_loggerCache.clear();
			m_lastCleanupTime = now;
		}

		if (m_loggerCache.count(outputs))
			return m_loggerCache[outputs];

		// 缓存不存在，创建新日志器
		auto logger = CreateTempLogger(outputs);
		if (logger)
			m_loggerCache[outputs] = logger;
		return logger;
	}

	std::shared_ptr<spdlog::logger> Logger::CreateTempLogger(LogOutput outputs)
	{
		std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;

		if (static_cast<int>(outputs & LogOutput::Console))
		{
			auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(consoleSink);
		}

		if (static_cast<int>(outputs & LogOutput::File))
		{
			std::shared_ptr<spdlog::sinks::sink> fileSink = nullptr;

			do
			{
				if (m_config.filePath.empty() || m_config.fileName.empty())
					break;

				std::string date = TimePoint::Now().ToString("%Y-%m-%d");

				std::string fullPath = m_config.filePath;
				if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\')
					fullPath += '/';

				fullPath += date; // d:/log/yyyy-mm-dd

				FileSystem::CreateDirectorys(fullPath);

				if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\')
					fullPath += '/';

				//fullPath += date + "_" + m_config.fileName; // d:/log/yyyy-mm-dd/yyyy-mm-dd_xxx.ini
				fullPath += m_config.fileName; // d:/log/yyyy-mm-dd/xxx.ini

				try
				{
					fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
						fullPath, m_config.maxFileSize, m_config.maxFileCount);
				}
				catch (const spdlog::spdlog_ex& ex)
				{
					std::cerr << ex.what() << std::endl;
				}
			}
			while (false);

			if (fileSink)
				sinks.push_back(fileSink);
		}

		// 删除spdlog自动回调，使用手动实现的回调
		//if (static_cast<int>(outputs & LogOutput::Gui))
		//{
		//	auto it = m_customCallbacks.find(LogOutput::Gui);
		//	if (it != m_customCallbacks.end() && it->second)
		//	{
		//		auto customSink = CreateCustomSink(it->second);
		//		sinks.push_back(customSink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::VsTrace))
		//{
		//	auto it = m_customCallbacks.find(LogOutput::VsTrace);
		//	if (it != m_customCallbacks.end() && it->second)
		//	{
		//		auto customSink = CreateCustomSink(it->second);
		//		sinks.push_back(customSink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::XTrace))
		//{
		//	auto it = m_customCallbacks.find(LogOutput::XTrace);
		//	if (it != m_customCallbacks.end() && it->second)
		//	{
		//		auto customSink = CreateCustomSink(it->second);
		//		sinks.push_back(customSink);
		//	}
		//}

		if (sinks.empty())
			return nullptr;

		// 异步
		//try
		//{
		//    static std::atomic<uint64_t> tempLoggerCounter{ 0 };
		//    std::ostringstream oss;
		//    oss << "tempLogger_" << std::this_thread::get_id() << "_" << ++tempLoggerCounter;
		//    std::string tempLoggerName = oss.str();
		//
		//    auto tempLogger = std::make_shared<spdlog::async_logger>(
		//        tempLoggerName,
		//        sinks.begin(),
		//        sinks.end(),
		//        spdlog::thread_pool(),  // 使用全局线程池
		//        spdlog::async_overflow_policy::block  // 缓冲区满时阻塞（避免丢失日志）
		//        );
		//
		//    tempLogger->set_level(static_cast<spdlog::level::level_enum>(m_config.level));
		//    tempLogger->set_pattern("%v");
		//    return tempLogger;
		//}
		//catch (const spdlog::spdlog_ex& ex)
		//{
		//    std::cerr << ex.what() << std::endl;
		//    return nullptr;
		//}

		// 同步
		try
		{
			static std::atomic<uint64_t> tempLoggerCounter{0};
			std::ostringstream oss;
			oss << "tempLogger_" << std::this_thread::get_id() << "_" << ++tempLoggerCounter;
			std::string tempLoggerName = oss.str();

			auto tempLogger = std::make_shared<spdlog::logger>(tempLoggerName, sinks.begin(), sinks.end());
			tempLogger->set_level(static_cast<spdlog::level::level_enum>(m_config.level));
			tempLogger->set_pattern("%v");
			return tempLogger;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << ex.what() << std::endl;
			return nullptr;
		}
	}

	std::string Logger::FormatLogMessage(LogOutput outputs, LogLevel level, const char* file, int line,
	                                     const char* function, const char* message)
	{
		std::stringstream threadId;
		threadId << std::this_thread::get_id();
		size_t tid = 0;
		threadId >> tid;

		std::string levelName;
		if (level == LogLevel::InfoRed)
			levelName = "Info";
		else if (level == LogLevel::Info)
			levelName = "Info";
		else if (level == LogLevel::InfoGreen)
			levelName = "Info";
		switch (level)
		{
		case LogLevel::Trace: levelName = "Trace";
			break;
		case LogLevel::Debug: levelName = "Debug";
			break;
		case LogLevel::Info: levelName = "Info";
			break;
		case LogLevel::Warn: levelName = "Warn";
			break;
		case LogLevel::Error: levelName = "Error";
			break;
		case LogLevel::Critical: levelName = "Critical";
			break;
		case LogLevel::InfoRed:
		case LogLevel::InfoGreen:
		case LogLevel::InfoBlack:
		default: levelName = "Info";
			break;
		}

		std::string fileName;
		if (file) fileName = file;

		size_t pos = fileName.find_last_of("\\/");
		if (pos != std::string::npos) fileName = fileName.substr(pos + 1);

		std::string lineNum;
		if (line > 0) lineNum = std::to_string(line);

		std::string funcName;
		if (function) funcName = function;

		std::string time = TimePoint::Now().ToString("%H:%M:%S.%F");

		auto first = true;
		for (auto& item : m_customCallbacks)
		{
			if ((static_cast<int>(outputs & item.first)) && item.second)
			{
				MetaMsg metaMsg;
				metaMsg.time = std::chrono::system_clock::now();
				metaMsg.logger_name = LogName::DEFAULT;
				metaMsg.thread_id = tid;
				metaMsg.level = level;
				metaMsg.output = item.first;
				metaMsg.file = fileName.length() ? fileName.c_str() : nullptr;
				metaMsg.line = line;
				metaMsg.func = funcName.length() ? funcName.c_str() : nullptr;
				metaMsg.message = message;

				item.second(metaMsg);

				if (first && m_config.enableDatabase) // 一条消息
				{
					first = false;
					LogDbManager::GetInstance().AddMsgToQueue(metaMsg);
				}
			}
		}

		std::string result = "[" + time + "] [" + threadId.str() + "] [" + levelName + "] [" + fileName + ":" + lineNum
			+ ":" + funcName + "] → " + message;

		return result;
	}

	void Logger::Log(LogLevel level, const char* file, int line, const char* function, const char* message)
	{
		if (m_config.outputs == LogOutput::None || !HasValidOutput(m_config.outputs))
			return;

		auto tempLogger = GetCachedLogger(m_config.outputs);
		if (tempLogger)
		{
			std::string formattedMsg = FormatLogMessage(m_config.outputs, level, file, line, function, message);

			switch (level)
			{
			case LogLevel::Trace: tempLogger->trace(formattedMsg);
				break;
			case LogLevel::Debug: tempLogger->debug(formattedMsg);
				break;
			case LogLevel::Info: tempLogger->info(formattedMsg);
				break;
			case LogLevel::Warn: tempLogger->warn(formattedMsg);
				break;
			case LogLevel::Error: tempLogger->error(formattedMsg);
				break;
			case LogLevel::Critical: tempLogger->critical(formattedMsg);
				break;
			case LogLevel::InfoRed:
			case LogLevel::InfoGreen:
			case LogLevel::InfoBlack:
			default: tempLogger->info(formattedMsg);
				break;
			}

			tempLogger->flush();
		}
	}

	bool Logger::HasValidOutput(LogOutput outputs) const
	{
		if (outputs == LogOutput::None)
			return false;

		if (static_cast<int>(outputs) == 0)
			return false;

		if ((static_cast<int>(outputs & LogOutput::File)) && m_config.filePath.empty())
			return false;

		if ((static_cast<int>(outputs & LogOutput::Gui)) && m_customCallbacks.find(LogOutput::Gui) == m_customCallbacks.
			end())
			return false;

		if ((static_cast<int>(outputs & LogOutput::VsTrace)) && m_customCallbacks.find(LogOutput::VsTrace) ==
			m_customCallbacks.end())
			return false;

		if ((static_cast<int>(outputs & LogOutput::Tracer)) && m_customCallbacks.find(LogOutput::Tracer) ==
			m_customCallbacks.end())
			return false;

		return true;
	}

	std::shared_ptr<Logger::CustomSink> Logger::CreateCustomSink(const std::function<void(const MetaMsg&)>& callback)
	{
		return std::make_shared<CustomSink>(callback);
	}

	LogDbManager& LogDbManager::GetInstance()
	{
		static LogDbManager instance;
		return instance;
	}

	bool LogDbManager::Init()
	{
		if (m_running) return true;
		m_running = true;

		std::thread(&LogDbManager::WorkerThread, this).detach();
		return true;
	}

	void LogDbManager::Exit()
	{
		if (!m_running) return;
		m_running = false;

		m_cv.notify_one();

		std::lock_guard<std::mutex> lock(m_dbMutex);
		if (m_db)
			m_db->Close();
		m_db.reset();
	}

	void LogDbManager::AddMsgToQueue(const Logger::MetaMsg& msg)
	{
		if (!m_running)
			return;
		m_queueMsg.emplace(msg);
		m_cv.notify_one();
	}

	void LogDbManager::WorkerThread()
	{
		constexpr uint8_t FIELD_COUNT_MAX = 20; // 最大字段数
		auto CREATE_SQL = R"(CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, 
					timestamp TEXT,
					logger_name TEXT,
					thread_id INTEGER,
					output INTEGER,
					level INTEGER,
					file_name TEXT,
					line_num INTEGER,
					func_name TEXT,
					message TEXT,
					user_name TEXT,
					s11 TEXT, s12 TEXT, s13 TEXT, s14 TEXT, s15 TEXT, s16 TEXT, s17 TEXT, s18 TEXT, s19 TEXT, s20 TEXT);)";
		auto INSERT_SQL = R"(INSERT INTO logs (
					timestamp, 
					logger_name,
					thread_id, 
					output,
					level, 
					file_name, 
					line_num, 
					func_name, 
					message,
					user_name,
					s11, s12, s13, s14, s15, s16, s17, s18, s19, s20) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);)";

		auto LamdbaFunc = [this, FIELD_COUNT_MAX, CREATE_SQL, INSERT_SQL](const std::vector<Logger::MetaMsg>& metaMsg)
		{
			if (!metaMsg.size())
				return false;

			std::lock_guard<std::mutex> lock(m_dbMutex);

			std::string date = TimePoint::Now().ToString("%Y-%m-%d");

			if (date != m_dbLastDate || !m_db || !m_db->IsOpen())
			{
				if (m_db)
					m_db->Close();

				FileSystem::CreateDirectorys("d:/log/logger");
				m_db = std::make_shared<SqliteManager>("d:/log/logger/" + date + "_log.db");
				// d:/log/logger/2025-10-10_log.db

				if (!m_db->IsOpen())
					return false;

				bool rc = m_db->ExecuteNonQuery(CREATE_SQL);
				m_dbLastDate = date;
			}

			SqliteManager::BatchParamsList params_list;
			params_list.reserve(metaMsg.size());
			for (const auto& msg : metaMsg)
			{
				SqliteManager::ParamsList params;

				// 压入顺序与日志数据库字段保持一致
				params.emplace_back(TimePoint::Now().ToString("%H:%M:%S.%F"));
				params.emplace_back(static_cast<int>(msg.logger_name));
				params.emplace_back(static_cast<uint64_t>(msg.thread_id));
				params.emplace_back(static_cast<int>(msg.output));
				params.emplace_back(static_cast<int>(msg.level));
				params.emplace_back(msg.file ? msg.file : "");
				params.emplace_back(msg.line);
				params.emplace_back(msg.func ? msg.func : "");
				params.emplace_back(msg.message);
				while (params.size() < FIELD_COUNT_MAX)
					params.emplace_back("");
				params_list.push_back(std::move(params));
			}
			bool rc = m_db->ExecuteBatchNonQuery(INSERT_SQL, params_list);

			return true;
		};

		while (m_running)
		{
			std::vector<Logger::MetaMsg> batch;
			{
				std::lock_guard<std::mutex> lock(m_queueMutex);
				for (auto* q : m_queues)
				{
					while (q && !q->empty() && batch.size() < 1000)
					{
						batch.push_back(std::move(q->front()));
						q->pop();
					}
				}
				m_queues.insert(&m_queueMsg);
			}

			auto ok = LamdbaFunc(batch);

			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_cv.wait_for(lock, std::chrono::milliseconds(1000), [this]()
			{
				if (!m_running)
					return true;
				for (auto* q : m_queues)
				{
					if (q && !q->empty())
						return true;
				}
				return false;
			});
		}

		// 退出前处理剩余日志
		std::vector<Logger::MetaMsg> remain;
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			for (auto* q : m_queues)
			{
				while (q && !q->empty())
				{
					remain.push_back(std::move(q->front()));
					q->pop();
				}
			}
		}

		if (remain.empty() || !m_db || !m_db->IsOpen())
			return;

		auto ok = LamdbaFunc(remain);
	}

	LoggerManager& LoggerManager::GetInstance()
	{
		static LoggerManager instance;
		return instance;
	}

	std::shared_ptr<Logger> LoggerManager::logger(const LogName& name)
	{
		std::lock_guard<std::mutex> lock(m_loggerMutex);

		auto it = m_loggers.find(name);
		if (it != m_loggers.end())
			return it->second;
		// 没有找到，返回默认 Logger
		return m_loggers[LogName::DEFAULT];
	}

	std::vector<LogName> LoggerManager::loggerNames() const
	{
		std::lock_guard<std::mutex> lock(m_loggerMutex);

		std::vector<LogName> names;
		for (const auto& pair : m_loggers)
		{
			if (pair.first != LogName::DEFAULT)
				names.push_back(pair.first);
		}
		return names;
	}

	LoggerManager::LoggerManager()
	{
		LogDbManager::GetInstance().Init();

		//【1】创建日志器（当模块日志器不存在时返回默认日志器）
		for (auto log_name_index = 0; log_name_index < static_cast<int>(LogName::LOGNAME_MAX); ++log_name_index)
		{
			auto log_name = static_cast<LogName>(log_name_index);
			Logger::Config config(log_name);
			config.filePath = "d:/log";
			config.fileName = LogNameToStr(log_name) + ".ini";
			config.level = LogLevel::Debug;
			createLogger(config);
		}

		//【2】启动定期清理线程
		m_stopCleanupThread = false;
		m_cleanupThread = std::thread(&LoggerManager::CleanupThread, this);
	}

	LoggerManager::~LoggerManager()
	{
		m_stopCleanupThread = true;
		if (m_cleanupThread.joinable())
			m_cleanupThread.join();

		LogDbManager::GetInstance().Exit();
	}

	bool LoggerManager::createLogger(const LogName& name)
	{
		std::lock_guard<std::mutex> lock(m_loggerMutex);

		if (m_loggers.find(name) != m_loggers.end())
			return true;

		auto logger = std::make_shared<Logger>(name);
		logger->GetConfig() = Logger::Config(name);

		if (!logger->IsInitialized())
			logger->Initialize();

		m_loggers[name] = logger;

		return logger ? true : false;
	}

	bool LoggerManager::createLogger(const Logger::Config& config)
	{
		std::lock_guard<std::mutex> lock(m_loggerMutex);

		if (m_loggers.find(config.logName) != m_loggers.end())
			return true;

		auto logger = std::make_shared<Logger>(config);

		if (!logger->IsInitialized())
			logger->Initialize(); // 内部已持有 config

		m_loggers[config.logName] = logger;

		return logger ? true : false;
	}

	std::string LoggerManager::LogNameToStr(const LogName& name)
	{
		switch (name)
		{
		case LogName::MOUNT: return "mount";
		case LogName::MOTION: return "motion";
		case LogName::SMT_DATA: return "smt_data";
		case LogName::OPTIMIZE: return "optimize";
		case LogName::SLAVE_CONTROL: return "slave_control";
		case LogName::CAMERA_TOP: return "camera_top";
		case LogName::DEFAULT: return "default";
		default: return "unknown";
		}
	}

	void LoggerManager::AddCleanupDirectory(const std::string& path, int days)
	{
		std::lock_guard<std::mutex> lock(m_cleanupMutex);
		if (days <= 0)
			m_cleanupMap.erase(path);
		else
			m_cleanupMap[path] = days;
	}

	bool GetFileCreateTime(const std::string& filePath, time_t& createTime)
	{
		WIN32_FILE_ATTRIBUTE_DATA fileData = {0};
		if (!GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fileData))
			return false;

		SYSTEMTIME sysTime = {0};
		if (!FileTimeToSystemTime(&fileData.ftCreationTime, &sysTime))
			return false;

		tm tmTime = {0};
		tmTime.tm_year = sysTime.wYear - 1900;
		tmTime.tm_mon = sysTime.wMonth - 1;
		tmTime.tm_mday = sysTime.wDay;
		tmTime.tm_hour = sysTime.wHour;
		tmTime.tm_min = sysTime.wMinute;
		tmTime.tm_sec = sysTime.wSecond;

		createTime = mktime(&tmTime);
		return createTime != -1;
	}

	void DeleteExpiredLogFiles(const std::string& dirPath, int days)
	{
		std::string searchPath = dirPath + "\\*";
		WIN32_FIND_DATAA findData = {0};
		HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

		if (hFind == INVALID_HANDLE_VALUE)
			return;

		do
		{
			std::string fileName = findData.cFileName;
			if (fileName == "." || fileName == "..")
				continue;

			std::string fullPath = dirPath + "\\" + fileName;

			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				DeleteExpiredLogFiles(fullPath, days);

				if (PathIsDirectoryEmptyA(fullPath.c_str()))
					RemoveDirectoryA(fullPath.c_str());
			}
			else
			{
				time_t createTime = 0;
				if (!GetFileCreateTime(fullPath, createTime))
					continue;

				// 判断是否过期
				time_t currentTime = time(nullptr);
				time_t expireTime = createTime + static_cast<time_t>(days) * 24 * 60 * 60;
				if (currentTime > expireTime)
					DeleteFileA(fullPath.c_str());
			}
		}
		while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
	}

	void LoggerManager::CleanupThread()
	{
		while (!m_stopCleanupThread)
		{
			std::string msg("Start cleanup thread...\n");

			// 拷贝配置（避免持有锁期间耗时）
			std::unordered_map<std::string, int> cleanupMap;
			{
				std::lock_guard<std::mutex> lock(m_cleanupMutex);
				cleanupMap = m_cleanupMap;
			}

			for (const auto& config : cleanupMap)
			{
				const std::string& path = config.first;
				int days = config.second;

				if (days <= 0)
					continue;

				if (!PathIsDirectoryA(path.c_str()))
					continue;

				DeleteExpiredLogFiles(path, days);
			}

			constexpr auto cleanupInterval = 3600; // 等待指定间隔（秒），避免频繁刷新IO
			for (auto i = 0; i < cleanupInterval && !m_stopCleanupThread; ++i)
				std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
#pragma endregion
}
