cmake_minimum_required (VERSION 3.0)

set(INTERCEPT_CLIENT_PATH "${CMAKE_SOURCE_DIR}/intercept/src/client")

set(INTERCEPT_INCLUDE_PATH "${INTERCEPT_CLIENT_PATH}/headers" "${INTERCEPT_CLIENT_PATH}/headers/client/" "${INTERCEPT_CLIENT_PATH}/headers/client/sqf")


if(USE_64BIT_BUILD)
    set(INTERCEPT_SQFASM_NAME "sqf-assembly_x64")
else()
    set(INTERCEPT_SQFASM_NAME "sqf-assembly")
endif()


add_definitions(/DINTERCEPT_NO_THREAD_SAFETY)
add_definitions(/DNOMINMAX)
add_definitions(/DINTERCEPT_SQF_STRTYPE_RSTRING)
add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS) #No I don't cheat!

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/rv/${INTERCEPT_SQFASM_NAME}/intercept")

file(GLOB INTERCEPT_SOURCES "${INTERCEPT_CLIENT_PATH}/intercept/client/*.cpp"  "${INTERCEPT_CLIENT_PATH}/intercept/client/sqf/*.cpp" "${INTERCEPT_CLIENT_PATH}/intercept/shared/*.cpp")
SOURCE_GROUP("intercept" FILES ${INTERCEPT_SOURCES})






file(GLOB_RECURSE INTERCEPT_SQFASM_SOURCES *.h *.hpp *.c *.cpp)
SOURCE_GROUP("src" FILES ${INTERCEPT_SQFASM_SOURCES})

set(library_sources ${INTERCEPT_SQFASM_SOURCES})

add_library( ${INTERCEPT_SQFASM_NAME} SHARED ${library_sources} ${INTERCEPT_SOURCES} ${INTERCEPT_SQFASM_SQF_EXT_SOURCES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${INTERCEPT_INCLUDE_PATH} )

target_link_libraries (${INTERCEPT_SQFASM_NAME} ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(${INTERCEPT_SQFASM_NAME} ${LINK_LIBS})

target_link_libraries(${INTERCEPT_SQFASM_NAME} ${LINK_LIBS})
set_target_properties(${INTERCEPT_SQFASM_NAME} PROPERTIES PREFIX "")
set_target_properties(${INTERCEPT_SQFASM_NAME} PROPERTIES FOLDER intercept-cba)

if(CMAKE_COMPILER_IS_GNUCXX)
	SET(CMAKE_CXX_FLAGS "-std=c++1z -O2 -s -fPIC -fpermissive -static-libgcc -static-libstdc++")#-march=i686 -m32
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1 /MP /EHsc")
	set(CMAKE_CXX_FLAGS_RELEASE "/MT /Zi /O2 /Ob1 /EHsc /MP") #with debug info
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /DEBUG:FULL") 
endif()


if(CMAKE_COMPILER_IS_GNUCXX)
	set_target_properties(${INTERCEPT_NETWORK_NAME} PROPERTIES LINK_SEARCH_START_STATIC 1)
	set_target_properties(${INTERCEPT_NETWORK_NAME} PROPERTIES LINK_SEARCH_END_STATIC 1)
endif()
