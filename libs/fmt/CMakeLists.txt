set(MINIMP3_INCLUDE_DIR ${INCLUDES_BASE_DIR}/minimp3)
set(MIKKTSPACE_INCLUDE_DIR ${INCLUDES_BASE_DIR}/mikktspace)

add_library(fmt.hdll
    fmt.c
    sha1.c
    dxt.c
    mikkt.c
    ${MIKKTSPACE_INCLUDE_DIR}/mikktspace.c
)

if (WIN32)
    set(VORBIS_INCLUDE_DIR ${INCLUDES_BASE_DIR}/vorbis)
    set(ZLIB_INCLUDE_DIRS ${INCLUDES_BASE_DIR}/zlib)

    target_sources(fmt.hdll PRIVATE
        ${INCLUDES_BASE_DIR}/vorbis/bitrate.c
        ${INCLUDES_BASE_DIR}/vorbis/bitwise.c
        ${INCLUDES_BASE_DIR}/vorbis/block.c
        ${INCLUDES_BASE_DIR}/vorbis/codebook.c
        ${INCLUDES_BASE_DIR}/vorbis/envelope.c
        ${INCLUDES_BASE_DIR}/vorbis/floor0.c
        ${INCLUDES_BASE_DIR}/vorbis/floor1.c
        ${INCLUDES_BASE_DIR}/vorbis/framing.c
        ${INCLUDES_BASE_DIR}/vorbis/info.c
        ${INCLUDES_BASE_DIR}/vorbis/lookup.c
        ${INCLUDES_BASE_DIR}/vorbis/lpc.c
        ${INCLUDES_BASE_DIR}/vorbis/lsp.c
        ${INCLUDES_BASE_DIR}/vorbis/mapping0.c
        ${INCLUDES_BASE_DIR}/vorbis/mdct.c
        ${INCLUDES_BASE_DIR}/vorbis/psy.c
        ${INCLUDES_BASE_DIR}/vorbis/registry.c
        ${INCLUDES_BASE_DIR}/vorbis/res0.c
        ${INCLUDES_BASE_DIR}/vorbis/sharedbook.c
        ${INCLUDES_BASE_DIR}/vorbis/smallft.c
        ${INCLUDES_BASE_DIR}/vorbis/synthesis.c
        ${INCLUDES_BASE_DIR}/vorbis/vorbisfile.c
        ${INCLUDES_BASE_DIR}/vorbis/window.c
        ${INCLUDES_BASE_DIR}/zlib/adler32.c
        ${INCLUDES_BASE_DIR}/zlib/crc32.c
        ${INCLUDES_BASE_DIR}/zlib/deflate.c
        ${INCLUDES_BASE_DIR}/zlib/inffast.c
        ${INCLUDES_BASE_DIR}/zlib/inflate.c
        ${INCLUDES_BASE_DIR}/zlib/inftrees.c
        ${INCLUDES_BASE_DIR}/zlib/trees.c
        ${INCLUDES_BASE_DIR}/zlib/zutil.c
    )
elseif(DOWNLOAD_DEPENDENCIES)
    find_package(ZLIB REQUIRED)

    ExternalProject_Add(libogg
        URL https://github.com/xiph/ogg/releases/download/v1.3.6/libogg-1.3.6.tar.gz
        URL_HASH SHA256=83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638
        CMAKE_ARGS
            ${CMAKE_COMMON_DEP_ARGS}
            -DCMAKE_INSTALL_LIBDIR=lib
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libogg.a
    )
    ExternalProject_Get_Property(libogg INSTALL_DIR)
    set(OGG_INSTALL_DIR ${INSTALL_DIR})

    set(OGG_LIBRARY ${OGG_INSTALL_DIR}/lib/libogg.a)
    set(OGG_INCLUDE_DIR ${OGG_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR})

    ExternalProject_Add(libvorbis
        URL https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.xz
        URL_HASH SHA256=b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b
        CMAKE_ARGS
            ${CMAKE_COMMON_DEP_ARGS}
            -DOGG_LIBRARY=${OGG_LIBRARY}
            -DOGG_INCLUDE_DIR=${OGG_INCLUDE_DIR}
            -DCMAKE_INSTALL_LIBDIR=lib
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        BUILD_BYPRODUCTS
            <INSTALL_DIR>/lib/libvorbis.a
            <INSTALL_DIR>/lib/libvorbisfile.a
    )
    add_dependencies(libvorbis libogg)
    ExternalProject_Get_Property(libvorbis INSTALL_DIR)
    set(VORBIS_INSTALL_DIR ${INSTALL_DIR})

    add_library(ogg STATIC IMPORTED)
    set_target_properties(ogg PROPERTIES IMPORTED_LOCATION ${OGG_LIBRARY})

    add_library(vorbis STATIC IMPORTED)
    set_target_properties(vorbis PROPERTIES IMPORTED_LOCATION ${VORBIS_INSTALL_DIR}/lib/libvorbis.a)
    add_library(vorbisfile STATIC IMPORTED)
    set_target_properties(vorbisfile PROPERTIES IMPORTED_LOCATION ${VORBIS_INSTALL_DIR}/lib/libvorbisfile.a)

    set(VORBIS_INCLUDE_DIR ${VORBIS_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR} ${OGG_INCLUDE_DIR})
    set(OGGVORBIS_LIBRARIES vorbisfile vorbis ogg)

    add_dependencies(vorbisfile libvorbis)
    add_dependencies(vorbis libvorbis)
    add_dependencies(ogg libogg)
else()
    find_package(ZLIB REQUIRED)

    find_package(OggVorbis QUIET)
    if(NOT OGGVORBIS_FOUND)
        pkg_check_modules(OGGVORBIS REQUIRED vorbis vorbisenc vorbisfile)
    endif()
endif()

# --- SDL3 & SDL3_image ---
set(SDL3_IMAGE_INCLUDE_DIRS "")
set(SDL3_IMAGE_LIBRARIES "")

if(TARGET SDL3::SDL3)
    list(APPEND SDL3_IMAGE_LIBRARIES SDL3::SDL3)
else()
    find_package(SDL3 QUIET)
    if(SDL3_FOUND)
        list(APPEND SDL3_IMAGE_LIBRARIES SDL3::SDL3)
    elseif(WIN32)
        set(SDL_ARCH_DIR ${INCLUDES_BASE_DIR}/sdl/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>)
        find_library(SDL3_LIB NAMES SDL3 PATHS
            ${INCLUDES_BASE_DIR}/sdl/lib/x64
            ${INCLUDES_BASE_DIR}/sdl/lib/x86
            ${INCLUDES_BASE_DIR}/sdl/lib
            NO_DEFAULT_PATH
        )
        if(NOT SDL3_LIB)
            find_library(SDL3_LIB NAMES SDL3 REQUIRED)
        endif()
        list(APPEND SDL3_IMAGE_INCLUDE_DIRS
            ${INCLUDES_BASE_DIR}/sdl
            ${INCLUDES_BASE_DIR}/sdl/include
        )
        list(APPEND SDL3_IMAGE_LIBRARIES ${SDL3_LIB})
    else()
        pkg_check_modules(SDL3 REQUIRED sdl3)
        list(APPEND SDL3_IMAGE_INCLUDE_DIRS ${SDL3_INCLUDE_DIRS})
        list(APPEND SDL3_IMAGE_LIBRARIES ${SDL3_LIBRARIES})
    endif()
endif()

if(TARGET SDL3_image::SDL3_image)
    list(APPEND SDL3_IMAGE_LIBRARIES SDL3_image::SDL3_image)
else()
    find_package(SDL3_image QUIET)
    if(SDL3_image_FOUND)
        list(APPEND SDL3_IMAGE_LIBRARIES SDL3_image::SDL3_image)
    elseif(WIN32)
        find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS
            ${INCLUDES_BASE_DIR}/sdl-image/lib/x64
            ${INCLUDES_BASE_DIR}/sdl-image/lib/x86
            ${INCLUDES_BASE_DIR}/sdl-image/lib
            NO_DEFAULT_PATH
        )
        if(NOT SDL3_IMAGE_LIB)
            find_library(SDL3_IMAGE_LIB NAMES SDL3_image REQUIRED)
        endif()
        list(APPEND SDL3_IMAGE_INCLUDE_DIRS
            ${INCLUDES_BASE_DIR}/sdl-image
            ${INCLUDES_BASE_DIR}/sdl-image/include
        )
        list(APPEND SDL3_IMAGE_LIBRARIES ${SDL3_IMAGE_LIB})
    else()
        pkg_check_modules(SDL3_IMG REQUIRED sdl3-image)
        list(APPEND SDL3_IMAGE_INCLUDE_DIRS ${SDL3_IMG_INCLUDE_DIRS})
        list(APPEND SDL3_IMAGE_LIBRARIES ${SDL3_IMG_LIBRARIES})
    endif()
endif()

set_as_hdll(fmt)

target_include_directories(fmt.hdll
    PRIVATE
    ${ZLIB_INCLUDE_DIRS}
    ${SDL3_IMAGE_INCLUDE_DIRS}
    ${VORBIS_INCLUDE_DIR}
    ${MINIMP3_INCLUDE_DIR}
    ${MIKKTSPACE_INCLUDE_DIR}
)

target_link_libraries(fmt.hdll
    libhl
    ${ZLIB_LIBRARIES}
    ${SDL3_IMAGE_LIBRARIES}
    ${OGGVORBIS_LIBRARIES}
)

install(
    TARGETS
        fmt.hdll
    DESTINATION ${HDLL_DESTINATION}
)