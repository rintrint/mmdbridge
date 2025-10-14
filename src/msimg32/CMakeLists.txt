include_directories(
    $ENV{DXSDK_DIR}/include
)
add_definitions(
    -D_GDI32_
    -DNOMINMAX
    -DUNICODE
    -D_UNICODE
)
link_directories(
    $ENV{DXSDK_DIR}/lib/x64
)
add_library(msimg32 SHARED
    msimg32.cpp
    msimg32.h
    msimg32.def
)
target_link_libraries(msimg32
    minhook
)
target_link_options(msimg32 PRIVATE
    /ignore:4104 # disable "export of symbol 'DllInitialize' should be PRIVATE" warning
)
install(TARGETS msimg32
    RUNTIME DESTINATION ${MMD_DIR}
)
