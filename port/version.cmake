# writes os_version.h (OS_VERSION) from tools/version.sh; run at every build,
# and the file is only touched when the version has changed
execute_process(COMMAND sh ${SRC}/../tools/version.sh OUTPUT_VARIABLE V OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT V)
  set(V "unknown")
endif()
set(TEXT "#define OS_VERSION \"${V}\"\n")
set(OLD "")
if(EXISTS ${OUT})
  file(READ ${OUT} OLD)
endif()
if(NOT OLD STREQUAL TEXT)
  file(WRITE ${OUT} "${TEXT}")
endif()
