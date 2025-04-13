cmake_minimum_required(VERSION 3.30)
project(examples LANGUAGES C)

if(MSVC)
  set(GETOPT_SOURCES
    "${CMAKE_SOURCE_DIR}/win32/experimental/wincompat/getopt.c"
    "${CMAKE_SOURCE_DIR}/win32/experimental/wincompat/getopt_long.c"
  )
  set(GETOPT_INCLUDE_DIR
    "${CMAKE_SOURCE_DIR}/win32/experimental/wincompat"
  )
else()
  set(GETOPT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/getopt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/getopt1.c"
  )
  set(GETOPT_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()

find_package(Vorbis CONFIG)

add_executable(dump_psnr dump_psnr.c ${GETOPT_SOURCES})
target_include_directories(dump_psnr PRIVATE ${GETOPT_INCLUDE_DIR})
target_link_libraries(dump_psnr PRIVATE theora)

add_executable(dump_video dump_video.c ${GETOPT_SOURCES})
target_include_directories(dump_video PRIVATE ${GETOPT_INCLUDE_DIR})
target_link_libraries(dump_video PRIVATE theora)

if(Vorbis_FOUND)
  add_executable(encoder_example encoder_example.c ${GETOPT_SOURCES})
  target_include_directories(encoder_example PRIVATE ${GETOPT_INCLUDE_DIR})
  target_link_libraries(encoder_example PRIVATE theora Vorbis::vorbisenc)
endif()

add_executable(libtheora_info libtheora_info.c)
target_link_libraries(libtheora_info PRIVATE theora)

if(MSVC)
  add_executable(transcoder_example
    "${CMAKE_SOURCE_DIR}/win32/experimental/transcoder/transcoder_example.c"
    ${GETOPT_SOURCES}
  )
  target_include_directories(transcoder_example PRIVATE ${GETOPT_INCLUDE_DIR})
  target_link_libraries(transcoder_example PRIVATE theora Vorbis::vorbisenc)
endif()
