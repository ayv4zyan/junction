# Starts the real app (offscreen) on a small Library with a Card, a Group and a Connection,
# lets it run for a few seconds and fails if QML reported anything: a TypeError, a missing
# property, a binding loop. Unit tests don't load the QML, so this is what catches those.
#   cmake -DAPP=<junction binary> -DWORK=<empty folder> -P app_starts_cleanly.cmake
file(REMOVE_RECURSE "${WORK}")
set(library "${WORK}/data/Junction/Junction/Library")
file(MAKE_DIRECTORY "${library}/Test")
file(WRITE "${library}/Test/Canvas.canvas" [=[
{"nodes":[{"id":"g","type":"group","x":-40,"y":-40,"width":720,"height":300,"label":"G"},
 {"id":"a","type":"text","x":0,"y":0,"width":240,"height":80,"text":"Alpha"},
 {"id":"b","type":"text","x":400,"y":0,"width":240,"height":80,"text":"Beta"},
 {"id":"u","type":"file","x":0,"y":400,"width":240,"height":80}],
 "edges":[{"id":"e","fromNode":"a","toNode":"b","label":"calls","pathStyle":"elbow"}]}
]=])

set(ENV{XDG_DATA_HOME} "${WORK}/data")
set(ENV{XDG_CONFIG_HOME} "${WORK}/config")
set(ENV{QT_QPA_PLATFORM} offscreen)
set(ENV{QT_FORCE_STDERR_LOGGING} 1)
execute_process(COMMAND "${APP}" TIMEOUT 3 OUTPUT_VARIABLE out ERROR_VARIABLE err)

string(STRIP "${out}${err}" log)
if(NOT log STREQUAL "")
    message(FATAL_ERROR "The app reported:\n${log}")
endif()
if(NOT EXISTS "${library}/.junction")
    message(FATAL_ERROR "The app did not load the Library (no index written)")
endif()
