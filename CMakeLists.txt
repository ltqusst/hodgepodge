cmake_minimum_required(VERSION 3.5)

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

#pkg_check_modules (ocv REQUIRED opencv)
#pkg_check_modules (ft2 REQUIRED freetype)
#debug_var(^ft2)

set(ft2_INCLUDE_DIRS /usr/include/freetype2)
set(ft2_LIBRARIES libfreetype.so)
set(ocv_INCLUDE_DIRS /opt/intel/computer_vision_sdk_2017.0.113/opencv/include/opencv)
set(ocv_LIBRARIES libopencv_core.so libopencv_imgproc.so libopencv_highgui.so)

#debug_var(^ocv)

add_executable              (tfreetype tfreetype.cpp)
target_include_directories  (tfreetype PUBLIC ${ocv_INCLUDE_DIRS} ${ft2_INCLUDE_DIRS})
target_link_libraries       (tfreetype pthread ${ocv_LIBRARIES} ${ft2_LIBRARIES}) 


