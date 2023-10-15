workspace "LagGui"
	configurations { "Debug", "Release" }

-- TODO: Make separate config for raylib test and library

project "LagGui"
	kind "ConsoleApp"
	language "C++"
	targetdir "bin/%{cfg.buildcfg}"

	-- TODO: Enable again when building raylib separately
	-- Currently this doesn't work in debug mode because my Raylib binaries are only for release mode
	--flags { "FatalWarnings" }

	includedirs { "./inc" }
	files { "**.h", "**.cpp", "**.c" }

	libdirs { "lib/raylib/lib" }
	includedirs { "./lib/raylib/include" }
	links { "raylib", "windowsapp" }

    architecture "x64"

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
