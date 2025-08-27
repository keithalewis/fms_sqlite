// fms_sqlite.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#define SQLITE_ENABLE_NORMALIZE
#include "sqlite-amalgamation-3470200/sqlite3.h"
#include "fms_error.h"
#include "fms_parse.h"

// https://briandouglas.ie/sqlite-defaults/
#define SQLITE_DEFAULTS(X)       \
    X(journal_mode, WAL)         \
    X(synchronous,  NORMAL)      \
    X(busy_timeout, 5000)        \
    X(cache_size,  -20000)       \
    X(foreign_keys, ON)          \
    X(auto_vacuum,  INCREMENTAL) \
    X(temp_store,   MEMORY)      \
    X(mmap_size,    2147483648)  \
    X(page_size,    8192)        \

// call OP and throw on error
#define FMS_SQLITE_OK(DB, OP) { int __ret__ = OP; if (SQLITE_OK != __ret__) { \
		throw std::runtime_error(fms::error(sqlite3_errmsg(DB)).what()); } }

// TODO: evaluate to __ret__?
#define FMS_SQLITE_AOK(OP) if (int __ret__ = (OP); __ret__ != SQLITE_OK) { \
		throw std::runtime_error(fms::error(sqlite3_errstr(__ret__)).what()); }

// Fundamental SQLite type, SQL name, C type
#define SQLITE_TYPE_ENUM(X)  \
X(SQLITE_INTEGER, "INTEGER", sqlite3_int64)  \
X(SQLITE_FLOAT,   "FLOAT",	 double)         \
X(SQLITE_TEXT,    "TEXT",    char*)          \
X(SQLITE_BLOB,    "BLOB",    void*)          \
X(SQLITE_NULL,    "NULL",    void)           \

// Phony extended types
#define SQLITE_UNKNOWN   0
#define SQLITE_NUMERIC  -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN  -3

// SQL name, affinity, fundamental type, extended type
#define SQLITE_DECLTYPE(X) \
X("INTEGER",           "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("INT",               "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("TINYINT",           "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("SMALLINT",          "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("MEDIUMINT",         "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("BIGINT",            "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("UNSIGNED BIG INT",  "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("INT2",              "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("INT8",              "INTEGER", SQLITE_INTEGER, SQLITE_INTEGER)  \
X("TEXT",              "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("CHARACTER",         "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("VARCHAR",           "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("VARYING CHARACTER", "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("NCHAR",             "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("NATIVE CHARACTER",  "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("NVARCHAR",          "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("CLOB",              "TEXT",    SQLITE_TEXT,    SQLITE_TEXT)     \
X("BLOB",              "BLOB",    SQLITE_BLOB,    SQLITE_BLOB)     \
X("DOUBLE",            "REAL",    SQLITE_FLOAT,   SQLITE_FLOAT)    \
X("DOUBLE PRECISION",  "REAL",    SQLITE_FLOAT,   SQLITE_FLOAT)    \
X("REAL",              "REAL",    SQLITE_FLOAT,   SQLITE_FLOAT)    \
X("FLOAT",             "REAL",    SQLITE_FLOAT,   SQLITE_FLOAT)    \
X("NUMERIC",           "NUMERIC", SQLITE_NUMERIC, SQLITE_TEXT)     \
X("DECIMAL",           "NUMERIC", SQLITE_NUMERIC, SQLITE_TEXT)     \
X("BOOL",              "NUMERIC", SQLITE_NUMERIC, SQLITE_BOOLEAN)  \
X("BIT",               "NUMERIC", SQLITE_NUMERIC, SQLITE_BOOLEAN)  \
X("DATETIME",          "NUMERIC", SQLITE_NUMERIC, SQLITE_DATETIME) \
X("DATE",              "NUMERIC", SQLITE_NUMERIC, SQLITE_DATETIME) \

namespace sqlite {

	// Type string name to fundamental type.
	constexpr int sql_type(const std::string_view& sqlname)
	{
#define SQLITE_TYPE(a, b, c, d) if (sqlname.starts_with(a)) return c;
		SQLITE_DECLTYPE(SQLITE_TYPE);
#undef SQLITE_TYPE

		return SQLITE_TEXT;
	}
	// For use with https://sqlite.org/c3ref/column_decltype.html
	constexpr int sql_extended_type(std::string_view sqlname)
	{
#define SQLITE_TYPE(a, b, c, d) if (sqlname.starts_with(a)) return d;
		SQLITE_DECLTYPE(SQLITE_TYPE);
#undef SQLITE_TYPE

		return SQLITE_TEXT;
	}
	// Extended type to string name. 
	constexpr const std::string_view sql_name(int sqltype)
	{
#define SQLITE_NAME(a, b, c, d) if (sqltype == d) return a;
		SQLITE_DECLTYPE(SQLITE_NAME);
#undef SQLITE_NAME

		return "TEXT"; // sqlite default type
	}
#ifdef _DEBUG
	static_assert(sql_type("") == SQLITE_TEXT);
	static_assert(sql_type("INTEGER") == SQLITE_INTEGER);
	static_assert(sql_type("INT") == SQLITE_INTEGER);
	static_assert(sql_type("BOOLEAN") == SQLITE_NUMERIC);
	static_assert(sql_extended_type("BOOL") == SQLITE_BOOLEAN);
	static_assert(sql_extended_type("DATE") == SQLITE_DATETIME);
	static_assert(sql_name(SQLITE_INTEGER) == "INTEGER");
	static_assert(sql_name(SQLITE_DATETIME) == "DATETIME");
#endif // _DEBUG

	// https://sqlite.org/datatype3.html#determination_of_column_affinity
	constexpr int affinity(const std::string_view& decl)
	{
		if (decl.contains("INT")) {
			return SQLITE_INTEGER;
		}
		if (decl.contains("CHAR") or decl.contains("CLOB") or decl.contains("TEXT")) {
			return SQLITE_TEXT;
		}
		if (decl.contains("BLOB") or decl.size() == 0) {
			return SQLITE_BLOB;
		}
		if (decl.contains("REAL") or decl.contains("FLOA") or decl.contains("DOUB")) {
			return SQLITE_FLOAT;
		}

		return SQLITE_NUMERIC;
	}
#ifdef _DEBUG
#define TEST_SQLITE_AFFINITY(a, b, c, d) static_assert(affinity(a) == c);
	SQLITE_DECLTYPE(TEST_SQLITE_AFFINITY)
#undef TEST_SQLITE_AFFINITY
#endif // _DEBUG

	// left and right quotes
	constexpr std::string quote(const std::string_view& s, char l, char r)
	{
		std::string t(s);

		if (!s.starts_with(l)) t.insert(t.begin(), l);
		if (!s.ends_with(r))   t.insert(t.end(), r);

		return t;
	}
	// surround table name with [name]
	constexpr std::string table_name(const std::string_view& table)
	{
		return quote(table, '[', ']');
	}
	// surround variable name with 'var'
	constexpr std::string variable_name(const std::string_view& var)
	{
		return quote(var, '\'', '\'');
	}

	// sqlite datetime
	struct datetime {
		union {
			double f; // number of days since noon in Greenwich on November 24, 4714 B.C.
			time_t i; // Unix Time, the number of seconds since 1970-01-01 00:00:00 UTC.
			const unsigned char* t; // ISO8601 string ("YYYY-MM-DD HH:MM:SS.SSS").
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;

		datetime()
			: value{ .i = -1 }, type{ SQLITE_INTEGER }
		{
		}
		explicit datetime(double f) noexcept
			: value{ .f = f }, type{ SQLITE_FLOAT }
		{
		}
		explicit datetime(time_t i) noexcept
			: value{ .i = i }, type{ SQLITE_INTEGER }
		{
		}
		explicit datetime(const unsigned char* t)
			: value{ .t = t }, type{ SQLITE_TEXT }
		{
		}
		// datetime("2023-4-5")
		explicit datetime(const char* t)
			: value{ .t = (unsigned const char*)t }, type{ SQLITE_TEXT }
		{
		}

		// Same type and exact value.
		bool operator==(const datetime& dt) const
		{
			if (type != dt.type) {
				return false;
			}

			switch (type) {
			case SQLITE_FLOAT:
				return value.f == dt.value.f;
			case SQLITE_INTEGER:
				return value.i == dt.value.i;
			case SQLITE_TEXT:
				return fms::compare(value.t, dt.value.t) == 0;
			}

			return false;
		}
		// Convert to time_t
		time_t to_time_t()
		{
			if (type == SQLITE_FLOAT) {
				// 1970-01-01 00:00:00 is JD 2440587.5
				value.i = static_cast<time_t>((value.f - 2440587.5) * 24 * 60 * 60);
				type = SQLITE_INTEGER;
			}
			else if (type == SQLITE_TEXT) {
				fms::view<char> v((char*)value.t);
				struct tm tm;
				// Postel parse ISO 8601-ish strings found in the wild
				if (!fms::parse_tm(v, &tm)) {
					fms::error(v.string_view());
				}
				value.i = _mkgmtime64(&tm);
				type = SQLITE_INTEGER;
			}

			return value.i;
		}
#ifdef _DEBUG
		static int test()
		{
			{
				time_t t = 1;
				datetime dt(t);
				assert(dt.type == SQLITE_INTEGER);
				assert(dt.value.i == t);

				datetime dt2{ dt };
				assert(dt2 == dt);
				dt = dt2;
				assert(!(dt != dt2));
			}
			{
				datetime dt(2.);
				assert(dt.type == SQLITE_FLOAT);
				assert(dt.value.f == 2);
				assert(dt != datetime(time_t(2)));
			}
			{
				const unsigned char t[] = "2023-4-5"; // not a valid sqlite date
				datetime dt(t);
				assert(dt.type == SQLITE_TEXT);
				//assert(0 == strcmp(dt.value.t, t));
				dt.to_time_t();
				assert(dt.type == SQLITE_INTEGER);
#pragma warning(push)
#pragma warning(disable : 4996)
				tm* ptm = _gmtime64(&dt.value.i);
#pragma warning(pop)
				assert(ptm->tm_year == 2023 - 1900);
				assert(ptm->tm_mon == 4 - 1);
				assert(ptm->tm_mday == 5);
			}

			return 0;
		}
#endif // _DEBUG
	};

	// move-only sqlite string
	class string {
		const char* str;
	public:
		string()
			: str{ nullptr }
		{ }
		string(const char* str)
			: str{ str }
		{ }
		string(const string&) = delete;
		string& operator=(const string&) = delete;
		string(string&& _str) noexcept
			: str{ std::exchange(_str.str, nullptr) }
		{ }
		string& operator=(string&& _str) noexcept
		{
			if (this != &_str) {
				str = std::exchange(_str.str, nullptr);
			}

			return *this;
		}
		~string()
		{
			sqlite3_free((void*)str);
		}

		operator const char* () const
		{
			return str;
		}
	};

	// RAII class for sqlite3* database handle.
	class db {
		sqlite3* pdb;
	public:
		char* perrmsg; // might not be the same as sqlite3_errmsg()

		db()
			: pdb(nullptr)
		{
		}
		// db("") for in-memory database
		db(const char* filename = "", int flags = 0, const char* zVfs = nullptr)
			: pdb(nullptr)
		{
			open(filename, flags, zVfs);
		}
		// so ~db is called only once
		db(const db&) = delete;
		db& operator=(const db&) = delete;
		// https://sqlite.org/c3ref/close.html
		~db()
		{
			close();
		}

		// For use in the sqlite C API.
		operator sqlite3* ()
		{
			return pdb;
		}

		bool operator==(const db& db) const
		{
			return pdb == db.pdb;
		}

		// https://sqlite.org/c3ref/open.html
		db& open(const char* filename, int flags = 0, const char* zVfs = nullptr)
		{
			if (!flags) {
				flags = (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
			}
			if (!filename || !*filename) {
				flags |= SQLITE_OPEN_MEMORY;
			}

			FMS_SQLITE_AOK(sqlite3_open_v2(filename, &pdb, flags, zVfs));

			return *this;
		}
		// Default encoding will be UTF-16 in the native byte order.
		db& open(const wchar_t* filename)
		{
			FMS_SQLITE_AOK(sqlite3_open16(filename, &pdb));

			return *this;
		}
		db& close()
		{
			if (perrmsg) {
				sqlite3_free(perrmsg);
				perrmsg = nullptr;
			}
			sqlite3_close(pdb);
			pdb = nullptr;

			return *this;
		}
		// PRAGMA key = value;
		template<class T>
		int pragma(const std::string_view& key, const T& value)
		{
			return exec(std::format("PRAGMA {} = {};", key, value).c_str());
		}
		int default_pragmas()
		{
#define SQLITE_DEFAULT_PRAGMA(a, b) pragma(#a, #b);
			SQLITE_DEFAULTS(SQLITE_DEFAULT_PRAGMA)
#undef SQLITE_DEFAULT_PRAGMA
			return SQLITE_OK;
		}
		// Run zero or more UTF-8 encoded, semicolon-separate SQL statements.
		// https://sqlite.org/c3ref/exec.html
		using callback = int (*)(void* data, int col, char** text, char** name);
		int exec(const char* sql, callback cb = nullptr, void* data = nullptr)
		{
			if (perrmsg) {
				sqlite3_free(perrmsg);
				perrmsg = nullptr;
			}
			int ret = sqlite3_exec(pdb, sql, cb, data, &perrmsg);
			if (ret != SQLITE_OK) {
				const auto err = fms::error(perrmsg).at(sql, sqlite3_error_offset(pdb));
				throw std::runtime_error(err.what());
			}

			return ret;
		}

		// https://sqlite.org/c3ref/errcode.html
		int errcode() const
		{
			return sqlite3_errcode(pdb);
		}
		int extended_errcode() const
		{
			return sqlite3_extended_errcode(pdb);
		}
		const char* errmsg() const
		{
			return sqlite3_errmsg(pdb);
		}
		const void* errmsg16() const
		{
			return sqlite3_errmsg16(pdb);
		}
		static const char* errstr(int i)
		{
			return sqlite3_errstr(i);
		}
		int error_offset() const
		{
			return sqlite3_error_offset(pdb);
		}

	};

	/*
		bool operator==(const values&) const = default;


#ifdef _DEBUG
		static int test() {
			sqlite::db db("");

			auto exec = [&db](const char* sql) {
				return sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
				};
			auto prepare = [&db](sqlite3_stmt** pstmt, const char* sql) {
				return sqlite3_prepare(db, sql, -1, pstmt, 0);
				};

			try {
				{
					sqlite3_stmt* pstmt = nullptr;
					assert(SQLITE_OK == exec("DROP TABLE IF EXISTS t"));
					assert(SQLITE_OK == exec("CREATE TABLE t (a)"));
					assert(SQLITE_OK == prepare(&pstmt, "INSERT INTO t VALUES(?)"));
					values vs(pstmt);
					vs.bind(1); // null
					assert(SQLITE_DONE == sqlite3_step(vs));
					sqlite3_finalize(pstmt);

					assert(SQLITE_OK == prepare(&pstmt, "SELECT * FROM t"));
					assert(SQLITE_ROW == sqlite3_step(pstmt));
					vs = values(pstmt);
					//auto val = vs.value(0);
					assert(1 == sqlite3_column_count(pstmt));
					// assert(SQLITE_NULL == vs.type(0)); // null value???
					assert(SQLITE_DONE == sqlite3_step(pstmt));
					sqlite3_finalize(pstmt);
				}
				{
					sqlite3_stmt* pstmt = nullptr;
					assert(SQLITE_OK == exec("DROP TABLE IF EXISTS t"));
					assert(SQLITE_OK == exec("CREATE TABLE t (a REAL)"));
					assert(SQLITE_OK == prepare(&pstmt, "INSERT INTO t VALUES(?)"));
					values vs(pstmt);
					vs.bind(1, 1.23);
					assert(SQLITE_DONE == sqlite3_step(vs));
					sqlite3_finalize(pstmt);

					assert(SQLITE_OK == prepare(&pstmt, "SELECT * FROM t"));
					values vs2(pstmt);
					assert(SQLITE_ROW == sqlite3_step(pstmt));
					assert(1 == sqlite3_column_count(pstmt));
					int type = sqlite3_column_type(pstmt, 0);
					assert(type == vs2.column_type(0));
					assert(sqlite3_column_double(pstmt, 0) == 1.23);
					assert(vs2.column_double(0) == 1.23);
					assert(SQLITE_DONE == sqlite3_step(pstmt));
					sqlite3_finalize(pstmt);
				}
			}
			catch (const std::exception& ex) {
				std::cerr << ex.what() << '\n';
			}

			return 0;
		}
#endif // _DEBUG
	};
	*/
	/*
	// Forward iterator over columns of a row.
	class stmt_iterable {
		stmt vs;
		int i; // current index
		int e; // number of columns
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = value;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;
		using difference_type = std::ptrdiff_t;

		stmt_iterable(sqlite3_stmt* pstmt, int i = 0)
			: vs{ pstmt }, i{ i }, e{ vs.column_count() }
		{
		}
		stmt_iterable(const values& vs, int i = 0)
			: vs{ vs }, i{ i }, e{ vs.column_count() }
		{
		}
		stmt_iterable(const stmt_iterable&) = default;
		stmt_iterable& operator=(const stmt_iterable&) = default;
		~stmt_iterable()
		{
		}

		bool operator==(const stmt_iterable&) const = default;

		int sqltype() const
		{
			return vs.sqltype(i);
		}
		int size() const
		{
			return e;
		}

		// STL friendly
		stmt_iterable begin() const
		{
			return stmt_iterable(vs, 0);
		}
		stmt_iterable end() const
		{
			return stmt_iterable(vs, e);
		}

		// iterable
		explicit operator bool() const
		{
			return 0 <= i and i < e;
		}
		value_type operator*()
		{
			return value(vs, i);
		}
		stmt_iterable& operator++()
		{
			if (operator bool()) {
				++i;
			}

			return *this;
		}
		stmt_iterable operator++(int)
		{
			auto _this = *this;

			operator++();

			return _this;
		}

	};
	*/
	// RAII for sqlite3_stmt*
	class stmt {
		sqlite3_stmt* pstmt;
		const char* ptail;
		int ret;
	public:
		stmt()
			: pstmt{ nullptr }, ptail{ nullptr }
		{ }
		stmt(sqlite3* pdb, const std::string_view& sql)
			: pstmt{ nullptr }, ptail{ nullptr }
		{
			FMS_SQLITE_AOK(prepare(pdb, sql));
		}
		stmt(const stmt&) = delete;
		stmt(stmt&& _stmt) = delete;
		stmt& operator=(const stmt&) = delete;
		stmt& operator=(stmt&& _stmt) = delete;
		~stmt()
		{
			sqlite3_finalize(pstmt);
		}

		bool operator==(const stmt& stmt) const
		{
			return pstmt == stmt.pstmt;
		}

		int clone(sqlite3_stmt* p)
		{
			return SQLITE_OK;
		}

		const char* tail() const
		{
			return ptail;
		}
		int last() const
		{
			return ret;
		}

		// For use in native sqlite3 C functions.
		operator sqlite3_stmt* () const noexcept
		{
			return pstmt;
		}

		// https://www.sqlite.org/c3ref/expanded_sql.html
		const char* sql() const
		{
			return sqlite3_sql(pstmt);
		}
		const string expanded_sql() const
		{
			return sqlite3_expanded_sql(pstmt);
		}
		const string normalized_sql() const
		{
			return string(sqlite3_normalized_sql(pstmt));
		}

		// Return true if the prepared statement has been stepped at least once
		// but has not run to completion or been reset.
		// https://sqlite.org/c3ref/stmt_busy.html
		bool busy() const
		{
			return sqlite3_stmt_busy(pstmt) != 0;
		}

		// Compile a SQL statement: https://www.sqlite.org/c3ref/prepare.html
		// SQL As Understood By SQLite: https://www.sqlite.org/lang.html
		int prepare(sqlite3* pdb, const std::string_view& sql)
		{
			FMS_SQLITE_OK(pdb, sqlite3_finalize(pstmt));
			ret = sqlite3_prepare_v2(pdb, sql.data(), static_cast<int>(sql.size()), &pstmt, &ptail);
			if (ret != SQLITE_OK) {
				const auto err = fms::error(sqlite3_errmsg(pdb)).at(sql, sqlite3_error_offset(pdb));
				throw std::runtime_error(err.what());
			}

			return ret;
		}
		// int ret = stmt.step();  while (ret == SQLITE_ROW) { ...; ret = stmt.step()) { }
		// if (ret != SQLITE_DONE) then error
		// https://sqlite.org/c3ref/step.html
		int step()
		{
			ret = sqlite3_step(pstmt);

			if (ret != SQLITE_ROW and ret != SQLITE_DONE) {
				throw std::runtime_error(fms::error(sqlite3_errstr(ret)).what());
			}

			return ret;
		}
		// Reset a prepared statement but preserve bindings.
		// https://sqlite.org/c3ref/reset.html
		int reset()
		{
			return ret = sqlite3_reset(pstmt);
		}
		// Reset bindings to NULL.
		// https://www.sqlite.org/c3ref/clear_bindings.html
		int clear_bindings()
		{
			return ret = sqlite3_clear_bindings(pstmt);
		}

		// 0-based proxy for mediating internal SQLite bits to C++ types.
		class proxy {
			stmt& s;
			int i;
		public:
			proxy(stmt& s, int i)
				: s{ s }, i{ i }
			{ }
			proxy(const proxy&) = default;
			proxy& operator=(const proxy&) = default;
			~proxy() = default;

			bool operator==(const proxy& p) const
			{
				return &s == &p.s and i == p.i;
			}

			int type() const
			{
				return s.column_type(i);
			}

			stmt& operator=(double d)
			{
				return s.bind(i + 1, d);
			}
			double column_double() const
			{
				return s.column_double(i);
			}
			operator double() const
			{
				return column_double();
			}
			bool operator==(double d) const
			{
				return s.column_double(i) == d;
			}

			stmt& operator=(int j)
			{
				return s.bind(i + 1, j);
			}
			int column_int() const
			{
				return s.column_int(i);
			}
			bool operator==(int j) const
			{
				return s.column_int(i) == j;
			}
			operator int() const
			{
				return column_int();
			}

			stmt& operator=(sqlite_int64 j)
			{
				return s.bind(i + 1, j);
			}
			sqlite_int64 column_int64() const
			{
				return s.column_int64(i);
			}
			bool operator==(sqlite_int64 j) const
			{
				return s.column_int64(i) == j;
			}
			operator sqlite_int64() const
			{
				return column_int64();
			}

			stmt& operator=(const char* str)
			{
				return operator=(std::string_view(str));
			}
			stmt& operator=(const std::string_view& str)
			{
				return s.bind(i + 1, str);
			}
			const unsigned char* column_text() const
			{
				return s.column_text(i);
			}
			const std::string_view column_text_view() const
			{
				return s.column_text_view(i);
			}
			bool operator==(const char* str) const
			{
				return s.column_text_view(i) == str;
			}
			bool operator==(const std::string_view& str) const
			{
				return s.column_text_view(i) == str;
			}
			operator const std::string_view() const
			{
				return column_text_view();
			}

			stmt& operator=(const wchar_t* str)
			{
				return operator=(std::wstring_view(str));
			}
			stmt& operator=(const std::wstring_view& str)
			{
				return s.bind(i + 1, str);
			}
			const void* column_text16() const
			{
				return s.column_text16(i);
			}
			const std::wstring_view column_text16_view() const
			{
				return s.column_text16_view(i);
			}
			bool operator==(const wchar_t* str) const
			{
				return s.column_text16_view(i) == str;
			}
			bool operator==(const std::wstring_view& str) const
			{
				return s.column_text16_view(i) == str;
			}
			operator const std::wstring_view() const
			{
				return column_text16_view();
			}

			stmt& operator=(bool b)
			{
				return s.bind(i + 1, b);
			}
			bool column_boolean() const
			{
				return s.column_boolean(i);
			}
			bool operator==(bool b) const
			{
				return s.column_boolean(i) == b;
			}
			operator bool() const
			{
				return column_boolean();
			}

			stmt& operator=(const datetime& dt)
			{
				return s.bind(i + 1, dt);
			}
			datetime column_datetime() const
			{
				return s.column_datetime(i);
			}
			bool operator==(const datetime& dt) const
			{
				return s.column_datetime(i) == dt;
			}
			operator const datetime() const
			{
				return column_datetime();
			}
		};

		// 0-based column
		proxy operator[](int i)
		{
			return proxy(*this, i);
		}
		// name based column
		proxy operator[](const std::string_view& name)
		{
			int i = -1;

			if (0 == name.find_first_of(":@$")) {
				i = bind_parameter_index(name.data()) - 1;
			}
			else {
				i = column_index(name);
			}

			return proxy(*this, i);
		}

		// https://www.sqlite.org/c3ref/bind_parameter_index.html
		int bind_parameter_index(const std::string_view& name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name.data());
		}

		sqlite3* db_handle() const
		{
			return sqlite3_db_handle(pstmt);
		}

		//
		// 1-based binding with error checks
		//

		// https://www.sqlite.org/c3ref/bind_parameter_count.html
		int bind_parameter_count() const
		{
			return sqlite3_bind_parameter_count(pstmt);
		}

		// null
		stmt& bind(int i)
		{
			FMS_SQLITE_AOK(ret = sqlite3_bind_null(pstmt, i));

			return *this;
		}

		stmt& bind(int i, double d)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_double(pstmt, i, d));

			return *this;
		}

		// int 
		stmt& bind(int i, int j)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int(pstmt, i, j));

			return *this;
		}
		// int64
		stmt& bind(int i, sqlite_int64 j)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}

		// text, make copy by default
		// Use SQLITE_STATIC if str will live until sqlite3_step is called.
		stmt& bind(int i, const std::string_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text(pstmt, i,
				str.data(), static_cast<int>(str.size()), cb));

			return *this;
		}
		stmt& bind(int i, const char* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::string_view(str), cb);
		}
		// text16 with length in characters
		stmt& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text16(pstmt, i,
				(const void*)str.data(), static_cast<int>(2 * str.size()), cb));

			return *this;
		}
		stmt& bind(int i, const wchar_t* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::wstring_view(str), cb);
		}

		// Default to static.
		stmt& bind(int i, const void* data, size_t len, void(*cb)(void*) = SQLITE_STATIC)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_blob(pstmt, i,
				data, static_cast<int>(len), cb));

			return *this;
		}

		//
		// extended types
		//

		stmt& bind(int i, bool b)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int(pstmt, i, b));

			return *this;
		}

		stmt& bind(int i, const datetime& dt)
		{
			switch (dt.type) {
			case SQLITE_FLOAT:
				bind(i, dt.value.f);
				break;
			case SQLITE_INTEGER:
				bind(i, (sqlite3_int64)dt.value.i);
				break;
			case SQLITE_TEXT:
				// cast from unsigned char
				bind(i, (const char*)dt.value.t);
				break;
			}

			return *this;
		}

		// https://sqlite.org/c3ref/bind_parameter_index.html
		// Name must have the same form as specified in bind.
		// E.g., :name, @name, or $name 
		int bind_parameter_index(const char* name)
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		template<typename T>
		stmt& bind(const char* name, const T& t)
		{
			int i = bind_parameter_index(name);
			if (!i) {
				throw std::runtime_error(fms::error("unrecognized name").what());
			}

			return bind(i, t);
		}

		//
		// 0-based names
		//

		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}

		const char* column_name(int j) const
		{
			return sqlite3_column_name(pstmt, j);
		}
		int column_index(const std::string_view& name) const
		{
			for (int i = 0; i < column_count(); ++i) {
				if (name == column_name(i)) {
					return i;
				}
			}

			return -1; // out-of-range lookup returns NULL
		}
		const void* column_name16(int j) const
		{
			return sqlite3_column_name16(pstmt, j);
		}

		//
		// 0-based types
		//

		// Fundamental SQLITE_* type.
		int column_type(int j) const
		{
			return sqlite3_column_type(pstmt, j);
		}

		// SQL string used in CREATE_TABLE
		// https://sqlite.org/c3ref/column_decltype.html
		const char* column_decltype(int j) const
		{
			return sqlite3_column_decltype(pstmt, j);
		}
		// SQLITE_* type or fallback to fundamental type.
		int sql_type(int j) const
		{
			const char* type = column_decltype(j);

			return type ? sqlite::sql_type(column_decltype(j)) : column_type(j);
		}
		// Extended SQLITE_* type or fallback to fundamental type.
		int sql_extended_type(int j) const
		{
			const char* type = column_decltype(j);

			return type ? sqlite::sql_extended_type(column_decltype(j)) : column_type(j);
		}

		//
		// 0-based sqlite3_column_xxx wrappers.
		// https://sqlite.org/c3ref/column_blob.html
		//

		const void* column_blob(int j)
		{
			return sqlite3_column_blob(pstmt, j);
		}

		double column_double(int j) const
		{
			return sqlite3_column_double(pstmt, j);
		}

		int column_int(int j) const
		{
			return sqlite3_column_int(pstmt, j);
		}

		sqlite_int64 column_int64(int j) const
		{
			return sqlite3_column_int64(pstmt, j);
		}

		const unsigned char* column_text(int j) const
		{
			return sqlite3_column_text(pstmt, j);
		}
		const std::string_view column_text_view(int j) const
		{
			return std::string_view((const char*)sqlite3_column_text(pstmt, j), column_bytes(j));
		}
		const void* column_text16(int j) const
		{
			return sqlite3_column_text16(pstmt, j);
		}
		const std::wstring_view column_text16_view(int j) const
		{
			return std::wstring_view((const wchar_t*)sqlite3_column_text16(pstmt, j), column_bytes16(j) / 2);
		}

		sqlite3_value* value(int j) const
		{
			return sqlite3_column_value(pstmt, j);
		}

		// bytes, not chars
		int column_bytes(int j) const
		{
			return sqlite3_column_bytes(pstmt, j);
		}
		int column_bytes16(int j) const
		{
			return sqlite3_column_bytes16(pstmt, j);
		}

		// extended types
		bool column_boolean(int j) const
		{
			return column_int(j) != 0;
		}

		datetime column_datetime(int j) const
		{
			switch (column_type(j)) {
			case SQLITE_FLOAT:
				return datetime(column_double(j));
			case SQLITE_INTEGER:
				return datetime((time_t)column_int64(j));
			case SQLITE_TEXT:
				return datetime(column_text(j));
			}

			return datetime(time_t(-1));
		}

	};

	enum class transaction_mode {
		deferred,
		immediate,
		exclusive,
	};

	// Execute prepared statement as a transaction.
	// Consumes stmt, commit if succesfull, rollback otherwise.
	// https://sqlite.org/lang_transaction.html
	int transact(stmt& s, transaction_mode mode = transaction_mode::deferred)
	{
		stmt t;

		const char* trans;
		switch (mode) {
		case transaction_mode::immediate:
			trans = "BEGIN TRANSACTION IMMEDIATE;";
			break;
		case transaction_mode::exclusive:
			trans = "BEGIN TRANSACTION EXCLUSIVE;";
			break;
		case transaction_mode::deferred:
		default:
			trans = "BEGIN TRANSACTION DEFERRED;";
			break;
		}
		FMS_SQLITE_AOK(sqlite3_exec(s.db_handle(), trans, 0, 0, 0));
		
		try {
			while (SQLITE_ROW == s.step())
				;
		}
		catch (const std::exception& ex) {
			int ret = sqlite3_exec(s.db_handle(), "ROLLBACK TRANSACTION;", 0, 0, 0);
			if (SQLITE_OK != ret) {
				throw std::runtime_error(fms::error(ex.what()).what());
			}
		}
		FMS_SQLITE_AOK(sqlite3_exec(s.db_handle(), "COMMIT TRANSACTION;", 0, 0, 0));

		return s.last(); 
	}

} // sqlite
