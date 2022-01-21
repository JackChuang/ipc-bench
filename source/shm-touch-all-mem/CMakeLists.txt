###########################################################
## TARGETS
###########################################################

add_executable(shm-touchall-client client.c)
add_executable(shm-touchall-server server.c)
#add_executable(shm shm.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(shm-touchall-client ipc-bench-common)
target_link_libraries(shm-touchall-server ipc-bench-common)
#target_link_libraries(shm ipc-bench-common)
