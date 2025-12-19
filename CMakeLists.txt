cmake_minimum_required(VERSION 3.16)
project(CalendarApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

add_library(calendar_data STATIC
    src/data/DataProvider.cpp
)
target_include_directories(calendar_data PUBLIC include)
target_link_libraries(calendar_data PUBLIC Qt5::Core)

add_library(calendar_core STATIC
    src/core/AppContext.cpp
)
target_include_directories(calendar_core PUBLIC include)
target_link_libraries(calendar_core PUBLIC Qt5::Core calendar_data)

add_library(calendar_ui STATIC
    src/ui/MainWindow.cpp
    include/calendar/ui/MainWindow.hpp
)
target_include_directories(calendar_ui PUBLIC include)
target_link_libraries(calendar_ui PUBLIC Qt5::Widgets calendar_core calendar_data)

add_executable(calendar_app
    src/main.cpp
)
target_link_libraries(calendar_app PRIVATE calendar_ui calendar_core calendar_data)

install(TARGETS calendar_app
    RUNTIME DESTINATION bin
)
