cmake_minimum_required(VERSION 3.23)
project(PyramidConsumer LANGUAGES CXX)

find_package(PyramidEngine CONFIG REQUIRED)

add_executable(PyramidConsumer main.cpp)
target_link_libraries(PyramidConsumer PRIVATE Pyramid::Engine)
target_compile_features(PyramidConsumer PRIVATE cxx_std_17)
