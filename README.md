# fms_sqlite

A breviloquent C++ wrapper for the sqlite C API.

## Usage

Create a new, or open an existing. sqlite database with `sqlite::db db(file, flags)`.
Use `sqlite::db db("")` to create a temporary in-memory database.

Create a sqlite statement with `sqlite::stmt stmt(db)`.

Compile a SQL statement with `stmt.prepare(sql)` and call `stmt.bind(index, value)`
to set parameters.

Run the SQL statement using `stmt.step()`. If it returns `SQLITE_ROW`
then keep calling `stmt.step()` until it returns `SQLITE_DONE`.

The columns of a statement can be accessed with `sqlite::value(stmt, i)`.
Use `sqlite::value::sqltype()` to detect the declared type and 
`sqlite::value::as`_type_ to retreive data as _type_. The function
`sqlite::value::type()` returns the the fundamental type used to store the value.

To iterate over all rows returned by a query use `sqlite::stmt::iterable i(stmt)`.
It implement `explicit operator bool() const` to detect when there
are no more results. 

```
stmt::iterable i(stmt);
while (i) {
	... use *i ...
	++i;
}
```

The result of `iterable::operator*() const` is a `sqlite::stmt::iterator` that publicly
inherits from `sqlite::value`. (So does `iterable`.) Use `operator[](int)` to access columns using a
0-based index or `operator[](std::string_view)` to access columns by their name.
It is also an iterable that provides `explicit operator bool() const`.

## Typing

Sqlite has [flexible typing](https://www3.sqlite.org/flextypegood.html).  
Internally, everything is one of the 
[Fundamental Datatypes](https://www3.sqlite.org/c3ref/c_blob.html) 
64-bit signed integer, 64-bit IEEE floating point, character string, BLOB, or NULL. 
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. The function 
[`int sqlite3_column_type(sqlite3_stmt* stmt, int i)`](https://www3.sqlite.org/c3ref/column_blob.html) 
returns the internal type of column `i` for an active `stmt`.
Database operatons might change the internal column types.
Types in sqlite are so flexible they can change due to a query, unlike most SQL databases. 

Flexible typing makes it convenient to use sqlite as a key-value store. 
The first column is text and the second column can be 
any [Dynamically Typed Value Object](https://www.sqlite.org/c3ref/value.html). 
It is not so convenient for getting data in and out of sqlite while preserving 
their original types.

## Compatibility

One of the many reasons for sqlite's popularity is that it accommodates
using SQL [CREATE TABLE](https://www3.sqlite.org/lang_createtable.html) 
syntax encountered in the wild. 
The type-name used in a 
[column definition](https://www3.sqlite.org/syntax/column-def.html)
is made available by  
the function [`sqlite3_column_decltype`](https://www3.sqlite.org/c3ref/column_decltype.html).

This is used when inserting or `CAST`ing values based on 
[Column Affinity](https://sqlite.org/datatype3.html#determination_of_column_affinity).
External types are converted to a fundmental sqlite type based on this, except for
the quirky `NUMERIC` affinity to handle `DECIMAL(?,?)`, `BOOLEAN`, `DATE`,
and `DATETIME` types.
There is no definition of `SQLITE_NUMERIC` in the sqlite source code.

## `fms_sqlite.h`

The file `fms_sqlite.h` defines `SQLITE_NUMERIC` so `int sqlite::affinity(const char*)` 
can be implemented based on the sqlite affinity algorithm for declared column types.

It also defines the _extended types_ `SQLITE_BOOLEAN` and `SQLITE_DATETIME`.
Use `int sqlite_type(std::string_view type)` to convert the declared
type in `CREATE TABLE` to an extended type.

### `SQLITE_BOOLEAN`

A sqlite boolean is stored as an integer where non-zero indicates `true`.
The function `sqlite_type` returns `SQLITE_BOOLEAN` if a column
was declare with a type starting with `BOOL`.

### `SQLITE_DATETIME`

The [`datetime`](https://www.sqlite.org/lang_datefunc.html) type is a union
that can contain a floating point Gegorian date, 64-bit integer `time_t` seconds
since Unix epoch, or an ISO 8601-ish format string.
The function `sqlite_type` returns `SQLITE_DATETIME` if a column
was declare with a type starting with `DATE`.

Some affordances have been made for constructing a `datetime` type
to use with sqlite. It has constructors from floating point, time_t,
and text, but sqlite only recognizes a subset of 
[ISO 8601](https://www.w3.org/TR/NOTE-datetime) string formats.

Use `datetime::as_time_t()` to convert to
the preferred integer `time_t` representaion.
It uses `fms_parse.h` to convert non-ISO 8601 strings
from the wild into valid dates.

### `fms::sqlite::db`

A thin RAII wrapper for `sqlite3_open_v2` and `sqlite3_close`.
Use `sqlite::db db("")` if you just need a temporary in-memory database.

### `fms::sqlite::value`

This is not a value type in the Alexander Stepanov sense.
It uses an active `sqlite3_stmt*` and an index as a proxy
for a non-owning view. It is your responsibility to ensure
the statment pointer is valid when using it.



## Using `fms::sqlite`

This libary endeavours to provide the thinest possible C++ API
to the underlying sqlite C API. All classes wrapping any `sqlite3_xxx*` 
opaque type provide an `operator sqlite_xxx*()` that allows the C++ class to
be used with any sqlite C API function.
The `fms_sqlite` library does not wrap every function in the C API,
but it makes it easy to call any C API function from C++.

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
I was not able to devine how a `sqlite3*` pointer could be reliably cloned.

### `sqlite3_stmt*`

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
by compiling the sql statement it is given. This is relatively expensive.

Any parameters in the statement can be bound using `stmt::bind`.
This is relatively inexpensive.
The rows of a query result are returned by calling
[`sqlite3_step`](https://www3.sqlite.org/c3ref/step.html) until
it returns `SQLITE_DONE`.

```
sqlite::db db(""); // create an in-memory database
sqlite::stmt stmt(db); // calls operator sqlite3*() on db
stmt.prepare("CREATE TABLE t (a INT, b REAL, c TEXT)");
stmt.step(); // execute statement

stmt.reset(); // reuse stmt
stmt.prepare("INSERT INTO t VALUES (?, ?, ?)");
stmt.bind(1, 123); // bind is 1-based
stmt.bind(2, 1.23);
stmt.bind(3, "text");
// use stmt.sql() to get the original text of the prepare statement
// use stmt.sql_expanded() to get the text that will be executed after binding
stmt.step(); // insert values

stmt.reset();
stmt.prepare("SELECT * from t");
int ret = stmt.step();
while (SQLITE_ROW == ret) {
	// use operator sqlite3_stmt*() on stmt
	assert(123 == sqlite3_column_int(stmt, 0)); // 0-based
	assert(1.23 == sqlite3_column_double(stmt, 1));
	// sqlite3_column_text returns const unsigned char*
	assert(0 == strcmp("text", (const char*)sqlite3_column_text(stmt, 2)));

	ret = stmt.step();
}
assert(SQLITE_DONE == ret);
```

Note we first iterate over rows, then iterate over columns.

### `sqlite::iterable`

An _iterable_ is a generalization of a null terminated pointer.
It uses `explicit operator bool() const` to detect when it is done.
Values are extracted using `operator*()` and advanced to
the next value with `operator++()`.
An iterable can iterate over the indeterminite number of rows returned by a statement.

```
stmt.reset();
stmt.iterable i(stmt);
while (i) {
	assert(123 == sqlite3_column_int(stmt, 0)); // 0-based
	assert(1.23 == sqlite3_column_double(stmt, 1));
	assert(0 == strcmp("text", (const char*)sqlite3_column_text(stmt, 2)));

	++i;
}
```
The iterable gets a copy of the pointer to `stmt` and changes its state
when `operator++()` calls `sqlite3_step`.

### `sqlite::iterator`

A statement _iterator_ iterates over the columns of a row
returned by an iterable. It is an STL iterator and columns
can be accessed using 0-based `operator[](int)` or by
column name using `operator[](const char*)`.
```
stmt.reset();
stmt::iterable i(stmt);

while (i) {
	auto j = *i;

	assert(123 == (*j).as<int>());
	++j;
	assert(1.23 == (*j).as<double>());
	++j;
	assert((*j).as<std::string_view>() == "text");
	++j;

	++i;
}
```

### `sqlite::value`

A `stmt::iterator::operator*()` returns a `sqlite::value`.
It is not a value type like `std::variant`. It holds
an opaque pointer to a `sqlite3_value*` and a column index to represent the value
of the statement row at that index. It provides 
`template<T> T as()` member functions to extract values of type `T`,
as illustrated above. It also provides `bool operator==(const T&) const`
for known types.
```
stmt.reset();
stmt::iterable i(stmt);
while (i) {
	auto j = *i;
	assert(*j == 123);
	++j;
	assert(*j == 1.23);
	++j;
	assert(*j == "text");
	++j;
	assert(!j);
	assert(j == j.end());

	assert(stmt[1] == 1.23); // access by index
	assert(stmt["c"] == "text"); // access by name
	
	++i;
}
```
We can use C++20 [indirectly_readable](https://en.cppreference.com/w/cpp/iterator/indirectly_readable)
and [indirectly_writable](https://en.cppreference.com/w/cpp/iterator/indirectly_writable)
to furthur simplify things.