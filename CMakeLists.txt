cmake_minimum_required (VERSION 3.1)
project (cpp_projects)

# 使得 Make 时打印更多内容
# set(CMAKE_VERBOSE_MAKEFILE ON)
# set(CMAKE_COLOR_MAKEFILE   ON)

# 使得bin文件进入 bin/ 文件夹下
# 也可以通过 set_target_properties 对单个targe进行设置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_STANDARD 14)

add_subdirectory(Aho-Corasick)
