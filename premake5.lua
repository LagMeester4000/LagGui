workspace "LagGui"
	configurations { "Debug", "Release" }

project "LagGui"
	kind "ConsoleApp"
	language "C++"
	targetdir "bin/%{cfg.buildcfg}"

	includedirs { "./inc" }
	files { "**.h", "**.cpp" }

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
