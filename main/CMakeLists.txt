set(srcs "main.cpp" "card_ui.cpp" "character.cpp" "knoedel.cpp" "knoedel_bubbles.cpp" "katze.cpp" "dinge.cpp" "scenery.cpp"
         "deck.cpp" "content.cpp" "progress.cpp" "motion.cpp" "audio_player.cpp" "third_party/minimp3_impl.c"
         "credentials.cpp" "usb_commands.cpp" "net.cpp" "openai.cpp" "memos.cpp" "notes_sync.cpp" "notes_flow.cpp"
         "notes_ui.cpp" "notes_page.cpp")

# The board's side of main/board/board.h, chosen in Kconfig ("Target board").
if(CONFIG_KNOEDEL_BOARD_M5_STOPWATCH)
    list(APPEND srcs "board/m5_stopwatch.cpp")
elseif(CONFIG_KNOEDEL_BOARD_WAVESHARE_175C)
    list(APPEND srcs "board/waveshare_175c/board.cpp" "board/waveshare_175c/display.cpp"
                     "board/waveshare_175c/speaker.cpp" "board/waveshare_175c/clock.cpp")
endif()
# Hold the right key to record and send it over USB (Kconfig, off by default).
if(CONFIG_KNOEDEL_MIC_TEST)
    list(APPEND srcs "mic_test.cpp")
endif()

# The roots api.openai.com is checked against (main/openai.cpp)
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." "board" EMBED_TXTFILES "certs/api_roots.pem")

# Third-party decoder: built as-is, without this project's warning settings.
set_source_files_properties("third_party/minimp3_impl.c" PROPERTIES COMPILE_OPTIONS "-w")
