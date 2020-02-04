set ( common_src Source/esShader.c 
                 Source/esShapes.c
                 Source/esTransform.c
                 Source/esUtil.c )


# Win32 Platform files
if(WIN32)
    set( common_platform_src Source/Win32/esUtil_win32.c )
    add_library( Common STATIC ${common_src} ${common_platform_src} )
    target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} )
elseif(UseDRM)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")
    find_library(DRM_LIB drm)
    find_library(GBM_LIB gbm)
    find_library(M_LIB m)
    include_directories( /usr/include/libdrm/ )
    set( common_platform_src Source/LinuxDRM/esUtil_DRM.c Source/LinuxDRM/common.c Source/LinuxDRM/drm-common.c Source/LinuxDRM/drm-legacy.c )

    add_library( Common STATIC ${common_src} ${common_platform_src} )
    target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${GBM_LIB} ${DRM_LIB} ${M_LIB} )
else()
    find_package(X11)
    find_library(M_LIB m)
    set( common_platform_src Source/LinuxX11/esUtil_X11.c )
    add_library( Common STATIC ${common_src} ${common_platform_src} )
    target_link_libraries( Common ${OPENGLES3_LIBRARY} ${EGL_LIBRARY} ${X11_LIBRARIES} ${M_LIB} )
endif()

             


