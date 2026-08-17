#pragma once
// ============================================================================
// 公共基础设施：导出宏 / 通用返回值 / 前置声明
// 本文件不包含任何 std 头文件，各模块头文件自包含所需 include
// ============================================================================

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
