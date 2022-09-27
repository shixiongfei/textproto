solution ( "textproto-test" )
  configurations { "Release", "Debug" }
  platforms { "x64" }

  if _ACTION == "clean" then
    os.rmdir(".vs")
    os.rmdir("test")
    os.remove("textproto-test.VC.db")
    os.remove("textproto-test.sln")
    os.remove("textproto-test.vcxproj")
    os.remove("textproto-test.vcxproj.filters")
    os.remove("textproto-test.vcxproj.user")
    os.remove("textproto-test.make")
    os.remove("Makefile")
    return
  end

  -- A project defines one build target
  project ( "textproto-test" )
  kind ( "ConsoleApp" )
  language ( "C" )
  targetname ("textproto-test")
  files { "./*.h", "./*.c",
          "./sstr/sstr.h", "./sstr/sstr.c"}
  includedirs { "./sstr" }
  defines { "_UNICODE" }
  staticruntime "On"

  configuration ( "Release" )
    optimize "On"
    objdir ( "./test/tmp" )
    targetdir ( "./test" )
    defines { "NDEBUG", "_NDEBUG" }

  configuration ( "Debug" )
    symbols "On"
    objdir ( "./test/tmp" )
    targetdir ( "./test" )
    defines { "DEBUG", "_DEBUG" }

  configuration ( "vs*" )
    defines { "WIN32", "_WIN32", "_WINDOWS",
              "_CRT_SECURE_NO_WARNINGS", "_CRT_SECURE_NO_DEPRECATE",
              "_CRT_NONSTDC_NO_DEPRECATE", "_WINSOCK_DEPRECATED_NO_WARNINGS" }

  configuration ( "gmake" )
    warnings  "Default" --"Extra"

  configuration { "gmake", "macosx" }
    defines { "__APPLE__", "__MACH__", "__MRC__", "macintosh" }

  configuration { "gmake", "linux" }
    defines { "__linux__" }

  configuration { "gmake", "bsd" }
    defines { "__BSD__" }
