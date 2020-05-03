local buildroot = ""
if _ACTION then buildroot = _ACTION end

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --

solution "zak"
  location(buildroot)
	configurations { "Debug", "Release" }
	startproject "simplest"	
	targetdir "../bin"
	debugdir "../bin"
	flags { "NoExceptions", "NoRTTI", "NoPCH" }
    if (os.is("Windows")) then flags {"StaticRuntime"} end
	if (os.is("Windows")) then defines { "_CRT_SECURE_NO_WARNINGS" } end
    configuration { "x32", "Debug" }
        targetsuffix "_x86_d"   
    configuration { "x32", "Release" }
		flags {	"EnableSSE2" }
        targetsuffix "_x86"
    configuration { "x64", "Debug" }
        targetsuffix "_x64_d"    
    configuration { "x64", "Release" }
        targetsuffix "_x64"
    configuration { "Release" }
    	flags { "Optimize", "OptimizeSpeed", "NoEditAndContinue", "No64BitChecks" }   
		defines { "NDEBUG" }
		objdir (buildroot .. "/release")
    configuration { "Debug" }
		flags {"Symbols" }
		defines { "DEBUG" }
		objdir (buildroot .. "/debug")
	
    configuration {}

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --

project "tedsid2zak"
	kind "ConsoleApp"
	language "C++"
	files {
	  "../tedsid2zak/**.cpp"
	}
	targetname "tedsid2zak"

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --


project "ay2zak"
	kind "ConsoleApp"
	language "C++"
	files {
	  "../ay2zak/*.c*"
	}
	targetname "ay2zak"

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --


project "ym2zak"
	kind "ConsoleApp"
	language "C++"
	files {
	  "../ym2zak/*.c*"
	}
	targetname "ym2zak"

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --

project "zakopt"
	kind "ConsoleApp"
	language "C++"
	files {
	  "../src/tools/zakopt/*.c*"
	}
	targetname "zakopt"

-- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< -- 8< --
    
