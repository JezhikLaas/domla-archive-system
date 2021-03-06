cmake_minimum_required(VERSION 3.8)

project("Domla Archive")

enable_testing()
find_package(ICU)
find_package(Ice)
find_package(Boost)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/lib ${CMAKE_CURRENT_SOURCE_DIR}/src/archs/backend/bzip2-1.0.5 $ENV{XAPIAN_HOME}/include ${Ice_INCLUDE_DIR} ${ICU_INCLUDE_DIR} SYSTEM ${Boost_INCLUDE_DIRS})
file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/tests/*.cc)

add_custom_command(
    OUTPUT ../src/lib/Archive.cpp ../src/lib/Common.cpp ../src/lib/Authentication.cpp ../src/lib/Administrator.cpp
    COMMAND ${Ice_SLICE2CPP_EXECUTABLE} --output-dir ../src/lib ../src/lib/Ice/Archive.ice ../src/lib/Ice/Common.ice ../src/lib/Ice/Authentication.ice ../src/lib/Ice/Administrator.ice
    DEPENDS ../src/lib/Ice/Archive.ice ../src/lib/Ice/Common.ice ../src/lib/Ice/Authentication.ice ../src/lib/Ice/Administrator.ice
)

if (WIN32)
    link_directories(${Ice_INCLUDE_DIR}/../lib/vc140 ${Boost_INCLUDE_DIRS}/lib32-msvc-14.0 ${Ice_INCLUDE_DIR}/../lib)
endif (WIN32)

add_library(
    archlib STATIC
    src/lib/Common.cpp
    src/lib/Archive.cpp
    src/lib/Authentication.cpp
    src/lib/Administrator.cpp
    src/lib/utils.cxx
)

add_executable(
    archc
    src/archc/main.cxx
    src/archc/commands.cxx
    src/archc/command.cxx
    src/archc/backup.cxx
    src/archc/rebuild.cxx
    src/archc/restore.cxx
)

target_link_libraries(
    archc
    archlib
)

add_library(
    sqlitelib STATIC
    src/archs/backend/sqlite/sqlite3.c
)

target_compile_definitions(
    sqlitelib
    PUBLIC SQLITE_THREADSAFE=2 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_ENABLE_FTS5 SQLITE_ENABLE_ICU
)

if (WIN32)
    target_link_libraries(
        sqlitelib
        ${ICU_INCLUDE_DIR}/../lib/icuucd.lib
        ${ICU_INCLUDE_DIR}/../lib/icuind.lib
    )
endif (WIN32)

add_library(
    bzip2lib STATIC
    src/archs/backend/bzip2-1.0.5/blocksort.c
    src/archs/backend/bzip2-1.0.5/bzlib.c
    src/archs/backend/bzip2-1.0.5/compress.c
    src/archs/backend/bzip2-1.0.5/crctable.c
    src/archs/backend/bzip2-1.0.5/decompress.c
    src/archs/backend/bzip2-1.0.5/huffman.c
    src/archs/backend/bzip2-1.0.5/randtable.c
)

target_compile_definitions(
    bzip2lib
    PUBLIC _CRT_SECURE_NO_WARNINGS
)

add_library(
    backendlib STATIC
    src/archs/backend/binary_data.cxx
    src/archs/backend/data_bucket.cxx
    src/archs/backend/document_schema.cxx
    src/archs/backend/document_storage.cxx
    src/archs/backend/sqlite.cxx
    src/archs/backend/transformer.cxx
    src/archs/backend/virtual_tree.cxx
    src/archs/backend/full_text.cxx
    src/archs/backend/settings_provider.cxx
)

add_executable(
    archs
    src/archs/main.cxx
)

target_link_libraries(
    archs
    backendlib
    bzip2lib
    sqlitelib
    archlib
    $ENV{XAPIAN_HOME}/.libs/xapian-win.lib
)

foreach(testSrc ${TEST_SRCS})
    get_filename_component(testName ${testSrc} NAME_WE)
    add_executable(${testName} ${testSrc})

    target_link_libraries(
        ${testName}
        backendlib
        sqlitelib
        archlib
        bzip2lib
        $ENV{XAPIAN_HOME}/.libs/xapian-win.lib
    )

    set_target_properties(
        ${testName} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/testBin
    )

    add_test(
        NAME ${testName} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/testBin 
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testBin/${testName}
    )
endforeach(testSrc)

