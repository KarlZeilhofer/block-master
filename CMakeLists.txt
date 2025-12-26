cmake_minimum_required(VERSION 3.16)
project(BlockMaster VERSION 25.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Test)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(calendar_data STATIC
    src/data/DataProvider.cpp
    src/data/InMemoryTodoRepository.cpp
    src/data/InMemoryEventRepository.cpp
    src/data/FileCalendarStorage.cpp
    src/data/FileEventRepository.cpp
    src/data/FileTodoRepository.cpp
)
target_include_directories(calendar_data PUBLIC include)
target_link_libraries(calendar_data PUBLIC Qt5::Core)

add_library(calendar_core STATIC
    src/core/AppContext.cpp
    src/core/UndoStack.cpp
)
target_include_directories(calendar_core PUBLIC include)
target_link_libraries(calendar_core PUBLIC Qt5::Core calendar_data)

add_library(calendar_ui STATIC
    src/ui/MainWindow.cpp
    include/calendar/ui/MainWindow.hpp
    src/ui/models/TodoListModel.cpp
    include/calendar/ui/models/TodoListModel.hpp
    src/ui/models/TodoFilterProxyModel.cpp
    include/calendar/ui/models/TodoFilterProxyModel.hpp
    src/ui/viewmodels/TodoListViewModel.cpp
    include/calendar/ui/viewmodels/TodoListViewModel.hpp
    src/ui/widgets/CalendarView.cpp
    include/calendar/ui/widgets/CalendarView.hpp
    src/ui/viewmodels/ScheduleViewModel.cpp
    include/calendar/ui/viewmodels/ScheduleViewModel.hpp
    src/ui/widgets/EventInlineEditor.cpp
    include/calendar/ui/widgets/EventInlineEditor.hpp
    src/ui/widgets/EventPreviewPanel.cpp
    include/calendar/ui/widgets/EventPreviewPanel.hpp
    src/ui/widgets/TodoListView.cpp
    include/calendar/ui/widgets/TodoListView.hpp
    src/ui/dialogs/EventDetailDialog.cpp
    include/calendar/ui/dialogs/EventDetailDialog.hpp
    src/ui/mime/TodoMime.cpp
    include/calendar/ui/mime/TodoMime.hpp
)
target_include_directories(calendar_ui PUBLIC include)
target_link_libraries(calendar_ui PUBLIC Qt5::Widgets calendar_core calendar_data)

add_executable(block-master
    src/main.cpp
)
target_include_directories(block-master PRIVATE include)
target_link_libraries(block-master PRIVATE calendar_ui calendar_core calendar_data)

install(TARGETS block-master
    RUNTIME DESTINATION bin
)

enable_testing()

add_executable(calendar_test_undo_stack
    tests/core/UndoStackTest.cpp
)
target_link_libraries(calendar_test_undo_stack PRIVATE Qt5::Test calendar_core)
add_test(NAME UndoStackTest COMMAND calendar_test_undo_stack)

add_executable(calendar_test_todo_repository
    tests/data/TodoRepositoryTest.cpp
)
target_link_libraries(calendar_test_todo_repository PRIVATE Qt5::Test calendar_data)
add_test(NAME TodoRepositoryTest COMMAND calendar_test_todo_repository)

add_executable(calendar_test_todo_list_model
    tests/ui/TodoListModelTest.cpp
)
target_link_libraries(calendar_test_todo_list_model PRIVATE Qt5::Test calendar_data calendar_ui)
add_test(NAME TodoListModelTest COMMAND calendar_test_todo_list_model)

add_executable(calendar_test_schedule_viewmodel
    tests/ui/ScheduleViewModelTest.cpp
)
target_link_libraries(calendar_test_schedule_viewmodel PRIVATE Qt5::Test calendar_data calendar_ui)
add_test(NAME ScheduleViewModelTest COMMAND calendar_test_schedule_viewmodel)
