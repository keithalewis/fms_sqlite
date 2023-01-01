SQLITE_DIR = sqlite-amalgamation-3390400
CXXFLAGS += -g -std=c++2b -D_DEBUG -Wall -Wno-unknown-pragmas
LDFLAGS = -pthread -ldl

fms_sqlite: fms_sqlite.cpp sqlite3.o

sqlite3.o: $(SQLITE_DIR)/sqlite3.c
	$(CC) -DSQLITE_OMIT_LOAD_EXTENSION -c $<

.PHONY: clean test check

clean:
	rm -f *.o fms_sqlite

test: fms_sqlite
	./fms_sqlite

check: fms_sqlite
	valgrind --leak-check=yes ./fms_sqlite
