//#include "pch.h"
// 1. 标准C++头文件（按功能分类，优先基础类型/容器）
#include <cstdint>        // C++14 必需：uint64_t
#include <string>         // 字符串基础
#include <vector>         // 容器
#include <map>            // 容器
#include <fstream>        // 文件流
#include <sstream>        // 字符串流
#include <algorithm>      // 算法（find/find_first_of等）
#include <chrono>         // 时间相关
#include <iomanip>        // 时间格式化（put_time）
#include <stdexcept>      // 异常处理
#include <cctype>         // 字符处理（isdigit/toupper等）
#include <cstdio>         // snprintf/vsnprintf
#include <cstdarg>        // 可变参数（va_list）
#include <cstring>        // memcpy/strcpy_s
#include <cerrno>         // 错误码（errno）
#include <mutex>          // 互斥锁（虽未直接用，但保留兼容）
#include <thread>         // 线程（保留兼容，若后续扩展）
#include <regex>          // 正则（保留，若后续用到）
#include <ctime>          // 时间处理（mktime/localtime_s）
#include <sys/stat.h>     // 文件属性（stat）

// 2. Windows API头文件（仅保留必需部分）
#include <Windows.h>      // 核心Windows API（MultiByteToWideChar/CreateFile等）
#include <direct.h>       // _mkdir/_rmdir/_getcwd
#include <corecrt_io.h>   // _access
#include <Shlwapi.h>      // PathFindFileNameA/PathRemoveFileSpecA（仅FileSystem用到）
#pragma comment(lib, "Shlwapi.lib") // 对应库依赖

// 3. 项目自定义头文件（最后引入）
#include "CommonTools.h"
#include "inicpp.hpp" // INI文件处理（保留，若后续用到）
// jsoncpp
#include "json/json.h"
// tinyxml2 for XML handler
#include "tinyxml2/tinyxml2.h"

namespace CommonTools
{
#pragma region BitTools
	// ========== 32/64位通用实现 ==========
	template <int BitWidth>
	void BitTools<BitWidth>::Set(ValueType& value, int bit_idx, bool bit_value)
	{
		CheckBitIndex(bit_idx);
		const UnsignedType mask = ShiftBase << static_cast<UnsignedType>(bit_idx);
		value = (value & ~static_cast<ValueType>(mask)) | (bit_value ? static_cast<ValueType>(mask) : 0);
	}

	template <int BitWidth>
	bool BitTools<BitWidth>::Get(ValueType value, int bit_idx)
	{
		CheckBitIndex(bit_idx);
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
		while (val != 0)
		{
			val &= val - 1;
			++count;
		}
		return count;
	}

	// 显式实例化模板（C++14兼容）
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
		else if (typeid(value) == typeid(bool))
			oss << (static_cast<bool>(value) ? "true" : "false");
		else if (typeid(value) == typeid(BOOL))
			oss << (value != FALSE ? "TRUE" : "FALSE");
		else
			oss << value;
		return oss.str();
	}

	//// 模板显式实例化（常用类型）
	//template std::string StringUtils::ToString<int>(const int&, int);
	//template std::string StringUtils::ToString<double>(const double&, int);
	//template std::string StringUtils::ToString<bool>(const bool&, int);
	//template std::string StringUtils::ToString<long long>(const long long&, int);

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
			if (max_split_num > 0 && tokens.size() >= static_cast<size_t>(max_split_num) - 1)
				break;

			start = end + 1;
			end = str.find_first_of(seps, start);
		}

		if ((max_split_num <= 0 || tokens.size() < static_cast<size_t>(max_split_num) - 1) && start <= str.size())
			tokens.emplace_back(str, start);

		if (max_split_num > 0 && tokens.size() < static_cast<size_t>(max_split_num))
			tokens.resize(static_cast<size_t>(max_split_num), "");

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

		char* dst = &merge[0]; // C++14: data()返回const char*，用&[0]操作可写缓冲区
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
			{
				throw std::runtime_error(
					"StringUtils::Format: failed to format string (ret=" + std::to_string(ret) + ")");
			}

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
		{
			throw std::runtime_error(
				"G2U: GBK to WideChar convert failed (error=" + std::to_string(GetLastError()) + ")");
		}

		const int u8_len = WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), w_len, nullptr, 0, nullptr, nullptr);
		if (u8_len <= 0)
			throw std::runtime_error("G2U: WideChar to UTF8 failed (error=" + std::to_string(GetLastError()) + ")");

		std::string utf8_str;
		utf8_str.reserve(static_cast<size_t>(u8_len));
		utf8_str.resize(static_cast<size_t>(u8_len));
		if (WideCharToMultiByte(CP_UTF8, 0, w_str.c_str(), w_len, &utf8_str[0], u8_len, nullptr, nullptr) <= 0)
		{
			throw std::runtime_error(
				"G2U: WideChar to UTF8 convert failed (error=" + std::to_string(GetLastError()) + ")");
		}

		utf8_str.resize(static_cast<size_t>(u8_len));
		return utf8_str;
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
		{
			throw std::runtime_error(
				"U2G: UTF8 to WideChar convert failed (error=" + std::to_string(GetLastError()) + ")");
		}

		const int gbk_len = WideCharToMultiByte(CP_ACP, 0, w_str.c_str(), w_len, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("U2G: WideChar to GBK failed (error=" + std::to_string(GetLastError()) + ")");

		std::string gbk_str;
		gbk_str.reserve(static_cast<size_t>(gbk_len));
		gbk_str.resize(static_cast<size_t>(gbk_len));
		if (WideCharToMultiByte(CP_ACP, 0, w_str.c_str(), w_len, &gbk_str[0], gbk_len, nullptr, nullptr) <= 0)
		{
			throw std::runtime_error(
				"U2G: WideChar to GBK convert failed (error=" + std::to_string(GetLastError()) + ")");
		}

		gbk_str.resize(static_cast<size_t>(gbk_len));
		return gbk_str;
#endif
	}

	std::string StringUtils::TrimLeft(const std::string& str)
	{
		if (str.empty())
			return str;

		static auto whitespace = " \t\n\r\v\f";
		const size_t start = str.find_first_not_of(whitespace);

		return (start == std::string::npos) ? std::string() : str.substr(start);
	}

	std::string StringUtils::TrimRight(const std::string& str)
	{
		if (str.empty())
			return str;

		static auto whitespace = " \t\n\r\v\f";
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
		if (path.empty())
			return false;

		return _access(path.c_str(), 0) == 0;
	}

	bool FileSystem::IsFile(const std::string& path)
	{
		if (!Exists(path))
			return false;

		struct stat info{};
		if (stat(path.c_str(), &info) != 0)
			return false;

		return (info.st_mode & S_IFMT) == S_IFREG;
	}

	bool FileSystem::CreateFileX(const std::string& path)
	{
		if (path.empty()) return false;

		std::ofstream file(path, std::ios::out | std::ios::trunc);
		return file.good();
	}

	bool FileSystem::RenameFileX(const std::string& srcpath, const std::string& dstpath)
	{
		if (srcpath.empty() || dstpath.empty() || !IsFile(srcpath)) return false;

		if (std::rename(srcpath.c_str(), dstpath.c_str()) == 0)
			return true;

		return MoveFileExA(srcpath.c_str(), dstpath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
	}

	bool FileSystem::MoveFileX(const std::string& srcpath, const std::string& dstpath)
	{
		return RenameFileX(srcpath, dstpath);
	}

	bool FileSystem::CopyFileX(const std::string& srcpath, const std::string& dstpath)
	{
		if (srcpath.empty() || dstpath.empty() || !IsFile(srcpath)) return false;

		std::ifstream in(srcpath, std::ios::binary);
		if (!in.is_open()) return false;
		in.sync_with_stdio(false);

		std::ofstream out(dstpath, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) return false;
		out.sync_with_stdio(false);

		char buf[4096];
		while (in.read(buf, sizeof(buf)))
			out.write(buf, in.gcount());

		out.write(buf, in.gcount());

		return in.eof() && out.good();
	}

	bool FileSystem::DeleteFileX(const std::string& path)
	{
		if (path.empty() || !IsFile(path))
			return false;

		return remove(path.c_str()) == 0;
	}

	size_t FileSystem::GetFileSize(const std::string& path)
	{
		if (path.empty() || !IsFile(path))
			return 0;

#ifdef _WIN32
		WIN32_FILE_ATTRIBUTE_DATA fileData{};
		if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData) != FALSE)
		{
			const uint64_t file_size_64 =
				(static_cast<uint64_t>(fileData.nFileSizeHigh) << 32) +
				static_cast<uint64_t>(fileData.nFileSizeLow);

			if (file_size_64 > SIZE_MAX)
				return SIZE_MAX;

			return file_size_64;
		}
#endif

		std::ifstream file_stream(path, std::ios::binary | std::ios::ate);
		if (!file_stream.is_open())
			return 0;

		file_stream.sync_with_stdio(false);

		const std::streampos pos = file_stream.tellg();
		if (pos == std::streampos(-1))
			return 0;

		const auto file_size_64 = pos;
		if (file_size_64 > SIZE_MAX)
			return SIZE_MAX;

		return file_size_64;
	}

	std::time_t FileSystem::GetFileCreateTime(const std::string& path)
	{
		if (path.empty() || !Exists(path))
			return 0;

#ifdef _WIN32
		HANDLE hFile = CreateFileA(
			path.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (hFile == INVALID_HANDLE_VALUE)
			return 0;

		FILETIME createTime{};
		if (!GetFileTime(hFile, &createTime, nullptr, nullptr))
		{
			CloseHandle(hFile);
			return 0;
		}

		CloseHandle(hFile);

		FILETIME localTime{};
		FileTimeToLocalFileTime(&createTime, &localTime);
		SYSTEMTIME sysTime{};
		FileTimeToSystemTime(&localTime, &sysTime);

		std::tm tm{};
		tm.tm_year = sysTime.wYear - 1900;
		tm.tm_mon = sysTime.wMonth - 1;
		tm.tm_mday = sysTime.wDay;
		tm.tm_hour = sysTime.wHour;
		tm.tm_min = sysTime.wMinute;
		tm.tm_sec = sysTime.wSecond;

		return mktime(&tm);
#else
		struct stat info{};
		if (stat(path.c_str(), &info) != 0)
			return 0;
		return info.st_ctime;
#endif
	}

	std::time_t FileSystem::GetFileModifiedTime(const std::string& path)
	{
		if (path.empty() || !Exists(path))
			return 0;

#ifdef _WIN32
		HANDLE hFile = CreateFileA(
			path.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (hFile == INVALID_HANDLE_VALUE)
			return 0;

		FILETIME modifyTime{};
		if (!GetFileTime(hFile, nullptr, nullptr, &modifyTime))
		{
			CloseHandle(hFile);
			return 0;
		}

		CloseHandle(hFile);

		FILETIME localTime{};
		FileTimeToLocalFileTime(&modifyTime, &localTime);
		SYSTEMTIME sysTime{};
		FileTimeToSystemTime(&localTime, &sysTime);

		std::tm tm{};
		tm.tm_year = sysTime.wYear - 1900;
		tm.tm_mon = sysTime.wMonth - 1;
		tm.tm_mday = sysTime.wDay;
		tm.tm_hour = sysTime.wHour;
		tm.tm_min = sysTime.wMinute;
		tm.tm_sec = sysTime.wSecond;

		return mktime(&tm);
#else
		struct stat info{};
		if (stat(path.c_str(), &info) != 0)
			return 0;
		return info.st_mtime;
#endif
	}

	std::string FileSystem::GetFileName(const std::string& path)
	{
		if (path.empty())
			return "";

#ifdef _WIN32
		char fileName[MAX_PATH] = {0};
		const char* p = PathFindFileNameA(path.c_str());
		if (p)
			strcpy_s(fileName, MAX_PATH, p);
		return std::string(fileName);
#else
		size_t pos = path.find_last_of("/");
		if (pos == std::string::npos)
			return path;
		return path.substr(pos + 1);
#endif
	}

	std::string FileSystem::GetFileExtensionName(const std::string& path)
	{
		if (path.empty())
			return "";

#ifdef _WIN32
		char ext[MAX_PATH] = {0};
		const char* p = PathFindFileNameA(path.c_str());
		if (p)
			strcpy_s(ext, MAX_PATH, p);
		std::string extension(ext);
		if (!extension.empty() && extension[0] == '.')
			extension = extension.substr(1);
		return extension;
#else
		size_t pos = path.find_last_of(".");
		if (pos == std::string::npos)
			return "";
		return path.substr(pos + 1);
#endif
	}

	std::string FileSystem::GetFileDirectory(const std::string& path)
	{
		if (path.empty())
			return "";

#ifdef _WIN32
		char dir[MAX_PATH] = {0};
		strcpy_s(dir, path.c_str());
		PathRemoveFileSpecA(dir);
		return std::string(dir);
#else
		size_t pos = path.find_last_of("/");
		if (pos == std::string::npos)
			return ".";
		return path.substr(0, pos);
#endif
	}

	std::vector<std::string> FileSystem::GetFilesList(const std::string& path, const std::string& extension)
	{
		std::vector<std::string> files;
		if (!IsDirectory(path))
			return files;

#ifdef _WIN32
		std::string searchPath = path + "\\*";
		if (!extension.empty())
			searchPath = path + "\\*." + extension;

		WIN32_FIND_DATAA findData{};
		HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			files.push_back(std::string(findData.cFileName));
		}
		while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
#else
		// Linux/macOS 实现（简化版）
		DIR* dir = opendir(path.c_str());
		if (!dir)
			return files;

		struct dirent* entry = nullptr;
		while ((entry = readdir(dir)) != nullptr)
		{
			if (entry->d_type != DT_REG)
				continue;

			std::string fileName(entry->d_name);
			if (!extension.empty())
			{
				size_t pos = fileName.find_last_of(".");
				if (pos == std::string::npos || fileName.substr(pos + 1) != extension)
					continue;
			}

			files.push_back(fileName);
		}

		closedir(dir);
#endif

		return files;
	}

	bool FileSystem::IsDirectory(const std::string& path)
	{
		if (!Exists(path))
			return false;

#ifdef _WIN32
		DWORD attr = GetFileAttributesA(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
		struct stat info{};
		if (stat(path.c_str(), &info) != 0)
			return false;
		return (info.st_mode & S_IFMT) == S_IFDIR;
#endif
	}

	bool FileSystem::CreateDirectorys(const std::string& path)
	{
		if (Exists(path))
			return IsDirectory(path);

#ifdef _WIN32
		return CreateDirectoryA(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
		return ::mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
	}

	bool FileSystem::DeleteDirectorys(const std::string& path)
	{
		if (!IsDirectory(path))
			return false;

#ifdef _WIN32
		std::string searchPath = path + "\\*";
		WIN32_FIND_DATAA findData{};
		HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::string fileName(findData.cFileName);
				if (fileName == "." || fileName == "..")
					continue;

				std::string fullPath = path + "\\" + fileName;
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (!DeleteDirectorys(fullPath))
					{
						FindClose(hFind);
						return false;
					}
				}
				else
				{
					if (!DeleteFileX(fullPath))
					{
						FindClose(hFind);
						return false;
					}
				}
			}
			while (FindNextFileA(hFind, &findData));

			FindClose(hFind);
		}

		return RemoveDirectoryA(path.c_str()) != 0;
#else
		DIR* dir = opendir(path.c_str());
		if (!dir)
			return false;

		struct dirent* entry = nullptr;
		while ((entry = readdir(dir)) != nullptr)
		{
			std::string fileName(entry->d_name);
			if (fileName == "." || fileName == "..")
				continue;

			std::string fullPath = path + "/" + fileName;
			if (entry->d_type == DT_DIR)
			{
				if (!DeleteDirectorys(fullPath))
				{
					closedir(dir);
					return false;
				}
			}
			else
			{
				if (::unlink(fullPath.c_str()) != 0)
				{
					closedir(dir);
					return false;
				}
			}
		}

		closedir(dir);
		return ::rmdir(path.c_str()) == 0;
#endif
	}

	std::vector<std::string> FileSystem::GetDirectorysList(const std::string& path)
	{
		std::vector<std::string> dirs;
		if (!IsDirectory(path))
			return dirs;

#ifdef _WIN32
		std::string searchPath = path + "\\*";
		WIN32_FIND_DATAA findData{};
		HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

		if (hFind == INVALID_HANDLE_VALUE)
			return dirs;

		do
		{
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;

			std::string dirName(findData.cFileName);
			if (dirName == "." || dirName == "..")
				continue;

			dirs.push_back(dirName);
		}
		while (FindNextFileA(hFind, &findData));

		FindClose(hFind);
#else
		DIR* dir = opendir(path.c_str());
		if (!dir)
			return dirs;

		struct dirent* entry = nullptr;
		while ((entry = readdir(dir)) != nullptr)
		{
			if (entry->d_type != DT_DIR)
				continue;

			std::string dirName(entry->d_name);
			if (dirName == "." || dirName == "..")
				continue;

			dirs.push_back(dirName);
		}

		closedir(dir);
#endif

		return dirs;
	}

	std::string FileSystem::GetCurrentWorkDirectory()
	{
#ifdef _WIN32
		char buf[MAX_PATH] = {0};
		GetCurrentDirectoryA(MAX_PATH, buf);
		return std::string(buf);
#else
		char buf[PATH_MAX] = {0};
		if (::getcwd(buf, sizeof(buf)) == nullptr)
			return ".";
		return std::string(buf);
#endif
	}

	bool FileSystem::SetCurrentWorkDirectory(const std::string& path)
	{
		if (!IsDirectory(path))
			return false;

#ifdef _WIN32
		return SetCurrentDirectoryA(path.c_str()) != 0;
#else
		return ::chdir(path.c_str()) == 0;
#endif
	}

	std::string FileSystem::ReadAllText(const std::string& path)
	{
		if (!IsFile(path))
			return "";

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
			return "";

		file.seekg(0, std::ios::end);
		auto size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::string content;
		content.resize(size);
		file.read(&content[0], size);

		return content;
	}

	bool FileSystem::WriteAllText(const std::string& path, const std::string& text)
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file.is_open())
			return false;

		file.write(text.data(), text.size());
		return file.good();
	}
#pragma endregion

#pragma region CustomSettings

	class CSImpl
	{
	public:
		Json::Value m_jsonRoot;
		std::string m_basePath;
		std::string m_lastError;

		CRITICAL_SECTION m_cs;
		CRITICAL_SECTION m_csLoadFile;
		CRITICAL_SECTION m_csSaveFile;

		CSImpl() : m_basePath("d:/param/custom_settings/")
		{
			m_lastError = "";

			std::string path(m_basePath);
			std::replace(path.begin(), path.end(), '/', '\\');
			CreateDirectory(path.c_str(), nullptr);

			InitializeCriticalSection(&m_cs);
			InitializeCriticalSection(&m_csLoadFile);
			InitializeCriticalSection(&m_csSaveFile);
			LoadJsonFiles();
		}

		~CSImpl()
		{
			SaveJsonFiles();
			DeleteCriticalSection(&m_cs);
			DeleteCriticalSection(&m_csLoadFile);
			DeleteCriticalSection(&m_csSaveFile);
		}

		static std::string EnsureTrailingSlash(const std::string& path)
		{
			if (!path.empty() && (path[path.size() - 1] != '\\' && path[path.size() - 1] != '/'))
				return path + "/";
			return path;
		}

		void EnsureNodeExists(const std::string& fileName, const std::string& section, const std::string& key)
		{
			if (fileName.empty() || section.empty() || key.empty()) return; // 避免处理空字符串的情况
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
			CSLocker locker(m_csLoadFile);

			m_lastError = "";
			std::string filePath = m_basePath + fileName;
			std::ifstream ifs(filePath, std::ios::in | std::ios::binary);

			if (!ifs.is_open())
			{
				m_lastError = "加载文件失败: " + filePath;
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
			CSLocker locker(m_csSaveFile);

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
					writerBuilder["enable_escaping_for_non_ascii"] = false; // 禁用非ASCII字符转义
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
					writerBuilder["enable_escaping_for_non_ascii"] = false; // 禁用非ASCII字符转义
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
			CSLocker locker(m_cs);

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
			CSLocker locker(m_cs);

			m_lastError = "";
			std::string oldPath = m_basePath + oldFileName;
			std::string newPath = m_basePath + newFileName;

			if (std::rename(oldPath.c_str(), newPath.c_str()) != 0)
			{
				m_lastError = "重命名文件失败: " + oldFileName + " -> " + newFileName;
				return false;
			}

			// 更新内存中的文件信息
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
			CSLocker locker(m_cs);

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
				m_lastError = "读取文件失败";
			else
				m_lastError = "成功加载所有JSON文件";
			FindClose(hFind);
			return true;
		}

		bool SaveJsonFiles()
		{
			CSLocker locker(m_cs);

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
			m_lastError = "部分文件保存失败";
			return false;
		}

		bool JsonToVector(const std::string& fileName, std::vector<CustomSettings::Members>& currentFileData)
		{
			CSLocker locker(m_cs);

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
					item.fileName = StringUtils::U2G(fileName);
					item.section = StringUtils::U2G(section);
					item.key = StringUtils::U2G(key);
					item.description = keyObj.isMember("description")
						                   ? StringUtils::U2G(keyObj["description"].asString())
						                   : StringUtils::U2G("");

					const Json::Value& val = keyObj["value"];

					if (val.isDouble())
					{
						std::ostringstream ss;
						ss.precision(3);
						ss << std::fixed << val.asDouble();
						item.value = StringUtils::U2G(ss.str());
					}
					else
						item.value = StringUtils::U2G(val.asString());

					currentFileData.push_back(item);
				}
			}

			m_lastError = "JSON转Vector成功: " + fileName;
			return true;
		}

		bool VectorToJson(const std::string& fileName, const std::vector<CustomSettings::Members>& currentFileData)
		{
			CSLocker locker(m_cs);

			m_lastError = "";
			m_jsonRoot.removeMember(fileName);
			m_jsonRoot[fileName] = Json::Value(Json::objectValue);
			for (size_t i = 0; i < currentFileData.size(); ++i)
			{
				const CustomSettings::Members& member = currentFileData[i];

				CustomSettings::Members item;
				item.fileName = StringUtils::G2U(member.fileName);
				item.section = StringUtils::G2U(member.section);
				item.key = StringUtils::G2U(member.key);
				item.description = StringUtils::G2U(member.description);
				item.value = StringUtils::G2U(member.value);

				Json::Value& keyObj = m_jsonRoot[item.fileName][item.section][item.key];

				keyObj["description"] = item.description;

				// 直接根据值的类型设置
				if (item.value == "true" || item.value == "false")
					keyObj["value"] = (item.value == "true");
				else if (std::all_of(item.value.begin(), item.value.end(), isdigit) || (item.value.size() > 1 && item.
					value[0] == '-' && std::all_of(item.value.begin() + 1, item.value.end(), isdigit)))
					keyObj["value"] = atoi(item.value.c_str());
				else if (item.value.find('.') != std::string::npos)
					keyObj["value"] = atof(item.value.c_str());
				else
					keyObj["value"] = item.value;
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
			CSLocker locker(m_cs);

			m_lastError = "";
			return m_jsonRoot.getMemberNames();
		}

		std::string JsonToString()
		{
			CSLocker locker(m_cs);

			m_lastError = "";
			return m_jsonRoot.toStyledString();
		}

		std::string GetLastErrorMsg() const
		{
			CSLocker locker(const_cast<CRITICAL_SECTION&>(m_cs));
			return m_lastError;
		}
	};


	// -------------------------- CSKey 实现 --------------------------
	void GetJsonValue(int& out, Json::Value& jsonValue) { out = jsonValue.asInt(); }
	void GetJsonValue(bool& out, Json::Value& jsonValue) { out = jsonValue.asBool(); }
	void GetJsonValue(double& out, Json::Value& jsonValue) { out = jsonValue.asDouble(); }
	void GetJsonValue(std::string& out, Json::Value& jsonValue) { out = jsonValue.asString(); }

	CSKey::CSKey(CSImpl* impl, const std::string& fileName, const std::string& section, const std::string& key)
		: m_impl(impl),
		  m_fileName(StringUtils::ToLower(fileName)),
		  m_section(StringUtils::ToLower(section)),
		  m_key(StringUtils::ToLower(key))
	{
		if (!key.empty())
			m_impl->EnsureNodeExists(m_fileName, m_section, m_key);
	}

	CSKey::CSKey(CSImpl* impl, const std::string& fileName, const std::string& section)
		: CSKey(impl, fileName, section, "")
	{
	}

	CSKey CSKey::operator[](const std::string& key)
	{
		std::string lowerKey = StringUtils::ToLower(key);
		if (lowerKey != "value" && lowerKey != "description")
		{
			m_key = lowerKey;
			m_impl->EnsureNodeExists(m_fileName, m_section, m_key);
		}
		return *this;
	}

	CSKey& CSKey::operator=(const int& value)
	{
		CSLocker locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key] = value;
		return *this;
	}

	CSKey& CSKey::operator=(const double& value)
	{
		CSLocker locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key] = value;
		return *this;
	}

	CSKey& CSKey::operator=(const std::string& value)
	{
		CSLocker locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key] = value;
		return *this;
	}

	CSKey& CSKey::operator=(const char* value)
	{
		return operator=(std::string(value));
	}

	template <typename T>
	CSKey& CSKey::SetValue(const T& value)
	{
		CSLocker locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key] = value;
		return *this;
	}

	template <typename T>
	T CSKey::GetValue(const T& defaultValue) const
	{
		CSLocker locker(m_impl->m_cs);

		T value = defaultValue;
		Json::Value jsonValue = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("value", Json::Value::null);
		if (jsonValue.type() == Json::nullValue)
			m_impl->m_jsonRoot[m_fileName][m_section][m_key]["value"] = defaultValue;
		else
			GetJsonValue(const_cast<T&>(value), jsonValue);

		return value;
	}

	CSKey& CSKey::SetDescription(const std::string& value)
	{
		CSLocker locker(m_impl->m_cs);
		m_impl->m_jsonRoot[m_fileName][m_section][m_key]["description"] = value;
		return *this;
	}

	std::string CSKey::GetDescription(const std::string& defaultValue) const
	{
		CSLocker locker(m_impl->m_cs);

		std::string value = defaultValue;
		Json::Value jsonValue = m_impl->m_jsonRoot[m_fileName][m_section][m_key].get("description", Json::Value::null);
		if (jsonValue.type() == Json::nullValue)
			m_impl->m_jsonRoot[m_fileName][m_section][m_key]["description"] = defaultValue;
		else
			GetJsonValue(value, jsonValue);

		return value;
	}

	CSSection::CSSection(CSImpl* impl, const std::string& fileName)
		: m_impl(impl),
		  m_fileName(StringUtils::ToLower(fileName))
	{
	}

	CSKey CSSection::operator[](const std::string& section)
	{
		return CSKey(m_impl, m_fileName, StringUtils::ToLower(section));
	}

	CustomSettings& CustomSettings::GetInstance()
	{
		static CustomSettings instance;
		return instance;
	}

	CSSection CustomSettings::operator[](const std::string& fileName)
	{
		return CSSection(m_impl, StringUtils::ToLower(fileName));
	}

	CSKey CustomSettings::Access(const std::string& fileName, const std::string& section, const std::string& key)
	{
		return CSKey(m_impl,
		             StringUtils::ToLower(fileName),
		             StringUtils::ToLower(section),
		             StringUtils::ToLower(key)
		);
	}

	std::vector<std::string> CustomSettings::GetJsonFileList() { return m_impl->GetJsonFileList(); }

	std::string CustomSettings::JsonToString() const { return m_impl->JsonToString(); }

	std::string CustomSettings::GetLastErrorMsg() const { return m_impl->GetLastErrorMsg(); }

	bool CustomSettings::LoadJsonFile(const std::string& fileName) { return m_impl->LoadJsonFile(fileName); }

	bool CustomSettings::SaveJsonFile(const std::string& fileName, const bool& isNewFile)
	{
		return m_impl->SaveJsonFile(fileName, isNewFile);
	}

	bool CustomSettings::DeleteJsonFile(const std::string& fileName) { return m_impl->DeleteJsonFile(fileName); }

	bool CustomSettings::RenameJsonFile(const std::string& oldFileName, const std::string& newFileName)
	{
		return m_impl->RenameJsonFile(oldFileName, newFileName);
	}

	bool CustomSettings::LoadJsonFiles() { return m_impl->LoadJsonFiles(); }

	bool CustomSettings::SaveJsonFiles() { return m_impl->SaveJsonFiles(); }

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
	}

	CustomSettings::~CustomSettings()
	{
		if (m_impl)
		{
			delete m_impl;
			m_impl = nullptr;
		}
	}
#pragma endregion
}
