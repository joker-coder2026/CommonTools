#pragma once
// ============================================================================
// 键值对配置类(KeyValueMap)
// 基于 Json::Value 的键值对容器，线程安全，支持类型化 Set/Get 与序列化。
// ============================================================================
#include "common_export.h"
#include <string>
#include <memory>
#include <mutex>

namespace common_tools
{
	/** @brief 线程安全的键值对容器（基于 JSON 存储，键区分大小写） */
	class COMMONTOOLS_API KeyValueMap
	{
	public:
		/** @brief 构造函数 */
		KeyValueMap();

		/** @brief 拷贝构造函数 */
		KeyValueMap(const KeyValueMap& other);

		/** @brief 拷贝赋值运算符 */
		KeyValueMap& operator=(const KeyValueMap& other);

		/** @brief 析构函数 */
		~KeyValueMap();

		/**
		 * @brief 写入布尔值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] value - 值
		 **/
		void Set(const std::string& key, const bool& value);

		/**
		 * @brief 写入整数值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] value - 值
		 **/
		void Set(const std::string& key, const int& value);

		/**
		 * @brief 写入单精度浮点值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] value - 值
		 **/
		void Set(const std::string& key, const float& value);

		/**
		 * @brief 写入双精度浮点值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] value - 值
		 **/
		void Set(const std::string& key, const double& value);

		/**
		 * @brief 写入字符串值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] value - 值
		 **/
		void Set(const std::string& key, const std::string& value);

		/**
		 * @brief 读取布尔值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] default_value - 键不存在时的默认值
		 * @return 值
		 **/
		bool Get(const std::string& key, bool default_value = false);

		/**
		 * @brief 读取整数值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] default_value - 键不存在时的默认值
		 * @return 值
		 **/
		int Get(const std::string& key, int default_value = 0);

		/**
		 * @brief 读取单精度浮点值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] default_value - 键不存在时的默认值
		 * @return 值
		 **/
		float Get(const std::string& key, float default_value = 0.0f);

		/**
		 * @brief 读取双精度浮点值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] default_value - 键不存在时的默认值
		 * @return 值
		 **/
		double Get(const std::string& key, double default_value = 0.0);

		/**
		 * @brief 读取字符串值
		 * @param [in] key - 键名（区分大小写）
		 * @param [in] default_value - 键不存在时的默认值
		 * @return 值
		 **/
		std::string Get(const std::string& key, const std::string& default_value = "");

		/**
		 * @brief 序列化为 JSON 字符串
		 * @param [in] style - true 输出带缩进格式，false 输出紧凑格式
		 * @return JSON 文本
		 **/
		std::string ToString(bool style = false) const;

		/**
		 * @brief 从 JSON 字符串反序列化（覆盖当前内容）
		 * @param [in] str - JSON 文本
		 **/
		void FromString(const std::string& str);
	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
		mutable std::mutex mutex_;
	};
}
