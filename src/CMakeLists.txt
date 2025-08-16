add_subdirectory(MikuMikuFormats)
add_subdirectory(d3dx9_43)

##############################################################################
# d3d9
##############################################################################
file(GLOB d3d9_sources
    d3d9/*.cpp
    d3d9/*.h
    d3d9/d3d9.rc
    d3d9/hook.def
    umbase/*.cpp
    umbase/*.h
)
include_directories(
    umbase
    d3dx9_43
    MikuMikuFormats
    ${MMD_DIR}/Data
    $ENV{DXSDK_DIR}/include
)
link_directories(
    ${MMD_DIR}/Data
    $ENV{DXSDK_DIR}/lib/x64
)
add_definitions(
    -DNOMINMAX
    -DUNICODE
    -D_UNICODE
    -DWITH_VMD
    -DWITH_PMX
    -DWITH_ALEMBIC
)
add_library(d3d9 SHARED
    ${d3d9_sources}
)
target_link_libraries(d3d9
    Shlwapi
    WinMM
    d3dx9_43
    MMDExport
    MikuMikuFormats
    Alembic::Alembic
    pybind11::embed
    Python3::Python
)
install(TARGETS d3d9
    RUNTIME DESTINATION ${MMD_DIR}
)
