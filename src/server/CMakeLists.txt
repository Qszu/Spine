SET(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
SET(includedir ${CMAKE_SOURCE_DIR}/include/server)

INCLUDE_DIRECTORIES(${includedir})

FILE(GLOB SpineServerSrc ${srcdir}/*.cpp)
FILE(GLOB ServerHeader ${includedir}/*.h)

ADD_EXECUTABLE(SpineServer ${SpineServerSrc} ${ServerHeader})

target_link_libraries(SpineServer SpineCommon ${BOOST_LIBRARIES} ${CLOCKUTILS_LIBRARIES} ${CLOCKWORK_LIBRARIES} ${MARIADB_LIBRARIES} ${OPENSSL_LIBRARIES})

IF(UNIX)
	target_link_libraries(SpineServer pthread)
ENDIF(UNIX)

IF(WIN32)
	target_link_libraries(SpineServer ws2_32)
	add_custom_command(TARGET SpineServer POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/SpineServer.exe ${CMAKE_BINARY_DIR}/bin)
ENDIF(WIN32)
