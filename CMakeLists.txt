cmake_minimum_required(VERSION 3.20)

set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)

project(mmdbridge) # .sln

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(/utf-8)

# shared compiler options
add_compile_options(
    /EHsc
    # /W0
    # /W1
    # /W2
    # /W3
    /W4 # high warning level (NOT /Wall)
    # /Wall
    /WX # warnings as errors
    /analyze # static analysis
    /external:W0 # disable warnings for external headers
    /external:I${CMAKE_INSTALL_PREFIX}/include # Mark external include directories
    /analyze:external- # disable static analysis for external headers
    /wd4267 # disable "conversion from size_t to int, possible loss of data"
    /wd6326 # disable "potential comparison of constant with constant"

    /wd4100 # unreferenced formal parameter
    # /wd4189 # unreferenced local variable
    /wd4505 # unreferenced local function has been removed
    # /wd4514 # unreferenced inline function has been removed
    # /wd5245 # unreferenced function with internal linkage has been removed

    # /wd5045 # disable Spectre mitigation warnings
)

# embedded pdb
add_compile_options(
    /Z7
)

# find dependencies
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Alembic CONFIG REQUIRED)

# set paths
get_filename_component(MMD_DIR
    ${CMAKE_CURRENT_LIST_DIR}/MikuMikuDance_v932x64
    ABSOLUTE
)

# build dependencies
add_subdirectory(third_party/MinHook)
target_compile_options(minhook PRIVATE
    /W3
)

# build targets
add_subdirectory(src)

# set startup project for Visual Studio  
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT d3d9)

# install runtime dependencies
install(TARGETS d3d9 d3dx9_43 RUNTIME DESTINATION ${MMD_DIR})
install(FILES $<TARGET_RUNTIME_DLLS:d3d9> DESTINATION ${MMD_DIR})
# install(FILES $<TARGET_RUNTIME_DLLS:Alembic::Alembic> DESTINATION ${MMD_DIR})
# The automatic approach using $<TARGET_RUNTIME_DLLS:Alembic::Alembic> fails.
# Manually install Alembic's runtime dependencies (e.g., hdf5.dll).
# release build
set(MISSING_DLLS_RELEASE
    "${CMAKE_INSTALL_PREFIX}/bin/hdf5.dll"
    "${CMAKE_INSTALL_PREFIX}/bin/szip.dll"
    "${CMAKE_INSTALL_PREFIX}/bin/zlib1.dll"
)
install(
    FILES ${MISSING_DLLS_RELEASE}
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
)
# debug build
set(MISSING_DLLS_DEBUG
    "${CMAKE_INSTALL_PREFIX}/debug/bin/hdf5_D.dll"
    "${CMAKE_INSTALL_PREFIX}/debug/bin/szip.dll"
    "${CMAKE_INSTALL_PREFIX}/debug/bin/zlibd1.dll"
)
install(
    FILES ${MISSING_DLLS_DEBUG}
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Debug
)

# install Python scripts
file(GLOB python_scripts Release/Win32/*.py
)
install(FILES ${python_scripts}
    DESTINATION ${MMD_DIR}
)

# install Python runtime
install(DIRECTORY ${CMAKE_INSTALL_PREFIX}/tools/python3/
    DESTINATION ${MMD_DIR}
    PATTERN "test*" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "__phello__*" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "_freeze_module.exe" EXCLUDE
    PATTERN "tkinter*" EXCLUDE
    PATTERN "idlelib*" EXCLUDE
    PATTERN "turtle*" EXCLUDE
    PATTERN "unittest*" EXCLUDE
    PATTERN "pydoc*" EXCLUDE
    PATTERN "turtledemo*" EXCLUDE
    PATTERN "email*" EXCLUDE
    PATTERN "urllib*" EXCLUDE
    PATTERN "http*" EXCLUDE
    PATTERN "xmlrpc*" EXCLUDE
    PATTERN "wsgiref*" EXCLUDE
    PATTERN "sqlite3*" EXCLUDE
    PATTERN "distutils*" EXCLUDE
    PATTERN "ensurepip*" EXCLUDE
    PATTERN "lib2to3*" EXCLUDE
    PATTERN "msilib*" EXCLUDE
    PATTERN "venv*" EXCLUDE
    PATTERN "curses*" EXCLUDE
    PATTERN "dbm*" EXCLUDE
)

# install additional resources
install(DIRECTORY Release/Win32/alembic_assign_scripts
    DESTINATION ${MMD_DIR}
)
install(DIRECTORY
    DESTINATION ${MMD_DIR}/out
)

# visual studio debugger configuration
if(MSVC)
    set_target_properties(d3d9 PROPERTIES
        VS_DEBUGGER_COMMAND "${MMD_DIR}/MikuMikuDance.exe"
    )
    set_target_properties(d3d9 PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${MMD_DIR}"
    )
endif()
