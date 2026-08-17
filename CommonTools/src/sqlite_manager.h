#pragma once
// ============================================================================
// Sqlite配置文件管理类(SqliteManager)
// 封装 SQLite3：支持预处理语句缓存、参数化查询、分页查询、BLOB 分块读写、
// 事务控制与断线重连，线程安全（内部递归互斥锁）。
// ============================================================================
#include "common_export.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace common_tools
{
	/** @brief SQLite 数据库管理器 */
	class COMMONTOOLS_API SqliteManager
	{
	public:
		/** @brief 绑定参数类型枚举 */
		enum class ParamType
		{
			Null,
			Bool,
			Int,
			UInt,
			Int64,
			UInt64,
			String,
			Double,
			Blob,
		};


		/** @brief SQL 绑定参数（按构造函数自动推断类型，支持 bool/整型/浮点/字符串/BLOB） */
		struct SqliteParam
		{
			ParamType type = ParamType::Null;
			int int_val = 0;
			int64_t int64_val = 0;
			std::string str_val;
			double double_val = 0.0;
			std::vector<char> blob_val;

			/** @brief bool 参数 */
			SqliteParam(bool val)
				: type(ParamType::Bool),
				  int_val(val ? 1 : 0)
			{
			}

			/** @brief 32 位有符号整数参数 */
			SqliteParam(int32_t val)
				: type(ParamType::Int),
				  int_val(val)
			{
			}

			/** @brief 32 位无符号整数参数 */
			SqliteParam(uint32_t val)
				: type(ParamType::UInt),
				  int64_val(static_cast<int64_t>(val))
			{
			}

			/** @brief 64 位有符号整数参数 */
			SqliteParam(int64_t val)
				: type(ParamType::Int64),
				  int64_val(val)
			{
			}

			/** @brief 64 位无符号整数参数 */
			SqliteParam(uint64_t val)
				: type(ParamType::UInt64),
				  int64_val(static_cast<int64_t>(val))
			{
			}

			/** @brief 字符串参数 */
			SqliteParam(const std::string& val)
				: type(ParamType::String),
				  str_val(val)
			{
			}

			/** @brief 双精度浮点参数 */
			SqliteParam(double val)
				: type(ParamType::Double),
				  double_val(val)
			{
			}

			/** @brief C 字符串参数 */
			SqliteParam(const char* val)
				: type(ParamType::String),
				  str_val(val)
			{
			}

			/** @brief 指定长度的 BLOB 参数 */
			SqliteParam(const char* val, size_t len)
				: type(ParamType::Blob),
				  blob_val(val, val + len)
			{
			}

			/** @brief BLOB 参数（拷贝） */
			SqliteParam(const std::vector<char>& val)
				: type(ParamType::Blob),
				  blob_val(val)
			{
			}

			/** @brief BLOB 参数（移动） */
			SqliteParam(std::vector<char>&& val)
				: type(ParamType::Blob),
				  blob_val(std::move(val))
			{
			}
		};


		using ParamsList = std::vector<SqliteParam>; // 单条语句参数列表
		using BatchParamsList = std::vector<ParamsList>; // 批量语句参数列表
		using RowList = std::vector<std::unordered_map<std::string, std::string>>; // 查询结果行列表（列名→字符串值）

		/** @brief 默认构造函数（未打开数据库） */
		SqliteManager() noexcept;

		/** @brief 析构函数：关闭数据库并释放缓存 */
		~SqliteManager();

		/**
		 * @brief 构造并打开数据库
		 * @param [in] file_name - 数据库文件路径
		 **/
		explicit SqliteManager(const std::string& file_name);

		/** @brief 移动构造函数 */
		SqliteManager(SqliteManager&& other) noexcept;

		/** @brief 移动赋值运算符 */
		SqliteManager& operator=(SqliteManager&& other) noexcept;

		SqliteManager(const SqliteManager&) = delete;

		SqliteManager& operator=(const SqliteManager&) = delete;

		/**
		 * @brief 打开数据库（自动创建文件，线程安全）
		 * @param [in] file_name - 数据库文件路径
		 * @return 打开是否成功
		 **/
		bool open(const std::string& file_name);

		/** @brief 关闭数据库（释放语句缓存与句柄，线程安全） */
		void close();

		/**
		 * @brief 检查数据库连接是否可用（执行 SELECT 1 探测）
		 * @return 可用返回 true
		 **/
		bool is_open() noexcept;

		/**
		 * @brief 设置预处理语句缓存最大数量（超出时淘汰最旧语句）
		 * @param [in] count - 最大缓存条数
		 **/
		void set_max_stmt_cache_count(size_t count) noexcept;

		/**
		 * @brief 执行非查询 SQL（INSERT/UPDATE/DELETE 等，支持参数化）
		 * @param [in] sql - SQL 语句（参数用 ? 占位）
		 * @param [in] params - 绑定参数列表
		 * @return 执行是否成功
		 **/
		bool execute_non_query(const std::string& sql, const ParamsList& params = ParamsList());

		/**
		 * @brief 批量执行非查询 SQL（内部使用事务，全部成功才提交）
		 * @param [in] sql - SQL 语句
		 * @param [in] params_list - 多组绑定参数
		 * @return 全部执行成功返回 true
		 **/
		bool execute_batch_non_query(const std::string& sql, const BatchParamsList& params_list);

		/**
		 * @brief 执行查询 SQL 并返回结果集
		 * @param [in] sql - 查询语句
		 * @param [in] params - 绑定参数列表
		 * @param [out] result - 查询结果（BLOB 列以大写十六进制字符串返回）
		 * @return 查询是否成功
		 **/
		bool execute_query(const std::string& sql, const ParamsList& params, RowList& result);

		/**
		 * @brief 分页查询（自动统计总数与总页数）
		 * @param [in] sql - 查询语句（不含 LIMIT/OFFSET）
		 * @param [in] params - 绑定参数列表
		 * @param [in] current_page - 页码（从 1 开始）
		 * @param [in] page_size - 每页行数
		 * @param [out] data - 当前页数据
		 * @param [out] total_count - 总记录数
		 * @param [out] total_pages - 总页数
		 * @return 查询是否成功
		 **/
		bool execute_query_page(const std::string& sql, const ParamsList& params, int current_page, int page_size,
		                      RowList& data, int& total_count, int& total_pages);

		/**
		 * @brief 分块读取 BLOB 列数据（适合大字段流式读取）
		 * @param [in] table_name - 表名
		 * @param [in] col_name - 列名
		 * @param [in] row_id - 行 ID（rowid）
		 * @param [in] offset - 读取起始偏移
		 * @param [in] chunk_size - 本次读取长度
		 * @param [out] chunk_data - 读取到的数据（偏移超限时返回空）
		 * @return 读取是否成功
		 **/
		bool read_blob_chunk(const std::string& table_name, const std::string& col_name, int64_t row_id, size_t offset,
		                   size_t chunk_size, std::vector<char>& chunk_data);

		/**
		 * @brief 分块写入 BLOB 列数据（覆盖式写入）
		 * @param [in] table_name - 表名
		 * @param [in] col_name - 列名
		 * @param [in] row_id - 行 ID（rowid）
		 * @param [in] offset - 写入起始偏移
		 * @param [in] chunk_data - 待写入数据
		 * @return 写入是否成功
		 **/
		bool write_blob_chunk(const std::string& table_name, const std::string& col_name, int64_t row_id, size_t offset,
		                    const std::vector<char>& chunk_data);

		/** @brief 开启事务 @return 是否成功 */
		bool begin_transaction();

		/** @brief 提交事务 @return 是否成功 */
		bool commit_transaction();

		/** @brief 回滚事务 @return 是否成功 */
		bool rollback_transaction();

		/**
		 * @brief 获取最近一次插入的自增 ID
		 * @return 自增 ID（无连接时返回 0）
		 **/
		int64_t get_last_insert_id() const noexcept;

		/**
		 * @brief 获取最近一次操作的错误信息
		 * @return 错误描述（无错误时为空串）
		 **/
		std::string get_last_error_msg() const noexcept;

	private:
		/** @brief 检查连接有效性（SELECT 1 探测） */
		bool check_connection() noexcept;

		/** @brief 尝试重连数据库（按上次路径重新打开并清空缓存） */
		bool try_reconnect() noexcept;

		/** @brief 从缓存获取预处理语句（命中时重置复用） */
		sqlite3_stmt* get_stmt_cache(const std::string& sql);

		/** @brief 添加预处理语句到缓存（超出上限时淘汰最旧） */
		void add_stmt_cache(const std::string& sql, sqlite3_stmt* stmt);

		/** @brief 清空预处理语句缓存 */
		void clear_stmt_cache();

		/** @brief 将参数列表绑定到预处理语句 */
		bool bind_params(sqlite3_stmt* stmt, const ParamsList& params);

		/** @brief 执行已绑定的查询语句并填充结果集 */
		bool execute_prepared_query(sqlite3_stmt* stmt, RowList& result);

		sqlite3* db_ = nullptr; // 数据库句柄
		mutable std::recursive_mutex mutex_; // 线程安全锁
		std::string last_error_; // 错误信息
		std::string file_name_; // 数据库路径
		size_t stmt_cache_max_count_ = 50; // 最大缓存语句数
		std::unordered_map<std::string, sqlite3_stmt*> stmt_cache_; // 预处理语句缓存
	};
}
