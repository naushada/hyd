cmake_minimum_required(VERSION 3.16.3)
project(uniimage)


#include_directories(/usr/local/include/bsoncxx/v_noabi)

#Adding Complier flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -g -Wall -Wextra")

#include_directories(inc)
include_directories(/usr/local/openssl-3.1.1/include)
link_directories(/usr/local/openssl-3.1.1/lib64)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.cc")

add_executable(uniimage ${SOURCES})

#target_link_libraries(uniimage bsoncxx)
target_link_libraries(uniimage pthread ssl crypto)

install(TARGETS uniimage
	CONFIGURATIONS Release
        RUNTIME DESTINATION /usr/local/bin
)

