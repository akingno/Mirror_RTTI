cmake_minimum_required(VERSION 3.15)
project(MirrorEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MirrorTool Tools/MirrorTool/main.cpp)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

# 搜集所有.h
file(GLOB_RECURSE ALL_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(REFLECTED_CPP_FILES "")

foreach(HEADER_FILE ${ALL_HEADERS})

    file(READ ${HEADER_FILE} FILE_CONTENT)
    string(FIND "${FILE_CONTENT}" "reflect_struct" FOUND_POS)

    if(NOT FOUND_POS EQUAL -1)
        get_filename_component(BASENAME ${HEADER_FILE} NAME_WE)

        set(OUTPUT_CPP "${GENERATED_DIR}/${BASENAME}_reflected.cpp")

        add_custom_command(
                OUTPUT  ${OUTPUT_CPP}
                COMMAND MirrorTool ${HEADER_FILE} ${OUTPUT_CPP}
                DEPENDS MirrorTool ${HEADER_FILE}
                COMMENT "正在解析并生成反射代码: ${BASENAME}.h"
        )

        list(APPEND REFLECTED_CPP_FILES ${OUTPUT_CPP})
    endif()

endforeach()


add_executable(MyApp
        main.cpp
        ${REFLECTED_CPP_FILES}
)

target_include_directories(MyApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/
)