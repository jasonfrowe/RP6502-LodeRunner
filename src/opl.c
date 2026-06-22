#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "opl.h"

// F-Number table for Octave 4 @ 4.0 MHz
const uint16_t fnum_table[12] = {
    308, 325, 345, 365, 387, 410, 434, 460, 487, 516, 547, 579
};

uint8_t shadow_b0[9] = {0}; 
uint8_t shadow_ksl_m[9] = {0};
uint8_t shadow_ksl_c[9] = {0};

uint16_t midi_to_opl_freq(uint8_t midi_note) {
    if (midi_note < 12) midi_note = 12;
    
    int block = (midi_note - 12) / 12;
    int note_idx = (midi_note - 12) % 12;
    
    if (block > 7) block = 7;

    uint16_t f_num = fnum_table[note_idx];
    uint8_t high_byte = 0x20 | (block << 2) | ((f_num >> 8) & 0x03);
    uint8_t low_byte = f_num & 0xFF;

    return (high_byte << 8) | low_byte;
}

void opl_write(uint8_t reg, uint8_t data) {
#ifdef USE_NATIVE_OPL2
    RIA.addr1 = OPL_ADDR + reg;
    RIA.rw1 = data;
#else
    RIA.addr1 = OPL_ADDR;
    RIA.step1 = 1;
    RIA.rw1 = reg;
    RIA.rw1 = data;
#endif
}

void opl_silence_all() {
    for (uint8_t i = 0; i < 9; i++) {
        opl_write(0xB0 + i, 0x00);
    }
}

void opl_fifo_clear() {
    RIA.addr1 = OPL_ADDR + 2; 
    RIA.step1 = 0;
    RIA.rw1 = 1;         
}

void OPL_NoteOn(uint8_t channel, uint8_t midi_note) {
    if (channel > 8) return;
    
    uint16_t freq = midi_to_opl_freq(midi_note);
    opl_write(0xA0 + channel, freq & 0xFF);
    opl_write(0xB0 + channel, (freq >> 8) & 0xFF);
    shadow_b0[channel] = (freq >> 8) & 0x1F;
}

void OPL_NoteOff(uint8_t channel) {
    if (channel > 8) return;
    opl_write(0xB0 + channel, shadow_b0[channel] & 0x1F); // Write Octave & F-Num with KeyOn = 0
}

void opl_clear() {
    for (int i = 0; i < 256; i++) {
        opl_write(i, 0x00);
    }
    for (int i = 0; i < 9; i++) {
        shadow_b0[i] = 0;
    }
}

void OPL_SetVolume(uint8_t chan, uint8_t velocity) {
    uint8_t vol = 63 - (velocity >> 1);
    static const uint8_t car_offsets[] = {0x03,0x04,0x05,0x0B,0x0C,0x0D,0x13,0x14,0x15};
    opl_write(0x40 + car_offsets[chan], (shadow_ksl_c[chan] & 0xC0) | vol);
}

void OPL_SetPatch(uint8_t channel, const OPL_Patch* p) {
    static const uint8_t mod_offsets[] = {0x00,0x01,0x02,0x08,0x09,0x0A,0x10,0x11,0x12};
    static const uint8_t car_offsets[] = {0x03,0x04,0x05,0x0B,0x0C,0x0D,0x13,0x14,0x15};
    
    uint8_t m = mod_offsets[channel];
    uint8_t c = car_offsets[channel];

    shadow_ksl_m[channel] = p->m_ksl;
    shadow_ksl_c[channel] = p->c_ksl;

    opl_write(0x20 + m, p->m_ave);
    opl_write(0x20 + c, p->c_ave);
    opl_write(0x40 + m, p->m_ksl);
    opl_write(0x40 + c, p->c_ksl);
    opl_write(0x60 + m, p->m_atdec);
    opl_write(0x60 + c, p->c_atdec);
    opl_write(0x80 + m, p->m_susrel);
    opl_write(0x80 + c, p->c_susrel);
    opl_write(0xE0 + m, p->m_wave);
    opl_write(0xE0 + c, p->c_wave);
    opl_write(0xC0 + channel, p->feedback);
}

void opl_init() {
    for (uint8_t i = 0; i < 9; i++) {
        opl_write(0xB0 + i, 0x00);
        shadow_b0[i] = 0;
    }

    for (int i = 0x01; i <= 0xF5; i++) {
        opl_write(i, 0x00);
    }

    for (int i = 0; i < 9; i++) {
        shadow_b0[i] = 0;
    }

    opl_write(0x01, 0x20); // Enable Waveform Select
    opl_write(0xBD, 0x00); // Ensure Melodic Mode
}

void opl_fifo_flush() {
    RIA.addr1 = OPL_ADDR + 2;
    RIA.step1 = 0;
    RIA.rw1 = 0xAA; 
}

void shutdown_audio() {
    opl_silence_all();       
    opl_fifo_flush();        
    OPL_Config(0, OPL_ADDR);   
}

void OPL_Config(uint8_t enable, uint16_t addr) {
#ifdef USE_NATIVE_OPL2
    xreg(0, 1, 0x01, addr); 
#else
    xregn(2, 0, 0, 2, enable, addr);
#endif
}

static int music_fd = -1;
static uint8_t music_buffer[512];
static uint16_t music_buf_idx = 0;
static uint16_t music_bytes_ready = 0; 
static uint16_t music_wait_ticks = 0;
static bool music_error_state = false;
static bool music_just_looped = false;

void music_init(const char* filename) {
    if (music_fd >= 0) close(music_fd);
    music_fd = open(filename, O_RDONLY);
    
    music_buf_idx = 0;
    music_wait_ticks = 0;
    music_just_looped = false;
    music_error_state = (music_fd < 0);

    if (music_error_state) {
        return;
    }

    int res = read(music_fd, music_buffer, 512);
                
    if (res < 0) {
        music_error_state = true;
        return;
    }

    music_bytes_ready = res; 
}

void music_stop(void) {
    if (music_fd >= 0) {
        close(music_fd);
        music_fd = -1;
    }
    // Silence channels 0 and 1 used by music
    opl_write(0xB0, 0x00);
    opl_write(0xB1, 0x00);
    opl_write(0x40, 0x3F); // channel 0 modulator volume to zero
    opl_write(0x43, 0x3F); // channel 0 carrier volume to zero
    opl_write(0x41, 0x3F); // channel 1 modulator volume to zero
    opl_write(0x44, 0x3F); // channel 1 carrier volume to zero
    shadow_b0[0] = 0;
    shadow_b0[1] = 0;
}

void update_music() {
    if (music_error_state || music_fd < 0) return;

    if (music_wait_ticks > 0) {
        music_wait_ticks--;
    }

    if (music_wait_ticks == 0) {
        while (music_wait_ticks == 0) {

            if (music_buf_idx >= 512){
                int res = read(music_fd, &music_buffer, 512);
                if (res < 0) {
                    music_error_state = true;
                    return;
                }
                music_buf_idx = 0;
            }

            uint8_t reg  = music_buffer[music_buf_idx++];
            uint8_t val  = music_buffer[music_buf_idx++];
            uint8_t d_lo = music_buffer[music_buf_idx++];
            uint8_t d_hi = music_buffer[music_buf_idx++];
            uint16_t delay = ((uint16_t)d_hi << 8) | d_lo;

            if (music_just_looped &&
                reg >= 0xB0 && reg <= 0xB8 &&
                (val & 0x20u) == 0u &&
                delay <= 1u) {
                music_just_looped = false;
                continue;
            }
            music_just_looped = false;

            if (reg == 0xFF && val == 0xFF) {
                if (lseek(music_fd, 0, SEEK_SET) < 0) {
                    music_error_state = true;
                    return;
                }
                music_buf_idx = music_bytes_ready; // Force immediate buffer reload
                music_just_looped = true;
                continue;
            } else {
                opl_write(reg, val);
            }

            if (delay > 0) {
                music_wait_ticks = delay;
            }
        }
    }
}
