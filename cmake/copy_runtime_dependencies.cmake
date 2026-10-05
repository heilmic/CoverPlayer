cmake_policy(SET CMP0207 NEW)

if(NOT DEFINED EXECUTABLE OR NOT DEFINED DESTINATION OR NOT DEFINED RUNTIME_DIRECTORY)
    message(FATAL_ERROR "Runtime dependency script is missing a required argument")
endif()

file(GLOB existing_runtime_copies "${DESTINATION}/*.dll")
file(REMOVE ${existing_runtime_copies})

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    DIRECTORIES "${RUNTIME_DIRECTORY}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ww]indows[/\\\\][Ss]ystem32[/\\\\].*"
)

if(unresolved_dependencies)
    message(FATAL_ERROR "Unresolved runtime dependencies: ${unresolved_dependencies}")
endif()

file(TO_CMAKE_PATH "${RUNTIME_DIRECTORY}" normalized_runtime_directory)
foreach(dependency IN LISTS resolved_dependencies)
    file(TO_CMAKE_PATH "${dependency}" normalized_dependency)
    string(FIND "${normalized_dependency}" "${normalized_runtime_directory}/" runtime_prefix_position)
    if(runtime_prefix_position EQUAL 0)
        file(COPY "${dependency}" DESTINATION "${DESTINATION}")
    endif()
endforeach()
