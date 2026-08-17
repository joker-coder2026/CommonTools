#pragma once
// ============================================================================
// 自定义配置类(CfgCustom)：ConfigDataType / ConfigKey / ConfigSection / ConfigCustom
// 将 custom_settings 目录下的 JSON 文件作为配置库管理，
// 通过 CFG["文件名"]["节"]["键"].get/set(...) 链式访问，键自动转小写。
// ============================================================================
#include "common_export.h"
#include <string>
#include <vector>
#include <map>

namespace common_tools
{
	// 前置声明（实现位于 config_custom.cpp）
	class ConfigImpl;

	/** @brief 配置值数据类型枚举 */
	enum class ConfigDataType { Null, Int, Double, String };


	/**
	 * @brief 键访问器：CFG["文件"]["节"]["键"] 的最终层
	 * 提供 get/set/set_description/get_type 等读写接口
	 **/
	class COMMONTOOLS_API ConfigKey
	{
	public:
		/** @brief 构造键访问器（自动创建节点） */
		ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section,
		          const std::string& key);

		/** @brief 构造节级访问器（键为空） */
		ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section);

		/** @brief 继续下钻到指定键（自动创建节点） */
		ConfigKey operator[](const std::string& key);

		/**
		 * @brief 设置键的描述信息
		 * @param [in] value - 描述文本
		 * @return 自身引用（支持链式调用）
		 **/
		ConfigKey& set_description(const std::string& value);

		/**
		 * @brief 写入配置值（支持 int/double/std::string 等类型）
		 * @param [in] value - 值
		 * @param [in] description - 描述信息（可选）
		 * @return 自身引用（支持链式调用）
		 **/
		template<typename T>
		ConfigKey& set(const T& value, const std::string& description = "");

		/** @brief 赋值运算符重载（等价于 set(value)） */
		template<typename T>
		ConfigKey& operator=(const T& value)
		{
			return set(value, "");
		}

		/**
		 * @brief 读取配置值（键不存在时自动按默认值创建节点）
		 * @param [in] default_value - 默认值
		 * @param [in] description - 描述信息（可选）
		 * @return 读取到的值
		 **/
		template<typename T>
		T get(const T& default_value, const std::string& description = "") const;

		/**
		 * @brief 获取键的描述信息
		 * @param [in] default_value - 无描述时的默认返回
		 * @return 描述文本
		 **/
		std::string get_description(const std::string& default_value = "") const;

		/**
		 * @brief 获取配置值的数据类型
		 * @return 数据类型枚举（ConfigDataType）
		 **/
		ConfigDataType get_type() const;

	private:
		ConfigImpl* impl_;
		std::string file_name_;
		std::string section_;
		std::string key_;
	};


	/**
	 * @brief 节访问器：CFG["文件名"]["节"] 的中间层
	 **/
	class COMMONTOOLS_API ConfigSection
	{
	public:
		/** @brief 构造节访问器 */
		ConfigSection(ConfigImpl* impl, const std::string& file_name);

		/** @brief 下钻到指定节，返回键访问器 */
		ConfigKey operator[](const std::string& section);

	private:
		ConfigImpl* impl_;
		std::string file_name_;
	};


	/**
	 * @brief 自定义 JSON 配置管理类（单例，通过 CFG 宏访问）
	 * 启动时自动加载 base_path 下所有 *.json 文件
	 **/
	class COMMONTOOLS_API ConfigCustom
	{
	public:
		/** @brief 键值对成员描述结构体 */
		struct COMMONTOOLS_API Members
		{
			std::string file_name; // 文件名
			std::string section; // 节点
			std::string key; // 键名
			std::string value; // 值
			std::string description; // 描述
		};


		/**
		 * @brief 获取单例实例
		 * @return 全局唯一 ConfigCustom 实例引用
		 **/
		static ConfigCustom& get_instance();

		/**
		 * @brief 按文件名获取节访问器：CFG["demo.json"]
		 * @param [in] file_name - JSON 文件名（如 "demo.json"）
		 * @return 节访问器
		 **/
		ConfigSection operator[](const std::string& file_name);

		/**
		 * @brief 获取已加载的所有 JSON 文件名列表
		 * @return 文件名列表
		 **/
		std::vector<std::string> get_json_file_list();

		/**
		 * @brief 将全部配置序列化为 JSON 字符串
		 * @return 格式化后的 JSON 文本
		 **/
		std::string json_to_string() const;

		/**
		 * @brief 获取最近一次操作的错误信息
		 * @return 错误描述（无错误时为空串）
		 **/
		std::string get_last_error_msg() const;

		/**
		 * @brief 加载指定 JSON 配置文件
		 * @param [in] file_name - 文件名
		 * @return 加载是否成功
		 **/
		bool load_json_file(const std::string& file_name);

		/**
		 * @brief 保存指定 JSON 配置文件
		 * @param [in] file_name - 文件名
		 * @param [in] is_new_file - 是否为新建文件（true 时文件不存在才创建）
		 * @return 保存是否成功
		 **/
		bool save_json_file(const std::string& file_name, const bool& is_new_file = false);

		/**
		 * @brief 删除指定 JSON 配置文件
		 * @param [in] file_name - 文件名
		 * @return 删除是否成功
		 **/
		bool delete_json_file(const std::string& file_name);

		/**
		 * @brief 重命名 JSON 配置文件
		 * @param [in] old_file_name - 原文件名
		 * @param [in] new_file_name - 新文件名
		 * @return 重命名是否成功
		 **/
		bool rename_json_file(const std::string& old_file_name, const std::string& new_file_name);

		/** @brief 加载配置目录下所有 JSON 文件 @return 是否成功 */
		bool load_json_files();

		/** @brief 保存全部内存中配置到磁盘 @return 是否全部成功 */
		bool save_json_files();

		/**
		 * @brief 将指定文件的配置转换为成员列表
		 * @param [in] file_name - 文件名
		 * @param [out] current_file_data - 成员列表输出
		 * @return 转换是否成功
		 **/
		bool json_to_vector(const std::string& file_name, std::vector<Members>& current_file_data);

		/**
		 * @brief 将成员列表写回指定文件配置
		 * @param [in] file_name - 文件名
		 * @param [in] current_file_data - 成员列表
		 * @return 转换是否成功
		 **/
		bool vector_to_json(const std::string& file_name, const std::vector<Members>& current_file_data);

		/**
		 * @brief 按路径移除 JSON 对象
		 * @param [in] object_path - 路径，格式 "fileName/section/key" → "demo.json/test/enable"
		 * @return 是否成功
		 **/
		bool remove_json_object(const std::string& object_path);

		/**
		 * @brief 获取指定文件的所有节名
		 * @param [in] file_name - 文件名
		 * @return 节名列表
		 **/
		std::vector<std::string> get_sections(const std::string& file_name);

		/**
		 * @brief 获取指定文件指定节的所有键值对
		 * @param [in] file_name - 文件名
		 * @param [in] section - 节名
		 * @return 键值对成员列表
		 **/
		std::vector<Members> get_section_key_value_pairs(const std::string& file_name, const std::string& section);

		/**
		 * @brief 获取指定文件所有节的键值对（按节分组）
		 * @param [in] file_name - 文件名
		 * @param [out] result - 节名 → (键名 → 值) 的映射
		 * @return 是否成功
		 **/
		bool get_all_sections_key_value_pairs(const std::string& file_name, std::map<std::string, std::map<std::string, std::string>>& result);

	private:
		ConfigCustom();

		~ConfigCustom();

		ConfigCustom(const ConfigCustom&) = delete;

		ConfigCustom(const ConfigCustom&&) = delete;

		ConfigCustom& operator=(const ConfigCustom&) = delete;

		ConfigCustom& operator=(const ConfigCustom&&) = delete;

		ConfigImpl* impl_;
	};


/** @brief 全局自定义配置单例宏：CFG["文件"]["节"]["键"].get/set(...) */
#define CFG common_tools::ConfigCustom::get_instance()
}
