# Copyright (c) 2026 Enaium
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Merges static archives into a single archive.
#
# Usage:
#   cmake -DAR=<ar executable> -DOUTPUT=<merged.a> -DWORK=<work dir>
#         -DARCHIVES_FILE=<file, one archive path per line>
#         -P merge_archives.cmake
#
# Each archive is extracted into its own directory and every member is renamed
# with a per-archive prefix, so members with the same basename (e.g. base64.o
# exists in both libcurl and mbedTLS) never collide. The resulting object list
# is packed with `ar rcs`. Portable across GNU ar, llvm-ar and Apple ar.
#
# The archive list travels in a file instead of a `;`-separated -D argument:
# custom commands are rendered into a shell script by the Makefile generator,
# where unquoted semicolons would split the command line in two.

if(NOT DEFINED AR OR NOT DEFINED OUTPUT OR NOT DEFINED WORK OR NOT DEFINED ARCHIVES_FILE)
    message(FATAL_ERROR "AR, OUTPUT, WORK and ARCHIVES_FILE are required")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(STRINGS "${ARCHIVES_FILE}" ARCHIVES)

set(ALL_OBJECTS "")
set(INDEX 0)

foreach(ARCHIVE IN LISTS ARCHIVES)
    if(ARCHIVE STREQUAL "")
        continue()
    endif()
    set(DIR "${WORK}/archive${INDEX}")
    file(MAKE_DIRECTORY "${DIR}")
    execute_process(COMMAND "${AR}" x "${ARCHIVE}" WORKING_DIRECTORY "${DIR}")
    # Apple's ar also extracts the archive's symbol table (__.SYMDEF);
    # collect only object files.
    file(GLOB MEMBERS "${DIR}/*.o" "${DIR}/*.obj")
    foreach(MEMBER IN LISTS MEMBERS)
        get_filename_component(NAME "${MEMBER}" NAME)
        set(RENAMED "${DIR}/a${INDEX}_${NAME}")
        file(RENAME "${MEMBER}" "${RENAMED}")
        list(APPEND ALL_OBJECTS "${RENAMED}")
    endforeach()
    math(EXPR INDEX "${INDEX}+1")
endforeach()

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(COMMAND "${AR}" rcs "${OUTPUT}" ${ALL_OBJECTS})
