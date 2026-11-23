
set(IFACE_C_IN ${CMAKE_CURRENT_LIST_DIR}/resources.c.in)
set(IFACE_H_IN ${CMAKE_CURRENT_LIST_DIR}/resources.h.in)
set(AUTOGEN_DATA ${CMAKE_CURRENT_LIST_DIR}/autogendata.cmake)

function(AddResources TARGET_NAME RESOURCE_SOURCE_DIR)
    list(LENGTH ARGN RESOURCE_COUNT)

    make_directory(${CMAKE_CURRENT_BINARY_DIR}/autogen)
    make_directory(${CMAKE_CURRENT_BINARY_DIR}/include)

    set(RSRC_DATA_C ${CMAKE_CURRENT_BINARY_DIR}/autogen/${TARGET_NAME}_autogen_resource_data.c)
    set(RSRC_DATA_H ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET_NAME}_autogen_resource_data.h)

    set(OUT_DEPENDENCIES)
    set(RESOURCE_ARRAY)
    
    foreach(RESOURCE_NAME ${ARGN})
        set(RESOURCE_PATH ${RESOURCE_SOURCE_DIR}/${RESOURCE_NAME})

        if(NOT EXISTS ${RESOURCE_PATH})
            message(FATAL_ERROR "Could not find resource ${RESOURCE_NAME}")
        endif()

        string(REGEX MATCH "([^/]+)$" FILENAME ${RESOURCE_NAME})
        string(REGEX REPLACE "\\.| |-" "_" FILENAME ${FILENAME})
        string(TOLOWER ${FILENAME} FILENAME)
        list(APPEND OUT_DEPENDENCIES ${RESOURCE_PATH})

        string(APPEND RESOURCE_ARRAY "{\"${RESOURCE_NAME}\", ${FILENAME}, &${FILENAME}_size},\n")
    endforeach()

    add_custom_command(
        OUTPUT ${RSRC_DATA_C} ${RSRC_DATA_H}
        COMMAND ${CMAKE_COMMAND} "-DRSRC_DATA_C=${RSRC_DATA_C}" "-DRSRC_DATA_H=${RSRC_DATA_H}" "\"-DRESOURCE_NAMES=${ARGN}\"" "-DRESOURCE_SOURCE_DIR=${RESOURCE_SOURCE_DIR}" -P "${AUTOGEN_DATA}"
        DEPENDS ${OUT_DEPENDENCIES}
    )

    set(IFACE_C_OUT ${CMAKE_CURRENT_BINARY_DIR}/autogen/${TARGET_NAME}_resources.c)
    set(IFACE_H_OUT ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET_NAME}_resources.h)
    
    configure_file(${IFACE_C_IN} ${IFACE_C_OUT})
    configure_file(${IFACE_H_IN} ${IFACE_H_OUT})
    
    target_sources(${TARGET_NAME} PRIVATE
        ${RSRC_DATA_C}
        ${RSRC_DATA_H}
        ${IFACE_C_OUT}
        ${IFACE_H_OUT}
    )
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

    message(STATUS ${OUT_DEPENDENCIES})

    message(STATUS "Total ${TARGET_NAME} resources: ${RESOURCE_COUNT}")
    message(STATUS "#include <${TARGET_NAME}_resources.h> to query resources")
endfunction()
