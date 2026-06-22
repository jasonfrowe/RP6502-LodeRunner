#!/usr/bin/env python3
import sys
import os
import struct
import mido

# YM3812 / OPL2 register definitions and offsets
mod_offsets = [0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12]
car_offsets = [0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15]

FNUM_TABLE = [308, 325, 345, 365, 387, 410, 434, 460, 487, 516, 547, 579]

class OPLPatch:
    def __init__(self, m_ave, m_ksl, m_atdec, m_susrel, m_wave,
                       c_ave, c_ksl, c_atdec, c_susrel, c_wave, feedback):
        self.m_ave = m_ave
        self.m_ksl = m_ksl
        self.m_atdec = m_atdec
        self.m_susrel = m_susrel
        self.m_wave = m_wave
        self.c_ave = c_ave
        self.c_ksl = c_ksl
        self.c_atdec = c_atdec
        self.c_susrel = c_susrel
        self.c_wave = c_wave
        self.feedback = feedback

# Predefined YM3812 FM patches to mimic NES 2A03 synthesizer
pulse_patch = OPLPatch(
    m_ave=0x21, m_ksl=0x00, m_atdec=0xF2, m_susrel=0xF6, m_wave=0x03,  # Waveform 3: Square
    c_ave=0x21, c_ksl=0x00, c_atdec=0xF1, c_susrel=0xF4, c_wave=0x03,  # Waveform 3: Square
    feedback=0x01  # Additive synthesis
)

triangle_patch = OPLPatch(
    m_ave=0x21, m_ksl=0x00, m_atdec=0xF2, m_susrel=0xF6, m_wave=0x00,  # Waveform 0: Sine (soft triangle proxy)
    c_ave=0x21, c_ksl=0x00, c_atdec=0xF1, c_susrel=0xF4, c_wave=0x00,  # Waveform 0: Sine
    feedback=0x01  # Additive synthesis
)

noise_patch = OPLPatch(
    m_ave=0x2F, m_ksl=0x00, m_atdec=0xF9, m_susrel=0x0F, m_wave=0x00,
    c_ave=0x21, c_ksl=0x00, c_atdec=0xFA, c_susrel=0x0F, c_wave=0x00,
    feedback=0x0E  # Connection=0 (FM) with maximum modulator feedback for noise
)

def note_to_opl(midi_note, key_on=True):
    if midi_note < 12:
        midi_note = 12
    block = (midi_note - 12) // 12
    note_idx = (midi_note - 12) % 12
    if block > 7:
        block = 7
    f_num = FNUM_TABLE[note_idx]
    high_byte = (block << 2) | ((f_num >> 8) & 0x03)
    if key_on:
        high_byte |= 0x20
    low_byte = f_num & 0xFF
    return low_byte, high_byte

def write_patch(ch, patch):
    m = mod_offsets[ch]
    c = car_offsets[ch]
    return [
        (0x20 + m, patch.m_ave),
        (0x20 + c, patch.c_ave),
        (0x40 + m, patch.m_ksl),
        (0x40 + c, patch.c_ksl),
        (0x60 + m, patch.m_atdec),
        (0x60 + c, patch.c_atdec),
        (0x80 + m, patch.m_susrel),
        (0x80 + c, patch.c_susrel),
        (0xE0 + m, patch.m_wave),
        (0xE0 + c, patch.c_wave),
        (0xC0 + ch, patch.feedback)
    ]

def convert_midi(midi_path, out_bin_path):
    if not os.path.exists(midi_path):
        print(f"Error: MIDI file {midi_path} not found.")
        sys.exit(1)

    print(f"Reading MIDI file: {midi_path}")
    mid = mido.MidiFile(midi_path)
    
    # Scan active channels
    active_channels = set()
    for msg in mid:
        if msg.type in ('note_on', 'note_off'):
            active_channels.add(msg.channel)
    active_channels = sorted(list(active_channels))
    
    # Map MIDI channel to OPL2 channel (0-8)
    channel_map = {}
    opl_ch = 0
    for midi_ch in active_channels:
        if midi_ch == 9: # MIDI Drum channel
            channel_map[midi_ch] = 3 # Map drums to YM3812 Channel 3
        else:
            if opl_ch == 3: # Skip channel 3 if reserved for drums
                opl_ch += 1
            channel_map[midi_ch] = opl_ch
            opl_ch += 1
            
    print("MIDI to OPL2 Channel Mapping:")
    for m_ch, o_ch in channel_map.items():
        print(f"  MIDI Channel {m_ch} -> OPL2 Channel {o_ch}")
        
    # Group writes by 60Hz frame
    frames = {}
    
    def add_write(frame, reg, val):
        if frame not in frames:
            frames[frame] = []
        frames[frame].append((reg, val))
        
    # Setup OPL2 chip at frame 0
    add_write(0, 0x01, 0x20) # Enable Waveform Select
    add_write(0, 0xBD, 0x00) # Melodic Mode
    
    # Write initial patches for all mapped channels
    for m_ch, o_ch in channel_map.items():
        if m_ch == 9 or o_ch == 3:
            patch = noise_patch
        elif o_ch == 2:
            patch = triangle_patch
        else:
            patch = pulse_patch
        for reg, val in write_patch(o_ch, patch):
            add_write(0, reg, val)
            
    # Track note state to avoid redundant writes
    chan_notes = {} # o_ch -> active note
    
    time_accum = 0.0
    for msg in mid:
        time_accum += msg.time
        frame = int(time_accum * 60.0)
        
        if msg.type in ('note_on', 'note_off'):
            if msg.channel not in channel_map:
                continue
            o_ch = channel_map[msg.channel]
            
            is_on = (msg.type == 'note_on' and msg.velocity > 0)
            
            if is_on:
                # Set volume (Total Level) on Carrier
                vol = 63 - (msg.velocity // 2)
                c_offset = car_offsets[o_ch]
                add_write(frame, 0x40 + c_offset, vol)
                
                # Write F-number low and high (key on)
                low_byte, high_byte = note_to_opl(msg.note, key_on=True)
                add_write(frame, 0xA0 + o_ch, low_byte)
                add_write(frame, 0xB0 + o_ch, high_byte)
                chan_notes[o_ch] = msg.note
            else:
                # Key off
                if o_ch in chan_notes:
                    low_byte, high_byte = note_to_opl(chan_notes[o_ch], key_on=False)
                    add_write(frame, 0xB0 + o_ch, high_byte)
                    del chan_notes[o_ch]
                    
    # Generate the packet list
    sorted_frames = sorted(frames.keys())
    packets = []
    
    for i, frame in enumerate(sorted_frames):
        writes = frames[frame]
        next_frame = sorted_frames[i+1] if i + 1 < len(sorted_frames) else frame
        delay = next_frame - frame
        
        for j, (reg, val) in enumerate(writes):
            # The delay is only applied to the last write of the frame
            d = delay if j == len(writes) - 1 else 0
            packets.append((reg, val, d))
            
    # Add loop instruction (reg=0xFF, val=0xFF, delay=0)
    packets.append((0xFF, 0xFF, 0))
    
    # Write to binary file
    with open(out_bin_path, 'wb') as f:
        for reg, val, delay in packets:
            f.write(struct.pack('<BBH', reg, val, delay))
            
    print(f"Successfully converted to {out_bin_path} ({len(packets) * 4} bytes)")

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 midi_to_opl.py <input.mid> <output.bin>")
        sys.exit(1)
    convert_midi(sys.argv[1], sys.argv[2])

if __name__ == '__main__':
    main()
