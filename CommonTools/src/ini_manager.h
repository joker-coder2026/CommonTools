#pragma once
// ============================================================================
// INI配置文件管理类(IniManager)
// 封装 Windows INI 文件读写（WritePrivateProfileString 系列 API），
// 支持节/键的增删改查、读写类型化值（int/bool/double）、备份等。
// ============================================================================
#include "common_export.h"
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <typeinfo>
#include <type_traits>
#include <algorithm>

namespace common_tools
{
	/** @brief INI 配置文件管理器 */
	class COMMONTOOLS_API IniManager
	{
		/**
		 * @brief 将基础类型值转换为字符串（float 保留 6 位、double 保留 15 位精度）
		 * @param [in] value - 待转换的值
		 * @return 转换后的字符串
		 **/
		template <typename T>
		std::string to_string(const T& value)
		{
			std::ostringstream oss;
			if (typeid(value) == typeid(float))
				oss << std::setprecision(6) << value;
			else if (typeid(value) == typeid(double))
				oss << std::setprecision(15) << value;
			else
				oss << value;
			return oss.str();
		}

		/**
		 * @brief 将字符串转换为基础类型值（bool 支持 true/1/yes）
		 * @param [in] str - 待转换的字符串
		 * @param [in] default_value - 转换失败时的默认值
		 * @return 转换后的值
		 **/
		template <typename T>
		T from_string(const std::string& str, const T& default_value)
		{
			if (str.empty())
				return default_value;
			T value{};
			if (std::is_same<T, bool>::value)
			{
				std::string s = str;
				std::transform(s.begin(), s.end(), s.begin(), tolower);
				value = (s == "true" || s == "1" || s == "yes");
			}
			else
			{
				std::istringstream iss(str);
				if (!(iss >> value) || !iss.eof())
					value = default_value;
			}
			return value;
		}

	public:
		/**
		 * @brief 构造函数：指定 INI 文件路径（相对路径基于可执行文件目录拼接）
		 * @param [in] file_path - INI 文件路径（含盘符的绝对路径或相对文件名）
		 **/
		explicit IniManager(const std::string& file_path);

		IniManager(const IniManager&) = delete;

		IniManager(const IniManager&&) = delete;

		IniManager& operator=(const IniManager&) = delete;

		IniManager& operator=(const IniManager&&) = delete;

		/**
		 * @brief 写入字符串值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] value - 值
		 * @return 写入是否成功
		 **/
		bool write_value(const std::string& section, const std::string& key, const std::string& value);

		/**
		 * @brief 读取字符串值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] default_value - 读取失败时的默认值
		 * @return 读取到的值
		 **/
		std::string read_value(const std::string& section, const std::string& key, const std::string& default_value);

		/**
		 * @brief 写入整数值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] value - 整数值
		 * @return 写入是否成功
		 **/
		bool write_int(const std::string& section, const std::string& key, int value);

		/**
		 * @brief 写入布尔值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] value - 布尔值
		 * @return 写入是否成功
		 **/
		bool write_bool(const std::string& section, const std::string& key, bool value);

		/**
		 * @brief 写入双精度浮点值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] value - 双精度值
		 * @return 写入是否成功
		 **/
		bool write_double(const std::string& section, const std::string& key, double value);

		/**
		 * @brief 读取整数值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] default_value - 默认值（默认 0）
		 * @return 读取到的整数值
		 **/
		int read_int(const std::string& section, const std::string& key, int default_value = 0);

		/**
		 * @brief 读取布尔值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] default_value - 默认值（默认 false）
		 * @return 读取到的布尔值
		 **/
		bool read_bool(const std::string& section, const std::string& key, bool default_value = false);

		/**
		 * @brief 读取双精度浮点值
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @param [in] default_value - 默认值（默认 0.0）
		 * @return 读取到的双精度值
		 **/
		double read_double(const std::string& section, const std::string& key, double default_value = 0.0);

		/**
		 * @brief 读取整个节的所有键值对
		 * @param [in] section - 节名
		 * @return 键值对映射（节不存在时返回空）
		 **/
		std::map<std::string, std::string> ReadSection(const std::string& section);

		/**
		 * @brief 整体覆写一个节（先清空再写入）
		 * @param [in] section - 节名
		 * @param [in] keyValues - 键值对集合
		 * @return 写入是否成功
		 **/
		bool write_section(const std::string& section, const std::map<std::string, std::string>& keyValues);

		/**
		 * @brief 删除指定键
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @return 删除是否成功
		 **/
		bool delete_key(const std::string& section, const std::string& key);

		/**
		 * @brief 删除整个节
		 * @param [in] section - 节名
		 * @return 删除是否成功
		 **/
		bool delete_section(const std::string& section);

		/**
		 * @brief 检查 INI 文件是否存在
		 * @return 存在返回 true
		 **/
		bool file_exists();

		/**
		 * @brief 备份 INI 文件（复制为 .bak 文件）
		 * @param [in] backupPath - 备份路径；为空时自动生成 "<原文件名>.bak<扩展名>"
		 * @return 备份是否成功
		 **/
		bool backup_file(const std::string& backupPath);

		/**
		 * @brief 检查节是否存在
		 * @param [in] section - 节名
		 * @return 存在返回 true
		 **/
		bool section_exists(const std::string& section);

		/**
		 * @brief 检查键是否存在
		 * @param [in] section - 节名
		 * @param [in] key - 键名
		 * @return 存在返回 true
		 **/
		bool key_exists(const std::string& section, const std::string& key);

		/**
		 * @brief 获取文件中所有节名
		 * @return 节名列表
		 **/
		std::vector<std::string> get_section_names();

		/**
		 * @brief 获取指定节下所有键名
		 * @param [in] section - 节名
		 * @return 键名列表
		 **/
		std::vector<std::string> get_key_names(const std::string& section);

		/**
		 * @brief 获取最近一次操作的错误信息
		 * @return 错误描述（无错误时为空串）
		 **/
		std::string get_last_error();

	private:
		/** @brief 记录最近一次操作的错误信息 */
		void set_last_error(const std::string& error);

		std::string file_path_; // INI 文件路径
		std::string last_error_; // 最近错误信息
	};
}
