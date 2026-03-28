cmake_minimum_required(VERSION 3.16)
project(opengl_triangle LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include")

add_executable(opengl_triangle src/main.cpp)
target_link_libraries(opengl_triangle PRIVATE glfw glad)

set(SHADER_DIR_ABS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
target_compile_definitions(opengl_triangle PRIVATE "SHADER_DIR=\"${SHADER_DIR_ABS}\"")

add_custom_command(
  TARGET opengl_triangle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
          "$<TARGET_FILE_DIR:opengl_triangle>/shaders"
)

if(WIN32)
  target_link_libraries(opengl_triangle PRIVATE opengl32)
endif()
