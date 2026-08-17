#pragma once
// ============================================================================
// 文件编码检查(file_encoding)
// ============================================================================
#include "common_export.h"
#include <string>

namespace file_encoding
{
	/** @brief 文件编码类型枚举 */
	enum class Encoding
	{
		GBK, // ANSI 中文编码
		UTF8, // UTF-8 无BOM
		UTF8_BOM, // UTF-8 带BOM
		UTF16_LE, // Windows Unicode
		UTF16_BE, // 大端Unicode
		UNKNOWN
	};

	/**
	 * @brief 检测指定文件的编码格式
	 * @param [in] file_path - 文件路径
	 * @return 检测到的编码类型（无法识别时返回 Encoding::UNKNOWN）
	 * @remark 通过 BOM 与 UTF-8 字节规则判断，可区分 GBK 与 UTF-8
	 **/
	Encoding COMMONTOOLS_API get(const std::string& file_path);

	/**
	 * @brief 设置文件编码（预留接口，当前未实现内容转换）
	 * @param [in] encoding - 目标编码类型
	 **/
	void COMMONTOOLS_API set(const Encoding& encoding);
}
