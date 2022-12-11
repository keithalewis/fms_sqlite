// fms_sqlite.h
#pragma once
#include <map>
#include <span>
#include <string>
#include <stdexcept>
#include "sqlite-amalgamation-3390400/sqlite3.h"

#define FMS_SQLITE_OK(DB, OP) { int status = OP; if (SQLITE_OK != status) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

// phony types
#define SQLITE_UNKNOWN 0
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3

// type, affinity, enum, category
#define SQLITE_DECLTYPE(X) \
X("INT", "INTEGER", SQLITE_INTEGER, 1) \
X("INTEGER", "INTEGER", SQLITE_INTEGER, 1) \
X("TINYINT", "INTEGER", SQLITE_INTEGER, 1) \
X("SMALLINT", "INTEGER", SQLITE_INTEGER, 1) \
X("MEDIUMINT", "INTEGER", SQLITE_INTEGER, 1) \
X("BIGINT", "INTEGER", SQLITE_INTEGER, 1) \
X("UNSIGNED BIG INT", "INTEGER", SQLITE_INTEGER, 1) \
X("INT2", "INTEGER", SQLITE_INTEGER, 1) \
X("INT8", "INTEGER", SQLITE_INTEGER, 1) \
X("CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("VARCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("VARYING CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("NCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("NATIVE CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("NVARCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("TEXT", "TEXT", SQLITE_TEXT, 2) \
X("CLOB", "TEXT", SQLITE_TEXT, 2) \
X("BLOB", "BLOB", SQLITE_BLOB, 3) \
X("REAL", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE PRECISION", "REAL", SQLITE_FLOAT, 4) \
X("FLOAT", "REAL", SQLITE_FLOAT, 4) \
X("NUMERIC", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DECIMAL(?,?)", "NUMERIC", SQLITE_NUMERIC, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DATE", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DATETIME", "NUMERIC", SQLITE_NUMERIC, 5) \

namespace sqlite {

#define SQLITE_NAME(a,b) {a, b},
	inline const std::map<int, const char*> type_name = {
		SQLITE_TYPE_ENUM(SQLITE_NAME)
	};
#undef SQLITE_NAME

#define SQLITE_TYPE(a,b) {b, a},
	inline const std::map<std::string, int> type_value = {
		SQLITE_TYPE_ENUM(SQLITE_TYPE)
	};
#undef SQLITE_TYPE

	// declared type in CREATE TABLE
	inline int type(const char* str)
	{
		if (!str || !*str) {
			return SQLITE_TEXT;
		}
		switch (*str++) {
		case 'B': 
			return *str == 'O' ? SQLITE_BOOLEAN : SQLITE_BLOB;
		case 'C': case 'V': 
			return SQLITE_TEXT;
		case 'D': 
			return *str == 'A' ? SQLITE_DATETIME : *str == 'E' ? SQLITE_NUMERIC : SQLITE_FLOAT;
		case 'F': case 'R': 
			return SQLITE_FLOAT;
		case 'I': case 'M': case 'S': case 'U': 
			return SQLITE_INTEGER;
		case 'N': 
			return *str == 'U' ? SQLITE_NUMERIC : SQLITE_TEXT;
		case 'T': 
			return *str == 'I' ? SQLITE_INTEGER : SQLITE_TEXT;
		}

		return SQLITE_UNKNOWN;
	}

#ifdef _DEBUG
	inline int test_type()
	{
#define TYPE_TEST(a,b,c,d) if (c != sqlite::type(a)) return -1 - __COUNTER__;
		SQLITE_DECLTYPE(TYPE_TEST)
#undef TYPE_TEST
		return 0;
	}
#endif // _DEBUG

	// sqlite datetime
	struct datetime {
		union {
			double f;
			sqlite3_int64 i;
			const unsigned char* t;
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;
	};

	// open/close a sqlite3 database
	class db {
		sqlite3* pdb;
	public:
		db(const char* filename, int flags = 0)
			: pdb(nullptr)
		{
			if (!flags) {
				flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			}
			if (!filename || !*filename) {
				flags |= SQLITE_OPEN_MEMORY;
			}

			FMS_SQLITE_OK(pdb, sqlite3_open_v2(filename, &pdb, flags, nullptr));
		}
		db(const db&) = delete;
		db& operator=(const db&) = delete;
		~db()
		{
			sqlite3_close(pdb);
		}
		operator sqlite3* ()
		{
			return pdb;
		}
	};

	// https://www.sqlite.org/lang.html
	class stmt {
		sqlite3* pdb;
		sqlite3_stmt* pstmt;
		const char* ptail;
	public:
		stmt(sqlite3* pdb)
			: pdb(pdb), pstmt(nullptr), ptail(nullptr)
		{
			if (!pdb) {
				throw std::runtime_error(__FUNCTION__ ": database handle must not be null");
			}
		}
		stmt(const stmt&) = delete;
		stmt& operator=(const stmt&) = delete;
		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
			pstmt = nullptr;
		}

		operator sqlite3_stmt* ()
		{
			return pstmt;
		}

		sqlite3* db_handle() const
		{
			return pstmt ? sqlite3_db_handle(pstmt) : nullptr;
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(pdb);
		}

		const char* tail() const
		{
			return ptail;
		}

		const char* sql(bool expanded = false) const
		{
			return expanded ? sqlite3_expanded_sql(pstmt) : sqlite3_sql(pstmt);
		}

		stmt& prepare(const char* sql, int nsql = -1)
		{
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, sql, nsql, &pstmt, &ptail));

			return *this;
		}

		// reset a prepared statement
		stmt& reset()
		{
			FMS_SQLITE_OK(pdb, sqlite3_reset(pstmt));

			return *this;
		}

		// clear any existing bindings
		stmt& clear_bindings()
		{
			FMS_SQLITE_OK(pdb, sqlite3_clear_bindings(pstmt));

			return *this;
		}

		stmt& bind(int i, double d)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_double(pstmt, i, d));

			return *this;
		}

		// int
		stmt& bind(int i, int j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int(pstmt, i, j));

			return *this;
		}
		// int64
		stmt& bind(int i, int64_t j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}

		// null
		stmt& bind(int i)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_null(pstmt, i));

			return *this;
		}

		// text
		stmt& bind(int i, const char* str, int len = -1, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text(pstmt, i, str, len, cb));

			return *this;
		}
		stmt& bind(int i, const std::string_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, str.data(), static_cast<int>(str.length()), cb);
		}
		// text16 with length in characters
		stmt& bind(int i, const wchar_t* str, int len = -1, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text16(pstmt, i, str, 2*len, cb));

			return *this;
		}
		stmt& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, str.data(), static_cast<int>(str.length()), cb);
		}

		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		// int ret = step();  while (ret == SQLITE_ROW) { ...; ret = step()) { }
		// if !SQLITE_DONE then error
		int step()
		{
			return sqlite3_step(pstmt);
		}

		// number of columns returned
		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}
		// fundamental sqlite type
		int column_type(int i) const
		{
			return sqlite3_column_type(pstmt, i);
		}
		// bytes, not chars
		int column_bytes(int i) const
		{
			return sqlite3_column_bytes(pstmt, i);
		}
		int column_bytes16(int i) const
		{
			return sqlite3_column_bytes16(pstmt, i);
		}
		// internal representation
		sqlite3_value* column_value(int i)
		{
			return sqlite3_column_value(pstmt, i);
		}

		// 0-based
		const char* column_name(int i) const
		{
			return sqlite3_column_name(pstmt, i);
		}
		const wchar_t* column_name16(int i) const
		{
			return (const wchar_t*)sqlite3_column_name16(pstmt, i);
		}
		// type specified in CREATE TABLE
		const char* column_decltype(int i) const
		{
			return sqlite3_column_decltype(pstmt, i);
		}
		int type(int i) const
		{
			int t = sqlite::type(column_decltype(i));
			
			return SQLITE_UNKNOWN ? column_type(i) : t;
		}

		// 
		// Get column types and values.
		//
		bool is_null(int i) const
		{
			return SQLITE_NULL == column_type(i);
		}

		bool is_boolean(int i) const
		{
			return SQLITE_BOOLEAN == type(i);
		}
		bool columns_boolean(int i) const
		{
			return 0 != column_int(i);
		}

		bool is_int(int i) const
		{
			return SQLITE_INTEGER == type(i);
		}
		int column_int(int i) const
		{
			return sqlite3_column_int(pstmt, i);
		}
		sqlite3_int64 column_int64(int i) const
		{
			return sqlite3_column_int64(pstmt, i);
		}

		bool is_double(int i) const
		{
			return SQLITE_FLOAT == type(i);
		}
		double column_double(int i) const
		{
			return sqlite3_column_double(pstmt, i);
		}

		bool is_text(int i) const
		{
			return SQLITE_TEXT == type(i);
		}
		const unsigned char* column_text(int i) const
		{
			return sqlite3_column_text(pstmt, i);
		}
		const void* column_text16(int i) const
		{
			return sqlite3_column_text(pstmt, i);
		}

		bool is_blob(int i) const
		{
			return SQLITE_BLOB == type(i);
		}
		const void* column_blob(int i) const
		{
			return sqlite3_column_blob(pstmt, i);
		}

		bool is_datetime(int i) const
		{
			return SQLITE_DATETIME == type(i);
		}
		datetime column_datetime(int i) const
		{
			switch (column_type(i)) {
			case SQLITE_FLOAT:
				return datetime{ .value = {.f = column_double(i)}, .type = SQLITE_FLOAT};
			case SQLITE_INTEGER:
				return datetime{ .value = {.i = column_int64(i)}, .type = SQLITE_INTEGER };
			case SQLITE_TEXT:
				return datetime{ .value = {.t = column_text(i)}, .type = SQLITE_TEXT };
			}

			return datetime{.value = { .t = nullptr}, .type = SQLITE_UNKNOWN};
		}
	};

	struct cursor {
		enum class type {
			//_blob,
			_double,
			_int,
			//_int64,
			_null,
			_text,
			_text16
		};
		int column_count() const
		{
			return _column_count();
		}
		type column_type(int i) const
		{
			return _column_type(i);
		}
		bool done() const
		{
			return _done();
		}
		void step() 
		{
			return _step();
		}
		/*
		std::span<void*> as_blob(int i) const
		{
			return _as_blob(i);
		}
		*/
		double as_double(int i) const
		{
			return _as_double(i);
		}
		int as_int(int i) const
		{
			return _as_int(i);
		}
		std::string_view as_text(int i) const
		{
			return _as_text(i);
		}
		std::wstring_view as_text16(int i) const
		{
			return _as_text16(i);
		}
	private:
		virtual int _column_count() const = 0;
		virtual type _column_type(int i) const = 0;
		virtual bool _done() const = 0;
		virtual void _step() = 0;
		//virtual std::span<void*> _as_blob(int i) const = 0;
		virtual double _as_double(int i) const = 0;
		virtual int _as_int(int i) const = 0;
		virtual std::string_view _as_text(int i) const = 0;
		virtual std::wstring_view _as_text16(int i) const = 0;
	};

	inline void insert(sqlite3* db, const char* table, size_t len, cursor& cur)
	{
		sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
		
		std::string ii("INSERT INTO [");
		if (len <= 0) {
			len = strlen(table);
		}
		ii.append(table, len);
		ii.append("] VALUES (?1");
		char buf[8];
		for (int i = 2; i <= cur.column_count(); ++i) {
			ii.append(", ?");
			sprintf_s(buf, "%d", i);
			ii.append(buf);
		}
		ii.append(");");
		
		sqlite::stmt stmt(db);
		stmt.prepare(ii.c_str(), static_cast<int>(ii.length()));

		while (!cur.done()) {
			for (int i = 0; i < cur.column_count(); ++i) {
				switch (cur.column_type(i)) {
					/*
				case cursor::type::_blob:
					stmt.bind(i + 1, cur.as_blob(i));
					break;
					*/
				case cursor::type::_double:
					stmt.bind(i + 1, cur.as_double(i));
					break;
				case cursor::type::_int:
					stmt.bind(i + 1, cur.as_int(i));
					break;
				case cursor::type::_null:
					stmt.bind(i + 1);
					break;
				case cursor::type::_text:
					stmt.bind(i + 1, cur.as_text(i));
					break;
				case cursor::type::_text16:
					stmt.bind(i + 1, cur.as_text16(i));
					break;
				}
			}
			stmt.step();
			stmt.reset();
			cur.step();
		}

		sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, NULL);

	}

} // sqlite