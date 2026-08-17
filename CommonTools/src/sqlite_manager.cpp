#include "sqlite_manager.h"

#include "sqlite3.h"

#include <cstdio>
#include <algorithm>

namespace common_tools
{
	SqliteManager::SqliteManager() noexcept = default;

	SqliteManager::SqliteManager(const std::string& file_name)
	{
		Open(file_name);
	}

	SqliteManager::~SqliteManager()
	{
		Close();
	}

	SqliteManager::SqliteManager(SqliteManager&& other) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(other.mutex_);
		db_ = other.db_;
		last_error_ = std::move(other.last_error_);
		file_name_ = std::move(other.file_name_);
		stmt_cache_max_count_ = other.stmt_cache_max_count_;
		stmt_cache_ = std::move(other.stmt_cache_);

		other.db_ = nullptr;
		other.stmt_cache_.clear();
	}

	SqliteManager& SqliteManager::operator=(SqliteManager&& other) noexcept
	{
		if (this != &other)
		{
			std::lock_guard<std::recursive_mutex> lock_self(mutex_);
			std::lock_guard<std::recursive_mutex> lock_other(other.mutex_);

			Close();
			db_ = other.db_;
			last_error_ = std::move(other.last_error_);
			file_name_ = std::move(other.file_name_);
			stmt_cache_max_count_ = other.stmt_cache_max_count_;
			stmt_cache_ = std::move(other.stmt_cache_);

			other.db_ = nullptr;
			other.stmt_cache_.clear();
		}
		return *this;
	}

	void SqliteManager::SetMaxStmtCacheCount(size_t count) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		stmt_cache_max_count_ = count;
		// 超过新上限则清理缓存
		while (stmt_cache_.size() > stmt_cache_max_count_ && !stmt_cache_.empty())
		{
			auto it = stmt_cache_.begin();
			sqlite3_finalize(it->second);
			stmt_cache_.erase(it);
		}
	}

	bool SqliteManager::CheckConnection() noexcept
	{
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database is nullptr";
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(db_, "SELECT 1;", -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			if (stmt)
				sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		sqlite3_finalize(stmt); // 手动释放
		return true;
	}

	bool SqliteManager::TryReconnect() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (file_name_.empty())
		{
			last_error_ = "No database path for reconnect";
			return false;
		}

		if (db_)
		{
			Close();
		}

		// 重新打开
		int rc = sqlite3_open_v2(file_name_.c_str(), &db_,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_close(db_);
			db_ = nullptr;
			return false;
		}

		// 重连后清空缓存
		ClearStmtCache();
		return true;
	}

	bool SqliteManager::Open(const std::string& file_name)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		file_name_ = file_name;
		if (db_)
		{
			if (CheckConnection())
			{
				last_error_ = "Database already open";
				return false;
			}
			Close();
		}

		int rc = sqlite3_open_v2(file_name_.c_str(), &db_,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_close(db_);
			db_ = nullptr;
			return false;
		}

		return true;
	}

	void SqliteManager::Close()
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (db_)
		{
			ClearStmtCache();
			sqlite3_close(db_);
			db_ = nullptr;
		}
	}

	bool SqliteManager::IsOpen() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		bool connected = CheckConnection();
		return db_ != nullptr && connected == true;
	}

	sqlite3_stmt* SqliteManager::GetStmtCache(const std::string& sql)
	{
		last_error_.clear();
		auto it = stmt_cache_.find(sql);
		if (it != stmt_cache_.end() && it->second != nullptr)
		{
			sqlite3_reset(it->second); // 重置语句可复用
			return it->second;
		}
		return nullptr;
	}

	void SqliteManager::AddStmtCache(const std::string& sql, sqlite3_stmt* stmt)
	{
		last_error_.clear();
		if (!stmt)
		{
			last_error_ = "Database is nullptr";
			return;
		}

		// 超过最大缓存数，移除最早的
		while (stmt_cache_.size() >= stmt_cache_max_count_ && !stmt_cache_.empty())
		{
			auto it = stmt_cache_.begin();
			sqlite3_finalize(it->second);
			stmt_cache_.erase(it);
		}

		stmt_cache_[sql] = stmt;
	}

	void SqliteManager::ClearStmtCache()
	{
		stmt_cache_.clear();
		for (auto& pair : stmt_cache_)
		{
			sqlite3_finalize(pair.second);
		}
	}

	bool SqliteManager::BindParams(sqlite3_stmt* stmt, const ParamsList& params)
	{
		last_error_.clear();
		int idx = 1;
		for (const auto& param : params)
		{
			int rc = SQLITE_ERROR;
			switch (param.type)
			{
			case ParamType::Null:
				rc = sqlite3_bind_null(stmt, idx);
				break;
			case ParamType::Bool:
				rc = sqlite3_bind_int(stmt, idx, param.int_val);
				break;
			case ParamType::Int:
				rc = sqlite3_bind_int(stmt, idx, param.int_val);
				break;
			case ParamType::UInt:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::Int64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::UInt64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::String:
				rc = sqlite3_bind_text(stmt, idx, param.str_val.c_str(), static_cast<int>(param.str_val.size()),
				                       SQLITE_TRANSIENT);
				break;
			case ParamType::Double:
				rc = sqlite3_bind_double(stmt, idx, param.double_val);
				break;
			case ParamType::Blob:
				if (param.blob_val.empty())
				{
					last_error_ = "Empty blob data";
					return false;
				}
				rc = sqlite3_bind_blob(stmt, idx, param.blob_val.data(), static_cast<int>(param.blob_val.size()),
				                       SQLITE_TRANSIENT);
				break;
			}

			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
			idx++;
		}
		return true;
	}

	bool SqliteManager::ExecutePreparedQuery(sqlite3_stmt* stmt, RowList& result)
	{
		last_error_.clear();
		result.clear();
		int col_count = sqlite3_column_count(stmt);
		if (col_count == 0)
		{
			return true;
		}

		// 获取列名
		std::vector<std::string> col_names;
		for (int i = 0; i < col_count; ++i)
		{
			col_names.emplace_back(sqlite3_column_name(stmt, i));
		}

		// 遍历结果行
		int rc;
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			std::unordered_map<std::string, std::string> row;
			for (int i = 0; i < col_count; ++i)
			{
				if (sqlite3_column_type(stmt, i) == SQLITE_BLOB)
				{
					auto blob_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
					int blob_len = sqlite3_column_bytes(stmt, i);
					std::string hex_str;
					for (int j = 0; j < blob_len; ++j)
					{
						char buf[3];
						snprintf(buf, sizeof(buf), "%02X", blob_data[j]);
						hex_str += buf;
					}
					row[col_names[i]] = hex_str;
				}
				else
				{
					auto val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
					row[col_names[i]] = val ? val : "";
				}
			}
			result.push_back(std::move(row));
		}

		if (rc != SQLITE_DONE)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}
		return true;
	}

	bool SqliteManager::ExecuteNonQuery(const std::string& sql, const ParamsList& params)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		// 获取缓存语句
		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool ret = false;

		if (!is_cached)
		{
			// 预处理新语句
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			int rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE)
			{
				ret = true;
				last_error_.clear();
				if (!is_cached)
				{
					AddStmtCache(sql, stmt);
					stmt = nullptr; // 避免后续释放缓存语句
				}
			}
			else
			{
				last_error_ = sqlite3_errmsg(db_);
				// 缓存语句执行失败，移除缓存
				if (is_cached)
				{
					sqlite3_finalize(stmt);
					stmt = nullptr;
					stmt_cache_.erase(sql);
				}
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
			stmt = nullptr;
		}

		return ret;
	}

	bool SqliteManager::ExecuteBatchNonQuery(const std::string& sql, const BatchParamsList& params_list)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || params_list.empty())
		{
			last_error_ = "Database not open or empty batch params";
			return false;
		}

		if (!BeginTransaction())
		{
			return false;
		}

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool all_ok = true;

		if (!is_cached)
		{
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				RollbackTransaction();
				return false;
			}
		}

		for (const auto& params : params_list)
		{
			sqlite3_reset(stmt);
			if (!BindParams(stmt, params))
			{
				all_ok = false;
				break;
			}

			int rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
			{
				last_error_ = sqlite3_errmsg(db_);
				all_ok = false;
				break;
			}
		}

		if (all_ok)
		{
			CommitTransaction();
			if (!is_cached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}
		else
		{
			RollbackTransaction();
			if (is_cached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				stmt_cache_.erase(sql);
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
		}

		return all_ok;
	}

	bool SqliteManager::ExecuteQuery(const std::string& sql, const ParamsList& params, RowList& result)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool ret = false;

		if (!is_cached)
		{
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			ret = ExecutePreparedQuery(stmt, result);
			if (!ret && is_cached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				stmt_cache_.erase(sql); // 缓存语句执行失败，移除
			}

			if (ret && !is_cached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
		}

		return ret;
	}

	bool SqliteManager::ExecuteQueryPage(const std::string& sql, const ParamsList& params, int current_page,
	                                     int page_size, RowList& data, int& total_count, int& total_pages)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || current_page < 1 || page_size < 1)
		{
			last_error_ = "Invalid pagination parameters";
			return false;
		}

		data.clear();
		total_count = 0;
		total_pages = 0;

		// 查询总记录数
		std::string count_sql = "SELECT COUNT(*) AS total FROM (" + sql + ") AS t_count;";
		RowList count_result;
		if (!ExecuteQuery(count_sql, params, count_result))
		{
			return false;
		}

		if (!count_result.empty())
		{
			total_count = std::stoi(count_result[0]["total"]);
		}

		// 计算分页参数
		total_pages = (total_count + page_size - 1) / page_size;
		int offset = (current_page - 1) * page_size;

		// 查询当前页
		std::string page_sql = sql + " LIMIT ? OFFSET ?;";
		ParamsList all_params = params;
		all_params.emplace_back(page_size);
		all_params.emplace_back(offset);

		return ExecuteQuery(page_sql, all_params, data);
	}

	bool SqliteManager::ReadBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id,
	                                  size_t offset, size_t chunk_size, std::vector<char>& chunk_data)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		chunk_data.clear();
		sqlite3_blob* p_blob = nullptr;

		int rc = sqlite3_blob_open(db_, "main", table_name.c_str(), col_name.c_str(), row_id, 0, &p_blob);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}

		int blob_total_len = sqlite3_blob_bytes(p_blob);
		if (offset >= static_cast<size_t>(blob_total_len))
		{
			last_error_ = "Offset exceeds blob length";
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return true; // 偏移超限，返回空数据
		}

		// 计算实际读取长度
		size_t read_len = std::min(chunk_size, static_cast<size_t>(blob_total_len) - offset);
		chunk_data.resize(read_len);

		// 分块读取
		rc = sqlite3_blob_read(p_blob, chunk_data.data(), static_cast<int>(read_len), static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		sqlite3_blob_close(p_blob);
		p_blob = nullptr;
		return true;
	}

	bool SqliteManager::WriteBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id,
	                                   size_t offset, const std::vector<char>& chunk_data)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || chunk_data.empty())
		{
			last_error_ = "DB not open or empty chunk data";
			return false;
		}

		sqlite3_blob* p_blob = nullptr;

		int rc = sqlite3_blob_open(db_, "main", table_name.c_str(), col_name.c_str(), row_id, 1, &p_blob);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}

		// 检查偏移
		int blob_total_len = sqlite3_blob_bytes(p_blob);
		if (offset > static_cast<size_t>(blob_total_len))
		{
			last_error_ = "Offset exceeds BLOB length (write)";
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		// 分块写入
		rc = sqlite3_blob_write(p_blob, chunk_data.data(), static_cast<int>(chunk_data.size()),
		                        static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		sqlite3_blob_close(p_blob);
		p_blob = nullptr;
		return true;
	}

	bool SqliteManager::BeginTransaction()
	{
		return ExecuteNonQuery("BEGIN TRANSACTION;");
	}

	bool SqliteManager::CommitTransaction()
	{
		return ExecuteNonQuery("COMMIT;");
	}

	bool SqliteManager::RollbackTransaction()
	{
		return ExecuteNonQuery("ROLLBACK;");
	}

	int64_t SqliteManager::GetLastInsertId() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return db_ ? sqlite3_last_insert_rowid(db_) : 0;
	}

	std::string SqliteManager::GetLastErrorMsg() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return last_error_;
	}
}
