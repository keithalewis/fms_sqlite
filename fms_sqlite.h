// fms_sqlite.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif
#include <cstdint>
#include <iostream>
#include <string_view>
#include <stdexcept>
#include <utility>
#define SQLITE_ENABLE_NORMALIZE
#include "sqlite-amalgamation-3390400/sqlite3.h"
#include "fms_parse.h"

// call OP and throw on error
#define FMS_SQLITE_OK(DB, OP) { int __ret__ = OP; if (SQLITE_OK != __ret__) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
// type, name
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

// extended types
#define SQLITE_UNKNOWN 0
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3

// known types in CREATE TABLE
// SQL name, affinity, type, category
#define SQLITE_DECLTYPE(X) \
X("INTEGER", "INTEGER", SQLITE_INTEGER, 1) \
X("INT", "INTEGER", SQLITE_INTEGER, 1) \
X("TINYINT", "INTEGER", SQLITE_INTEGER, 1) \
X("SMALLINT", "INTEGER", SQLITE_INTEGER, 1) \
X("MEDIUMINT", "INTEGER", SQLITE_INTEGER, 1) \
X("BIGINT", "INTEGER", SQLITE_INTEGER, 1) \
X("UNSIGNED BIG INT", "INTEGER", SQLITE_INTEGER, 1) \
X("INT2", "INTEGER", SQLITE_INTEGER, 1) \
X("INT8", "INTEGER", SQLITE_INTEGER, 1) \
X("TEXT", "TEXT", SQLITE_TEXT, 2) \
X("CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("VARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("VARYING CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NCHAR", "TEXT", SQLITE_TEXT, 2) \
X("NATIVE CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NVARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("CLOB", "TEXT", SQLITE_TEXT, 2) \
X("BLOB", "BLOB", SQLITE_BLOB, 3) \
X("DOUBLE", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE PRECISION", "REAL", SQLITE_FLOAT, 4) \
X("REAL", "REAL", SQLITE_FLOAT, 4) \
X("FLOAT", "REAL", SQLITE_FLOAT, 4) \
X("NUMERIC", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DECIMAL", "NUMERIC", SQLITE_NUMERIC, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_BOOLEAN, 5) \
X("DATETIME", "NUMERIC", SQLITE_DATETIME, 5) \
X("DATE", "NUMERIC", SQLITE_DATETIME, 5) \


namespace sqlite {

// SQL declared name to SQLITE_* correspondence
#define SQLITE_DECL(a,b,c,d) {a, c},
	inline const struct { const char* name; int type; } sqlite_name_type[] = {
		SQLITE_DECLTYPE(SQLITE_DECL)
	};
#undef SQLITE_DECL

	inline int sqltype(std::string_view sqlname)
	{
		if (0 == sqlname.size()) {
			return SQLITE_TEXT; // sqlite default type
		}
		for (const auto [name, type] : sqlite_name_type) {
			if (sqlname == name) {
				return type;
			}
		}

		return SQLITE_TEXT;
	}

	inline const char* sqlname(int sqltype)
	{
		if (0 == sqltype) {
			return "TEXT"; // sqlite default type
		}
		for (const auto [name, type] : sqlite_name_type) {
			if (sqltype == type) {
				return name;
			}
		}

		return "TEXT";
	}

	// left and right quotes
	inline std::string quote(const std::string_view& s, char l, char r = 0)
	{
		std::string t(s);

		r = r ? r : l;

		if (!s.starts_with(l)) t.insert(t.begin(), l);
		if (!s.ends_with(r))   t.insert(t.end(), r);

		return t;
	}
	// surround table name with [name]
	inline std::string table_name(const std::string_view& table)
	{
		return quote(table, '[', ']');
	}
	// surround variable name with 'var'
	inline std::string variable_name(const std::string_view& var)
	{
		return quote(var, '\'');
	}
	/*
	// https://sqlite.org/datatype3.html#determination_of_column_affinity
	inline int affinity(const std::string_view& decl)
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
	*/

	// sqlite datetime
	struct datetime {
		union {
			double f; // number of days since noon in Greenwich on November 24, 4714 B.C.
			time_t i; // Unix Time, the number of seconds since 1970-01-01 00:00:00 UTC.
			const unsigned char* t; // ISO8601 string ("YYYY-MM-DD HH:MM:SS.SSS").
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;

		explicit datetime(double f) noexcept
			: value{ .f = f }, type{ SQLITE_FLOAT }
		{ }
		explicit datetime(time_t i) noexcept
			: value{ .i = i }, type{ SQLITE_INTEGER }
		{ }
		explicit datetime(const char* t) noexcept
			: value{ .t = (const unsigned char*)t }, type{ SQLITE_TEXT }
		{ }

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
				return 0 == strcmp((const char*)value.t, (const char*)dt.value.t);
			}

			return false;
		}
		// canonicalize to time_t
		time_t to_time_t()
		{
			if (type == SQLITE_FLOAT) {
				// 1970-01-01 00:00:00 is JD 2440587.5
				value.i = static_cast<time_t>((value.f - 2440587.5) * 24 * 60 * 60);
				type = SQLITE_INTEGER;
			}
			else if (type == SQLITE_TEXT) {
				fms::view v((const char*)value.t);
				struct tm tm;
				// Postel parse ISO 8601-ish strings found in the wild
				if (!fms::parse_tm(v, &tm)) {
					throw std::runtime_error(__FUNCTION__ ": unable to parse date");
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
				const char* t = "2023-4-5"; // not a valid sqlite date
				datetime dt(t);
				assert(dt.type == SQLITE_TEXT);
				assert(0 == strcmp((const char*)dt.value.t, t));
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
		{
			*this = std::move(_str);
		}
		string& operator=(string&& _str) noexcept
		{
			if (this != &_str) {
				str = std::exchange(_str.str, nullptr);
			}

			return *this;
		}
		~string()
		{
			if (str) {
				sqlite3_free((void*)str);
			}
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
		// db("") for in-memory database
		// https://sqlite.org/c3ref/open.html
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
		// so ~db is called only once
		db(const db&) = delete;
		db& operator=(const db&) = delete;
		// https://sqlite.org/c3ref/close.html
		~db()
		{
			sqlite3_close(pdb);
		}

		// For use in the sqlite C API.
		operator sqlite3* ()
		{
			return pdb;
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

	// View of a sqlite statement.
	// Assumes lifetime of pstmt.
	class values {
	protected:
		sqlite3_stmt* pstmt;
	public:
		values()
			: pstmt{ nullptr }
		{ }
		values(sqlite3_stmt* pstmt)
			: pstmt{ pstmt }
		{ }
		values(const values&) = default;
		values& operator=(const values&) = default;
		~values() // not virtual
		{ }

		bool operator==(const values&) const = default;

		// For use in native sqlite3 C functions.
		operator sqlite3_stmt* () noexcept
		{
			return pstmt;
		}

		sqlite3* db_handle() const
		{
			return sqlite3_db_handle(pstmt);
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(db_handle());
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
		values& bind(int i)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_null(pstmt, i));

			return *this;
		}

		values& bind(int i, double d)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_double(pstmt, i, d));

			return *this;
		}

		// int 
		values& bind(int i, int j)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int(pstmt, i, j));

			return *this;
		}
		// int64
		values& bind(int i, int64_t j)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}

		// text, make copy by default
		// Use SQLITE_STATIC if str will live until sqlite3_step is called.
		values& bind(int i, const std::string_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text(pstmt, i, 
				str.data(), static_cast<int>(str.size()), cb));

			return *this;
		}
		values& bind(int i, const char* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::string_view(str), cb);
		}
		// text16 with length in characters
		values& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text16(pstmt, i, 
				(const void*)str.data(), static_cast<int>(2 * str.size()), cb));

			return *this;
		}
		values& bind(int i, const wchar_t* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::wstring_view(str), cb);
		}

		values& bind(int i, const std::basic_string_view<uint8_t>& b)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_blob(pstmt, i, 
				(const void*)b.data(), static_cast<int>(b.size()), nullptr));

			return *this;
		}

		//
		// extended types
		//

		values& bind(int i, bool b)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int(pstmt, i, b));

			return *this;
		}

		values& bind(int i, const datetime& dt)
		{
			switch (dt.type) {
			case SQLITE_FLOAT:
				bind(i, dt.value.f);
				break;
			case SQLITE_INTEGER:
				bind(i, dt.value.i);
				break;
			case SQLITE_TEXT:
				// cast from unsigned char
				bind(i, (const char*)dt.value.t);
				break;
			}

			return *this;
		}

		// https://sqlite.org/c3ref/bind_parameter_index.html
		// Name must have the same form as specified in prepare.
		// E.g., :name, @name, or $name 
		int bind_parameter_index(const char* name)
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		template<typename T>
		values& bind(const char* name, const T& t)
		{
			int i = bind_parameter_index(name);
			if (!i) {
				throw std::runtime_error(__FUNCTION__ ": unrecognized name");
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
		// Extended SQLITE_* type or fallback to fundamental type.
		int sqltype(int j) const
		{
			const char* type = column_decltype(j);

			return type ? sqlite::sqltype(column_decltype(j)) : column_type(j);
		}

		// position of name in row
		int column_index(const std::string_view& name) const
		{
			for (int i = 0; i < column_count(); ++i) {
				if (name == column_name(i)) {
					return i;
				}
			}

			return -1; // out-of-range lookup returns NULL
		}

		//
		// 0-based sqlite3_column_xxx wrappers.
		// https://sqlite.org/c3ref/column_blob.html
		//

		// bytes, not chars
		int column_bytes(int j) const
		{
			return sqlite3_column_bytes(pstmt, j);
		}
		int column_bytes16(int j) const
		{
			return sqlite3_column_bytes16(pstmt, j);
		}

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
		const void* column_text16(int j) const
		{
			return sqlite3_column_text16(pstmt, j);
		}

		sqlite3_value* value(int j) const
		{
			return sqlite3_column_value(pstmt, j);
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
				// cast from unsigned char
				return datetime((const char*)column_text(j));
			}

			return datetime(time_t(-1));
		}

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

	// Value of column at index i.
	// sqlite3_value is opaque
	class value {
	protected:
		values vs;
		int i;
	public:
		value(sqlite3_stmt* pstmt, int i)
			: vs{ pstmt }, i{ i }
		{ }
		value(const values& vs, int i)
			: vs{ vs }, i{ i }
		{ }
		value(const value&) = default;
		value& operator=(const value&) = default;
		~value()
		{ }

		bool operator==(const value&) const = default;

		// fundamental type
		int column_type() const
		{
			return vs.column_type(i);
		}
		// extended type
		int sqltype() const
		{
			return vs.sqltype(i);
		}
		// fundamental type
		const char* column_decltype() const
		{
			return vs.column_decltype(i);
		}

		const char* name() const
		{
			return vs.column_name(i);
		}

		//
		// Prettified access to corresponding raw SQLITE_* types
		//

		double as_float() const
		{
			return vs.column_double(i);
		}
		bool operator==(double d) const
		{
			return SQLITE_FLOAT == column_type() and d == as_float();
		}
		value operator=(double d)
		{
			return value(vs.bind(i + 1, d), i);
		}

		int as_int() const
		{
			return vs.column_int(i);
		}
		bool operator==(int j) const
		{
			return SQLITE_INTEGER == column_type() and j == as_int();
		}
		value operator=(int j)
		{
			return value(vs.bind(i + 1, j), i);
		}

		int64_t as_int64() const
		{
			return vs.column_int64(i);
		}
		bool operator==(int64_t j) const
		{
			return SQLITE_INTEGER == column_type() and j == as_int64();
		}
		value operator=(int64_t j)
		{
			return value(vs.bind(i + 1, j), i);
		}

		std::string_view as_text() const
		{
			return std::string_view((const char*)vs.column_text(i), vs.column_bytes(i));
		}
		bool operator==(const std::string_view& v) const
		{
			return SQLITE_TEXT == column_type() and v == as_text();
		}
		// specialize == "str"
		bool operator==(const char* str) const
		{
			return operator==(std::string_view(str));
		}
		value operator=(const std::string_view& str)
		{
			return value(vs.bind(i + 1, str), i);
		}
		value operator=(const char* str)
		{
			return value(vs.bind(i + 1, std::string_view(str)), i);
		}

		std::wstring_view as_text16() const
		{
			return std::wstring_view((wchar_t*)vs.column_text16(i), vs.column_bytes16(i));
		}
		bool operator==(const std::wstring_view& v) const
		{
			return SQLITE_TEXT == column_type() and v == as_text16();
		}
		// specialize == L"str"
		bool operator==(const wchar_t* str) const
		{
			return operator==(std::wstring_view(str));
		}
		value operator=(const std::wstring_view& str)
		{
			return value(vs.bind(i + 1, str), i);
		}
		value operator=(const wchar_t* str)
		{
			return value(vs.bind(i + 1, std::wstring_view(str)), i);
		}

		// Extended types
		bool as_boolean() const
		{
			return vs.column_boolean(i);
		}
		bool operator==(bool b) const
		{
			return SQLITE_BOOLEAN == sqltype() and b == as_boolean();
		}
		value operator=(bool b)
		{
			return value(vs.bind(i + 1, b), i);
		}

		datetime as_datetime() const
		{
			return vs.column_datetime(i);
		}
		bool operator==(const datetime& dt) const
		{
			return SQLITE_DATETIME == sqltype() and dt == as_datetime();
		}
		value operator=(const datetime& dt)
		{
			return value(vs.bind(i + 1, dt), i);
		}

		template<class V>
		// requires sqltype() and and as_xxx()
		value& operator=(const V& v)
		{
			// if constexpr (v.name()) { i = vs.column_index(v.name()); }
			switch (v.sqltype()) {
			case SQLITE_FLOAT:
				vs.bind(i, v.as_float());
				break;
			case SQLITE_INTEGER:
				vs.bind(i, v.as_int64());
				break;
			case SQLITE_BLOB:
				vs.bind(i, v.as_blob());
				break;
			case SQLITE_TEXT:
				vs.bind(i, v.as_text());
				break;
			case SQLITE_BOOLEAN:
				vs.bind(i, v.as_boolean());
				break;
			case SQLITE_DATETIME:
				vs.bind(i, v.as_datetime());
				break;
			default:
				std::runtime_error(__FUNCTION__ ": unknown type");
			}

			return *this;
		}
#ifdef _DEBUG
		static int test()
		{
			return 0;
		}
#endif // _DEBUG
	};

	// Forward iterator over columns of a row.
	class iterator {
		values vs;
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

		iterator(sqlite3_stmt* pstmt, int i = 0)
			: vs{ pstmt }, i{ i }, e{ vs.column_count() }
		{ }
		iterator(const values& vs, int i = 0)
			: vs{ vs }, i{ i }, e{ vs.column_count() }
		{ }
		iterator(const iterator&) = default;
		iterator& operator=(const iterator&) = default;
		~iterator()
		{ }

		bool operator==(const iterator&) const = default;

		int sqltype() const
		{
			return vs.sqltype(i);
		}
		int size() const
		{
			return e;
		}

		// STL friendly
		iterator begin() const
		{
			return iterator(vs, 0);
		}
		iterator end() const
		{
			return iterator(vs, e);
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
		iterator& operator++()
		{
			if (operator bool()) {
				++i;
			}

			return *this;
		}
		iterator operator++(int)
		{
			auto _this = *this;

			operator++();

			return _this;
		}

	};

	// RAII for sqlite3_stmt*
	class stmt : public values {
		sqlite3* pdb;
	public:
		stmt(sqlite3* pdb)
			: sqlite::values(nullptr), pdb{pdb}
		{ }
		// so ~stmt is called only once
		stmt(const stmt& _stmt) = delete;
		stmt(stmt&& _stmt) noexcept
		{
			pdb = std::exchange(_stmt.pdb, nullptr);
			values::pstmt = std::exchange(_stmt.pstmt, nullptr);
		}
		stmt& operator=(const stmt& _stmt) = delete;
		~stmt()
		{
			sqlite3_finalize(pstmt);
		}

		// https://www.sqlite.org/c3ref/expanded_sql.html
		const char* sql() const
		{
			return sqlite3_sql(pstmt);
		}
		string expanded_sql() const
		{
			return sqlite3_expanded_sql(pstmt);
		}
		const char* normalized_sql() const
		{
			return string(sqlite3_normalized_sql(pstmt));

		}

		// Compile An SQL Statement: https://www.sqlite.org/c3ref/prepare.html
		// SQL As Understood By SQLite: https://www.sqlite.org/lang.html
		int prepare(const std::string_view& sql)
		{
			pstmt && sqlite3_finalize(pstmt);
			int ret = sqlite3_prepare_v2(pdb, sql.data(), (int)sql.size(), &pstmt, 0);
			FMS_SQLITE_OK(pdb, ret);

			return ret;
		}

		// int ret = stmt.step();  while (ret == SQLITE_ROW) { ...; ret = stmt.step()) { }
		// if (ret != SQLITE_DONE) then error
		// https://sqlite.org/c3ref/step.html
		int step()
		{
			int ret = sqlite3_step(pstmt);

			if (ret != SQLITE_ROW and ret != SQLITE_DONE) {
				FMS_SQLITE_OK(db_handle(), ret);
			}

			return ret;
		}

		// execute a sql statement
		int exec(const std::string_view& sql)
		{
			int ret = SQLITE_OK;

			ret = prepare(sql);
			FMS_SQLITE_OK(pdb, ret);
			ret = step();
			if (SQLITE_DONE != ret) {
				FMS_SQLITE_OK(pdb, ret);
			}

			return ret;
		}

		// Reset a prepared statement but preserve bindings.
		// https://sqlite.org/c3ref/reset.html
		int reset()
		{
			return sqlite3_reset(pstmt);
		}

		// Reset bindings to NULL.
		// https://www.sqlite.org/c3ref/clear_bindings.html
		int clear_bindings()
		{
			return sqlite3_clear_bindings(pstmt);
		}

		sqlite::value operator[](int i) const
		{
			return sqlite::value(pstmt, i);
		}
		sqlite::value operator[](const char* name) const
		{
			int i = 0;

			if (!name) {
				i = -1;
			}
			else if (0 != strchr(":@$", *name)) {
				i = bind_parameter_index(name) - 1;
			}
			else {
				i = column_index(name);
			}

			return sqlite::value(pstmt, i);
		}

		// https://www.sqlite.org/c3ref/bind_parameter_index.html
		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

#ifdef _DEBUG
		static int test()
		{
			int ret = SQLITE_OK;
			//const char* s = nullptr;

			try {
				db db("a.db");
				{
					sqlite3_stmt* stmt;
					sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt, 0);
					sqlite3* pdb = sqlite3_db_handle(stmt);
					sqlite3_finalize(stmt);
					assert(pdb);
				}
				stmt stmt(db);

				std::string_view sql("DROP TABLE IF EXISTS a");
				ret = stmt.prepare(sql);
				stmt.step();
				stmt.reset();

				sql = "CREATE TABLE a (b INT, c REAL, d TEXT, e DATETIME)";
				ret = stmt.prepare(sql);
				assert(sql == stmt.sql());
				assert(SQLITE_DONE == stmt.step());
				assert(0 == stmt.column_count());

				stmt.reset();
				ret = stmt.prepare("INSERT INTO a VALUES (123, 1.23, 'foo', "
					"'2023-04-05')"); // datetime must be YYYY-MM-DD HH:MM:SS.SSS
				ret = stmt.step();
				assert(SQLITE_DONE == ret);

				stmt.reset();
				stmt.prepare("SELECT * FROM a");
				int rows = 0;

				while (SQLITE_ROW == stmt.step()) {
					assert(4 == stmt.column_count());
					assert(SQLITE_INTEGER == stmt.column_type(0));
					assert(stmt.column_int(0) == 123);
					assert(stmt[0] == 123);
					assert(stmt["b"] == 123);
					assert(SQLITE_FLOAT == stmt.column_type(1));
					assert(stmt.column_double(1) == 1.23);
					assert(stmt[1] == 1.23);
					assert(stmt["c"] == 1.23);
					assert(SQLITE_TEXT == stmt.column_type(2));
					assert(0 == strcmp((const char*)stmt.column_text(2), "foo"));
					assert(SQLITE_TEXT == stmt.column_type(3));
					assert(SQLITE_DATETIME == stmt.sqltype(3));
					assert(stmt.column_datetime(3) == datetime("2023-04-05"));
					++rows;
				}
				assert(1 == rows);

				int b = 2;
				double c = 2.34;
				char d[2] = { 'a', 0 };

				stmt.reset();
				stmt.prepare("SELECT unixepoch(e) from a");
				stmt.step();
				time_t e = stmt.column_int64(0);

				// fix up column e to time_t
				// DATEIME column must be homogeneous
				stmt.reset();
				stmt.prepare("UPDATE a SET e = ?");
				stmt.bind(1, e);
				stmt.step();

				stmt.reset();
				ret = stmt.prepare("INSERT INTO a VALUES (?, ?, ?, ?)");
				for (int j = 0; j < 3; ++j) {
					stmt.reset();
					// 1-based
					stmt.bind(1, b);
					stmt.bind(2, c);
					stmt.bind(3, d);
					stmt.bind(4, datetime(e));
					stmt.step();

					++b;
					c += 0.01;
					d[0] = d[0] + 1;
					e += 86400;
				}

				stmt.reset();
				stmt.prepare("select count(*) from a");
				assert(SQLITE_ROW == stmt.step());
				assert(1 == stmt.column_count());
				assert(stmt.column_int(0) == 4);
				assert(SQLITE_DONE == stmt.step());
			}
			catch (const std::exception& ex) {
				puts(ex.what());
			}

			return 0;
		}
#endif // _DEBUG
	};

	// Cursor over rows of result set.
	// Use `explicit operator bool() const` to detect when done.
	// cursor c(stmt); while (c) { ... use *c ...; ++c; }
	class cursor {
		sqlite3_stmt* pstmt;
		int ret;
	public:
		using iterator_category = std::output_iterator_tag;
		using value_type = sqlite::iterator;

		cursor(sqlite3_stmt* pstmt)
			: pstmt(pstmt), ret{ SQLITE_ROW }
		{
			operator++();
		}
		cursor(const cursor&) = default;
		cursor& operator=(const cursor&) = default;
		~cursor()
		{ }

		bool operator==(const cursor&) const = default;

		explicit operator bool() const noexcept
		{
			return SQLITE_DONE != ret;
		}
		value_type operator*() const noexcept
		{
			return sqlite::iterator(pstmt);
		}
		cursor& operator++()
		{
			if (SQLITE_ROW == ret) {
				ret = sqlite3_step(pstmt);
				if (!(SQLITE_ROW == ret or SQLITE_DONE == ret)) {
					throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(pstmt)));
				}
			}

			return *this;
		}
	};

	// copy a single row
	template<class O>
	inline O copy(sqlite::iterator i, O o)
	{
		std::copy(i.begin(), i.end(), o);

		return o;
	}
	// copy all rows
	template<class O>
	inline O copy(sqlite::stmt& stmt, O o)
	{
		sqlite::cursor i(stmt);
		while (i) {
			sqlite::iterator _i = *i;
			copy(_i, o);
			++i;
		}

		return o;
	}
	template<class I>
	inline sqlite::iterator copy(I i, sqlite::iterator o)
	{
		std::copy(i.begin(), i.end(), o);

		return o;
	}

	template<class O, class F>
	inline O map(sqlite::iterator i, O o, F f)
	{
		std::transform(i.begin(), i.end(), o, f);

		return o;
	}
	template<class O, class F>
	inline O map(sqlite::stmt& stmt, O o, F f)
	{
		sqlite::cursor i(stmt);
		while (i) {
			sqlite::iterator _i = *i;
			map(_i, o, f);
			++i;
		}

		return o;
	}

	/*
	// Wrap sql in a transaction.
	inline void transaction(sqlite3* pdb, void(*op)())
	{
		sqlite3* pdb = sqlite3_db_handle(pstmt);
		sqlite::stmt stmt(pdb);
		stmt.prepare("BEGIN TRANSACTION");
		stmt.step();

		try {
			op();
		}
		catch (const std::exception&) {
			stmt.reset();
			stmt.prepare("ROLLBACK TRANSACTION");
			stmt.step();
		}

		stmt.reset();
		stmt.prepare("COMMIT TRANSACTION");
		// check for SQLITE_BUSY and retry???
		stmt.step();
	}
	*/

} // sqlite

namespace std {

	inline std::ostream& operator<<(std::ostream& o, const sqlite::value& v)
	{
		switch (v.sqltype()) {
		case SQLITE_INTEGER:
			o << v.as_int64();
			break;
		case SQLITE_FLOAT:
			o << v.as_float();
			break;
		case SQLITE_TEXT:
			o << v.as_text();
			break;
			//case SQLITE_BLOB:
		case SQLITE_BOOLEAN:
			o << v.as_boolean();
			break;
		case SQLITE_DATETIME:
			o << v.as_datetime().value.t;
			break;
		case SQLITE_NULL:
			o << "{}";
			break;
		default:
			o << "<unknown>";
		}

		return o;
	}

} // namespace std
