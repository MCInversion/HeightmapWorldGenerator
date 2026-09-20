cmake_minimum_required(VERSION 3.21)
project(HeightmapWorldGenerator VERSION 1.0 LANGUAGES C CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/zlib.h")
    message(FATAL_ERROR "zlib submodule is missing. In the source directory run: git submodule update --init --recursive. Then configure again in CMake GUI.")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/laszip/dll/laszip_api.h")
    message(FATAL_ERROR "LASzip submodule is missing. In the source directory run: git submodule update --init --recursive. Then configure again in CMake GUI.")
endif()

# Build the pinned zlib sources ourselves so no installed ZLib package is required.
add_library(WorldZlib STATIC
    third_party/zlib/adler32.c
    third_party/zlib/compress.c
    third_party/zlib/crc32.c
    third_party/zlib/deflate.c
    third_party/zlib/gzclose.c
    third_party/zlib/gzlib.c
    third_party/zlib/gzread.c
    third_party/zlib/gzwrite.c
    third_party/zlib/infback.c
    third_party/zlib/inffast.c
    third_party/zlib/inflate.c
    third_party/zlib/inftrees.c
    third_party/zlib/trees.c
    third_party/zlib/uncompr.c
    third_party/zlib/zutil.c
)
include(CheckIncludeFile)
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    target_compile_definitions(WorldZlib PRIVATE HAVE_UNISTD_H=1)
endif()
target_include_directories(WorldZlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib")
set_target_properties(WorldZlib PROPERTIES FOLDER Dependencies)

# LASzip supplies both the uncompressed LAS writer and LAZ compression. Keep it
# static so the generated executable does not require a companion DLL.
set(LASZIP_BUILD_STATIC ON CACHE BOOL "Build LASzip statically" FORCE)
add_subdirectory(third_party/laszip EXCLUDE_FROM_ALL)
if(WIN32)
    set(WORLD_LASZIP_TARGET laszip3)
else()
    set(WORLD_LASZIP_TARGET laszip)
endif()
set_target_properties(${WORLD_LASZIP_TARGET} PROPERTIES FOLDER Dependencies)

add_executable(HeightmapWorldGenerator
    src/main.cpp
    src/ColorMap.cpp
    src/Heightmap.cpp
    src/PngImage.cpp
    src/PoissonSphere.cpp
    src/SurfaceProjector.cpp
    src/BinaryPly.cpp
    src/LasPointWriter.cpp
)
target_compile_features(HeightmapWorldGenerator PRIVATE cxx_std_20)
find_package(Threads REQUIRED)
target_include_directories(HeightmapWorldGenerator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/laszip/dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/laszip/include/laszip")
target_link_libraries(HeightmapWorldGenerator PRIVATE WorldZlib Threads::Threads ${WORLD_LASZIP_TARGET})
if(MSVC)
    target_compile_options(HeightmapWorldGenerator PRIVATE /W4 /permissive-)
else()
    target_compile_options(HeightmapWorldGenerator PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PoissonSphereTests tests/PoissonSphereTests.cpp src/PoissonSphere.cpp)
    target_compile_features(PoissonSphereTests PRIVATE cxx_std_20)
    target_include_directories(PoissonSphereTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(PoissonSphereTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(PoissonSphereTests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME LargeIndexedSampling COMMAND PoissonSphereTests)
    add_test(NAME SmokePGM COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Smoke.ply"
        --radius 1000 --density 10 --height-min 0 --height-max 100 --seed 42)
    add_test(NAME SmokePNG16 COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gray16.png"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Smoke16.ply"
        --radius 1000 --density 10 --height-min 0 --height-max 100 --seed 42)
    add_test(NAME SmokeNormals COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --output "${CMAKE_CURRENT_BINARY_DIR}/SmokeNormals.ply"
        --radius 1000 --point-count 1000 --height-min 0 --height-max 100
        --height-scale 5 --seed 42 --normals)
    add_test(NAME SmokeColors COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --color-map "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gray16.png"
        --output "${CMAKE_CURRENT_BINARY_DIR}/SmokeColors.ply"
        --radius 1000 --point-count 1000 --height-min 0 --height-max 100
        --height-scale 5 --seed 42)
    add_test(NAME SmokeLAS COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --color-map "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gray16.png"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Smoke.las"
        --radius 1000 --point-count 1000 --height-min 0 --height-max 100
        --height-scale 5 --seed 42 --normals)
    add_test(NAME SmokeLAZ COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --color-map "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gray16.png"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Smoke.laz"
        --radius 1000 --point-count 1000 --height-min 0 --height-max 100
        --height-scale 5 --seed 42 --normals)
    add_test(NAME SmokeLASPosition COMMAND HeightmapWorldGenerator
        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/Gradient.pgm"
        --output "${CMAKE_CURRENT_BINARY_DIR}/SmokePosition.las"
        --radius 1000 --point-count 1000 --height-min 0 --height-max 100 --seed 42)

    add_executable(LasOutputTests tests/LasOutputTests.cpp)
    target_compile_features(LasOutputTests PRIVATE cxx_std_20)
    target_include_directories(LasOutputTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/laszip/dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/laszip/include/laszip")
    target_link_libraries(LasOutputTests PRIVATE ${WORLD_LASZIP_TARGET})
    if(MSVC)
        target_compile_options(LasOutputTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(LasOutputTests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME VerifyLAS COMMAND LasOutputTests
        "${CMAKE_CURRENT_BINARY_DIR}/Smoke.las" uncompressed color-normals)
    add_test(NAME VerifyLAZ COMMAND LasOutputTests
        "${CMAKE_CURRENT_BINARY_DIR}/Smoke.laz" compressed color-normals)
    add_test(NAME VerifyLASPosition COMMAND LasOutputTests
        "${CMAKE_CURRENT_BINARY_DIR}/SmokePosition.las" uncompressed position)
    set_tests_properties(VerifyLAS PROPERTIES DEPENDS SmokeLAS)
    set_tests_properties(VerifyLAZ PROPERTIES DEPENDS SmokeLAZ)
    set_tests_properties(VerifyLASPosition PROPERTIES DEPENDS SmokeLASPosition)
endif()
