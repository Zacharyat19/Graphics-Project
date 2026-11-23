cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(learnply2)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/genrsrc.cmake)

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory(glfw-3.5.1)

add_subdirectory(glm)

set(SRC "src")
add_executable(learnply2
	${SRC}/camera.cpp ${SRC}/camera.h
	${SRC}/main.cpp
	${SRC}/shader.cpp ${SRC}/shader.h

	glad/src/gl.c
)

# Files embedded in the executable (load them with learnply2_find_resource)
set(RESOURCES
	"shaders/quad.vert"
	"shaders/raytrace.frag"
)

AddResources(learnply2 ${CMAKE_CURRENT_SOURCE_DIR} ${RESOURCES})

target_link_libraries(learnply2 glfw glm OpenGL::GL)
target_include_directories(learnply2 PRIVATE glad/include)
