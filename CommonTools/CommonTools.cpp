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
		int count = 0;
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
		int result = 0;
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

		static const char* whitespace = " \t\n\r\v\f";
		const size_t start = str.find_first_not_of(whitespace);

		return (start == std::string::npos) ? std::string() : str.substr(start);
	}

	std::string StringUtils::TrimRight(const std::string& str)
	{
		if (str.empty())
			return str;

		static const char* whitespace = " \t\n\r\v\f";
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
		char ms_buf[4] = { 0 };
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
		return TimePoint(std::chrono::system_clock::from_time_t(static_cast<std::time_t>(sec)) + ms).ToString(format);
	}

	int64_t TimePoint::ToTimestamp(const std::string& timeStr, const std::string& format)
	{
		if (timeStr.empty() || format.empty())
			throw std::invalid_argument("empty input");

		std::string fmt = format;
		const size_t ms_pos = fmt.find("%f") != std::string::npos ? fmt.find("%f") : fmt.find("%F");
		if (ms_pos != std::string::npos)
			fmt.erase(ms_pos, 2);

		int ms = 0;
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

		struct stat info {};
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
		if (::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData) != FALSE)
		{
			const uint64_t file_size_64 =
				(static_cast<uint64_t>(fileData.nFileSizeHigh) << 32) +
				static_cast<uint64_t>(fileData.nFileSizeLow);

			if (file_size_64 > SIZE_MAX)
				return SIZE_MAX;

			return static_cast<size_t>(file_size_64);
		}
#endif

		std::ifstream file_stream(path, std::ios::binary | std::ios::ate);
		if (!file_stream.is_open())
		{
			return 0;
		}

		file_stream.sync_with_stdio(false);

		const std::streampos pos = file_stream.tellg();
		if (pos == std::streampos(-1))
			return 0;

		const uint64_t file_size_64 = static_cast<uint64_t>(pos);
		if (file_size_64 > SIZE_MAX)
			return SIZE_MAX;

		return static_cast<size_t>(file_size_64);
	}

	std::time_t FileSystem::GetFileCreateTime(const std::string& path)
	{
		if (path.empty() || !Exists(path))
			return 0;

#ifdef _WIN32
		HANDLE hFile = ::CreateFileA(
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
		if (!::GetFileTime(hFile, &createTime, nullptr, nullptr))
		{
			::CloseHandle(hFile);
			return 0;
		}

		::CloseHandle(hFile);

		FILETIME localTime{};
		::FileTimeToLocalFileTime(&createTime, &localTime);
		SYSTEMTIME sysTime{};
		::FileTimeToSystemTime(&localTime, &sysTime);

		std::tm tm{};
		tm.tm_year = sysTime.wYear - 1900;
		tm.tm_mon = sysTime.wMonth - 1;
		tm.tm_mday = sysTime.wDay;
		tm.tm_hour = sysTime.wHour;
		tm.tm_min = sysTime.wMinute;
		tm.tm_sec = sysTime.wSecond;

		return mktime(&tm);
#else
		struct stat info {};
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
		HANDLE hFile = ::CreateFileA(
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
		if (!::GetFileTime(hFile, nullptr, nullptr, &modifyTime))
		{
			::CloseHandle(hFile);
			return 0;
		}

		::CloseHandle(hFile);

		FILETIME localTime{};
		::FileTimeToLocalFileTime(&modifyTime, &localTime);
		SYSTEMTIME sysTime{};
		::FileTimeToSystemTime(&localTime, &sysTime);

		std::tm tm{};
		tm.tm_year = sysTime.wYear - 1900;
		tm.tm_mon = sysTime.wMonth - 1;
		tm.tm_mday = sysTime.wDay;
		tm.tm_hour = sysTime.wHour;
		tm.tm_min = sysTime.wMinute;
		tm.tm_sec = sysTime.wSecond;

		return mktime(&tm);
#else
		struct stat info {};
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
		char fileName[MAX_PATH] = { 0 };
		const char* p = ::PathFindFileNameA(path.c_str());
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
		char ext[MAX_PATH] = { 0 };
		const char* p = ::PathFindFileNameA(path.c_str());
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
		char dir[MAX_PATH] = { 0 };
		::strcpy_s(dir, path.c_str());
		::PathRemoveFileSpecA(dir);
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
		HANDLE hFind = ::FindFirstFileA(searchPath.c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			files.push_back(std::string(findData.cFileName));
		} while (::FindNextFileA(hFind, &findData));

		::FindClose(hFind);
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
		DWORD attr = ::GetFileAttributesA(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
		struct stat info {};
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
		return ::CreateDirectoryA(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
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
		HANDLE hFind = ::FindFirstFileA(searchPath.c_str(), &findData);

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
						::FindClose(hFind);
						return false;
					}
				}
				else
				{
					if (!DeleteFileX(fullPath))
					{
						::FindClose(hFind);
						return false;
					}
				}
			} while (::FindNextFileA(hFind, &findData));

			::FindClose(hFind);
		}

		return ::RemoveDirectoryA(path.c_str()) != 0;
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
		HANDLE hFind = ::FindFirstFileA(searchPath.c_str(), &findData);

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
		} while (::FindNextFileA(hFind, &findData));

		::FindClose(hFind);
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
		char buf[MAX_PATH] = { 0 };
		::GetCurrentDirectoryA(MAX_PATH, buf);
		return std::string(buf);
#else
		char buf[PATH_MAX] = { 0 };
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
		return ::SetCurrentDirectoryA(path.c_str()) != 0;
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
		size_t size = static_cast<size_t>(file.tellg());
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

#pragma region ConfigFileManager
	// ===================== 1. INI处理器实现 =====================
	// 定义inicpp_ini结构体（封装inicpp::ini）
	struct IniConfigHandler::inicpp_ini
	{
		inicpp::IniManager impl;
		inicpp_ini() = default;
		explicit inicpp_ini(const std::string& filePath) : impl(filePath) {}
	};

	// IniConfigHandler实现
	void IniConfigHandler::Load(const std::string& filePath)
	{
		m_ini = std::make_unique<inicpp_ini>(filePath);
	}

	void IniConfigHandler::Save(const std::string& filePath)
	{
		if (!m_ini) return;
		if (!filePath.empty())
		{
			m_ini->impl.setFileName(filePath);
		}
		// inicpp::IniManager writes changes on SetValue via IniManager::set, so nothing else to do here.
	}

	void IniConfigHandler::SetValue(const std::string& section, const std::string& key, const std::string& strVal)
	{
		// Use IniManager::set which writes changes to the INI file
		m_ini->impl.set(section, key, strVal);
	}

	std::string IniConfigHandler::GetValue(const std::string& section, const std::string& key, const std::string& defaultVal)
	{
		if (!m_ini) return defaultVal;
		if (!m_ini->impl.isSectionExists(section)) return defaultVal;
		std::string v = m_ini->impl[section].toString(key);
		return v.empty() ? defaultVal : v;
	}

	// ===================== 2. JSON/XML处理器实现（预留） =====================
	// JSON handler implementation using jsoncpp (json/json.h)
	struct JsonConfigHandler::json_impl
	{
		Json::Value root;
		std::string filePath;
		Json::StreamWriterBuilder writerBuilder;

		json_impl()
		{
			writerBuilder["enable_escaping_for_non_ascii"] = false;
			writerBuilder["emitUTF8"] = true;
		}
	};

	void JsonConfigHandler::Load(const std::string& filePath)
	{
		m_json = std::make_unique<json_impl>();
		m_json->filePath = filePath;

		if (filePath.empty())
			return;

		std::ifstream ifs(filePath, std::ios::in | std::ios::binary);
		if (!ifs.is_open())
		{
			// leave empty root
			return;
		}

		Json::Reader reader;
		Json::Value root;
		if (!reader.parse(ifs, root))
		{
			// parse failed -> keep empty
			ifs.close();
			return;
		}
		m_json->root = std::move(root);
		ifs.close();
	}

	void JsonConfigHandler::Save(const std::string& filePath)
	{
		if (!m_json) m_json = std::make_unique<json_impl>();
		if (!filePath.empty()) m_json->filePath = filePath;
		if (m_json->filePath.empty()) return; // no target file

		std::ofstream ofs(m_json->filePath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!ofs.is_open()) return;

		std::string str = Json::writeString(m_json->writerBuilder, m_json->root);
		ofs << str;
		ofs.close();
	}

	static Json::Value* navigate_create(Json::Value& root, const std::string& node)
	{
		if (node.empty()) return &root;
		std::vector<std::string> parts;
		{
			std::stringstream ss(node);
			std::string item;
			while (std::getline(ss, item, '/'))
			{
				if (!item.empty()) parts.push_back(item);
			}
		}
		Json::Value* cur = &root;
		for (const auto& p : parts)
		{
			if (!cur->isObject()) *cur = Json::Value(Json::objectValue);
			if (!cur->isMember(p)) (*cur)[p] = Json::Value(Json::objectValue);
			cur = &(*cur)[p];
		}
		return cur;
	}

	void JsonConfigHandler::SetValue(const std::string& node, const std::string& key, const std::string& strVal)
	{
		if (!m_json) m_json = std::make_unique<json_impl>();
		Json::Value* target = navigate_create(m_json->root, node);
		if (!target) return;
		(*target)[key] = strVal;
		// persist
		Save("");
	}

	static std::string value_to_string(const Json::Value& v)
	{
		if (v.isString()) return v.asString();
		if (v.isBool()) return v.asBool() ? "true" : "false";
		if (v.isInt() || v.isUInt() || v.isInt64() || v.isUInt64()) return std::to_string(v.asInt64());
		if (v.isDouble())
		{
			std::ostringstream ss;
			ss << v.asDouble();
			return ss.str();
		}
		return "";
	}

	std::string JsonConfigHandler::GetValue(const std::string& node, const std::string& key, const std::string& defaultVal)
	{
		if (!m_json) return defaultVal;
		Json::Value* cur = &m_json->root;
		if (!node.empty())
		{
			std::stringstream ss(node);
			std::string part;
			while (std::getline(ss, part, '/'))
			{
				if (part.empty()) continue;
				if (!cur->isMember(part)) return defaultVal;
				cur = &(*cur)[part];
			}
		}
		if (!cur->isMember(key)) return defaultVal;
		const Json::Value& v = (*cur)[key];
		std::string s = value_to_string(v);
		return s.empty() ? defaultVal : s;
	}

// XML handler implementation using tinyxml2
struct XmlConfigHandler::xml_impl
{
    tinyxml2::XMLDocument doc;
    std::string filePath;
};

static tinyxml2::XMLElement* xml_navigate_create(tinyxml2::XMLDocument& doc, const std::string& node)
{
    if (node.empty())
    {
        // ensure root exists
        if (!doc.RootElement())
            return doc.NewElement("root");
        return doc.RootElement();
    }

    // Ensure document has root
    if (!doc.RootElement())
    {
        tinyxml2::XMLElement* newRoot = doc.NewElement("root");
        doc.InsertFirstChild(newRoot);
    }

    tinyxml2::XMLElement* cur = doc.RootElement();
    std::stringstream ss(node);
    std::string part;
    while (std::getline(ss, part, '/'))
    {
        if (part.empty()) continue;
        tinyxml2::XMLElement* child = cur->FirstChildElement(part.c_str());
        if (!child)
        {
            child = doc.NewElement(part.c_str());
            cur->InsertEndChild(child);
        }
        cur = child;
    }
    return cur;
}

void XmlConfigHandler::Load(const std::string& filePath)
{
    m_xml = std::make_unique<xml_impl>();
    m_xml->filePath = filePath;
    if (filePath.empty()) return;
    tinyxml2::XMLError e = m_xml->doc.LoadFile(filePath.c_str());
    if (e != tinyxml2::XML_SUCCESS)
    {
        // leave empty document
        m_xml->doc.Clear();
    }
}

void XmlConfigHandler::Save(const std::string& filePath)
{
    if (!m_xml) m_xml = std::make_unique<xml_impl>();
    if (!filePath.empty()) m_xml->filePath = filePath;
    if (m_xml->filePath.empty()) return;
    m_xml->doc.SaveFile(m_xml->filePath.c_str());
}

void XmlConfigHandler::SetValue(const std::string& node, const std::string& key, const std::string& strVal)
{
    if (!m_xml) m_xml = std::make_unique<xml_impl>();
    tinyxml2::XMLElement* target = xml_navigate_create(m_xml->doc, node);
    if (!target)
    {
        // nothing
        return;
    }
    // If target is not attached to doc (possible when doc had no root and xml_navigate_create returned new element), ensure it's inserted
    if (!target->Parent())
    {
        if (!m_xml->doc.RootElement())
            m_xml->doc.InsertEndChild(target);
        else
            m_xml->doc.RootElement()->InsertEndChild(target);
    }

    tinyxml2::XMLElement* child = target->FirstChildElement(key.c_str());
    if (!child)
    {
        child = m_xml->doc.NewElement(key.c_str());
        target->InsertEndChild(child);
    }
    // set text
    child->SetText(strVal.c_str());
    // persist
    Save("");
}

std::string XmlConfigHandler::GetValue(const std::string& node, const std::string& key, const std::string& defaultVal)
{
    if (!m_xml) return defaultVal;
    tinyxml2::XMLElement* cur = m_xml->doc.RootElement();
    if (!node.empty())
    {
        std::stringstream ss(node);
        std::string part;
        while (std::getline(ss, part, '/'))
        {
            if (part.empty()) continue;
            if (!cur) return defaultVal;
            cur = cur->FirstChildElement(part.c_str());
        }
    }
    if (!cur) return defaultVal;
    tinyxml2::XMLElement* child = cur->FirstChildElement(key.c_str());
    if (!child) return defaultVal;
    const char* txt = child->GetText();
    return txt ? std::string(txt) : defaultVal;
}

	// ===================== 3. 通用值转换工具实现 =====================
	template <typename T>
	std::string ConfigValueToString(const T& value)
	{
		if constexpr (std::is_same_v<T, std::string>)
			return value;
		else if constexpr (std::is_same_v<T, bool>)
			return value ? "true" : "false";
		else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
			return std::to_string(value);
		else if constexpr (std::is_floating_point_v<T>)
		{
			std::string str = std::to_string(value);
			str.erase(str.find_last_not_of('0') + 1, std::string::npos);
			if (str.back() == '.') str.pop_back();
			return str;
		}
		else
			throw std::invalid_argument("Unsupported value type");
	}

	// 显式实例化常用类型（避免链接错误）
	template std::string ConfigValueToString(const std::string&);
	template std::string ConfigValueToString(const bool&);
	template std::string ConfigValueToString(const int&);
	template std::string ConfigValueToString(const double&);
	template std::string ConfigValueToString(const float&);
	template std::string ConfigValueToString(const long long&);

	// ===================== 4. ConfigFile实现 =====================
	// ConfigFile构造函数
	template <typename HandlerT>
	ConfigFile<HandlerT>::ConfigFile(const std::string& filePath) : m_filePath(filePath)
	{
		m_handler = std::make_unique<HandlerT>();
		m_handler->Load(filePath);
	}

	// NodeProxy构造函数
	template <typename HandlerT>
	ConfigFile<HandlerT>::NodeProxy::NodeProxy(ConfigFile& cfg, const std::string& node)
		: m_cfg(cfg), m_node(node)
	{
	}

	// NodeProxy::operator[]
	template <typename HandlerT>
	typename ConfigFile<HandlerT>::KeyProxy ConfigFile<HandlerT>::NodeProxy::operator[](const std::string& key)
	{
		return KeyProxy(m_cfg, m_node, key);
	}

	// KeyProxy构造函数
	template <typename HandlerT>
	ConfigFile<HandlerT>::KeyProxy::KeyProxy(ConfigFile& cfg, const std::string& node, const std::string& key)
		: m_cfg(cfg), m_node(node), m_key(key)
	{
	}

	// KeyProxy::operator=
	template <typename HandlerT>
	template <typename T>
	typename ConfigFile<HandlerT>::KeyProxy& ConfigFile<HandlerT>::KeyProxy::operator=(const T& value)
	{
		m_cfg.SetValue(m_node, m_key, value);
		return *this;
	}

	// ConfigFile::operator[]
	template <typename HandlerT>
	typename ConfigFile<HandlerT>::NodeProxy ConfigFile<HandlerT>::operator[](const std::string& node)
	{
		return NodeProxy(*this, node);
	}

	// ConfigFile::SetValue
	template <typename HandlerT>
	template <typename T>
	void ConfigFile<HandlerT>::SetValue(const std::string& node, const std::string& key, const T& value)
	{
		std::string strVal = ConfigValueToString(value);
		m_handler->SetValue(node, key, strVal);
		m_handler->Save(m_filePath);
	}

	// ConfigFile::GetValue
	template <typename HandlerT>
	template <typename T>
	T ConfigFile<HandlerT>::GetValue(const std::string& node, const std::string& key, const T& defaultVal)
	{
		std::string strVal = m_handler->GetValue(node, key, "");
		if (strVal.empty()) return defaultVal;

		if constexpr (std::is_same_v<T, std::string>)
			return strVal;
		else if constexpr (std::is_same_v<T, bool>)
			return (strVal == "true" || strVal == "1");
		else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
			return static_cast<T>(std::stoll(strVal));
		else if constexpr (std::is_floating_point_v<T>)
			return static_cast<T>(std::stod(strVal));
		else
			return defaultVal;
	}

	// 显式实例化ConfigFile（避免链接错误）
	template class ConfigFile<IniConfigHandler>;
	template class ConfigFile<JsonConfigHandler>;
	template class ConfigFile<XmlConfigHandler>;

	// ===================== 5. ConfigManager实现 =====================
	ConfigManager& ConfigManager::Instance()
	{
		static ConfigManager instance;
		return instance;
	}

	std::shared_ptr<void> ConfigManager::operator[](const std::string& filePath)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::string ext = GetFileExtension(filePath);
		std::string cacheKey = filePath + "_" + ext;

		// 缓存命中
		if (m_cache.find(cacheKey) != m_cache.end())
		{
			return m_cache[cacheKey];
		}

		// 缓存未命中
		if (ext == "ini")
		{
			auto cfg = std::make_shared<ConfigFile<IniConfigHandler>>(filePath);
			m_cache[cacheKey] = cfg;
			return m_cache[cacheKey];
		}
		else if (ext == "json")
		{
			auto cfg = std::make_shared<ConfigFile<JsonConfigHandler>>(filePath);
			m_cache[cacheKey] = cfg;
			return m_cache[cacheKey];
		}
		else if (ext == "xml")
		{
			auto cfg = std::make_shared<ConfigFile<XmlConfigHandler>>(filePath);
			m_cache[cacheKey] = cfg;
			return m_cache[cacheKey];
		}
		else
			throw std::invalid_argument("Unsupported file format: " + ext);
	}

	void ConfigManager::ClearCache(const std::string& filePath)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (filePath.empty())
			m_cache.clear();
		else
		{
			for (auto it = m_cache.begin(); it != m_cache.end();)
			{
				if (it->first.find(filePath) == 0)
					it = m_cache.erase(it);
				else
					++it;
			}
		}
	}

	std::string ConfigManager::GetFileExtension(const std::string& filePath)
	{
		size_t dotPos = filePath.find_last_of('.');
		return (dotPos == std::string::npos) ? "" : filePath.substr(dotPos + 1);
	}
#pragma endregion
}
