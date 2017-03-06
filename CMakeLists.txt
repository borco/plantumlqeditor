project (plantumlqeditor)

cmake_minimum_required (VERSION 2.8)

if(WIN32)
    add_definitions(-DUNICODE -DQT_LARGEFILE_SUPPORT)
endif()

set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)
set (LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

option (ENABLE_QT5 "Use Qt5 instead of Qt4" OFF)
if (${ENABLE_QT5})
    find_package(Qt5Core)
    find_package(Qt5Widgets)
else()
    set (QT_QMAKE_EXECUTABLE $ENV{QT_QMAKE_EXECUTABLE})
    find_package (Qt4 4.8 REQUIRED)
    include (UseQt4)
endif()

set (CMAKE_AUTOMOC TRUE)

option (ENABLE_UNIT_TESTS "enable unit test" OFF)
if (${ENABLE_UNIT_TESTS})
    set (CMAKE_BUILD_TYPE "Debug")
    set (GMOCK_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/gmock-1.6.0)
    set (GTEST_INCLUDE_DIR ${GMOCK_ROOT}/gtest/include)
    set (GTEST_MAIN_LIBRARY gtest_main)
    set (GTEST_LIBRARY gtest)
    set (GMOCK_LIBRARY gmock)
    enable_testing ()
    find_package (GTest REQUIRED)
    include_directories (${GMOCK_ROOT}/include ${GTEST_INCLUDE_DIR})
endif()

message ("CMAKE_BUILD_TYPE:  ${CMAKE_BUILD_TYPE}")
message ("Qt Headers:       ${QT_INCLUDES}")

include_directories (${CMAKE_SOURCE_DIR})
include_directories (${QT_INCLUDES})
include_directories (${CMAKE_SOURCE_DIR}/thirdparty/qtsingleapplication/src)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows -Wl,-enable-auto-import -Wl,-enable-runtime-pseudo-reloc")
endif()

#-------------------------------------------------------------------------------
# add to SOURCES files that will be tested with GMock and GTest
#-------------------------------------------------------------------------------
set (SOURCES_LIB
    assistantxmlreader.cpp
    filecache.cpp
    recentdocuments.cpp
)

#-------------------------------------------------------------------------------
# add to EXTRA_HEADERS all headers but those for classes for QObject derived
# classes
#-------------------------------------------------------------------------------
set (EXTRA_HEADERS_LIB
)

add_library (plantumlqeditorlib STATIC
    ${SOURCES_LIB}
    ${EXTRA_HEADERS_LIB}
)

if (${ENABLE_QT5})
    qt5_use_modules(plantumlqeditorlib Core Widgets Gui)
endif()

target_link_libraries (plantumlqeditorlib
    ${QT_QTCORE_LIBRARY}
    ${QT_QTGUI_LIBRARY}
)

add_subdirectory (thirdparty)
if(${ENABLE_UNIT_TESTS})
    add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# the real app
#-------------------------------------------------------------------------------
set (SOURCES_APP
    main.cpp
    mainwindow.cpp
    preferencesdialog.cpp
    previewwidget.cpp
    utils.cpp
    textedit.cpp
)

set (EXTRA_HEADERS_APP
    utils.h
    settingsconstants.h
)

set(FORMS_APP
    preferencesdialog.ui
    logdialog.ui
)

if (${ENABLE_QT5})
    qt5_wrap_ui(SOURCES_FORMS_APP ${FORMS_APP})
else()
    qt4_wrap_ui(SOURCES_FORMS_APP ${FORMS_APP})
endif()


set(QRC_APP
    plantumlqeditor.qrc
)

if (${ENABLE_QT5})
    qt5_add_resources(SOURCES_QRC_APP ${QRC_APP})
else()
    qt4_add_resources(SOURCES_QRC_APP ${QRC_APP})
endif()


include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(plantumlqeditor
    ${SOURCES_APP}
    ${SOURCES_FORMS_APP}
    ${SOURCES_QRC_APP}
    ${EXTRA_HEADERS_APP}
)

if (${ENABLE_QT5})
    qt5_use_modules(plantumlqeditor Core Widgets Gui Svg)
endif()

target_link_libraries(plantumlqeditor
    ${QT_QTSVG_LIBRARY}
    plantumlqeditorlib
    qtsingleapplicationlib
)

#-------------------------------------------------------------------------------
# installer support
#-------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "plantuml-qeditor")
set(CPACK_PACKAGE_VENDOR "Ioan Calin Borcoman")
set(CPACK_PACKAGE_CONTACT "iborco@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PlantUML QEditor - a Qt editor for PlantUML")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "0")

set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/icon64.png")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PlantUML QEditor")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_SOURCE_IGNORE_FILES ".git/;.gitignore;thirdparty/gmock-1.6.0/")

if(WIN32)
    set(CPACK_GENERATOR "ZIP" "NSIS")

    set(CPACK_PACKAGE_EXECUTABLES plantumlqeditor "PlantUML QEditor")

    install(TARGETS plantumlqeditor
        RUNTIME
        DESTINATION bin
        COMPONENT applications
    )

    file(GLOB LIBGCC_S_SJLJ_DLL ${CMAKE_FIND_ROOT_PATH}/bin/libgcc_s_sjlj*.dll)
    file(GLOB LIBPNG_DLL ${CMAKE_FIND_ROOT_PATH}/bin/libpng*.dll)
    file(GLOB LIBSTDCPP_DLL ${CMAKE_FIND_ROOT_PATH}/bin/libstdc++-*.dll)

    install(FILES
        ${CMAKE_FIND_ROOT_PATH}/bin/QtCore${QT_VERSION_MAJOR}.dll
        ${CMAKE_FIND_ROOT_PATH}/bin/QtGui${QT_VERSION_MAJOR}.dll
        ${CMAKE_FIND_ROOT_PATH}/bin/QtNetwork${QT_VERSION_MAJOR}.dll
        ${CMAKE_FIND_ROOT_PATH}/bin/QtSvg${QT_VERSION_MAJOR}.dll
        ${LIBGCC_S_SJLJ_DLL}
        ${LIBPNG_DLL}
        ${LIBSTDCPP_DLL}
        ${CMAKE_FIND_ROOT_PATH}/bin/zlib1.dll
        DESTINATION bin
        COMPONENT applications
    )

    install(FILES assistant.xml
        DESTINATION share/plantumlqeditor
        COMPONENT assistant
    )

    install(DIRECTORY icons
        DESTINATION share/plantumlqeditor
        COMPONENT assistant
    )

    set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "PlantUML QEditor")
    set(CPACK_COMPONENT_ASSISTANT_DISPLAY_NAME "Default Assistant")

    set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION
        "A PlantUML editor written with Qt${QT_VERSION_MAJOR}"
        )
    set(CPACK_COMPONENT_ASSISTANT_DESCRIPTION
        "The default assistant.xml and associated icons"
        )

endif()

include (CPack)
