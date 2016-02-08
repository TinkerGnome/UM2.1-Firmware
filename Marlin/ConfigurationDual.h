#ifndef UM2_DUAL_H
#define UM2_DUAL_H

#include "Configuration.h"

#if (EXTRUDERS > 1)

#define DOCK_X_OFFSET      -47.0
#define DOCK_Y_OFFSET       10.0

#define WIPE_X_OFFSET        5.5
#define WIPE_Y_OFFSET        4.0

extern float add_homeing_z2;
extern float dock_position[2];
extern float wipe_position[2];
extern float priming_len[2];
extern float priming_retract[2];

void Dual_ResetDefault();

#ifdef EEPROM_CHITCHAT
void Dual_PrintSettings();
#else
FORCE_INLINE void Dual_PrintSettings() {}
#endif

#ifdef EEPROM_SETTINGS
void Dual_StoreSettings();
void Dual_RetrieveSettings();
#else
FORCE_INLINE void Dual_StoreSettings() {}
FORCE_INLINE void Dual_RetrieveSettings() { Dual_ResetDefault(); }
#endif

#endif //EXTRUDERS

#endif //UM2_DUAL_H
