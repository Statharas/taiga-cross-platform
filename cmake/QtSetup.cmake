set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

list(APPEND CMAKE_PREFIX_PATH "%QTDIR%/lib/cmake")

set(TAIGA_QT_COMPONENTS
	Core
	Gui
	LinguistTools
	Network
	Sql
	Svg
	Widgets
)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	list(APPEND TAIGA_QT_COMPONENTS DBus)
endif()

if (TAIGA_BUILD_TESTS)
	list(APPEND TAIGA_QT_COMPONENTS Test)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${TAIGA_QT_COMPONENTS})

qt_standard_project_setup(
	REQUIRES 6.8
)
