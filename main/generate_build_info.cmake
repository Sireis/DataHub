# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

string(TIMESTAMP TODAY)

variable_watch(GIT_COMMIT_HASH)
variable_watch(TODAY)

file(
    WRITE 
    ${CMAKE_CURRENT_LIST_DIR}/cmake_build_info.h 
    "#define GIT_COMMIT_HASH \"${GIT_COMMIT_HASH}\"\n"
)
file(
    APPEND 
    ${CMAKE_CURRENT_LIST_DIR}/cmake_build_info.h 
    "#define BUILD_TIME \"${TODAY}\"\n"
)