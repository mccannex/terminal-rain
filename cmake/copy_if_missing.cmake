# Usage: cmake -DSRC=<file> -DDEST=<file> -P copy_if_missing.cmake
# Copies SRC to DEST only if DEST doesn't already exist, so a user's edited
# runtime config isn't clobbered by every rebuild.
if(NOT EXISTS "${DEST}")
    file(READ "${SRC}" CONTENT)
    file(WRITE "${DEST}" "${CONTENT}")
endif()
