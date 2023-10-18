// sqlite_xxd.c: serialize database to header file
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sqlite3.h"

int header(const char* db, const char* schema)
{
	return printf("unsigned char sqlite3_%s_%s[] = {\n", db, schema);
}
int footer(const char* db, const char* schema, unsigned int size)
{
	return printf("};\nunsigned int sqlite3_%s_%s_len = %u;\n", db, schema, size);
}

int main(int ac, char** av)
{
	int rc;
	if (ac < 2) {
		errno = 0;
		perror("usage: sqlite_xxd database [schema]");

		return errno;
	}

	const char* schema = ac > 2 ? av[2] : "main";

	sqlite3* db;
	rc = sqlite3_open(av[1], &db);
	if (rc) {
		perror(sqlite3_errmsg(db));

		goto done;
	}

	sqlite3_int64 size;
	unsigned char* data = sqlite3_serialize(db, schema, &size, 0);
	if (!data) {
		perror(sqlite3_errmsg(db));
		rc = sqlite3_errcode(db);

		goto done;
	}

	char* dot = strchr(av[1], '.');
	*dot = 0;
	header(av[1], schema);
	for (sqlite3_int64 i = 0; i < size; ++i) {
		printf("0x%02x,\n", data[i]);
	}
	footer(av[1], schema, size);

done:
	sqlite3_free(data);
	sqlite3_close(db);

	return rc;
}
