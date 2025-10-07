include_directories(
    $ENV{DXSDK_DIR}/include
)
add_definitions(
    -DNOMINMAX
    -DUNICODE
    -D_UNICODE
)
link_directories(
    $ENV{DXSDK_DIR}/lib/x64
)
add_library(d3dx9_43 SHARED
    d3dx9.cpp
    d3dx9.h
    d3dx9.def
)
target_link_libraries(d3dx9_43
    Shlwapi
)
install(TARGETS d3dx9_43
    RUNTIME DESTINATION ${MMD_DIR}
)
