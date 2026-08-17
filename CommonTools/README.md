# CommonTools 更新记录（相对最初版本）

本文档仅记录相对最初版本（单文件巨石结构）的更新内容。

## 1. 工程结构重构

| 项目 | 最初 | 现在 |
|------|------|------|
| 源码组织 | `CommonTools.h`(1325行) + `CommonTools.cpp`(4552行) 单文件巨石 | 按模块拆分至 `src/` 平铺（每模块独立 .h/.cpp，snake_case 命名） |
| 三方库 | 平铺在项目根 | 统一收拢至 `third_party/`（jsoncpp/spdlog/sqlite3/tinyxml2/inicpp） |
| 汇总入口 | — | `CommonTools.h` 保留为汇总头，`#include "CommonTools.h"` 向后兼容 |
| 构建产物 | 散落 `bin/`、`obj/`、`x64/`、`Debug/`、`Release/` 多个目录 | **统一到 `build/` 一个目录**（`build/Debug`、`build/Release`、`build/obj`） |
| 平台 | Debug/Release × Win32/x64 4 个配置 | **仅保留 x64**（Win32 配置删除） |

## 2. 命名规范化（无兼容层，使用方需同步修改）

- **全部函数（含类方法）统一 snake_case**：自由函数 `string_utils::Split→split`、`file_system::Exists→exists` 等 35+ 个；类方法 `IniManager::WriteValue→write_value`、`SqliteManager::ExecuteQuery→execute_query`、`ConfigCustom::GetInstance→get_instance`、`Logger::Info→info`、`KeyValueMap::Set→set` 等 120+ 个（含 LOG/CFG 宏同步更新）
- **拼写修正**：`Repalce→Replace`、`show_precent→show_percent`、`precent_value→percent_value`、`CreateDirectorys→create_directories` 等
- **语义化**：`G2U→GBKToUTF8`、`U2G→UTF8ToGBK`、`UMapList→RowList`
- **风格统一**：`KeyValueMap::set/get→Set/Get`（类方法统一 CamelCase）、枚举简化 `using X = enum class X` → `enum class X`
- **全部函数补充 Doxygen 说明注释**

## 3. 代码修复

- `ConfigKey::GetType()` switch 缺 `break`（所有类型误判为 String）
- `LOG_DATA` 宏引用不存在的枚举 `LogName::DATA` → `LogName::DB_DATA`

## 4. 编译标准

- 显式 `/std:c++14` + `ConformanceMode=true`（`/permissive-` 严格模式），代码纯 C++14 兼容（无 C++17+ 特性）
- 警告级别提升至 **Level4**，Debug/Release 全量编译零警告（C4251 dll 接口误报已在公共头统一禁用）
- 构建环境：Visual Studio 2025+（v145）、Windows 10 SDK

## 5. 已删除项

- `std_utils.h`（`namespace std` 扩展污染，C++17 已原生 `std::clamp`）
- `sqlserver_manager.h`（空类）、`CommonTools.cpp`（巨石实现）
- `third_party/json/`、`third_party/tinyxml/`（未被引用的旧版拷贝）
- `Demo`、`test2016` 工程目录（sln 中引用同步移除）

## 遗留问题

- 硬编码路径（`d:/param/custom_settings/`、`d:/log`）为既有设计，换机器时需注意
- C4251 警告（导出类含 std 成员）为既有风格性警告，不影响功能
