#!lua

includeDirList = { 
    "shared",
    "shared/gl3w",
    "shared/imgui",
    "shared/include",
    "shared/stb"
}

libDirectories = { 
    "lib"
}

linkLibs = {
    "glframework",
    "imgui",
    "glfw3",
    "lua",
    "GL",
    "Xinerama",
    "Xcursor",
    "Xxf86vm",
    "Xi",
    "Xrandr",
    "X11",
    "stdc++",
    "dl",
    "pthread",
    "IrrKlang"
}

buildOptions = {"-std=c++11"}

linkOptions = {"'-Wl,-rpath,$$ORIGIN/lib'"}

-- Build glfw3 static library and copy it into <cs488_root>/lib if it is not
-- already present.
if not os.isfile("lib/libglfw3.a") then
    os.chdir("shared/glfw-3.1.1")
    os.mkdir("build")
    os.chdir("build")
    os.execute("cmake ../")
    os.execute("make")
    os.chdir("../../../")
    os.mkdir("lib")
    os.execute("cp shared/glfw-3.1.1/build/src/libglfw3.a lib/")
end

-- Build lua-5.3.1 library and copyt it into <cs488_root>/lib if it is not
-- already present.
if not os.isfile("lib/liblua.a") then
    os.chdir("shared/lua-5.3.1")
    os.execute("make linux")

    os.chdir("../../")
    os.execute("cp shared/lua-5.3.1/src/liblua.a lib/")
end


solution "BuildStaticLibs"
    configurations { "Debug", "Release" }

    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols" }

    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize" }

    -- Builds glframework static library
    project "glframework"
        kind "StaticLib"
        language "C++"
        location "build"
        objdir "build"
        targetdir "lib"
        buildoptions (buildOptions)
        includedirs (includeDirList)
        files { "shared/glframework/*.cpp" }

    -- Build imgui static library
    project "imgui"
        kind "StaticLib"
        language "C++"
        location "build"
        objdir "build"
        targetdir "lib"
        includedirs (includeDirList)
        includedirs {
            "shared/imgui/examples/opengl3_example",
            "shared/imgui/examples/libs/gl3w/",
        }
        files { 
            "shared/imgui/*.cpp",
            "shared/gl3w/GL/gl3w.c"
        }

    -- Build lodepng static library
    project "lodepng"
        kind "StaticLib"
        language "C++"
        location "build"
        objdir "build"
        targetdir "lib"
        includedirs (includeDirList)
        includedirs {
            "shared/lodepng"
        }
        files { 
            "shared/lodepng/lodepng.cpp"
        }

    project "Bouncer"
        kind "ConsoleApp"
        language "C++"
        location "build"
        objdir "build"
        targetdir "."
        buildoptions (buildOptions)
        linkoptions (linkOptions)
        libdirs (libDirectories)
        links (linkLibs)
        includedirs (includeDirList)
        files { "src/*.cpp" }
