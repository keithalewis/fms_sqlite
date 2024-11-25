// fms_parse.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif // _DEBUG
#include <climits>
#include <cmath>
#include <ctype.h>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <compare>
#include <limits>
#include <string_view>
#include <tuple>

#ifndef _WIN32
#define localtime_s(x,y) localtime_r(y,x)
#define	_gmtime64_s(x, y) gmtime_r(y, x);
#define _gmtime64(x) gmtime(x)
#define _mkgmtime64 timegm
#define __FUNCTION__
#define _stricmp(x, y) strcasecmp(x,y)
#endif

namespace fms {

	template<class T>
	constexpr int compare(const T* t, const T* u, size_t len)
	{
		while (len--) {
			if (*t != *u) {
				return *t - *u;
			}
			++t;
			++u;
		}

		return 0;
	}
	static_assert(compare("abc", "abc", 3) == 0);
	static_assert(compare("abc", "cbd", 3) < 0);
	static_assert(compare("bc", "abc", 2) > 0);

	template<class T>
	constexpr bool is_space(T t)
	{
		return t == ' ' or t == '\t' or t == '\n' or t == '\r' or t == '\v' or t == '\f';
	}
	template<class T>
	constexpr bool is_digit(T t)
	{
		return '0' <= t and t <= '9';
	}

	// non-owning view of buffer
	template<class T>
	struct view {
		T* buf;
		int len; // len < 0 indicates error
		constexpr view()
			: buf{ nullptr }, len{ 0 }
		{
		}
		constexpr view(T* buf, int len)
			: buf(buf), len(len)
		{
		}
		// null terminated buf
		constexpr view(T* buf)
			: buf(buf), len(static_cast<int>(std::basic_string_view<T>(buf).length()))
		{
		}
		// remove null terminator
		template<size_t N>
		constexpr view(T(&buf)[N])
			: view(buf, N - 1)
		{
		}
		constexpr view(const view&) = default;
		constexpr view& operator=(const view&) = default;
		constexpr ~view() = default;

		constexpr auto operator<=>(const view& w) const
		{
			return len != w.len ? len <=> w.len : compare(buf, w.buf, len) <=> 0;
		}

		constexpr operator bool() const
		{
			return len > 0;
		}
		constexpr T& operator*()
		{
			return *buf;
		}
		constexpr T operator*() const
		{
			return *buf;
		}
		constexpr view& operator++()
		{
			if (len > 0) {
				++buf;
				--len;
			}

			return *this;
		}
		constexpr view operator++(int)
		{
			auto t(*this);

			++t;

			return t;
		}

		// use negative len to indicate error
		constexpr bool is_error() const
		{
			return len < 0;
		}
		constexpr view as_error() const
		{
			return view(buf, -abs(len));
		}

		constexpr view& drop(int n)
		{
			n = std::clamp(n, -len, len);

			if (n > 0) {
				buf += n;
				len -= n;
			}
			else if (n < 0) {
				len += n;
			}

			return *this;
		}
		constexpr view& take(int n) {
			n = std::clamp(n, -len, len);

			if (n >= 0) {
				len = n;
			}
			else {
				buf += len + n;
				len = -n;
			}

			return *this;
		}
		// Eat single character t.
		constexpr bool eat(T t)
		{
			if (len and buf and *buf == t) {
				operator++();

				return true;
			}

			return false;
		}
		// Trim leading whitespace.
		constexpr view& eat_ws()
		{
			while (len and is_space(*buf)) {
				operator++();
			}

			return *this;
		}
#ifdef _DEBUG
		static int test()
		{
			static_assert(view<char>{}.len == 0);
			static_assert(view<char>{}.buf == nullptr);
			static_assert(view<const char>{ "abc" }.len == 3);
			static_assert(view<const char>{ "abc" }.buf != nullptr);
			static_assert((view<const char>("abc") <=> view<const char>("abc")) == 0);
			static_assert((view<const char>("abc") <=> view<const char>("bcd")) < 0);
			static_assert((view<const char>("bcd") <=> view<const char>("abc")) > 0);
			static_assert((view<const char>("ab") <=> view<const char>("abc")) < 0);
			static_assert((view<const char>("abc") <=> view<const char>("ab")) > 0);
			static_assert(!view<const char>(""));
			static_assert(*view<const char>("abc") == 'a');
			static_assert(view<const char>("abc").drop(1) == view<const char>("bc"));
			static_assert(view<const char>("abc").drop(-1) == view<const char>("ab"));
			static_assert(view<const char>("abc").drop(0) == view<const char>("abc"));
			static_assert(view<const char>("abc").drop(4) == view<const char>(""));
			static_assert(view<const char>("abc").take(1) == view<const char>("a"));
			static_assert(view<const char>("abc").take(-1) == view<const char>("c"));
			static_assert(view<const char>("abc").take(-4) == view<const char>("abc"));

			{
				view<char> v;
				assert(!v);
				view v2{ v };
				assert(!v2);
				assert(v == v2);
				v = v2;
				assert(!v);
				assert(v2 == v);
				assert(!(v != v2));
			}
			{
				view<const char> v("abc");
				assert(v);
				view<const char> v2(v);
				assert(v2);
				assert(v == v2);
				v = v2;
				assert(v);
				assert(v2 == v);
				assert(!(v != v2));

				assert(v.len == 3);

				assert(v == v.drop(0));
				assert(view<const char>("bc") == v.drop(1));
				assert(view<const char>("ab") == v.drop(-1));
				assert(view<const char>("ab") == v.take(2));
				assert(view<const char>("c") == v.take(-1));
			}

			return 0;
		}
#endif // _DEBUG
	};


	// Function versions
	template<class T>
	constexpr view<T> drop(view<T> v, int n)
	{
		return v.drop(n);
	}
	template<class T>
	constexpr view<T> take(view<T> v, int n)
	{
		return v.take(n);
	}

	// parse at least min and at most max leading digits to int
	template<class T>
	constexpr int parse_int(view<T>& v, int min = 0, int max = INT_MAX)
	{
		int i = 0;
		int sgn = 1;

		if (v.eat('-')) {
			sgn = -1;
		}
		else {
			v.eat('+');
		}

		while (v.len and '0' <= *v and *v <= '9') {
			if (max <= 0) {
				v = v.as_error();

				return INT_MAX;
			}

			int c = *v - '0';
			if (c < 10) {
				int j = *v - '0';
				if (j > INT_MAX - i) {
					v = v.as_error();

					return INT_MAX;
				}
				i = 10 * i + j;
			}
			else {
				v = v.as_error();

				return INT_MAX;
			}

			++v;
			--min;
			--max;
		}
		if (min > 0) {
			v = v.as_error();

			return INT_MIN;
		}

		return sgn * i;
	}
#ifdef _DEBUG
	//static_assert(parse_int(view("123")) == 123);
	constexpr int parse_int_test()
	{
		{
			char buf[] = "123";
			view v(buf);
			assert(123 == parse_int(v));
		}
		{
			wchar_t buf[] = L"123";
			view v(buf);
			assert(123 == parse_int(v));
		}
		{
			char buf[] = "-1";
			view v(buf);
			assert(-1 == parse_int(v));
		}
		{
			char buf[] = "+1";
			view v(buf);
			assert(1 == parse_int(v));
		}
		{
			char buf[] = "0";
			view v(buf);
			assert(0 == parse_int(v));
		}
		{
			char buf[] = "-0";
			view v(buf);
			assert(0 == parse_int(v));
		}
		{
			char buf[] = "+0";
			view v(buf);
			assert(0 == parse_int(v));
		}
		{
			char buf[] = "-";
			view v(buf);
			assert(0 == parse_int(v));
		}

		{
			//view<const char> v("12c");
			//assert(INT_MAX == parse_int(v, 0, 1));
			//assert(!v and '2' == *v.error_msg());
		}

		return 0;
	}
#endif // _DEBUG

	// [+-]ddd[.ddd][eE][+-]ddd
	template<class T>
	double parse_double(view<T>& v)
	{
		static const double NaN = std::numeric_limits<double>::quiet_NaN();

		double d = parse_int(v);
		if (v.is_error()) return NaN;

		if (v.eat('.')) {
			double e = .1;
			while (is_digit(*v)) {
				d += (*v - '0') * e;
				e /= 10;
				++v;
			}
		}

		if (v.eat('e') || v.eat('E')) {
			// +|-ddd
			double sgn = 1;
			if (v.eat('-')) {
				sgn = -1;
			}
			else {
				v.eat('+');
			}
			int e = parse_int(v);
			if (v.is_error()) return NaN;

			d *= pow(10., sgn*e);
		}

		return d;
	}

	// yyyy-mm-dd or yyyy/mm/dd
	template<class T>
	inline std::tuple<int, int, int> parse_ymd(fms::view<T>& v)
	{
		int y = INT_MIN, m = INT_MIN, d = INT_MIN;
		std::remove_const_t<T> sep = 0;

		y = parse_int(v, 1, 4);
		if (!v) return { y, m, d };

		sep = *v;
		if (sep != '-' and sep != '/') {
			v = v.as_error();
			return { y, m, d };
		}
		if (!v.eat(sep)) {
			v = v.as_error();
			return { y, m, d };
		}

		m = parse_int(v, 1, 2);
		if (v.as_error()) return { y, m, d };

		if (!v.eat(sep)) {
			v = v.as_error();
			return { y, m, d };
		}

		d = parse_int(v, 1, 2);

		return { y, m, d };
	}

	// hh:mm:ss
	template<class T>
	inline std::tuple<int, int, int> parse_hms(fms::view<T>& v)
	{
		int h = 0, m = 0, s = 0;

		h = parse_int(v, 1, 2);
		if (!v) return { h, m, s };

		if (!v.eat(':')) {
			v = v.as_error();
			return { h, m, s };
		}

		m = parse_int(v, 1, 2);
		if (!v) return { h, m, s };

		if (!v.eat(':')) {
			v = v.as_error();
			return { h, m, s };
		}

		s = parse_int(v, 1, 2);

		return { h, m, s };
	}

	// parse datetime string into struct tm
	// yyyy-mm-dd[ hh:mm::ss[[+-]dddd]]
	template<class T>
	inline bool parse_tm(fms::view<T>& v, tm* ptm)
	{
		// dddd-d-d shortest valid data
		if (v.len < 8) {
			return false;
		}

		ptm->tm_isdst = -1;
		ptm->tm_hour = 0;
		ptm->tm_min = 0;
		ptm->tm_sec = 0;

		auto [y, m, d] = parse_ymd(v);
		if (v.is_error()) return false;
		ptm->tm_year = y - 1900;
		ptm->tm_mon = m - 1;
		ptm->tm_mday = d;

		if (v.len == 0) return true;

		if (!v.eat(' ') and !v.eat('T')) {
			v = v.as_error();
			return false;
		}

		auto [hh, mm, ss] = parse_hms(v);
		if (v.is_error()) return false;
		ptm->tm_hour = hh;
		ptm->tm_min = mm;
		ptm->tm_sec = ss;

		// [+-]dddd timezone offset
		if (v.len) {
			if (v.eat('.')) { // ignore fractional seconds
				if (!is_digit(*v)) {
					v = v.as_error();
					return false;
				}
				parse_int(v);
				if (v.is_error()) {
					return false;
				}
			}
			if (v.eat('Z')) { // Zulu
				return true;
			}
			if (v.len) {
				int sgn = 1;
				if (v.eat('-')) {
					sgn = -1;
				}
				else if (!v.eat('+')) {
					v = v.as_error();
					return false;
				}
				int tz = parse_int(v);
				if (tz >= 10000) { // only hhmm allowed
					v = v.as_error();
					return false;
				}
				if (tz < 100) {
					ptm->tm_hour += sgn * tz;
					if (v.eat(':')) { // hh:mm
						tz = parse_int(v);
						if (tz >= 60) {
							v = v.as_error();
							return false;
						}
						ptm->tm_min += sgn * tz;
					}
				}
				else {
					ptm->tm_min += sgn * tz % 100;
					ptm->tm_hour += sgn * tz / 100;
				}
			}
		}

		return true;
	}


#ifdef _DEBUG
#include <cassert>

	template<class T>
	inline int parse_test()
	{
		{
			view<T> v;
			assert(!v);
			view<T> v2{ v };
			assert(!v2);
			assert(v == v2);
			v = v2;
			assert(!v);
			assert(v2 == v);
		}
		{
			T buf[] = { 'a', 'b', 'c' };
			view v(buf, 3);
			assert(v.len == 3);

			assert(drop(v, 0) == v);
			assert(!drop(v, 3));
			assert(drop(v, 1) == view(buf + 1, 2));
			assert(drop(v, -1) == view(buf, 2));

			assert(take(v, 3) == v);
			assert(!take(v, 0));
			assert(take(v, 1) == view(buf, 1));
			assert(take(v, -1) == view(buf + 2, 1));
		}
		{
			T buf[] = { '1', '2', '3' };
			view v(buf, 3);
			assert(123 == parse_int(v));
			assert(!v);
		}
		{
			T buf[] = { '-', '1' };
			view v(buf, 2);
			assert(-1 == parse_int(v));
			assert(!v);
		}
		{
			T buf[] = { '1', '2', '3', 'x' };
			view v(buf, 4);
			assert(123 == parse_int(v));
			assert(v);
			assert('x' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(12 == parse_int(v));
			assert(v and 'c' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(12 == parse_int(v, 2, 2));
			assert(v and 'c' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(INT_MAX == parse_int(v, 0, 1));
			//assert(!v and '2' == *v.error_msg());
		}
		{
			const char date[] = "2022-01-02 3:04:5Z";
			view v(date);
			auto [y, m, d] = parse_ymd<const char>(v);
			assert(2022 == y);
			assert(1 == m);
			assert(2 == d);
			v.eat_ws();
			auto [hh, mm, ss] = parse_hms<const char>(v);
			assert(3 == hh);
			assert(4 == mm);
			assert(5 == ss);
		}
		{
			time_t t = time(nullptr);
			tm tm;
			localtime_s(&tm, &t);
			char buf[256];
			strftime(buf, 256, "%Y-%m-%d %H:%M:%S", &tm);
			view<const char> v(&buf[0]);
			assert(parse_tm(v, &tm));
			time_t u = mktime(&tm);
			assert(t == u);
		}
		{
			const char* buf = "2022-1-1";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 0);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.0Z";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 0);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.-0Z";
			view<const char> v(buf);
			tm tm;
			assert(!parse_tm(v, &tm));
		}
		{
			const char* buf = "2022-1-1T0:0:0.0+1";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 1);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.0-1";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 23);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.0+0130";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 1);
			assert(tm.tm_min == 30);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.0-0130";
			view<const char> v(buf);
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime64(&tm);
			_gmtime64_s(&tm, &u);
			assert(tm.tm_hour == 22);
			assert(tm.tm_min == 30);
			assert(tm.tm_sec == 0);
		}

		return 0;
	}

#endif // _DEBUG

} // namespace fms
