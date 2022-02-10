cmake_minimum_required(VERSION 2.8)

project(drm_test)

set(CMAKE_CXX_STANDARD 11)
include_directories(include)

set(SOURCE_FILES single_buf.c)
add_executable(sing_buf ${SOURCE_FILES})
target_link_libraries(sing_buf  drm)
install(TARGETS sing_buf  DESTINATION bin)


set(SOURCE_FILES double_buf.c)
add_executable(double_buf ${SOURCE_FILES})
target_link_libraries(double_buf  drm)
install(TARGETS double_buf  DESTINATION bin)

set(SOURCE_FILES modeset_page_flip.c)
add_executable(modeset_page_flip ${SOURCE_FILES})
target_link_libraries(modeset_page_flip  drm)
install(TARGETS modeset_page_flip  DESTINATION bin)


set(SOURCE_FILES plane-test.c)
add_executable(plane-test ${SOURCE_FILES})
target_link_libraries(plane-test  drm)
install(TARGETS plane-test  DESTINATION bin)

set(SOURCE_FILES modeset-double-buffered.c)
add_executable(modeset-double-buffered ${SOURCE_FILES})
target_link_libraries(modeset-double-buffered  drm)
install(TARGETS modeset-double-buffered  DESTINATION bin)


set(SOURCE_FILES modeset.c)
add_executable(modeset ${SOURCE_FILES})
target_link_libraries(modeset  drm)
install(TARGETS modeset DESTINATION bin)

set(SOURCE_FILES modeset-vsync.c)
add_executable(modeset-vsync ${SOURCE_FILES})
target_link_libraries(modeset-vsync  drm)
install(TARGETS modeset-vsync DESTINATION bin)

set(SOURCE_FILES modset-bmp2.c)
add_executable(modset-bmp2 ${SOURCE_FILES})
target_link_libraries(modset-bmp2 drm)
install(TARGETS modset-bmp2 DESTINATION bin)
install(DIRECTORY . DESTINATION bin
        FILES_MATCHING PATTERN "*.bmp"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)