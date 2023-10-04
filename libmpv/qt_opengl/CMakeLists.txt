cmake_minimum_required(VERSION 3.16)
project(qt_opengl VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui OpenGLWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup()

qt_add_executable(qt_opengl WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp mainwindow.h
    mpvwidget.cpp mpvwidget.h
)


target_include_directories(qt_opengl PUBLIC ${MPV_INCLUDE_DIRS})
target_compile_options(qt_opengl PUBLIC ${MPV_CFLAGS_OTHER})
#target_link_libraries(qt_opengl ${MPV_LIBRARIES})

target_link_libraries(qt_opengl
    #PUBLIC {MPV_LIBRARIES}
    PUBLIC PkgConfig::MPV
    PRIVATE
    Qt::Core
    Qt::Gui
    Qt::OpenGLWidgets
)


install(TARGETS qt_opengl
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET qt_opengl
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
