cmake_minimum_required (VERSION 3.8)
project (fms_sqlite
  VERSION 0.0.1 # https://semver.org/spec/v0.1.0.html
  DESCRIPTION "C++ wrapper for sqlite"
  HOMEPAGE_URL "https://github.com/keithalewis/fms_sqlite"
  LANGUAGES C CXX
)
add_executable(fms_sqlite fms_sqlite.cpp sqlite-amalgamation-3460000/sqlite3.c)
find_package(Threads)
if(UNIX)
	target_link_libraries(fms_sqlite PRIVATE Threads::Threads dl)
endif()
if(MSVC)
	target_link_libraries(fms_sqlite)
endif()
target_compile_features(fms_sqlite PUBLIC cxx_std_23)
enable_testing()
