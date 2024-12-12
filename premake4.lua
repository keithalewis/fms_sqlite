workspace "sqlite"
	configurations { "Debug", "Release" }

project "fms_sqlite"
	kind "ConsoleApp"
	language "C++"
	file { "**.h", "**.cpp" }

	filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "On"
