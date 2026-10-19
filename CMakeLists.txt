cmake_minimum_required(VERSION 3.20)
project(mc_skip_menu CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(hook SHARED src/hook/main.cpp)
target_compile_definitions(hook PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(hook user32 ole32 oleaut32 uuid)

add_executable(launcher src/launcher/launcher.cpp)
target_compile_definitions(launcher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(launcher user32 advapi32)
