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
    set(GIT_TAG "v2.1")
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

# Get current branch so ship/vX.Y branches can advertise the branch version
execute_process(
    COMMAND ${GIT_EXECUTABLE} branch --show-current
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE BRANCH_RESULT
)
if(NOT BRANCH_RESULT EQUAL 0)
    set(GIT_BRANCH "")
endif()

set(BRANCH_VERSION "")
if(GIT_BRANCH MATCHES "^ship/(v[0-9]+\\.[0-9]+)$")
    set(BRANCH_VERSION "${CMAKE_MATCH_1}")
endif()

# Build version string: "v2.0" if on tag, "v2.0.3-a4a6176" if ahead of that tag,
# or "v2.1-a4a6176" on a ship/v2.1 branch before the v2.1 tag exists.
if(GIT_COMMITS STREQUAL "0")
    set(VERSION_STRING "${GIT_TAG}")
elseif(NOT BRANCH_VERSION STREQUAL "" AND NOT GIT_TAG STREQUAL BRANCH_VERSION)
    set(VERSION_STRING "${BRANCH_VERSION}-${GIT_HASH}")
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
