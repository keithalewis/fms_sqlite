// fms_error.h - error reporting
#pragma once
#include <exception>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace fms {

	// Error string base on souce location.
	class error {
		std::string message;
	public:
		// file: <file>
		// line: <line>
		// [func: <func>]
		// mesg: <mesg>
		error(const std::string_view& mesg, const std::source_location& loc = std::source_location::current())
			: message{ std::format("file: {}\nline: {}", loc.file_name(), loc.line()) }
		{
			if (loc.function_name()) {
				message.append("\nfunc: ").append(loc.function_name());
			}
			message.append("\nmesg: ").append(mesg);
		}
		error(const error&) = default;
		error& operator=(const error&) = default;
		~error() = default;

		// near: <near>
		// here: ---^
		error& at(const std::string_view& near, int here = 0)
		{
			if (!near.empty()) {
				message.append("\nnear: ").append(near);
				if (here > 0) {
					message.append("\nhere: ").append(here, '-').append("^");
				}
			}

			return *this;
		}

		const char* what() const noexcept
		{
			return message.c_str();
		}

	};


} // namespace fms
