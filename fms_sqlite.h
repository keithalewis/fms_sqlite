// fms_sqlite.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif
#if _MSC_VER >= 1929
#include <format>
#endif
#include <iostream>
#include <map>
#include <string_view>
#include <stdexcept>
#include <vector>
#define SQLITE_ENABLE_NORMALIZE
#include "sqlite-amalgamation-3390400/sqlite3.h"
#include "fms_parse.h"

// call OP and throw on error
inline int sqlite_status;
#define FMS_SQLITE_OK(DB, OP) { sqlite_status = OP; if (SQLITE_OK != sqlite_status) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
// type, name
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

#define SQLITE_NAME(a,b) {a, b},
inline const std::map<int, const char*> sqlite_type_name = {
	SQLITE_TYPE_ENUM(SQLITE_NAME)
};
#undef SQLITE_NAME

#define SQLITE_TYPE(a,b) {b, a},
inline const std::map<std::string, int> sqlite_name_type = {
	SQLITE_TYPE_ENUM(SQLITE_TYPE)
};
#undef SQLITE_TYPE

// extended types
#define SQLITE_UNKNOWN 0
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3

// permitted types in CREATE TABLE
// SQL name, affinity, type, category
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
X("CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("VARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("VARYING CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NCHAR", "TEXT", SQLITE_TEXT, 2) \
X("NATIVE CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NVARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("TEXT", "TEXT", SQLITE_TEXT, 2) \
X("CLOB", "TEXT", SQLITE_TEXT, 2) \
X("BLOB", "BLOB", SQLITE_BLOB, 3) \
X("REAL", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE PRECISION", "REAL", SQLITE_FLOAT, 4) \
X("FLOAT", "REAL", SQLITE_FLOAT, 4) \
X("NUMERIC", "NUMERIC", SQLITE_FLOAT, 5) \
X("DECIMAL", "NUMERIC", SQLITE_FLOAT, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_BOOLEAN, 5) \
X("DATE", "NUMERIC", SQLITE_DATETIME, 5) \
X("DATETIME", "NUMERIC", SQLITE_DATETIME, 5) \

#define SQLITE_DECL(a,b,c,d) {a, c},
inline const std::map<std::string_view, int> sqlite_decltype_map = {
	SQLITE_DECLTYPE(SQLITE_DECL)
};
#undef SQLITE_DECL

inline int sqlite_type(std::string_view type)
{
	if (0 == type.size()) {
		return SQLITE_TEXT; // sqlite default type
	}

	// ignore size info not used by sqlite
	if (size_t pos = type.find('('); pos != std::string_view::npos) {
		type = type.substr(0, pos);
	}

	const auto t = sqlite_decltype_map.find(type);
	if (t == sqlite_decltype_map.end()) {
		throw std::runtime_error(__FUNCTION__ ": unknown SQL type");
	}

	return t->second;
}

namespace sqlite {

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

	// sqlite datetime
	struct datetime {
		union {
			double f;
			time_t i;
			const unsigned char* t;
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;

		datetime() noexcept
			: value{ .t = nullptr }, type{ SQLITE_UNKNOWN }
		{ }
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
		time_t as_time_t()
		{
			if (type == SQLITE_FLOAT) {
				value.i = 0; //!!! to_time_t(value.f);
				type = SQLITE_INTEGER;
			}
			else if (type == SQLITE_TEXT) {
				fms::view v((const char*)value.t);
				struct tm tm;
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
			}
			{
				datetime dt(0.);
				assert(dt.type == SQLITE_FLOAT);
				assert(dt.value.f == 0.);
			}
			{
				const char* t = "2023-4-5"; // not valid sqlite date
				datetime dt("2023-4-5");
				assert(dt.type == SQLITE_TEXT);
				assert(0 == strcmp((const char*)dt.value.t, t));
				dt.as_time_t();
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

	// move only sqlite string
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

	// open/close a sqlite3 database
	class db {
		sqlite3* pdb;
	public:
		// db("") for in-memory database
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

		// for use in sqlite C API
		operator sqlite3* ()
		{
			return pdb;
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(pdb);
		}

	};

	// Values of 0-based columns of statement.
	// It assumes the lifetime of pstmt.
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

		// very equal
		bool operator==(const values&) const = default;

		// for use in native sqlite3 C functions
		operator sqlite3_stmt* () noexcept
		{
			return pstmt;
		}

		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}

		//
		// names
		//

		const char* column_name(int j) const
		{
			return sqlite3_column_name(pstmt, j);
		}
		const void* column_name16(int j) const
		{
			return sqlite3_column_name16(pstmt, j);
		}

		//
		// types
		//

		// Fundamental SQLITE_* type.
		int column_type(int j) const
		{
			return sqlite3_column_type(pstmt, j);
		}

		// SQL string used in CREATE_TABLE
		const char* column_decltype(int j) const
		{
			const char* ctype = sqlite3_column_decltype(pstmt, j);

			return ctype ? ctype : "TEXT";
		}
		// Extended SQLITE_* type.
		int column_sqltype(int j) const
		{
			return ::sqlite_type(column_decltype(j));
		}

		// position of case-insensitive name in row
		int column_index(const char* name) const
		{
			for (int i = 0; i < column_count(); ++i) {
				if (0 == _stricmp(column_name(i), name)) {
					return i;
				}
			}

			return -1; // out-of-range lookup returns NULL
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

		const void* column_blob(int j)
		{
			return sqlite3_column_name(pstmt, j);
		}

		double column_double(int j) const
		{
			// cast from void*
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

		sqlite3_value* value(int i) const
		{
			return sqlite3_column_value(pstmt, i);
		}
	};

	// value of column at index i
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
		~value() // not virtual
		{ }

		bool operator==(const value&) const = default;

		// fundamental type
		int type() const
		{
			return vs.column_type(i);
		}
		// extended type
		int sqltype() const
		{
			return vs.column_sqltype(i);
		}

		// correspond to SQLITE_* types
		double as_float() const
		{
			return vs.column_double(i);
		}
		bool operator==(double d) const
		{
			return SQLITE_FLOAT == type() and d == as_float();
		}

		int as_int() const
		{
			return vs.column_int(i);
		}
		bool operator==(int j) const
		{
			return SQLITE_INTEGER == type() and j == as_int();
		}

		int64_t as_int64() const
		{
			return vs.column_int64(i);
		}
		bool operator==(int64_t j) const
		{
			return SQLITE_INTEGER == type() and j == as_int64();
		}

		std::string_view as_text() const
		{
			return std::string_view((const char*)vs.column_text(i), vs.column_bytes(i));
		}
		bool operator==(const std::string_view& v) const
		{
			return SQLITE_TEXT == type() and v == as_text();
		}
		// specialize == "str"
		bool operator==(const char* str) const
		{
			return operator==(std::string_view(str));
		}

		std::wstring_view as_text16() const
		{
			return std::wstring_view((wchar_t*)vs.column_text16(i), vs.column_bytes16(i));
		}
		bool operator==(const std::wstring_view& v) const
		{
			return SQLITE_TEXT == type() and v == as_text16();
		}
		bool operator==(const wchar_t* str) const
		{
			return operator==(std::wstring_view(str));
		}

		// Extended types
		bool as_boolean() const
		{
			return 0 != vs.column_int(i);
		}
		bool operator==(bool b) const
		{
			return SQLITE_BOOLEAN == sqltype() and b == as_boolean();
		}

		datetime as_datetime() const
		{
			switch (type()) {
			case SQLITE_FLOAT:
				return datetime(as_float());
			case SQLITE_INTEGER:
				return datetime(as_int64());
			case SQLITE_TEXT:
				// cast from unsigned char
				return datetime(as_text().data());
			}

			return datetime{};
		}
		bool operator==(const datetime& dt) const
		{
			return SQLITE_DATETIME == sqltype() and dt == as_datetime();
		}

		//friend std::ostream& operator<<(std::ostream&, const value&);
	};

	// Iterator over columns of a row.
	class iterator {
		values vs;
		int i;
		int e; // number of columns
	public:
		using iterator_category = std::output_iterator_tag;
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

	// RAII for sqlite3*
	class stmt : public values {
	public:
		stmt(sqlite3* pdb)
			: sqlite::values(nullptr)
		{
			// scare up a sqlite3_stmt*
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, "SELECT 1", -1, &pstmt, 0));
			reset();
		}
		// so ~stmt is called only once
		stmt(const stmt& _stmt) = delete;
		stmt& operator=(const stmt& _stmt) = delete;
		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
		}

		sqlite3* db_handle() const
		{
			return sqlite3_db_handle(pstmt);
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(db_handle());
		}

		// https://www.sqlite.org/c3ref/expanded_sql.html
		const char* sql() const
		{
			return sqlite3_sql(pstmt);
		}
		const char* expanded_sql() const
		{
			return sqlite3_expanded_sql(pstmt);
		}
		string normalized_sql() const
		{
			return string(sqlite3_normalized_sql(pstmt));

		}

		// Compile An SQL Statement: https://www.sqlite.org/c3ref/prepare.html
		// SQL As Understood By SQLite: https://www.sqlite.org/lang.html
		int prepare(const std::string_view& sql)
		{
			reset();
			sqlite3* pdb = db_handle();
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

		//
		// 1-based binding
		// 

		// https://www.sqlite.org/c3ref/bind_parameter_index.html
		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		// null
		stmt& bind(int i)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_null(pstmt, i));

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
		stmt& bind(int i, int64_t j)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}

		// text, make copy by default
		stmt& bind(int i, const std::string_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text(pstmt, i, str.data(),
				static_cast<int>(str.size()), cb));

			return *this;
		}
		stmt& bind(int i, const char* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::string_view(str), cb);
		}
		// text16 with length in characters
		stmt& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text16(pstmt, i, (const void*)str.data(),
				static_cast<int>(2 * str.size()), cb));

			return *this;
		}
		stmt& bind(int i, const wchar_t* str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			return bind(i, std::wstring_view(str), cb);
		}

		//
		// extended types
		//

		// boolean
		stmt& bind(int j, bool b)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_int(pstmt, j, b));

			return *this;
		}

		// underlying datetime type
		stmt& bind(int i, const datetime& dt)
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
					assert(SQLITE_FLOAT == stmt.column_type(1));
					assert(stmt.column_double(1) == 1.23);
					assert(SQLITE_TEXT == stmt.column_type(2));
					assert(0 == strcmp((const char*)stmt.column_text(2), "foo"));
					assert(SQLITE_TEXT == stmt.column_type(3));
					assert(SQLITE_DATETIME == stmt.column_sqltype(3));
					//assert(stmt.column_datetime(3) == dt);
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

	// Iteratable over rows of result set.
	// Use `explicit operator bool() const` to detect when done.
	// iterable it(stmt); while (it) { ... use *it ...; ++it; }
	class iterable {
		sqlite3_stmt* pstmt;
		int ret;
	public:
		using iterator_category = std::output_iterator_tag;
		using value_type = sqlite::iterator;

		iterable(sqlite3_stmt* pstmt)
			: pstmt(pstmt), ret{ SQLITE_ROW }
		{
			ret = SQLITE_ROW;
			operator++();
		}

		bool operator==(const iterable&) const = default;

		explicit operator bool() const noexcept
		{
			return SQLITE_DONE != ret;
		}
		value_type operator*() const noexcept
		{
			return sqlite::iterator(pstmt);
		}
		iterable& operator++()
		{
			if (SQLITE_ROW == ret) {
				ret = sqlite3_step(pstmt);
				if (!(SQLITE_ROW == ret or SQLITE_DONE == ret)) {
					sqlite3* pdb = sqlite3_db_handle(pstmt);
					throw std::runtime_error(sqlite3_errmsg(pdb));
				}
			}

			return *this;
		}
	};

	template<class O>
	inline O copy(sqlite::iterator i, O o)
	{
		std::copy(i.begin(), i.end(), o);

		return o;
	}
	template<class O>
	inline O copy(sqlite::iterable i, O o)
	{
		while (i) {
			sqlite::iterator _i = *i;
			copy(_i, o);
			++i;
		}

		return o;
	}

	inline std::string quote(const std::string_view& s, char l, char r = 0)
	{
		std::string t(s);

		r = r ? r : l;

		if (!s.starts_with(l)) {
			t.insert(t.begin(), l);
		}
		if (!s.ends_with(r)) {
			t.insert(t.end(), r);
		}

		return t;
	}
	// surround table name with `[name]`
	inline std::string table_name(const std::string_view& name)
	{
		return quote(name, '[', ']');
	}
	// surround variable name with `'name'`
	inline std::string variable_name(const std::string_view& var)
	{
		return quote(var, '\'');
	}

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