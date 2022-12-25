// fms_sqlite.cpp - test platform independent sqlite
#include <cassert>
#include "fms_sqlite.h"

using namespace sqlite;

// scratch database for a single sqlite3_value
// https://www.sqlite.org/c3ref/value_blob.html
class _value {
	static sqlite3* db()
	{
		static sqlite::db db("");
		static bool init = true;

		if (init) {
			sqlite::stmt stmt(db);
			stmt.prepare("CREATE TABLE IF NOT EXISTS v (value DATETIME)");
			stmt.step();

			init = false;
		}

		return db;
	}
	mutable sqlite::stmt stmt;
	sqlite3_int64 rowid;
public:
	_value()
		: stmt(db()), rowid(0)
	{ }
	template<class T>
	_value(const T& t)
		: _value()
	{
		stmt.reset();
		stmt.prepare("INSERT INTO v VALUES (?)");
		stmt.bind(1, t);
		stmt.step();

		rowid = sqlite3_last_insert_rowid(db());
	}
	_value(const datetime& dt)
		: _value()
	{
		stmt.reset();
		stmt.prepare("INSERT INTO v VALUES (?)");
		stmt.bind(1, dt);
		stmt.step();

		rowid = sqlite3_last_insert_rowid(db());
	}
	_value(const _value&) = delete;
	_value& operator=(const _value&) = delete;
	~_value()
	{
		stmt.reset();
		stmt.prepare("DELETE FROM v WHERE rowid = ?");
		stmt.bind(1, rowid);
		stmt.step();
	}

	static int count()
	{
		sqlite::stmt stmt(db());

		stmt.prepare("SELECT COUNT(*) FROM v");
		stmt.step();

		return stmt.value(0).as<int>();
	}


	// Internal fundamental sqlite type.
	int type() const
	{
		return sqlite3_value_type(select());
	}
	// Best numeric datatype of the value.
	int numeric_type() const
	{
		return sqlite3_value_numeric_type(select());
	}

	sqlite3_value* select() const
	{
		stmt.reset();
		stmt.prepare("SELECT value FROM v WHERE rowid = ?");
		stmt.bind(1, rowid);
		stmt.step();

		return stmt.value(0);
	}
	sqlite3_value* select(const char* expr) const
	{
		auto sql = std::format("SELECT {} FROM v WHERE rowid = ?", expr);
		stmt.reset();
		stmt.prepare(sql);
		stmt.bind(1, rowid);
		stmt.step();

		return stmt.value(0);
	}

	int bytes() const
	{
		return sqlite3_value_bytes(select());
	}
	int bytes16() const
	{
		return sqlite3_value_bytes16(select());
	}

	const void* as_blob() const
	{
		return sqlite3_value_blob(select());
	}
	const double as_double() const
	{
		return sqlite3_value_double(select());
	}
	const int as_int() const
	{
		return sqlite3_value_int(select());
	}
	const sqlite3_int64 as_int64() const
	{
		return sqlite3_value_int64(select());
	}
	const unsigned char* as_text() const
	{
		return sqlite3_value_text(select());
	}
	const void* as_text16() const
	{
		return sqlite3_value_text16(select());
	}

	// CAST (v AS type)
	sqlite3_value* cast(const char* type) const
	{
		stmt.reset();
		stmt.prepare("SELECT CAST (value AS ?) FROM v WHERE rowid = ?");
		stmt.bind(1, type);
		stmt.bind(2, rowid);
		auto sql = stmt.expanded_sql();
		stmt.step();

		return stmt.value(0);
	}
	sqlite3_value* strftime(const char* format) const
	{
		stmt.reset();
		stmt.prepare("SELECT strftime(?, value) FROM v WHERE rowid = ?");
		stmt.bind(1, format);
		stmt.bind(2, rowid);
		auto sql = stmt.expanded_sql();
		stmt.step();

		return stmt.value(0);
	}
	double julianday() const
	{
		return sqlite3_value_double(strftime("%J"));
	}
	sqlite3_int64 unixepoch() const
	{
		return sqlite3_value_int64(strftime("%s"));
	}
#ifdef _DEBUG
	static int test()
	{
		try {
			{
				// must be ISO 8601 format
				_value v(datetime("2023-04-05"));

				auto d = v.julianday();
				_value dv(v.select("julianday(value)"));
				double dd = dv.as_double();
				assert(dd == d);

				auto t = v.unixepoch();
				_value tv(v.select("unixepoch(value)"));
				int64_t tt = tv.as_int64();
				assert(tt == t);
			}
			{
				_value v(1.23);
				assert(SQLITE_FLOAT == v.type());
				assert(1.23 == v.as_double());
			}
			{
				_value v(123);
				assert(SQLITE_INTEGER == v.type());
				assert(123 == v.as_int());
			}
			{
				_value v("string");
				assert(SQLITE_TEXT == v.type());
				assert(0 == strcmp("string", (const char*)v.as_text()));
			}
		}
		catch (const std::exception& ex) {
			puts(ex.what());
		}

		return 0;
	}
#endif // _DEBUG
};

void print(sqlite::stmt& stmt)
{
	stmt::iterable i(stmt);
	while (i) {
		for (auto r : *i) {
			puts(r.name());
		}
		++i;
	}
}

void insert(sqlite3* db)
{
	sqlite::stmt stmt(db);
	stmt.prepare("CREATE TABLE t (a INT, b FLOAT, c TEXT, d DATETIME)");
	stmt.step();

	stmt.reset();
	stmt.prepare("INSERT INTO t VALUES "
		"(1, .2, 'a', '2023-04-05'),"
		"(2, .3, 'b', '2023-04-06');"
	);
	stmt.step();
}

int main()
{
	int test_datetime = datetime::test();
	int test_sqlite = stmt::test();
	int test_value = _value::test();

	try {
		sqlite::db db("");
		insert(db);
		sqlite::stmt stmt(db);
		stmt.prepare("select * from t");
		print(stmt);
	}
	catch (const std::exception& ex) {
		puts(ex.what());
	}

	return 0;
}