idf_component_register(SRCS "bepio_system_manager.c" "bepio_mn316_manager.c"
                       INCLUDE_DIRS "."
                       REQUIRES mn316_service
                       PRIV_REQUIRES platform)
