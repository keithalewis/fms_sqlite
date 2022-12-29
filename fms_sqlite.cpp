// fms_sqlite.cpp - test platform independent sqlite
#include <cassert>
#include <iostream>
#include "fms_sqlite.h"

using namespace sqlite;

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

void print(sqlite::stmt& stmt)
{
	stmt::iterable i(stmt);
	copy(i, std::ostream_iterator<sqlite::stmt::iterator>(std::cout, ", "));
}


int main()
{

	try {
		int test_datetime = datetime::test();
		int test_sqlite = stmt::test();

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