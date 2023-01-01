// fms_parse.h
#pragma once
#include <climits>
#include <cmath>
#include <ctype.h>
#include <ctime>
#include <cstring>
#include <compare>
#include <limits>
#include <algorithm>
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
	// len < 0 is error
	template<class T>
	struct view {
		static int length(const T* t, int len = -1)
		{
			int n = 0;

			if (!t) {
				return -1;
			}
			while (len == -1 or len--) {
				if (0 == *t++) {
					break;
				}
				++n;
			}

			return n;
		}
		T* buf;
		int len;
		view()
			: buf{ nullptr }, len{ 0 }
		{ }
		view(T* buf)
			: view(buf, length(buf))
		{ }
		view(T* buf, int len)
			: buf(buf), len(len)
		{ }
		template<size_t N>
		view(T(&buf)[N])
			: view(buf, N - 1)
		{
			int n = length(buf, len);
			if (n >= 0) {
				len = n;
			}
		}


		int compare(const view& w) const
		{
			if (len != w.len) {
				return w.len - len;
			}
				
			return memcmp(buf, w.buf, len * sizeof(T));
		}
		auto operator<=>(const view & w) const
		{
			return compare(w) <=> 0;
		}

		operator bool() const
		{
			return len > 0;
		}
		T& operator*()
		{
			return *buf;
		}
		T operator*() const
		{
			return *buf;
		}
		view& operator++()
		{
			if (len) {
				++buf;
				--len;
			}

			return *this;
		}
		view operator++(int)
		{
			auto t(*this);

			++t;

			return t;
		}

		// use negative len to indicate error
		bool is_error() const
		{
			return len < 0;
		}
		view error() const
		{
			return view(buf, -abs(len));
		}
		// convert error to view
		view<T> error_msg() const
		{
			return view<T>(buf, abs(len));
		}

		view& drop(int n)
		{
			n = std::clamp(-len, n, len);

			if (n > 0) {
				buf += n;
				len -= n;
			}
			else if (n < 0) {
				len += n;
			}

			return *this;
		}
		view& take(int n) {
			n = std::clamp(-len, n, len);

			if (n >= 0) {
				len = n;
			}
			else {
				buf += len + n;
				len = -n;
			}

			return *this;
		}

		bool eat(T t)
		{
			if (len and *buf == t) {
				operator++();

				return true;
			}

			return false;
		}
		bool eat_all(const T* ts, int nts = -1)
		{
			while ((nts == -1 and *ts) or nts--) {
				if (!eat(*ts++)) return false;
			}

			return true;
		}

		bool eat_any(const T* ts, int nts = -1)
		{
			while ((nts == -1 and *ts) or nts--) {
				if (eat(*ts++)) return true;
			}

			return false;
		}

		view& eat_ws()
		{
			while (len and isspace(*buf)) {
				operator++();
			}

			return *this;
		}
	};

	template<class T>
	inline view<T> drop(view<T> v, int n)
	{
		return v.drop(n);
	}
	template<class T>
	inline view<T> take(view<T> v, int n)
	{
		return v.take(n);
	}


	// parse at least min and at most max leading digits to int
	template<class T>
	inline int parse_int(view<T>& v, int min = 0, int max = INT_MAX)
	{
		int i = 0;
		int sgn = 1;

		if (v.len and *v == '-') {
			sgn = -1;
			v.eat('-');
		}
		v.eat('+');

		while (v.len and isdigit(*v)) {
			if (max <= 0) {
				v = v.error();

				return INT_MAX;
			}

			int c = *v - '0';
			if (c < 10) {
				i = 10 * i + (*v - '0');
			}
			else {
				v = v.error();

				return INT_MAX;
			}

			++v;
			--min;
			--max;
		}
		if (min > 0) {
			v = v.error();

			return INT_MIN;
		}

		return sgn*i;
	}

	// [+-]ddd[.ddd][eE][+-]ddd
	template<class T>
	double parse_double(view<T>& v)
	{
		static const double NaN = std::numeric_limits<double>::quiet_NaN();

		double d = parse_int(v);
		if (v.error()) return NaN;

		if (v.eat('.')) {
			double e = .1;
			while (isdigit(*v)) {
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
			if (v.error()) return NaN;

			d *= pow(10., sgn * e);
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
			v = v.error();
			return { y, m, d };
		}
		if (!v.eat(sep)) {
			v = v.error();
			return { y, m, d };
		}

		m = parse_int(v, 1, 2);
		if (v.error()) return { y, m, d };

		if (!v.eat(sep)) {
			v = v.error();
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
			v = v.error();
			return { h, m, s };
		}

		m = parse_int(v, 1, 2);
		if (!v) return { h, m, s };

		if (!v.eat(':')) {
			v = v.error();
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
		ptm->tm_year = y - 1900;
		ptm->tm_mon = m - 1;
		ptm->tm_mday = d;

		if (!v.len) return true;
		if (!v) return false;

		if (!v.eat(' ') and !v.eat('T')) {
			v = v.error();
			return false;
		}

		if (!v.len) return true;

		auto [hh, mm, ss] = parse_hms(v);
		ptm->tm_hour = hh;
		ptm->tm_min = mm;
		ptm->tm_sec = ss;
		if (v.is_error()) return false;

		// [+-]dddd timezone offset
		if (v.len) {
			if (v.eat('.')) { // ignore fractional seconds
				if (!isdigit(*v)) {
					v = v.error();
					return false;
				}
				parse_int(v);
				if (v.error()) {
					return false;
				}
			}
			if (v.eat('Z')) { // Zulu
				return true;
			}
			int sgn = 1;
			if (v.eat('-')) {
				sgn = -1;
			}
			else if (!v.eat('+')) {
				v = v.error();
				return false;
			}
			int tz = parse_int(v);
			if (tz >= 10000) {
				v = v.error();
				return false;
			}
			if (tz < 100) {
				ptm->tm_hour += sgn * tz;
			}
			else {
				ptm->tm_min += sgn * tz % 100;
				ptm->tm_hour += sgn * tz / 100;
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
			T buf[] = {'1', '2', '3'};
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
			T buf[] = { '1', '2', '3', 'x'};
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
			assert(!v and '2' == *v.error_msg());
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
			view<const char> v(buf);
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
