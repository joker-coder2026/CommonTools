#pragma once
// ============================================================================
// CommonTools.h —— 汇总头文件
// 自研源码统一位于 src/（模块文件平铺，文件名即分类），三方库位于 third_party/。
// 本文件统一包含全部自研模块头，现有代码 #include "CommonTools.h" 无需任何改动。
//
// 目录结构：
//   src/                  —— 自研源码（模块 .h/.cpp 平铺）
//     common_export.h     —— 公共基础设施（导出宏/返回值/前置声明）
//     ini_manager.h/.cpp  —— INI配置文件管理类(IniManager)
//     xml_manager.h/.cpp  —— XML配置文件管理类(XmlManager)
//     json_manager.h/.cpp —— JSON配置文件管理类(JsonManager)
//     sqlite_manager.h/.cpp —— Sqlite配置文件管理类(SqliteManager)
//     config_custom.h/.cpp —— 自定义配置类(CfgCustom: ConfigKey/ConfigSection/ConfigCustom + CFG宏)
//     logger.h/.cpp       —— 日志管理类(LoggerManager: Logger/LogDbManager + LOG宏)
//     key_value_map.h/.cpp—— 键值对配置类(KeyValueMap)
//     string_utils.h/.cpp —— 字符串工具(string_utils)
//     file_system.h/.cpp  —— 文件目录操作(file_system)
//     file_encoding.h/.cpp—— 文件编码检查(file_encoding)
//     timestamp.h/.cpp    —— 高精度时间戳(timestamp)
//     bit_tools.h/.cpp    —— 基础数据转换位数据(bit32_tools)
//     progress_info.h     —— 进度条信息(ProgressInfo)
//     nameof.h            —— 获取名称/获取类型(nameof_detail) + NAMEOF/TYPEOF宏
//   third_party/          —— 三方库（jsoncpp/spdlog/sqlite3/tinyxml2/inicpp）
// ============================================================================

#include "src/common_export.h"

#include "src/ini_manager.h"
#include "src/xml_manager.h"
#include "src/json_manager.h"
#include "src/sqlite_manager.h"
#include "src/config_custom.h"

#include "src/logger.h"

#include "src/progress_info.h"
#include "src/key_value_map.h"
#include "src/bit_tools.h"
#include "src/string_utils.h"
#include "src/file_system.h"
#include "src/file_encoding.h"
#include "src/timestamp.h"
#include "src/nameof.h"
