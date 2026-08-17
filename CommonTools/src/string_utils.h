#pragma once
// ============================================================================
// 字符串工具(string_utils)
// ============================================================================
#include "common_export.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <typeinfo>
#include <type_traits>

namespace string_utils
{
	/**
	 * @brief 字符串分割
	 * @param [in] str - 将要分割的字符串
	 * @param [in] delimiter - 用于分割字符串的符号，支持单个字符。如 ','
	 * @param [in] number - 控制分割结果的最大数量
	 * @return 分割结果
	 **/
	std::vector<std::string> COMMONTOOLS_API split(const std::string& str, char delimiter, int pad_number = 0);

	/**
	 * @brief 字符串分割
	 * @param [in] str - 将要分割的字符串
	 * @param [in] delimiters - 用于分割字符串的符号，支持组合字符。如 ",\\/"
	 * @param [in] number - 控制分割结果的最大数量(=0时，不控制数量；>0时，仅当最大数量number大于分割结果数量时有效)
	 * @return 分割结果
	 **/
	std::vector<std::string> COMMONTOOLS_API split(const std::string& str, std::string delimiters, int pad_number = 0);

	/**
	 * @brief
	 * @param [in] list - 将要合并的字符串列表
	 * @param [in] delimiter - 用于分割字符串的符号
	 * @return 合并结果
	 **/
	std::string COMMONTOOLS_API merge(const std::vector<std::string>& list, char delimiter);

	/**
	 * @brief
	 * @param [in] list - 将要合并的字符串列表
	 * @param [in] delimiter - 用于分割字符串的符号
	 * @return 合并结果
	 **/
	std::string COMMONTOOLS_API merge(const std::vector<std::string>& list, std::string delimiters);

	/**
	 * @brief 字符串格式化
	 * @param [in] format - 格式化参数
	 * @return 格式化结果
	 **/
	std::string COMMONTOOLS_API format(const char* format, ...);

	/**
	 * @brief 字符串格式化
	 * @param [out] out - 格式化字符串
	 * @param [in] format - 格式化参数
	 * @return 格式化结果
	 **/
	void COMMONTOOLS_API format(std::string& out, const char* format, ...);

	/**
	 * @brief
	 * @param [in] value - bool，short，int，float，double等常规数据类型的值
	 * @param [in] precision - 浮点数时小数有效位数
	 * @return 字符串数值
	 **/
	template <typename T>
	std::string to_string(const T& value, int precision)
	{
		std::ostringstream oss;
		if (typeid(value) == typeid(float) || typeid(value) == typeid(double) || typeid(value) == typeid(long double))
		{
			if (precision > -1)
				oss << std::setprecision(precision) << value;
			else
				oss << value;
		}
		else
			oss << value;
		return oss.str();
	}

	/**
	 * @brief GBK 转 UTF-8
	 * @param [in] gbk - 转换前 GBK 原始字符串
	 * @return 转换后 UTF-8 字符串
	 **/
	std::string COMMONTOOLS_API GBKToUTF8(const std::string& gbk);

	/**
	* @brief UTF-8 转 GBK
	* @param [in] utf8 - 转换前 UTF-8 原始字符串
	* @return 转换后 GBK 字符串
	**/
	std::string COMMONTOOLS_API UTF8ToGBK(const std::string& utf8);

	/**
	* @brief 字符串移除左侧空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API trim_left(const std::string& str);

	/**
	* @brief 字符串移除右侧空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API trim_right(const std::string& str);

	/**
	* @brief 字符串移除首尾空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API trim(const std::string& str);

	/**
	* @brief 字符串移除首尾自定义字符集
	* @param [in] str - 原始字符串
	* @param [in] chars -
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API trim(const std::string& str, const std::string& chars);

	/**
	* @brief 字符串大写
	* @param [in] str - 原始字符串
	* @return 大写后的字符串
	**/
	std::string COMMONTOOLS_API to_upper(const std::string& str);

	/**
	* @brief 字符串小写
	* @param [in] str - 原始字符串
	* @return 小写后的字符串
	**/
	std::string COMMONTOOLS_API to_lower(const std::string& str);

	/**
	* @brief 字符串中子串替换
	* @param [in] str - 原始字符串
	* @param [in] old_str - 替换前子串
	* @param [in] new_str - 替换后子串
	* @return 结果字符串
	**/
	std::string COMMONTOOLS_API replace(const std::string& str, const std::string& old_str, const std::string& new_str);

	/**
	* @brief 基础类型数组数据拼接为字符串(bool，int，float...)
	* @param [in] array - 数组数据
	* @param [in] length - 数组长度
	* @param [in] delimiter - 拼接分隔符
	* @return 拼接字符串
	**/
	template <typename T>
	std::string array_to_string(const T* array, const int& length, const std::string& delimiter)
	{
		std::ostringstream oss;
		for (int i = 0; i < length; ++i)
		{
			if (std::is_same<T, bool>::value)
				oss << static_cast<int>(array[i]);
			else
				oss << array[i];
			if (i < length - 1)
				oss << delimiter.c_str();
		}
		return oss.str();
	}

	/**
	* @brief 基础类型数组数据拼接为字符串(自动推导数组长度)
	* @param [in] array - 数组数据
	* @param [in] delimiter - 拼接分隔符
	* @return 拼接字符串
	**/
	template <typename T, size_t N>
	std::string array_to_string(const T (&array)[N], const std::string& delimiter)
	{
		return array_to_string(array, N, delimiter);
	}
}
