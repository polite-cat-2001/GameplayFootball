# Copies the game data directory next to the binary.
#
# Player photos (databases/default/faces) are the portrait cut-outs: ~680 MB of
# small PNGs (~63k files). Copying them on every build made rebuilds slow, so
# they are copied only once, or again when the source directory changed (a
# re-import adds/removes files and bumps its timestamp). A stamp file next to
# the binary remembers which source snapshot was copied. To force a refresh,
# delete <build>/databases/default/faces and <build>/.faces_copied. Only the
# game target passes COPY_FACES, and only in Release builds; the other targets
# skip the folder but never delete it (a Release build may have just populated
# the shared output directory).
#
# Usage: cmake -DSRC=<data dir> -DDST=<target dir> [-DCOPY_FACES=1] -P copy_data.cmake

if(COPY_FACES)
  set(_faces_src "${SRC}/databases/default/faces")
  set(_faces_dst "${DST}/databases/default/faces")
  set(_faces_stamp "${DST}/.faces_copied")
  if(EXISTS "${_faces_src}")
    file(TIMESTAMP "${_faces_src}" _faces_src_ts "%Y%m%d%H%M%S" UTC)
    set(_faces_prev_ts "")
    if(EXISTS "${_faces_stamp}")
      file(READ "${_faces_stamp}" _faces_prev_ts)
    endif()
    if(NOT EXISTS "${_faces_dst}" OR NOT _faces_prev_ts STREQUAL "${_faces_src_ts}")
      message(STATUS "copy_data: copying player photos into ${_faces_dst}")
      file(COPY "${_faces_src}" DESTINATION "${DST}/databases/default")
      file(WRITE "${_faces_stamp}" "${_faces_src_ts}")
    endif()
  endif()
  file(COPY "${SRC}/" DESTINATION "${DST}" PATTERN "faces" EXCLUDE)
else()
  file(COPY "${SRC}/" DESTINATION "${DST}" PATTERN "faces" EXCLUDE)
endif()
