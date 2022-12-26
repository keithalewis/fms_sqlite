# fms_sqlite

C++ wrapper for the sqlite C API.

## Typing

Sqlite has [flexible typing](https://www3.sqlite.org/flextypegood.html).  
Internally, everything is one of the 
[fundamental types](https://www3.sqlite.org/c3ref/c_blob.html) 
64-bit signed integer, 64-bit IEEE floating point, character string, BLOB, or NULL. 
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. 
The function `sqlite3_column_type` returns the internal type currently being used.
Note that database operatons might change the internal representation.
Types in sqlite are so flexible they can change due to a query, unlike most SQL databases. 

Flexible typing makes it easy to use sqlite as a key-value store. The first column
is text and the second column can be any type. It is not so 
convenient for getting data in and out of sqlite while preserving their original types.

## Using sqlite

Like any database, you can [create tables](https://www3.sqlite.org/lang_createtable.html)
by giving them a name and a list of types with optional 
[column-contraints](https://www3.sqlite.org/syntax/column-constraint.html). 
The sqlite `CREATE TABLE` command recognizes
any of the usual SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).

_You can only insert values having one of the fundametal types_.

The function `sqlite3_column_decltype` returns the character string used
when creating a table.

and `int sqlite::stmt::sqltype(int i)` returns the **extended sqlite type**
based on string used when creating a table.
### `sqlite::value`

A `sqlite::value` is not a value type like `std::variant`. 

When creating a sqlite table it is possible to specify any of the usual
SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).
The function `sqlite3_column_decltype` returns the character string used
and `int sqlite::stmt::sqltype(int i)` returns the **extended sqlite type**
based on string used when creating a table.

The extended types `SQLITE_BOOLEAN` and 
[`SQLITE_DATETIME`](https://www.sqlite.org/lang_datefunc.html) are defined 
in `fms_sqlite.h` to preserve Excel and JSON/BSON fidelity.
Excel does not have a datetime type, JSON has neither, and BSON has both.
A sqlite boolean is stored as an integer. The datetime type is a union
that can contain a floating point Gegorian date, integer `time_t` seconds
since Unix epoch, or an ISO 8601 format string.

## `iterable`

An `iterable` allows iteration over rows in the result of a query.

```
stmt.prepare("SELECT * from table");
iterable rows(stmt);
while(rows) {
	const stmt& row = *rows;
	// use row...
	++rows;
}
```

Getting data in and out of sqlite is just copying an iterable.

A SELECT statement creates an output iterable `sqlite::stmt`.

An INSERT statement creates an input iterable `sqlite::stmt`.

`copy(I i, O o) { while (i and o) { *o++ = *i++} }`

Override `operator*` to return proxies.