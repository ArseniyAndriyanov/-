cmake_minimum_required(VERSION 3.10)

# Укажите название проекта
project(MyProject)

# Укажите версию C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Добавьте исполняемый файл, указав источник
add_executable(MyExecutable main.cpp)