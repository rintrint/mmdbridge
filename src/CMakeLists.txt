add_subdirectory(MikuMikuFormats)
add_subdirectory(d3dx9_43)
add_subdirectory(msimg32)

##############################################################################
# d3d9
##############################################################################
file(GLOB d3d9_sources
    d3d9/*.cpp
    d3d9/*.h
    d3d9/d3d9.rc
    d3d9/d3d9.def
    umbase/*.cpp
    umbase/*.h
)
include_directories(
    umbase
    d3dx9_43
    MikuMikuFormats
    $ENV{DXSDK_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/MinHook/include
)
link_directories(
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
    MikuMikuFormats
    Alembic::Alembic
    pybind11::embed
    Python3::Python
    minhook
)
# Auto-install after build for seamless debugging workflow
add_custom_command(TARGET d3d9 POST_BUILD
    COMMENT "Auto-install after build..."
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --config $<CONFIG> 2>nul || echo Install failed - MMD may be running
)
install(TARGETS d3d9
    RUNTIME DESTINATION ${MMD_DIR}
)
