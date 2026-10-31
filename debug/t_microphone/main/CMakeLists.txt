idf_component_register(SRCS "t_microphone.c" "../../t_audio/main/audio_signal.c"
                       INCLUDE_DIRS "." "../../t_audio/main"
                       PRIV_REQUIRES audio_board esp32s3)
