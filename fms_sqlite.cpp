// fms_sqlite.cpp - test platform independent sqlite
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
			::db.exec("DROP TABLE IF EXISTS t");
			::db.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)");

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

