cmake_minimum_required(VERSION 3.20)
project(mosaic_workspace LANGUAGES NONE)
include(CTest)

# The production runtime lives in native/.  Keeping this tiny root
# superbuild makes the documented presets and CI commands work from the
# repository root without duplicating native build logic.
add_subdirectory(native)
