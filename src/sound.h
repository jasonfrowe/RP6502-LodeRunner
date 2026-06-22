#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>

// Initialise OPL SFX states
void sound_init(void);

// Advance SFX step sequencers. Call once per frame (60 Hz VSync)
void sound_update(void);

// Trigger sound effects
void sound_play_dig(void);          // Channel 5: Dig sweep
void sound_play_fall(bool enable);   // Channel 6: Fall siren (continuous loop when active)
void sound_play_trap(void);         // Channel 7: Guard trapped alert
void sound_play_gold(void);         // Channel 8: Gold pickup arpeggio (priority 1)
void sound_play_death(void);        // Channel 8: Death arpeggio (priority 2)
void sound_play_win(void);          // Channel 8: Level clear fanfare (priority 3)

#endif // SOUND_H
