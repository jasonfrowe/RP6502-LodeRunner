cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(LodeRunner C CXX ASM)

add_executable(LodeRunner)

rp6502_asset(LodeRunner help src/help.txt)
# XRAM Assets
rp6502_asset(LodeRunner 0x10000 images/player_4bpp.bin)
rp6502_asset(LodeRunner 0x11400 images/maptiles_4bpp.bin)
rp6502_asset(LodeRunner 0x12B80 maps/default.bin)
rp6502_asset(LodeRunner 0x12D5C images/texttiles_4bpp.bin)

# Pack concatenated level maps to ROM
rp6502_asset(LodeRunner maps maps/maps.bin)

# Music assets
rp6502_asset(LodeRunner loderun Music/LODERUN2.BIN)

rp6502_executable(LodeRunner 
    DATA file 
    RESET file
)

target_sources(LodeRunner PRIVATE
    src/main.c
    src/sprite_mode_5.c
    src/input.c
    src/tile_mode_2.c
    src/player.c
    src/opl.c
    src/sound.c
)
