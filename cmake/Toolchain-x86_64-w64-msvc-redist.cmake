# Optimize for small, fast, and dependency-free exes

if (VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET MATCHES "-static$")
  message(FATAL_ERROR "${CMAKE_CURRENT_LIST_FILE} requires a -static vcpkg triplet")
endif()

# We're going to use the 'Hybrid CRT' approach, which is the combination of the
# UCRT and the static C++ Runtime
#
# https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md
#
# In Linux terms, this is roughly equivalent to dynamically linking libc but statically linking libc++
#
# This gets us roughly the portability of a static build, but with nearly none of the size cost
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>>")
add_compile_options("/MT$<$<CONFIG:Debug>:d>")
add_link_options(
  "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib"
  "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib")

# RelWithDebInfo is nice as then we still get symbols for our release builds (in a separate PDB file)
# However, MSVC has a bunch of optimizations that are only turned on for plain release builds. Turn
# them back on for everything except DEBUG. This gets us a smaller and faster binary
add_link_options(
  "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>"
  # COMDAT folding
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
  # Remove unused functions and data
  "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
)