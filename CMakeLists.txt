cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(LodeRunner C CXX ASM)

add_executable(LodeRunner)
rp6502_asset(LodeRunner help src/help.txt)
rp6502_executable(LodeRunner DATA file RESET file)
target_sources(LodeRunner PRIVATE
    src/main.c
)
