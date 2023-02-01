# fms_sqlite

A breviloquent header only C++ wrapper for the SQLite C API.

On linuxy platforms with g++ type `make check` to build tests and run valgrind.

```
sqlite::db db(""); // in-memory database
sqlite::stmt stmt(::db);
stmt.exec("DROP TABLE IF EXISTS t");
stmt.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)");

stmt.prepare("INSERT INTO t VALUES (?, ?, :c)");
stmt[0] = 123; // calls sqlite3_bind_int(stmt, 0 + 1, 123);
stmt[1] = 1.23;
stmt[":c"] = "str"; // bind parameter name
assert(SQLITE_DONE == stmt.step());

stmt.prepare("SELECT * FROM t");
stmt.step();
assert(stmt[0] == 123);
assert(stmt["b"] == 1.23); // lookup by column name
assert(stmt[2] == "str");
assert(SQLITE_DONE == stmt.step());
```

## Usage

This library does not wrap all of the functions in the SQLite C API
but it does make it convenient to use them from C++. 
The classes `sqlite::db` and `sqlite::stmt` wrap `sqlite3*` and `sqlite3_stmt*` 
pointers using RAII to manage memory allocation.  Their
copy constructor and copy assignment members are deleted so they work
similar to [linear types](https://ncatlab.org/nlab/show/linear+type+theory). 
They provide `operator sqlite3*() const` and `operator sqlite3_stmt*() const` member functions
so they can be passed as arguments to `sqlite3_*` C functions.

There are many [high quality implementations](http://srombauts.github.io/SQLiteCpp/)
of C++ wrappers for SQLite.
I reviewed those before writing this library and am grateful to the open source
library creators who allow others to leverage off their excellent work.
It seems everyone who looks closely at SQLite finds
a different solution to accomodating its variations from standard SQL.

None of them had affordances for counted strings or dealing with
JSON/BSON objects where unordered keys need to be mapped to SQLite
column names. Another issue was preserving the fidelity of boolean
and datetime types. 
This library makes it simple to copy data in and out of SQLite while
preserving fidelity using `sqlite::copy`. If you need to perform a
transformation while doing this use `sqlite::map`.

### `sqlite::db`

Create a new, or open an existing, SQLite database with 
[`sqlite::db db(file, flags)`](https://www.sqlite.org/c3ref/open.html).
Use `sqlite::db db("")` to create a temporary in-memory database.
Its destructor calls [`sqlite3_close`](https://www.sqlite.org/c3ref/close.html).
Its copy constructor and copy assignment operators are deleted to
ensure its destructor is only called once. 
It provides `operator sqlite3*() const` to make it convenient to call any SQLite C API function
by passing a `sqlite::db` object.
One feature of this library is there are many functions in the SQLite C API
it does not have wrappers for.
Make sure your `sqlite::db` exists while operating on a database.
Use `}` for garbage collection/borrow checking.

### `sqlite::stmt`

Create a SQLite statement with [`sqlite::stmt stmt(db)`](https://www.sqlite.org/c3ref/stmt.html)
to execute SQL commands on a database.
Its destructor calls [`sqlite3_finalize`](https://www.sqlite.org/c3ref/finalize.html).
Its copy constructor and copy assignment operators are deleted to
ensure its destructor is only called once. 
It provides `operator sqlite3_stmt*() const` to make it convenient to call any SQLite C API function
by passing a `sqlite::stmt` object.
This is another feature of this library that is almost as good as Rust linear types.
Make sure your `sqlite::stmt` exists while operating on a database.
Use `}` for garbage collection/borrow checking.

Compile a SQL statement with `stmt.prepare(sql)` and call `stmt.bind(index, value)`
to set `?NNN` parameters if necessary. The index is 1-based.
Use `stmt.bind("@name", value)` for `@name` parameters in the prepared statement.
You can also use `:name` or `$name` if you used those in the prepared statement.

Execute the SQL statement using `stmt.step()`. If it returns `SQLITE_ROW`
keep calling `stmt.step()` until it returns `SQLITE_DONE`.

## Typing

SQLite has [flexible typing](https://www3.sqlite.org/flextypegood.html).  
Internally, everything is one of the 
[Fundamental Datatypes](https://www3.sqlite.org/c3ref/c_blob.html) 
64-bit signed integer, 64-bit IEEE floating point, character string, BLOB, or NULL. 
These correspond to `SQLITE_INTEGER`, `SQLITE_FLOAT`, `SQLITE_TEXT`, `SQLITE_BLOB`,
and `SQLITE_NULL` respectively. The function 
[`int sqlite3_column_type(sqlite3_stmt* stmt, int i)`](https://www3.sqlite.org/c3ref/column_blob.html) 
returns the internal type of column `i` for an active `stmt`.
Database operations might change the internal column types.
Types in SQLite are so flexible they can change due to a query, unlike most SQL databases. 

Flexible typing makes it convenient to use SQLite as a key-value store. 
The first column is text and the second column can be 
any [Dynamically Typed Value Object](https://www.sqlite.org/c3ref/value.html). 
It is not so convenient for getting data in and out of SQLite while preserving 
their original types.

## `fms_sqlite.h`

The file `fms_sqlite.h` defines `SQLITE_NUMERIC` 
so [`int sqlite::affinity(const char*)`](https://sqlite.org/datatype3.html#determination_of_column_affinity). 
can be implemented based on the SQLite affinity algorithm for declared column types.

It also defines the _extended types_ `SQLITE_BOOLEAN` and `SQLITE_DATETIME`.

Boolean values are stored as `SQLITE_INTEGER`.

Datetime types can be stored as 
`SQLITE_INTEGER` for unix 64-bit `time_t` in seconds since midnight 1970/1/1,
`SQLITE_FLOAT` for Gregorian days since noon in Greenwich time on November 24, 4714 B.C., or 
`SQLITE_TEXT` for ISO 8601-ish strings.

The SQLite function 
[`int sqlite3_column_type(sqlite3_stmt*, int index)`](https://sqlite.org/c3ref/column_blob.html)
returns the fundamental type SQLite used to store data in the active statment
using 0-based column `index`.

The SQLite function [`const char* sqlite3_column_decltype(pstmt, index)`](https://sqlite.org/c3ref/column_decltype.html)
returns the type of the table column declared in `CREATE TABLE`. The character string
returned by this must be parsed in order to preserve the fidelity of boolean
and datetime types.

Use `int sqltype(std::string_view sqlname)` to convert the declared
type in `CREATE TABLE` to an extended type and `const char* sqlname(int sqltype)`
to convert an extended type to a character string that can be used
in `CREATE TABLE`.

### `sqlite::values`

The class `sqlite::stmt` publicly inherits from `sqlite::values` to provide
a view on statements. It assumes the lifetime of the statement used to construct it.
It implements copy constructor and copy assignment by default to copy the
statement pointerand the destructor is a no-op, just like any view type.

Use it to access values that are a result of executing a query.
This class implements `values::column_`_type_`(i)` for the 0-based index `i`
which call the raw `sqlite3_column_`_type_ functions. 

Use it to bind values to a prepared statement before executing it.
This class implements  `sqlite3_bind_`_type_`(j)` for the 1-based index `j`
which calls `sqlite3_bind_`_type_
for fundamental and extended types. 

### `sqlite::value`

The columns of a statement can be accessed with another view class `sqlite::value(stmt, i)`,
where `i` is the 0-based index of the row `stmt` currently holds.
Use `int sqlite::value::type()` to detect the declared type and 
`sqlite::value::as`_type_`()` to retrieve data as _type_. The function
`sqlite::value::column_type()` returns the the fundamental type used to store the value.
To bind a value of type `T` use `template<typename T> operator=(const T&)`.
It knows the column index needs to have 1 added before binding.

### `sqlite::iterator`

Use `sqlite::iterator` to iterate over columns of a statement.
It satisfies the requirements of an STL forward iterator.
Its `value_type` is `sqlite::value`.

### `sqlite::cursor`

Use `sqlite::cursor` to iterate over all rows returned by a query.
It does not have an STL `end()`. Use `explicit operator bool() const`
to detect when `stmt::step()` returns `SQLITE_DONE`.
Its `value_type` is `sqlite::iterator`

```
cursor c(stmt);
while (c) {
	... iterator i = *c ...
	++c;
}
```

### `sqlite::copy`

Use `sqlite::copy` to get data out from or in to a SQLite database.
There are implementations for `sqlite::iterator` and `sqlite::cursor`.

To get the columns of a row out from a statement iterator use 
`template<class O> copy(sqlite::iterator i, O o)` where `O`
satisfies the requirements of an output iterator.

To insert values of an input iterator use 
`template<class I> copy(I i, sqlite::iterator o)` where `I`
satisfies the requirements of an input iterator and has
functions `I::type()` and `I::as`_xxx_ for all extended types.
If it has an `I::name()` function that will be used to
insert a value by name.

To get all rows from a statement cursor use 
`template<class O> copy(sqlite::cursor i, O o)` where `O`
satisfies the requirements of an output iterator.

To insert all rows from of a cursor use 
`template<class I> copy(I i, sqlite::cursor o)` where `I`
satisfies the requirements of an input iterator and has
functions `I::type()` and `I::as`_xxx_ for all extended types.
If it has an `I::name()` function that will be used to
insert a value by name.


## Compatibility

One of the many reasons for SQLite's popularity is that it accommodates
using SQL [CREATE TABLE](https://www3.sqlite.org/lang_createtable.html) 
syntax encountered in the wild. 
The type-name used in a 
[column definition](https://www3.sqlite.org/syntax/column-def.html)
is made available by  
the function [`sqlite3_column_decltype`](https://www3.sqlite.org/c3ref/column_decltype.html).

This is used when inserting or `CAST`ing values based on 
[Column Affinity](https://sqlite.org/datatype3.html#determination_of_column_affinity).
External types are converted to a fundamental SQLite type based on this, except for
the quirky `NUMERIC` affinity to handle `DECIMAL(?,?)`, `BOOLEAN`, `DATE`,
and `DATETIME` types.
There is no definition of `SQLITE_NUMERIC` in the SQLite source code.


### `SQLITE_BOOLEAN`

A SQLite boolean is stored as an integer where non-zero indicates `true`.
The function `sqlite_type` returns `SQLITE_BOOLEAN` if a column
was declare with a type starting with `BOOL`.

### `SQLITE_DATETIME`

The [`datetime`](https://www.sqlite.org/lang_datefunc.html) type is a union
that can contain a floating point Gregorian date, 64-bit integer `time_t` seconds
since Unix epoch, or an ISO 8601-ish format string.
The function `sqlite_type` returns `SQLITE_DATETIME` if a column
was declare with a type starting with `DATE`.

Some affordances have been made for constructing a `datetime` type
to use with SQLite. It has constructors from floating point, time_t,
and text, but SQLite only recognizes a subset of 
[ISO 8601](https://www.w3.org/TR/NOTE-datetime) string formats.

Use `datetime::as_time_t()` to convert a `datetime` with type `SQLITE_TEXT` to
the preferred integer `time_t` representation.
It uses `fms_parse.h` to convert non-ISO 8601 strings
from the wild into valid dates. See [Posel's law](https://en.wikipedia.org/wiki/Jon_Postel)

## Example

This library endeavors to provide the thinnest possible C++ API to the SQLite C API.

```
sqlite::db db(""); // create an in-memory database
sqlite::stmt stmt(db); // calls operator sqlite3*() on db
stmt.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)");

stmt.prepare("INSERT INTO t VALUES (?, ?, :c)");
stmt[0] = 123; // calls sqlite3_bind_int(stmt, 0 + 1, 123);
stmt[1] = 1.23;
stmt[":c"] = "str"; // bind parameter name
assert(SQLITE_DONE == stmt.step());

stmt.prepare("SELECT * FROM t");
stmt.step();
assert(stmt[0] == 123);
assert(stmt["b"] == 1.23); // lookup by name
assert(stmt[2] == "str");
assert(SQLITE_DONE == stmt.step());
```

Note we first iterate over rows, then iterate over columns.

