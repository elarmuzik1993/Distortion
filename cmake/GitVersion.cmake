# Generates Source/GitVersion.h with version info from git

# Get the latest tag (e.g. v2.0) or fall back
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE TAG_RESULT
)
if(NOT TAG_RESULT EQUAL 0 OR GIT_TAG STREQUAL "")
    set(GIT_TAG "v2.0")
endif()

# Count commits since that tag
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list ${GIT_TAG}..HEAD --count
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMITS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE COUNT_RESULT
)
if(NOT COUNT_RESULT EQUAL 0 OR GIT_COMMITS STREQUAL "")
    set(GIT_COMMITS "0")
endif()

# Get short hash
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE HASH_RESULT
)
if(NOT HASH_RESULT EQUAL 0 OR GIT_HASH STREQUAL "")
    set(GIT_HASH "unknown")
endif()

# Build version string: "v2.0" if on tag, "v2.0.3-a4a6176" if 3 commits ahead
if(GIT_COMMITS STREQUAL "0")
    set(VERSION_STRING "${GIT_TAG}")
else()
    set(VERSION_STRING "${GIT_TAG}.${GIT_COMMITS}-${GIT_HASH}")
endif()

# Only write if changed (avoids unnecessary recompilation)
set(HEADER_CONTENT "#pragma once\n#define GIT_VERSION_STRING \"${VERSION_STRING}\"\n")
if(EXISTS ${OUTPUT_FILE})
    file(READ ${OUTPUT_FILE} EXISTING_CONTENT)
    if(EXISTING_CONTENT STREQUAL HEADER_CONTENT)
        return()
    endif()
endif()

file(WRITE ${OUTPUT_FILE} ${HEADER_CONTENT})
message(STATUS "GitVersion: ${VERSION_STRING}")
