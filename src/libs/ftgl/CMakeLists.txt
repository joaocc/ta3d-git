PROJECT(ftgl)
cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(
			${CMAKE_CURRENT_SOURCE_DIR}/src/
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/include/
			${CMAKE_CURRENT_SOURCE_DIR}/src/FTGL/)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

IF(WIN32 OR CMAKE_CROSSCOMPILING)
	SET( FREETYPE_INCLUDE_DIRS  "${CMAKE_CURRENT_SOURCE_DIR}/../../ta3d/src/tools/win32/mingw32/include/"
								"${CMAKE_CURRENT_SOURCE_DIR}/../../ta3d/src/tools/win32/mingw32/include/freetype2/")
	SET( FREETYPE_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/../../ta3d/src/tools/win32/mingw32/libs/libfreetype.dll.a")
	Message(STATUS "FreeType Library : ${FREETYPE_LIBRARIES}")
ELSE(WIN32 OR CMAKE_CROSSCOMPILING)
	FIND_PACKAGE(Freetype)
ENDIF(WIN32 OR CMAKE_CROSSCOMPILING)

include_directories( ${FREETYPE_INCLUDE_DIRS} )

ADD_SUBDIRECTORY(src)
