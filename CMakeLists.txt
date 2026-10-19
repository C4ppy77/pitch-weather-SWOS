cmake_minimum_required(VERSION 3.16)
project(pitch_weather CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_library(pitch_weather src/pitchWeather.cpp)
target_include_directories(pitch_weather PUBLIC src)
if(TARGET SDL2::SDL2)
    target_link_libraries(pitch_weather PUBLIC SDL2::SDL2)
else()
    target_include_directories(pitch_weather PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(pitch_weather PUBLIC ${SDL2_LIBRARIES})
endif()

add_executable(pitch_weather_demo demo/main.cpp)
target_link_libraries(pitch_weather_demo PRIVATE pitch_weather)
