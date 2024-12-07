# fms_sqlite

A parsimonious header only C++ wrapper for the 
[SQLite C API](https://www.sqlite.org/c3ref/intro.html)
that is faithul to its naming conventions.

Although SQLite is the most
[widely deployed and used](https://www.sqlite.org/mostdeployed.html)
database engine in the world it is not the most widely understood.
SQLite is a 
[self-contained, serverless, zero-configuration, transactional SQL database engine](https://www.sqlite.org/about.html).
SQL statements that work on statically typed databases 
work the same way in SQLite. 
However, the dynamic typing in SQLite allows it to do things which are not possible 
in traditional rigidly typed databases. 
The datatype of a value is associated with the value itself, not with its container. 

The two main SQLite C structs are 
[`sqlite3`](https://sqlite.org/c3ref/sqlite3.html), 
and [`sqlite3_stmt`](https://sqlite.org/c3ref/stmt.html)
They are wrapped by `sqlite::db` and `sqlite::stmt`.
Their copy constructors and copy assignments are deleted so they work
similar to [linear types](https://ncatlab.org/nlab/show/linear+type+theory) used in Rust. 
They provide `operator sqlite3*() const noexcept` 
and `operator sqlite3_stmt*() const noexcept` member functions
so they can be passed as arguments to any SQLite C API function.
The `fms_sqlite` library does not wrap all of the functions in the SQLite C API.

An important but less understood SQLite C struct is 
[`sqlite3_value`](https://sqlite.org/c3ref/value.html).
It is used to convert the bits c contained in SQLite to a C type.

## Example

Create an in-memory database.
```cpp
	sqlite::db db(""); // in-memory database
	db.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)"); // call sqlite3_exec
```

Insert values.
```
	db.exec("INSERT INTO t VALUES (123, 1.23, 'text')");
```

Use a statement to insert values into a database.
```cpp
	sqlite::stmt stmt;
	// Call sqlite3_prepare_v2 for a database.
	stmt.prepare(db, "INSERT INTO t VALUES (?, ?, ?)");
	// Bind values to the statement.
	stmt[0] = 123;   // SQLITE_INTEGER
	stmt[1] = 1.23;  // SQLITE_FLOAT
	stmt[2] = "text"; // SQLITE_TEXT
	stmt.step(); // calls sqlite3_step(stmt);
```

Query the database.
```cpp
	stmt.prepare(db, "SELECT * FROM t");
	assert(SQLITE_ROW == stmt.step());
	assert(stmt[0] == 123);
	assert(stmt[1] == 1.23);
	assert(stmt[2] == "text");
	assert(SQLITE_DONE == stmt.step());
```

The C++ wrapper is faithful to the SQLite C API naming conventions.

The SQLite value object https://www.sqlite.org/c3ref/value.html

On Unix platforms with g++ type `make check` to build tests and run valgrind.

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
pointers using RAII to manage memory allocation.  

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


## FAQ

<dl>
<dt>Why isn't the namespace called `sqlite3`_?</td>
<dd>Because the SQLite C API uses the struct `sqlite3`.<dd>
</dl>

Why..

```
sqlite::db db;
db.exec("CREATE TABLE t (a INT, b FLOAT, c TEXT)");
stmt(db, "SELECT * FROM t");
while (stmt.step() == SQLITE_ROW) {
	int a = stmt[0];
	double b = stmt[1];
	std::string c = stmt[2];
}
```