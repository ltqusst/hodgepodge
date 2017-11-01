cmake_minimum_required(VERSION 3.5)
project(hodgepodge)

set (CMAKE_CXX_STANDARD 11)

function(debug_var regex)
    message(STATUS " >>>>>>>>>>>>> ${ARGV0} <<<<<<<<<<<<<<<<")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        if (_variableName MATCHES ${ARGV0})
            message(STATUS " >>>>${_variableName}=${${_variableName}}")
        endif()
    endforeach()
endfunction(debug_var)


set(XXXx on)

INCLUDE(FindPkgConfig)
pkg_check_modules (libva REQUIRED libva)

message(STATUS "libva Found: libva_VERSION=${libva_VERSION}")

#pkg_check_modules(libva required libva)

# show vars start with libva
#debug_var(^libva)


add_executable(tfunctor_param tfunctor_param.cpp)
add_executable(tmem_problem tmem_problem.cpp)

# this sub dir will introduce project jingo
add_subdirectory(lib_jingo)

add_executable(tthreads tthreads.cpp)
target_link_libraries(tthreads 
        jingo# here "jingo" is "A library target name" rather than a plain/full lib name
        pthread
) 
add_executable              (tremove tremove.cpp)
add_executable              (tchrono tchrono.cpp)
add_executable              (tforward tforward.cpp)

#=====================================================================
pkg_check_modules (OpenCV REQUIRED opencv)
#find_package(OpenCV 2.4.3 REQUIRED core imgproc highgui)
message(STATUS "OpenCV found: ${OpenCV_FOUND}")
message(STATUS "OpenCV include dir found at ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

#pkg_check_modules (ft2 REQUIRED freetype)
#debug_var(^ft2)

set(ft2_INCLUDE_DIRS /usr/include/freetype2)
set(ft2_LIBRARIES libfreetype.so)

# should we use opencv3.3 binary?
# set(OpenCV_INCLUDE_DIRS /opt/intel/computer_vision_sdk_2017.0.113/opencv/include/opencv)
# set(OpenCV_LIBRARIES libopencv_core.so libopencv_imgproc.so libopencv_highgui.so)
# debug_var(^OpenCV)

add_executable              (tcvtest tcvtest.cpp)
target_include_directories  (tcvtest PUBLIC ${OpenCV_INCLUDE_DIRS} )
target_link_libraries       (tcvtest pthread ${OpenCV_LIBRARIES} ) 

add_executable              (tfreetype tfreetype.cpp)
target_include_directories  (tfreetype PUBLIC ${OpenCV_INCLUDE_DIRS} ${ft2_INCLUDE_DIRS})
target_link_libraries       (tfreetype pthread ${OpenCV_LIBRARIES} ${ft2_LIBRARIES}) 

#====================================================================
find_package(Eigen3 REQUIRED)
message(STATUS "Eigen3 found: ${EIGEN3_FOUND}")
message(STATUS "Eigen3 include dir found at ${EIGEN3_INCLUDE_DIRS}")
message(STATUS "Eigen3 version: ${EIGEN3_VERSION}")

include_directories(${EIGEN3_INCLUDE_DIR}) 
add_executable              (teigen3test teigen3test.cpp)
target_include_directories  (teigen3test PUBLIC ${EIGEN3_INCLUDE_DIR})

