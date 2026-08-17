#pragma once
// ============================================================================
// 文件目录操作(file_system)
// ============================================================================
#include "common_export.h"
#include <string>
#include <vector>
#include <ctime>

namespace file_system
{
	/**
	 * @brief 检查文件或目录是否存在
	 * @param [in] path - 文件路径或目录路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API exists(const std::string& path);

	/**
	 * @brief 检查是否为文件
	 * @param [in] path - 文件路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API is_file(const std::string& path);

	/**
	 * @brief 创建文件
	 * @param [in] path - 文件路径
	 * @return 创建结果
	 **/
	bool COMMONTOOLS_API create_file(const std::string& path);

	/**
	 * @brief 重命名文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 重命名结果
	 **/
	bool COMMONTOOLS_API rename_file(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 拷贝文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 拷贝结果
	 **/
	bool COMMONTOOLS_API copy_file(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 移动文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 移动结果
	 **/
	bool COMMONTOOLS_API move_file(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 删除文件
	 * @param [in] path - 文件路径
	 * @return 删除结果
	 **/
	bool COMMONTOOLS_API delete_file(const std::string& path);

	/**
	 * @brief 获取目录下所有文件或获取指定格式文件
	 * @param [in] path - 文件路径
	 * @param [in] extension - 扩展名为空时，获取目录下全部文件；扩展名不为空时，获取指定格式文件(如：".txt")
	 * @return 文件列表结果
	 **/
	std::vector<std::string> get_files(const std::string& path, const std::string& extension);

	/**
	 * @brief 获取文件大小(字节单位)
	 * @param [in] path - 文件路径
	 * @return 文件大小
	 **/
	size_t COMMONTOOLS_API get_file_size(const std::string& path);

	/**
	 * @brief 获取文件创建时间
	 * @param [in] path - 文件路径
	 * @return 创建时间
	 **/
	time_t COMMONTOOLS_API get_file_create_time(const std::string& path);

	/**
	 * @brief 获取文件修改时间
	 * @param [in] path - 文件路径
	 * @return 修改时间
	 **/
	time_t COMMONTOOLS_API get_file_modified_time(const std::string& path);

	/**
	 * @brief 获取文件名(不包含路径)
	 * @param [in] path - 文件路径
	 * @return 文件名
	 **/
	std::string COMMONTOOLS_API get_file_name(const std::string& path);

	/**
	 * @brief 获取文件路径(不包含文件名)
	 * @param [in] path - 文件路径
	 * @return 文件路径
	 **/
	std::string COMMONTOOLS_API get_file_path(const std::string& path);

	/**
	 * @brief 获取文件扩展名
	 * @param [in] path - 文件路径
	 * @return 文件扩展名
	 **/
	std::string COMMONTOOLS_API get_file_extension_name(const std::string& path);

	/**
	 * @brief 检查是否为目录
	 * @param [in] path - 目录路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API is_directory(const std::string& path);

	/**
	 * @brief 逐级创建目录
	 * @param [in] path - 目录路径
	 * @return 创建结果
	 **/
	bool COMMONTOOLS_API create_directories(const std::string& path);

	/**
	 * @brief 逐级删除目录
	 * @param [in] path - 目录路径
	 * @return 删除结果
	 **/
	bool COMMONTOOLS_API delete_directories(const std::string& path);

	/**
	 * @brief 获取目录下所有子目录
	 * @param [in] path - 目录路径
	 * @return 目录列表
	 **/
	std::vector<std::string> COMMONTOOLS_API get_directories(const std::string& path);

	/**
	 * @brief 获取实例当前工作目录
	 * @param [in] path - 目录路径
	 * @return 工作目录
	 **/
	std::string COMMONTOOLS_API get_current_work_directory();

	/**
	 * @brief 设置实例当前工作目录
	 * @param [in] path - 目录路径
	 * @return 设置结果
	 **/
	bool COMMONTOOLS_API set_current_work_directory(const std::string& path);

	/**
	 * @brief 读取指定文件全部内容
	 * @param [in] path - 文件路径
	 * @return 读取内容
	 **/
	std::string COMMONTOOLS_API read_all_text(const std::string& path);

	/**
	 * @brief 全部内容写入指定文件
	 * @param [in] path - 文件路径
	 * @return 写入结果
	 **/
	bool COMMONTOOLS_API write_all_text(const std::string& path, const std::string& text);
}
