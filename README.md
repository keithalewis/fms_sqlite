# fms_sqlite

A breviloquent C++ wrapper for the sqlite C API.

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
It is not so convenient for getting data in and out of sqlite while preserving 
their original types.

The _extended types_ `SQLITE_BOOLEAN` and `SQLITE_DATETIME` are defined 
in `fms_sqlite.h` to preserve, e.g., Excel and JSON/BSON fidelity.
Excel and JSON have a boolean type, but not a datetime type. BSON has both.
These extended types are not definded in the `sqlite.h` header file.

It takes some care to preserve fidelity when inserting and extracting boolean and datetime types.
The function [`sqlite3_column_decltype`](https://www3.sqlite.org/c3ref/column_decltype.html)
returns the character string used when creating a table
and `int sqlite_type(const char*)` returns the extended sqlite type
based on the string returned by this.
You can use this to preserve the original type of data being
inserted or extracted from sqlite.

### `SQLITE_BOOLEAN`

A sqlite boolean is stored as an integer where non-zero indicates `true`.
The function `sqlite_type` returns `SQLITE_BOOLEAN` if a column
was declare with a type starting with `BOOL`.

### `SQLITE_DATETIME`

The [`datetime`](https://www.sqlite.org/lang_datefunc.html) type is a union
that can contain a floating point Gegorian date, integer `time_t` seconds
since Unix epoch, or an ISO 8601 format string.
The function `sqlite_type` returns `SQLITE_DATETIME` if a column
was declare with a type starting with `DATE`.

Some affordances have been made for constructing a `datetime` type
to use with sqlite. It has constructors from floating point, time_t,
and text, but sqlite only recognizes a subset of 
[ISO 8601](https://www.w3.org/TR/NOTE-datetime) string formats.

Use `datetime::as_time_t()` to convert to
the preferred integer `time_t` representaion. 

## Using `fms::sqlite`

This libary endeavours to provide the thinest possible C++ API
to the underlying sqlite C API. All classes wrapping any `sqlite3_xxx*` 
opaque type provide an `operator sqlite_xxx*()` that allows the C++ class to
be used with any sqlite C API function.
The `fms_sqlite` library does not wrap every function in the C API,
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
associate a database with a statemnt. 
The destructor calls [`sqlite3_finalize`](https://www3.sqlite.org/c3ref/finalize.html).

The type `sqlite::stmt` is not 
[copy constructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible)
or [copy assignable](https://en.cppreference.com/w/cpp/named_req/CopyAssignable).

Statements are used to act on databases. A statement is first
[prepared](https://www3.sqlite.org/c3ref/prepare.html)
by compiling the sql statement it is given.
Any parameters in the statement can be bound using `stmt::bind`.
The results of the query are returned row-by-row using
(`sqlite3_step`)[https://www3.sqlite.org/c3ref/step.html].

```
sqlite::db db(""); // create an in-memory database
sqlite::stmt stmt(db); // calls operator sqlite*() on db
stmt.prepare("CREATE TABLE t (a INT, b REAL, c TEXT)");
stmt.step(); // execute statement

stmt.reset(); // reuse stmt
stmt.prepare("INSERT INTO t VALUES (?, ?, ?)");
stmt.bind(1, 123); // bind is 1-based
stmt.bind(2, 1.23);
stmt.bind(3, "text");
stmt.step(); // insert values

stmt.reset();
stmt.prepare("SELECT * from t");
while (SQLITE_ROW == stmt.step()) {
	std::cout << sqlite3_column_int(stmt, 0) << " "; // column is 0-based
	std::cout << sqlite3_column_double(stmt, 1) << " "; 
	std::cout << sqlite3_column_text(stmt, 2) << "\n"; 
}
```
Expected output
```
123 1.23 text
```

Note we first iterate over rows, then iterate over columns.

### `sqlite::iterable`

A statement _iterable_ is a generalization of a null terminated pointer.
It uses `explicit operator bool() const` to detect when it is done.
Values are extracted using `operator*()` and advanced to
the next value with `operator++()`.
An iterable can iterate over the indeterminite number of rows returned by a statement.

```
stmt.reset();
stmt.iterable i(stmt);
while (i) {
	std::cout << sqlite3_column_int(stmt, 0) << " "; // column is 0-based
	std::cout << sqlite3_column_double(stmt, 1) << " "; 
	std::cout << sqlite3_column_text(stmt, 2) << "\n";
	++i;
}
```
### `sqlite::iterator`

An statement _iterator_ iterates over the columns of a row
returned by an iterable. It is an STL iterator and columns
can be accessed using 0-based `operator[](int)` or by
column name using `operator[](const char*)`.
```
stmt.reset();
stmt::iterable i(stmt);

while (i) {
	auto j = stmt::iterator(*i);

	std::cout << (*j).as<int>() << " ";
	++j;
	std::cout << (*j).as<double>() << " ";
	++j;
	std::cout << (*j).as<std::string_view>() << "\n";

	++i;
}
```

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