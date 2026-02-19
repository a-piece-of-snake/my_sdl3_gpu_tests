cmake_minimum_required(VERSION 3.20)
project(sdltests VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============== 寻找包 (系统级) ===============
find_package(SDL3 REQUIRED CONFIG)
# find_package(SDL3_image REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(imgui REQUIRED CONFIG)

# =============== 源文件列表 ===============
set(SOURCES
    main.cpp
)

# =============== 可执行文件 ===============
add_executable(main ${SOURCES})

# =============== 包含目录 ===============
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR})

# =============== 链接库 ===============
target_link_libraries(main PRIVATE 
    SDL3::SDL3
    OpenGL::GL
    imgui::imgui
)
# =============== 复制 Content 文件夹 ===============
set(CONTENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Content)
set(CONTENT_BINARY_DIR ${CMAKE_BINARY_DIR}/Content)
if(EXISTS ${CONTENT_SOURCE_DIR})
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CONTENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CONTENT_SOURCE_DIR} ${CONTENT_BINARY_DIR}
        COMMENT "Copying Content directory to build folder"
    )
    add_dependencies(main copy_assets)
else()
    message(WARNING "Content directory not found at ${ASSETS_SOURCE_DIR}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
