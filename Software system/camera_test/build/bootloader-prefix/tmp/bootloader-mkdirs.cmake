# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/devesh/Desktop/esp-idf/components/bootloader/subproject"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/tmp"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/src/bootloader-stamp"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/src"
  "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/devesh/Desktop/Glasses/Eyes-Spice/Software system/camera_test/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
