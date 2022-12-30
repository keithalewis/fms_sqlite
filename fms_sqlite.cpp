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
		"(3, .4, 'b', '2023-04-06');"
	);
	stmt.step();
}

void print(sqlite::stmt& stmt)
{
	
	iterable i(stmt);
	iterator _i = *i;
	std::copy(_i.begin(), _i.end(), std::ostream_iterator<sqlite::value>(std::cout, ", "));
	std::cout << '\n';
	copy(_i, std::ostream_iterator<sqlite::value>(std::cout, ", "));
	std::cout << '\n';
	copy(i, std::ostream_iterator<sqlite::value>(std::cout, ", "));
	
}

int main()
{
	try {
		fms::parse_test<char>();
		datetime::test();
		stmt::test();

		db db("");
		insert(db);

		stmt stmt(db);
		stmt.prepare("select * from t");
		print(stmt);
	}
	catch (const std::exception& ex) {
		puts(ex.what());
	}

	return 0;
}
