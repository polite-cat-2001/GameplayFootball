# Copies the game data directory next to the binary. Player photos
# (databases/default/faces) are not used by the game yet and are excluded to
# keep the copy fast (tens of thousands of files).
#
# Usage: cmake -DSRC=<data dir> -DDST=<target dir> -P copy_data.cmake

file(REMOVE_RECURSE "${DST}/databases/default/faces")
file(COPY "${SRC}/" DESTINATION "${DST}" PATTERN "faces" EXCLUDE)