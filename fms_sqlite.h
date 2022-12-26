// fms_sqlite.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif
#if _MSC_VER >= 1929
#include <format>
#endif
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

	// https://sqlite.org/datatype3.html#determination_of_column_affinity"
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
			: value{ .t = nullptr }, type{ SQLITE_UNKNOWN}
		{ }
		explicit datetime(double f) noexcept
			: value{ .f = f }, type{ SQLITE_FLOAT}
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
			: str{nullptr}
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

	// Value of i-th column of statement.
	// Not a value type. It assumes the lifetime of pstmt.
	class value {
		// static null ??? return using index out of bounds?
		// use scratch db 
		/*
			template<class T>
			sqlite3_value* make_value(const T& t)
			{
				stmt s(tmp_db);
				s.prepare("SELECT ? FROM tmp");
				s.bind(1, t);
				s.step();
				return sqlite3_column_value(s, 0);
			}
			// ??? life times ???
		*/
		sqlite3_stmt* pstmt;
		int i;
	public:
		value()
			: pstmt{ nullptr }, i{ -1 }
		{ }
		value(sqlite3_stmt* pstmt, int i)
			: pstmt{ pstmt }, i{ i }
		{
			if (!pstmt) {
				throw std::runtime_error(__FUNCTION__ ": null statement");
			}
		}
		value(const value&) = default;
		value& operator=(const value&) = default;
		~value()
		{ }

		operator sqlite3_value* () const
		{
			return sqlite3_column_value(pstmt, i);
		}

		// very equal
		bool operator==(const value&) const = default;
		
		// Fundamental sqlite type.
		int type() const
		{
			return sqlite3_column_type(pstmt, i);
		}
		const char* column_decltype() const
		{
			return sqlite3_column_decltype(pstmt, i);
		}
		// Extended type.
		int sqltype() const
		{
			return sqlite_type(column_decltype());
		}

		// bytes, not chars
		int column_bytes(int j) const
		{
			return sqlite3_column_bytes(pstmt, j);
		}
		int bytes() const
		{
			return column_bytes(i);
		}
		int column_bytes16(int j) const
		{
			return sqlite3_column_bytes16(pstmt, j);
		}
		int bytes16() const
		{
			return column_bytes16(i);
		}
		const char* column_name(int j) const
		{
			return sqlite3_column_name(pstmt, j);
		}
		const char* name() const
		{
			return column_name(i);
		}
		const wchar_t* column_name16(int j) const
		{
			// cast from void*
			return (const wchar_t*)sqlite3_column_name16(pstmt, j);
		}
		const wchar_t* name16() const
		{
			return column_name16(i);
		}
		// Contents of column i as text.
		const unsigned char* column_text(int j) const
		{
			return sqlite3_column_text(pstmt, j);
		}
		const unsigned char* text() const
		{
			return column_text(i);
		}
		const void* column_text16(int j) const
		{
			return sqlite3_column_text16(pstmt, j);
		}
		const void* text16() const
		{
			return column_text16(i);
		}

		// convert to T
		template<class T> T as() const;

		template<>
		int as() const
		{
			return sqlite3_column_int(pstmt, i);
		}
		operator int() const
		{
			return as<int>();
		}
		bool operator==(int d) const
		{
			return SQLITE_INTEGER == type() and d == as<int>();
		}

		template<>
		int64_t as() const
		{
			return sqlite3_column_int64(pstmt, i);
		}
		operator int64_t() const
		{
			return as<int64_t>();
		}
		bool operator==(int64_t d) const
		{
			return SQLITE_INTEGER == type() and d == as<int64_t>();
		}

		template<>
		double as() const
		{
			return sqlite3_column_double(pstmt, i);
		}
		operator double() const
		{
			return as<double>();
		}
		bool operator==(double d) const
		{
			return SQLITE_FLOAT == type() and d == as<double>();
		}

		template<>
		std::string_view as() const
		{
			return std::string_view((const char*)column_text(i), column_bytes(i));
		}
		operator std::string_view() const
		{
			return as<std::string_view>();
		}
		bool operator==(const std::string_view& v) const
		{
			return SQLITE_TEXT == type() and v == as<std::string_view>();
		}
		bool operator==(const char* v) const
		{
			return operator==(std::string_view(v));
		}

		template<>
		std::wstring_view as() const
		{
			return std::wstring_view((wchar_t*)column_text16(i), column_bytes16(i));
		}
		operator std::wstring_view() const
		{
			return as<std::wstring_view>();
		}
		bool operator==(const std::wstring_view& v) const
		{
			return SQLITE_TEXT == type() and v == as<std::wstring_view>();
		}
		bool operator==(const wchar_t* v) const
		{
			return operator==(std::wstring_view(v));
		}

		// Extended types
		template<>
		bool as() const
		{
			return 0 != sqlite3_column_int(pstmt, i);
		}
		operator bool() const
		{
			return as<bool>();
		}
		bool operator==(bool b) const
		{
			return SQLITE_INTEGER == type() and b == as<bool>();
		}

		template<>
		datetime as() const
		{
			switch (type()) {
			case SQLITE_FLOAT:
				return datetime(operator double());
			case SQLITE_INTEGER:
				return datetime(operator int64_t());
			case SQLITE_TEXT:
				// cast from unsigned char
				return datetime(operator std::string_view().data());
			}

			return datetime{};
		}
		operator datetime() const
		{
			return as<datetime>();
		}
		bool operator==(const datetime& dt) const
		{
			return SQLITE_DATETIME == sqltype() and dt == as<datetime>();
		}
	};


	// https://www.sqlite.org/lang.html
	class stmt {
	protected:
		sqlite3_stmt* pstmt;
	public:
		stmt(sqlite3* pdb)
			: pstmt{ nullptr }
		{
			// scare up a sqlite3_stmt*
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, "SELECT 1", -1, &pstmt, 0));
			reset();
		}
		stmt(const stmt& _stmt) = delete;
		stmt& operator=(const stmt& _stmt) = delete;
		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
		}

		// for use in native sqlite3 C functions
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

		// https://www.sqlite.org/c3ref/prepare.html
		int prepare(const std::string_view& sql)
		{
			reset();
			sqlite3* pdb = db_handle();
			
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, sql.data(), (int)sql.size(), &pstmt, 0));
		
			return sqlite_status;
		}
		// int ret = step();  while (ret == SQLITE_ROW) { ...; ret = step()) { }
		// if (!SQLITE_DONE) then error
		// https://sqlite.org/c3ref/step.html
		int step()
		{
			int ret = sqlite3_step(pstmt);

			if (ret != SQLITE_ROW and ret != SQLITE_DONE) {
				FMS_SQLITE_OK(db_handle(), ret);
			}

			return ret;
		}

		// reset a prepared statement
		// https://sqlite.org/c3ref/reset.html
		int reset()
		{
			return sqlite3_reset(pstmt);
		}

		// clear any existing bindings
		int clear_bindings()
		{
			return sqlite3_clear_bindings(pstmt);
		}

		//
		// 1-based binding
		// 

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

		// int or bool
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
		// text16 with length in characters
		stmt& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_text16(pstmt, i, (const void*)str.data(),
				static_cast<int>(2 * str.size()), cb));

			return *this;
		}

		stmt& bind(int i, const sqlite3_value* value)
		{
			FMS_SQLITE_OK(db_handle(), sqlite3_bind_value(pstmt, i, value));

			return *this;
		}

		// bind underlying datetime type
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

		// https://www.sqlite.org/c3ref/bind_parameter_index.html
		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		//
		// 0-base column data
		// 

		// number of columns returned

		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}
		const char* column_name(int i) const
		{
			return sqlite3_column_name(pstmt, i);
		}

		sqlite::value value(int i) const
		{
			return sqlite::value(pstmt, i);
		}
		sqlite::value operator[](int i) const
		{
			return value(i);
		}

		// position of name in row
		int column_index(const char* name) const
		{
			for (int i = 0; i < column_count(); ++i) {
				if (0 == _stricmp(column_name(i), name)) {
					return i;
				}
			}

			return -1; // out-of-range lookup returns NULL
		}
		sqlite::value value(const char* name) const
		{
			return value(column_index(name));
		}
		sqlite::value operator[](const char* name) const
		{
			return value(name);
		}

		// Iterator over columns of a row.
		class iterator {
			sqlite3_stmt* pstmt;
			int i, e; // column index, number of columns
		public:
			using value_type = sqlite::value;

			iterator(sqlite3_stmt* pstmt, int i = 0)
				: pstmt(pstmt), i{ i }, e{ sqlite3_column_count(pstmt) }
			{ }

			bool operator==(const iterator&) const = default;

			operator const sqlite::stmt& () const
			{
				return *(sqlite::stmt*)this;
			}

			// STL friendly
			iterator begin() const
			{
				return iterator(pstmt, 0);
			}
			iterator end() const
			{
				return iterator(pstmt, e);
			}

			// iterable
			explicit operator bool() const
			{
				return 0 <= i and i < e;
			}
			value_type operator*()
			{
				return sqlite::value(pstmt, i);
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

		// Iteratable over rows of result set.
		// Use `explicit operator bool() const` to detect when done.
		// iterable it(stmt); while (it) { ... use *it ...; ++it; }
		class iterable {
			sqlite3_stmt* pstmt;
			int ret;
		public:
			using value_type = sqlite::stmt::iterator; // single row

			iterable(sqlite3_stmt* pstmt)
				: pstmt{pstmt}
			{
				ret = SQLITE_ROW;
				operator++();
			}
			explicit operator bool() const noexcept
			{
				return SQLITE_DONE != ret;
			}
			value_type operator*() const noexcept
			{
				return sqlite::stmt::iterator(pstmt);
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
					assert(SQLITE_INTEGER == stmt.value(0).type());
					assert(stmt.value(0) == 123);
					assert(SQLITE_FLOAT == stmt.value(1).type());
					assert(stmt.value(1) == 1.23);
					assert(SQLITE_TEXT == stmt.value(2).type());
					assert(stmt.value(2) == "foo");
					assert(SQLITE_TEXT == stmt.value(3).type());
					assert(SQLITE_DATETIME == stmt.value(3).sqltype());
					assert(stmt.value(3) == datetime("2023-04-05"));
					++rows;
				}
				assert(1 == rows);

				int b = 2;
				double c = 2.34;
				char d[2] = { 'a', 0};
				
				stmt.reset();
				stmt.prepare("SELECT unixepoch(e) from a");
				stmt.step();
				time_t e = stmt.value(0);

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
					d[0] = ++d[0];
					e += 86400;
				}

				stmt.reset();
				stmt.prepare("select count(*) from a");
				assert(SQLITE_ROW == stmt.step());
				assert(1 == stmt.column_count());
				assert(stmt.value(0) == 4);
				assert(SQLITE_DONE == stmt.step());
			}
			catch (const std::exception& ex) {
				puts(ex.what());
			}

			return 0;
		}
#endif // _DEBUG
	};

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
	inline std::string table_name(const std::string_view& t)
	{
		return quote(t, '[', ']');
	}
	inline std::string variable_name(const std::string_view& v)
	{
		return quote(v, '\'');
	}
#if 0
	/*
	template<class C>
	//	requires requires (C c) {
	//	c.push_back(); }
	inline auto copy(stmt& _stmt, C& c)
	{
		stmt::iterable i(_stmt);

		int col = _stmt.column_count();
		while (i) {
			const stmt& si = *i;
			
			// cache types
			std::vector<int> type(col);
			for (int j = 0; j < col; ++j) {
				type[j] = (*i).sqltype(j);
			}
			
			for (int j = 0; j < col; ++j) {
				switch (type[j]) {
				case SQLITE_NULL:
					c.push_back();
					break;
				case SQLITE_FLOAT:
					c.push_back(si.as_float(j));
					break;
				case SQLITE_BOOLEAN:
					c.push_back(si.as_boolean(j));
					break;
				case SQLITE_TEXT:
					c.push_back(si.as_text(j));
					break;
				case SQLITE_INTEGER:
					c.push_back(si.as_int(j));
					break;
				case SQLITE_DATETIME:
					auto dt = si.as_datetime(j);
					if (type[j] != SQLITE_INTEGER) {
						throw std::runtime_error(__FUNCTION__ ": datetime not stored as time_t");
					}
					c.push_back(dt.value.i);
					break;
					/*
				case SQLITE_BLOB:
					c.push_back(si.as_blob(j));
					break;
					*/
				default:
					c.push_back(); // null
				}
			}
			++i;
		}
		//c.resize(c.size() / col, col);
	}
#endif // 0
#if 0
	// V is a variant type with free functions 
	// `bool is_xxx(const V&)` and `xxx as_xxx(const V&)`
	// for xxx in `blob`, `double`, `int`, `int64`, `null`, `text`, `text16`, and `datetime` 
	template<class V>
	struct table_info {

		std::vector<std::string> name;
		std::vector<std::string> type;
		std::vector<int> sqltype; // SQLITE_XXX type
		std::vector<int> notnull;
		std::vector<std::string> dflt_value;
		std::vector<int> pk;

		table_info()
		{ }
		table_info(size_t n)
			: name(n), type(n), notnull(n), dflt_value(n), pk(n)
		{ }
		table_info(sqlite3* db, const std::string_view& table)
		{
			sqlite::stmt stmt(db);
			auto query = std::string("PRAGMA table_info(") + table_name(table) + ");";

			stmt.prepare(query.c_str());

			while (SQLITE_ROW == stmt.step()) {
				push_back(stmt.column_text(1), 
					stmt.column_text(2), 
					stmt.column_int(3),
					stmt.column_text(4),stmt.column_int(5));
			}
		}

		table_info& push_back(
			const std::string_view& _name,
			const std::string_view& _type,
			const int _notnull = 0,
			const std::string_view& _dflt_value = "",
			const int _pk = 0)
		{
			name.emplace_back(std::string(_name));
			type.emplace_back(std::string(_type));
			notnull.push_back(_notnull);
			dflt_value.emplace_back(std::string(_dflt_value));
			pk.push_back(_pk);
			sqltype.push_back(sqlite_type(_type));

			return *this;
		}

		size_t size() const
		{
			return name.size();
		}

		int parameter_index(const char* key) const
		{
			for (int i = 0; i < size(); ++i) {
				if (name[i] == key) {
					return i + 1;
				}
			}

			return 0;
		}

		// "(name type [modifiers], ...)"
		std::string schema() const
		{
			std::string sql("(");
			std::string comma("");
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append(name[i]);
				sql.append(" ");
				sql.append(type[i]);
				if (pk[i]) {
					sql.append(" PRIMARY KEY");
				}
				if (notnull[i]) {
					sql.append(" NOT NULL");
				}
				if (dflt_value[i] != "") {
					sql.append(" DEFAULT ('");
					sql.append(dflt_value[i]);
					sql.append("')");
				}
			}
			sql.append(")");

			return sql;
		}

		int drop_table(sqlite3* db, const std::string_view& table)
		{
			auto sql = std::string("DROP TABLE IF EXISTS ") + table_name(table);

			return sqlite3_exec(db, sql.data(), nullptr, nullptr, nullptr);
		}

		int create_table(sqlite3* db, const std::string_view& table)
		{
			drop_table(db, table);

			auto sql = std::string("CREATE TABLE ")
				+ table_name(table)
				+ schema();

			return sqlite3_exec(db, sql.data(), nullptr, nullptr, nullptr);
		}

		// prepare a statement for inserting values
		// stmt = "INSERT INTO table(key, ...) VALUES (@key, ...)"
		// for (row : rows) {
		//   for ([k, v]: row) {
		//     bind(stmt, k, v);
		//   }
		//   stmt.step();
		//   stmt.reset();
		// }
		sqlite::stmt insert_into(sqlite3* db, const std::string_view& table) const
		{
			sqlite::stmt stmt(db);

			auto sql = std::string(" INSERT INTO ") + table_name(table) + " (";
			std::string comma("");
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append(name[i]);
			}
			sql.append(")");
			sql.append(" VALUES (");
			comma = "";
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append("@");
				sql.append(name[i]);
			}
			sql.append(")");

			stmt.prepare(sql);

			return stmt;
		}

		template<class C, class X = typename C::value_type>
		int insert_values(stmt& stmt, C& cursor)
		{
			int count = 0;

			while (cursor) {
				auto row = *cursor;
				for (unsigned j = 0; row; ++j) {
					switch (sqltype[j]) {
					case SQLITE_NULL:
						stmt.bind(j + 1);
						break;
					case SQLITE_TEXT:
						stmt.bind(j + 1, as_text<X>(*row));
						break;
					case SQLITE_FLOAT:
					case SQLITE_NUMERIC:
						stmt.bind(j + 1, as_double<X>(*row));
						break;
					case SQLITE_INTEGER:
					case SQLITE_BOOLEAN:
						stmt.bind(j + 1, as_int<X>(*row));
						break;
					case SQLITE_DATETIME:
						stmt.bind(j + 1, as_datetime<X>(*row));
						break;
					}
					++row;
					++count;
				}
				stmt.step();
				stmt.reset();
				++cursor;
			}

			return count;
		}

		template<class C, class O>
		int exec(sqlite3_stmt* stmt, C& cursor, O& o, bool no_headers = false)
		{
			using X = typename O::value_type;

			while (cursor) {
				auto row = *cursor;
				for (unsigned j = 0; row; ++j) {
					switch (sqltype[j]) {
					case SQLITE_NULL:
						o.push_back(X{});
						break;
					case SQLITE_TEXT:
						o.push_back(stmt.column_text(j));
						break;
					case SQLITE_FLOAT:
						o.push_back(stmt.column_double(j));
						break;
					case SQLITE_INTEGER:
						o.push_back(stmt.column_int(j));
						break;
					}
					++row;
				}
				stmt.step();
				stmt.reset();
				++cursor;
			}
		}

		int bind(sqlite3_stmt* stmt, int j, const V& v) const
		{
			if (SQLITE_TEXT == sqltype[j]) {
				const auto t = as_text(v);
				return sqlite3_bind_text(stmt, j + 1, t.data(), (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (SQLITE_TEXT16 == sqltype[j]) {
				const auto t = as_text16(v);
				return sqlite3_bind_text16(stmt, j + 1, t.data(), 2 * (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (SQLITE_FLOAT == sqltype[j]) {
				return sqlite3_bind_double(stmt, j + 1, as_double(v));
			}
			else if (SQLITE_INTEGER == sqltype[j]) {
				return sqlite3_bind_int(stmt, j + 1, as_int(v));
			}
			else if (SQLITE_INT64 == sqltype[j]) {
				return sqlite3_bind_int64(stmt, j + 1, as_int64(v));
			}
			else if (SQLITE_NULL == sqltype[j]) { // ???
				return sqlite3_bind_null(stmt, j + 1);
			}
			else if (SQLITE_BLOB == sqltype[j]) {
				const auto& b = as_blob(v);
				return sqlite3_bind_blob(stmt, j, b.data(), (int)b.size(), nullptr);
			}

			return SQLITE_MISUSE;
		}
		template<class V>
		int bind(sqlite3_stmt* stmt, const char* key, const V& v) const
		{
			int j = parameter_index(key);
			if (j == 0) {
				throw std::runtime_error(__FUNCTION__ ": key not found");
			}

			return bind(stmt, j, v);
		}
	};
#endif // 0
} // sqlite
