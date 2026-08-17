#pragma once
// ============================================================================
// 公共基础设施：导出宏 / 通用返回值 / 前置声明
// 本文件不包含任何 std 头文件，各模块头文件自包含所需 include
// ============================================================================

// C4251：导出类成员为 std 类型（string/map/vector 等）时触发。
// MSVC 的 STL 已对 std 类型做了正确的 dll 导出处理，跨 DLL 使用无实际风险，
// 此警告对标准库类型属于误报，故统一禁用（仅影响本库及包含本头的使用方）。
#pragma warning(disable : 4251)

#ifdef _WINDLL
#ifdef COMMONTOOLS_DLL
#define COMMONTOOLS_API __declspec(dllexport)
#else
#define COMMONTOOLS_API __declspec(dllimport)
#endif // COMMONTOOLS_DLL
#else
#define COMMONTOOLS_API //_WINEXE
#endif // _WINDLL

// 通用的返回值枚举
enum ReturnCode
{
	RC_SUCCESS = 0, ///<成功
	RC_FAILED = 1, ///<失败
	RC_ERROR = 2, ///<错误
	RC_CANCELLED = 3, ///<已取消
	RC_ABORTED = 4, ///<已终止
	RC_TIMEOUT = 5, ///<已超时
	RC_BUSY = 6, ///<正在忙/占用中
	RC_NULL = 7, ///<空/空指针/...
};


// 前置声明
struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_blob;


namespace spdlog
{
	class logger;
}
