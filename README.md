# fms_sqlite

C++ wrapper for the sqlite C API.

## Typing

Sqlite has [flexible typing](https://www3.sqlite.org/flextypegood.html).  
Internally, everything is one of the 
[fundamental types](https://www3.sqlite.org/c3ref/c_blob.html) 
64-bit signed integer, 64-bit IEEE floating point, character string, BLOB, or NULL. 
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. 
The function `int sqlite3_column_type(sqlite3_stmt* stmt, int i)` 
returns the internal type of column `i` for an active `stmt`.
Database operatons might change the internal type.
Types in sqlite are so flexible they can change due to a query, unlike most SQL databases. 

Flexible typing makes it convenient to use sqlite as a key-value store. 
The first column is text and the second column can be 
[any type](http://oplaunch.com/blog/2015/04/30/the-truth-about-any-color-so-long-as-it-is-black/). 
It is not so convenient for getting data in and out of sqlite while preserving their original types.

The _extended types_ `SQLITE_BOOLEAN` and `SQLITE_DATETIME` are defined 
in `fms_sqlite.h` to preserve Excel and JSON/BSON fidelity.
Excel and JSON have a boolean type, but not a datetime type. BSON has both.
A sqlite boolean is stored as an integer where non-zero indicates `true`.
The [`datetime`](https://www.sqlite.org/lang_datefunc.html) type is a union
that can contain a floating point Gegorian date, integer `time_t` seconds
since Unix epoch, or an ISO 8601 format string.
These extended types are not definded in the `sqlite.h` header file.

Some affordances have been made for constructing a `datetime` type
to use with sqlite. Use `datetime::as_time_t()` to convert to
an integer `time_t` representaion. 

## Using `fms::sqlite`

This libary endeavours to provide the thinest possible C++ API
to the underlying sqlite C API. All classes wrapping any `sqlite3_xxx*` 
opaque type provide an `operator sqlite_xxx*()` that allows the C++ class to
be used with any sqlite C API function.
The `fms_sqlite` library does not wrap up every function in the C API,
but it makes it easy to use any C API function from C++.

### `sqlite3*`

The class `sqlite::db` provides an RAII object for 
an opaque `sqlite3*` pointer to a database.
Use `sqlite::db(file, flags)` to 
[open or create](https://www3.sqlite.org/c3ref/open.html)
a database that manages this pointer. 
The destructor calls [`sqlite3_close`](https://www3.sqlite.org/c3ref/close.html).

The type `sqlite::db` is not 
[copy constructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible)
or [copy assignable](https://en.cppreference.com/w/cpp/named_req/CopyAssignable).

### `sqlite_stmt*`

The class `sqlite::stmt` provides an RAII object for 
an opaque `sqlite3_stmt*` pointer to a sqlite statement.
Use `sqlite::stmt(sqlite3*)` to 
[open or create](https://www3.sqlite.org/c3ref/open.html)
a database that manages this pointer. 
The destructor calls [`sqlite3_finalize`](https://www3.sqlite.org/c3ref/close.html).

The type `sqlite::db` is not 
[copy constructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible)
or [copy assignable](ht 

Like any database, you can [create tables](https://www3.sqlite.org/lang_createtable.html)
by giving them a name and a list of types with optional 
[column-contraints](https://www3.sqlite.org/syntax/column-constraint.html). 
The sqlite `CREATE TABLE` command recognizes
any of the usual SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).

You can only insert values having one of the 
[_fundametal types_](http://oplaunch.com/blog/2015/04/30/the-truth-about-any-color-so-long-as-it-is-black/)..



The function [`sqlite3_column_decltype`](https://www3.sqlite.org/c3ref/column_decltype.html)
returns the character string used when creating a table
and `int sqlite_type(const char*)` returns the extended sqlite type
based on string used when creating a table.
You can use this to preserve the original type of data being
inserted or extracted from sqlite.

### `sqlite::value`

A `sqlite::value` is not a value type like `std::variant`. 

When creating a sqlite table it is possible to specify any of the usual
SQL data types specified in the 
[Affinity Name Examples](https://www.sqlite.org/datatype3.html#affinity_name_examples).
The function `sqlite3_column_decltype` returns the character string used
and `int sqlite::stmt::sqltype(int i)` returns the **extended sqlite type**
based on string used when creating a table.


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