cmake_minimum_required(VERSION 3.16)

project(xlib_package_consumer LANGUAGES C CXX)

find_package(xlib CONFIG REQUIRED)

add_executable(xlib_package_consumer main.c)
target_link_libraries(xlib_package_consumer PRIVATE xlib::xlib)

add_executable(xlib_package_consumer_cpp main.cpp)
target_compile_features(xlib_package_consumer_cpp PRIVATE cxx_std_11)
target_link_libraries(xlib_package_consumer_cpp PRIVATE xlib::xlib)
