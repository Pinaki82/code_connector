cmake_minimum_required(VERSION 3.10)
project(code_connector_shared C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -pedantic)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  message(FATAL_ERROR "MSVC is not supported for this project. Please use MinGW-w64.")
endif()

# Select platform-specific source file for shared library
if(WIN32)
  if(NOT MINGW)
    message(FATAL_ERROR "On Windows, this project requires MinGW-w64. Other compilers are not supported.")
  endif()
  set(SHARED_SRC code_connector_shared_windows.c)
else()
  set(SHARED_SRC code_connector_shared.c)
endif()

add_library(code_connector_shared SHARED ${SHARED_SRC} code_connector_shared.h)
set_target_properties(code_connector_shared PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin"
)

# Fix DLL naming for MinGW-w64 (remove 'lib' prefix)
if(MINGW)
  set_target_properties(code_connector_shared PROPERTIES PREFIX "")
endif()

target_include_directories(code_connector_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link UNIX-specific libraries only on non-Windows platforms
if(NOT WIN32)
  target_link_libraries(code_connector_shared PRIVATE dl m)
endif()

# Link POSIX regex library for MinGW-w64 on Windows
if(MINGW)
  target_link_libraries(code_connector_shared PRIVATE -lregex)
endif()

# Select platform-specific source file for code_connector_executable
if(WIN32)
  set(EXECUTABLE_SRC code_connector_executable_windows.c)
else()
  set(EXECUTABLE_SRC code_connector_executable.c)
endif()

add_executable(code_connector_executable ${EXECUTABLE_SRC})
set_target_properties(code_connector_executable PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin"
)

# Ensure console subsystem for Windows with MinGW
if(MINGW)
  target_link_options(code_connector_executable PRIVATE -mconsole)
endif()

target_link_libraries(code_connector_executable PRIVATE code_connector_shared)
if(NOT WIN32)
  target_link_libraries(code_connector_executable PRIVATE dl)
endif()

# Add new Windows-specific executable: ccls_index_gen_windows
if(WIN32)
  add_executable(ccls_index_gen_windows ccls_index_gen_windows.c)
  set_target_properties(ccls_index_gen_windows PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugin"
  )
  if(MINGW)
    target_link_options(ccls_index_gen_windows PRIVATE -mconsole)
  endif()
  target_link_libraries(ccls_index_gen_windows PRIVATE code_connector_shared)
endif()

# RPATH settings for UNIX-like systems only
if(NOT WIN32)
  set_target_properties(code_connector_executable PROPERTIES
    LINK_FLAGS "-Wl,-rpath=./plugin"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
    INSTALL_RPATH_USE_LINK_PATH TRUE
  )
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_compile_definitions(code_connector_shared PRIVATE DEBUG)
  target_compile_definitions(code_connector_executable PRIVATE DEBUG)
  target_compile_options(code_connector_shared PRIVATE -g)
  target_compile_options(code_connector_executable PRIVATE -g)
  if(WIN32)
    target_compile_definitions(ccls_index_gen_windows PRIVATE DEBUG)
    target_compile_options(ccls_index_gen_windows PRIVATE -g)
  endif()
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
  target_compile_definitions(code_connector_shared PRIVATE NDEBUG)
  target_compile_definitions(code_connector_executable PRIVATE NDEBUG)
  target_compile_options(code_connector_shared PRIVATE -O3)
  target_compile_options(code_connector_executable PRIVATE -O3)
  if(NOT WIN32)
    target_link_options(code_connector_executable PRIVATE -s)
  endif()
  if(WIN32)
    target_compile_definitions(ccls_index_gen_windows PRIVATE NDEBUG)
    target_compile_options(ccls_index_gen_windows PRIVATE -O3)
  endif()
endif()

install(TARGETS code_connector_shared code_connector_executable
  RUNTIME DESTINATION plugin
  LIBRARY DESTINATION plugin
)
if(WIN32)
  install(TARGETS ccls_index_gen_windows
    RUNTIME DESTINATION plugin
  )
endif()
install(FILES fun_complete.vim DESTINATION plugin)
install(DIRECTORY fun_complete_snippets DESTINATION .)
