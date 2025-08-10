# Install script for directory: D:/mmdbridge

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/dev/vcpkg/installed/x64-windows")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "D:/mmdbridge/MikuMikuDance_x64/python312.dll;D:/mmdbridge/MikuMikuDance_x64/Alembic.dll;D:/mmdbridge/MikuMikuDance_x64/hdf5.dll;D:/mmdbridge/MikuMikuDance_x64/imath-2_2.dll;D:/mmdbridge/MikuMikuDance_x64/iex-2_2.dll;D:/mmdbridge/MikuMikuDance_x64/half.dll;D:/mmdbridge/MikuMikuDance_x64/zlib1.dll;D:/mmdbridge/MikuMikuDance_x64/szip.dll")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "D:/mmdbridge/MikuMikuDance_x64" TYPE FILE FILES
    "C:/dev/vcpkg/installed/x64-windows/bin/python312.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/Alembic.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/hdf5.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/imath-2_2.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/iex-2_2.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/half.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/zlib1.dll"
    "C:/dev/vcpkg/installed/x64-windows/bin/szip.dll"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "D:/mmdbridge/MikuMikuDance_x64/_light_test.py;D:/mmdbridge/MikuMikuDance_x64/bone_test.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_3dsmax.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_UE4.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_blender.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_c4d.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_guerillarender.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_houdini.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_maya2014.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_maya2015.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_preview.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_alembic_for_renderman.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_avi.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_obj_general.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_obj_metaseq.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_octane.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_octane_ocs.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_pmx.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_rib.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_tungsten.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_vidro.py;D:/mmdbridge/MikuMikuDance_x64/mmdbridge_vmd.py;D:/mmdbridge/MikuMikuDance_x64/pyqttest.py")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "D:/mmdbridge/MikuMikuDance_x64" TYPE FILE FILES
    "D:/mmdbridge/Release/Win32/_light_test.py"
    "D:/mmdbridge/Release/Win32/bone_test.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_3dsmax.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_UE4.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_blender.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_c4d.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_guerillarender.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_houdini.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_maya2014.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_maya2015.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_preview.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_alembic_for_renderman.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_avi.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_obj_general.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_obj_metaseq.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_octane.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_octane_ocs.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_pmx.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_rib.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_tungsten.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_vidro.py"
    "D:/mmdbridge/Release/Win32/mmdbridge_vmd.py"
    "D:/mmdbridge/Release/Win32/pyqttest.py"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "D:/mmdbridge/MikuMikuDance_x64/alembic_assign_scripts")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "D:/mmdbridge/MikuMikuDance_x64" TYPE DIRECTORY FILES "D:/mmdbridge/Release/Win32/alembic_assign_scripts")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "D:/mmdbridge/MikuMikuDance_x64/out/")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "D:/mmdbridge/MikuMikuDance_x64/out" TYPE DIRECTORY FILES "")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("D:/mmdbridge/build_vs2022_64/src/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "D:/mmdbridge/build_vs2022_64/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
