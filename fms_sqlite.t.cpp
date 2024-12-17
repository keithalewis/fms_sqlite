// fms_sqlite.t.cpp - test platform independent sqlite
#include <cassert>
#include <iostream>
#include <iterator>
#include <sstream>
#include "fms_sqlite.h"

using namespace sqlite;

#ifdef _DEBUG
int test_view = fms::view<char>::test();
int test_parse_int = fms::parse_int_test();
#endif // _DEBUG

sqlite::db db(""); // in-memory database

int test_error()
{
	try {
		sqlite::stmt stmt;
		::db.exec("DROP !@#$");
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
		// near: DROP !@#$
		// here: -----^
	}

	return 0;
}

int test_simple()
{
	try {
		{
			sqlite::stmt stmt;
			assert(SQLITE_OK == ::db.exec("DROP TABLE IF EXISTS t"));
			assert(SQLITE_OK == ::db.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)"));

			stmt.prepare(::db, "INSERT INTO t VALUES (?, ?, :c)");
			stmt[0] = 123; // calls sqlite3_bind_int(stmt, 0 + 1, 123);
			stmt[1] = 1.23;
			stmt[":c"] = "str"; // bind parameter name

			assert(SQLITE_DONE == stmt.step());

			stmt.prepare(::db, "SELECT * FROM t");
			stmt.step();
			assert(stmt[0] == 123);
			assert(stmt["b"] == 1.23); // lookup by name
			assert(stmt[2] == "str");

			assert(SQLITE_DONE == stmt.step());
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
	}

	return 0;
}

int test_boolean()
{
	try {
		{
			::db.exec("DROP TABLE IF EXISTS t");
			::db.exec("CREATE TABLE t (b BOOLEAN)");
			::db.exec("INSERT INTO t (b) VALUES(TRUE)");

			sqlite::stmt stmt(::db, "SELECT * FROM t");
			stmt.step();
			assert(stmt[0].type() == SQLITE_INTEGER);
			//assert(stmt[0].type() == SQLITE_BOOLEAN);
			assert(stmt[0] == true);
			assert(stmt[0].column_boolean());

			assert(SQLITE_DONE == stmt.step());

			::db.exec("UPDATE t SET b = FALSE WHERE b = TRUE");
			stmt.prepare(::db, "SELECT * FROM t");
			stmt.step();
			assert(stmt[0].type() == SQLITE_INTEGER);
			//assert(stmt[0].type() == SQLITE_BOOLEAN);
			assert(stmt[0] == false);
			assert(!stmt[0].column_boolean());

			assert(SQLITE_DONE == stmt.step());
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
	}

	return 0;
}

int test_datetime()
{
	try {
		{
			::db.exec("DROP TABLE IF EXISTS dt");
			::db.exec("CREATE TABLE dt (t DATETIME)");
			
			// sqlite doesn't recognize this
			::db.exec("INSERT INTO dt (t) VALUES('1970-1-2')");

			sqlite::stmt stmt;
			stmt.prepare(::db, "SELECT t FROM dt");
			stmt.step();
			assert(stmt[0].type() == SQLITE_TEXT);
			//assert(stmt[0].type() == SQLITE_DATETIME);
			datetime t = stmt[0].column_datetime();
			
			// sqlite will store the string
			assert(t == datetime("1970-1-2"));

			assert(SQLITE_DONE == stmt.step());

			stmt.prepare(::db, "SELECT unixepoch(t) FROM dt");
			stmt.step();
			t = stmt[0].column_datetime();
			
			// but it can't parse it
			assert(t.type == SQLITE_INTEGER);
			assert(t.value.i == -1);

			assert(SQLITE_DONE == stmt.step());

			// sqlite wants an ISO 8601 date
			::db.exec("UPDATE dt SET t = '1970-01-02'");
			stmt.prepare(::db, "SELECT unixepoch(t) FROM dt");
			stmt.step();
			t = stmt[0].column_datetime();
			assert(t.type == SQLITE_INTEGER);
			// one day past unix epoch in seconds
			assert(t.value.i == 24*60*60);

			assert(SQLITE_DONE == stmt.step());

			stmt.prepare(::db, "UPDATE dt SET t = ?");
			datetime dt("1970-1-2");
			dt.to_time_t(); // call fms::parse_tm 
			stmt.bind(1, dt);
			stmt.step();
			stmt.prepare(::db, "SELECT t FROM dt");
			stmt.step();
			assert(stmt.column_type(0) == SQLITE_INTEGER);
			assert(stmt[0] == 86400);

			/*
			stmt.prepare("SELECT julianday(t) FROM dt");
			stmt.step();
			t = stmt[0].column_datetime();
			assert(t.type == SQLITE_FLOAT);
			// one day past unix epoch in days
			assert(t.value.f - 2440587.5 == 1);

			assert(SQLITE_DONE == stmt.step());
			*/
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
	}

	return 0;
}
#if 0
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
		stmt stmt{};

		std::string_view sql("DROP TABLE IF EXISTS a");
		assert(SQLITE_OK == stmt.prepare(db, sql));
		stmt.step();
		stmt.reset();

		sql = "CREATE TABLE a (b INT, c REAL, d TEXT, e DATETIME)";
		assert(SQLITE_OK == stmt.prepare(db, sql));
		assert(sql == stmt.sql());
		assert(SQLITE_DONE == stmt.step());
		assert(0 == stmt.column_count());

		stmt.reset();
		assert(SQLITE_OK == stmt.prepare(db, "INSERT INTO a VALUES (123, 1.23, 'foo', "
			"'2023-04-05')")); // datetime must be YYYY-MM-DD HH:MM:SS.SSS
		ret = stmt.step();
		assert(SQLITE_DONE == ret);

		stmt.reset();
		stmt.prepare(db, "SELECT * FROM a");
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
			//assert(SQLITE_DATETIME == stmt.sqltype(3));
			//!!!assert(stmt.column_datetime(3) == datetime("2023-04-05"));
			++rows;
		}
		assert(1 == rows);

		int b = 2;
		double c = 2.34;
		char d[2] = { 'a', 0 };

		stmt.reset();
		stmt.prepare(db, "SELECT unixepoch(e) from a");
		stmt.step();
		time_t e = stmt.column_int64(0);

		// fix up column e to time_t
		// DATEIME column must be homogeneous
		stmt.reset();
		stmt.prepare(db, "UPDATE a SET e = ?");
		stmt.bind(1, e);
		stmt.step();

		stmt.reset();
		assert(SQLITE_OK == stmt.prepare(db, "INSERT INTO a VALUES (?, ?, ?, ?)"));
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
	stmt.prepare(db, "select count(*) from a");
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
#endif // 0

void insert(sqlite3* db)
{
	::db.exec("DROP TABLE IF EXISTS t");
	::db.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT, d DATETIME)");

	sqlite::stmt stmt;
	stmt.prepare(::db, "INSERT INTO t VALUES "
		"(1, .2, 'a', '2023-04-05'),"
		"(3, .4, 'b', '2023-04-06');"
	);
	stmt.step();
}
/*
int test_copy()
{
	std::ostringstream s;

	try {
		insert(::db);
		sqlite::stmt stmt;

		stmt.prepare(::db, "SELECT * FROM t");
		copy(stmt, std::ostream_iterator<sqlite::value>(s, ", "));
		assert(s.str() == "1, 0.2, a, 2023-04-05, 3, 0.4, b, 2023-04-06, ");
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << '\n';
	}

	return 0;
}
*/
/*
sqlite::stmt stmt_create()
{
	::db.exec("CREATE TABLE u (a INT, b FLOAT, c TEXT, d DATETIME)");
	
	sqlite::stmt stmt;
	stmt.prepare(::db, R"(
	INSERT INTO u VALUES
		(1, .2, 'a', '2023-04-05'),
		(3, .4, 'b', '2023-04-06');
	)");
	stmt.step();
	
	return stmt;
}
int test_stmt_move()
{
	{
		auto stmt = stmt_create();
	}

	return 0;
}
*/
int main()
{
	try {
#ifdef _DEBUG
		test_error();
		fms::parse_test<char>();
		datetime::test();
		//value::test();
		//stmt::test();
#endif // _DEBUG
		test_simple();
		test_boolean();
		test_datetime();
		//test_copy();
		//test_stmt_move();
	}
	catch (const std::exception& ex) {
		puts(ex.what());
	}

	return 0;
}

