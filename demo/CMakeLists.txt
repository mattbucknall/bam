add_executable(bam-demo
        main.c
        font-deja-vu-sans-48.c
        font-material-icons-48.c
        "${CMAKE_SOURCE_DIR}/bam.c"
)

target_link_libraries(bam-demo PRIVATE "${SDL2_LIBRARIES}")
target_include_directories(bam-demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${SDL2_INCLUDE_DIRS}")
target_compile_options(bam-demo PRIVATE "${SDL2_CFLAGS_OTHER}" -DBAM_DEBUG)
