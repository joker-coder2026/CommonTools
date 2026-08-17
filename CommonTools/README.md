# CommonTools 工程规范

自研源码统一位于 `src/`（模块文件平铺，文件名即分类），三方库统一位于 `third_party/`，
`CommonTools.h` 为唯一汇总入口，现有代码 `#include "CommonTools.h"` 无需改动。

## 目录结构

```
CommonTools/
├── CommonTools.h            # 汇总头：统一包含 src/ 下全部自研模块
├── CommonTools.vcxproj      # 工程文件（.filters 为 VS 分组）
├── src/                     # —— 自研源码（模块 .h/.cpp 平铺）——
│   ├── common_export.h      # 公共基础设施：导出宏 / ReturnCode / 前置声明
│   ├── config_custom.h/.cpp # 自定义配置 CfgCustom（ConfigKey/Section/Custom + CFG 宏）
│   ├── ini_manager.h/.cpp   # INI 配置文件管理类
│   ├── xml_manager.h/.cpp   # XML 配置文件管理类
│   ├── json_manager.h/.cpp  # JSON 配置文件管理类
│   ├── sqlite_manager.h/.cpp# Sqlite 配置文件管理类
│   ├── logger.h/.cpp        # 日志管理（Logger/LogDbManager + LOG 宏）
│   ├── key_value_map.h/.cpp # 键值对配置类
│   ├── string_utils.h/.cpp  # 字符串工具（namespace string_utils）
│   ├── file_system.h/.cpp   # 文件目录操作（namespace file_system）
│   ├── file_encoding.h/.cpp # 文件编码检查（namespace file_encoding）
│   ├── timestamp.h/.cpp     # 高精度时间戳（namespace timestamp）
│   ├── bit_tools.h/.cpp     # 32 位位操作（namespace bit32_tools）
│   ├── progress_info.h      # 进度条信息结构体（纯头文件）
│   └── nameof.h             # 变量名/类型名获取 NAMEOF/TYPEOF（纯头文件）
└── third_party/             # —— 三方库（按库名分组）——
    ├── jsoncpp/             # JSON 解析（头文件+源码均在工程中按组显示）
    ├── spdlog/              # 日志库
    ├── sqlite3/             # SQLite
    ├── tinyxml2/            # XML 解析
    └── inicpp/              # INI 解析
```

## VS 项目结构（解决方案资源管理器）

- `src/` 下自研模块归入"头文件 / 源文件"（平铺显示，文件名即模块名）。
- `third_party/` 三方库按库名分组：`jsoncpp`（源文件+头文件）、`sqlite3`、`inicpp`、`tinyxml2`。
- 解决方案仅包含 CommonTools 一个项目。

## 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 文件名 | snake_case，模块一对 .h/.cpp | `config_custom.h`、`sqlite_manager.cpp` |
| 类/结构体 | PascalCase | `IniManager`、`ConfigCustom`、`ProgressInfo` |
| 类方法 | CamelCase，动词开头 | `WriteValue`、`ExecuteQuery`、`Set/Get` |
| 自由函数/命名空间 | snake_case | `string_utils::Split`、`file_system::Exists` |
| 私有成员变量 | 小写 + 尾下划线 `_` | `file_path_`、`last_error_`、`db_` |
| 公有结构体成员 | 小写，无下划线 | `ProgressInfo::percent_value`、`Logger::Config::log_name` |
| 枚举 | `enum class`，PascalCase | `LogName`、`ConfigDataType` |
| 类型别名 | PascalCase，含义明确 | `RowList`、`ParamsList` |
| 宏 | 全大写 + 下划线 | `CFG`、`LOG_MOUNT`、`COMMONTOOLS_API` |
| 头文件 | `#pragma once`，先含 `common_export.h`，自包含所需 include | — |
| 导出 | 对外声明统一 `COMMONTOOLS_API` | — |
| 文件编码 | **UTF-8 带 BOM**（MSVC 无 BOM 按 GBK 解码会吞行） | — |

### 关键约定

- **命名空间**：类在 `common_tools`；工具函数保留各自全局命名空间（string_utils 等）。
- **include**：模块间同目录直接引用（`src/` 平铺后如 `#include "sqlite_manager.h"`）；
  汇总头按 `#include "src/xxx.h"`；三方库经包含目录（`.\third_party\...`）解析。
- **不要依赖 `<windows.h>` 的 `min/max` 宏**，使用 `std::min/std::max`。
- 新增/删除文件时同步更新 `CommonTools.vcxproj` 与 `CommonTools.vcxproj.filters`。

## 本次规范化改名记录（无兼容层，使用方需同步修改）

| 旧名 | 新名 | 说明 |
|------|------|------|
| `string_utils::Repalce` | `string_utils::Replace` | 修正拼写 |
| `ProgressInfo::show_precent` | `ProgressInfo::show_percent` | 修正拼写 |
| `ProgressInfo::precent_value` | `ProgressInfo::percent_value` | 修正拼写 |
| `SqliteManager::UMapList` | `SqliteManager::RowList` | 含义明确化 |
| `KeyValueMap::set/get` | `KeyValueMap::Set/Get` | 统一类方法 CamelCase |
| `KeyValueMap::to_string/from_string` | `KeyValueMap::ToString/FromString` | 统一类方法 CamelCase |
| `using LoggerName = enum class LogName` | `enum class LogName` | 简化枚举声明（LogLevel/LogOutput 同） |


## 函数命名规范化（snake_case 统一，无兼容层）

自由函数全部统一为 snake_case（类方法保持 CamelCase）：

| 命名空间 | 旧名 | 新名 |
|----------|------|------|
| string_utils | Split / Merge / Format / Trim / TrimLeft / TrimRight / ToUpper / ToLower / Replace / ToString / ArrayToString | split / merge / format / trim / trim_left / trim_right / to_upper / to_lower / replace / to_string / array_to_string |
| string_utils | G2U / U2G | GBKToUTF8 / UTF8ToGBK（语义化） |
| file_system | Exists / IsFile / IsDirectory / GetFiles / GetFileSize / GetFileName / GetFilePath / GetFileExtensionName / GetFileCreateTime / GetFileModifiedTime / GetCurrentWorkDirectory / SetCurrentWorkDirectory / ReadAllText / WriteAllText | exists / is_file / is_directory / get_files / get_file_size / get_file_name / get_file_path / get_file_extension_name / get_file_create_time / get_file_modified_time / get_current_work_directory / set_current_work_directory / read_all_text / write_all_text |
| file_system | CreateFileX / CopyFileX / MoveFileX / DeleteFileX / RenameFile | create_file / copy_file / move_file / delete_file / rename_file（X 后缀消除：小写名不再与 Windows 宏冲突） |
| file_system | CreateDirectorys / DeleteDirectorys / GetDirectorys | create_directories / delete_directories / get_directories（修正复数） |
| bit32_tools | Set / Get | set / get |

> 注：timestamp（get_current_time_us 等）与 file_encoding（get/set）原本即 snake_case，未改动。
## 已删除项

- `std_utils.h`：`namespace std` 扩展（`std::clamp` 污染标准命名空间，C++17 已原生支持）。
- `sqlserver_manager.h`：空类（无任何成员，暂未实现）。
- `CommonTools.cpp`：原巨石实现文件（实现已全部拆入各模块 .cpp）。
- `third_party/json/`、`third_party/tinyxml/`：未被工程引用的旧版拷贝。
- `Demo`、`test2016` 工程目录（sln 中对应引用已同步移除）。

## 遗留问题（拆分前已存在，未改动）

- `LOG_DATA` 宏引用了未定义的 `LogName::DATA` 枚举值，使用时需修正为 `LogName::DB_DATA`。