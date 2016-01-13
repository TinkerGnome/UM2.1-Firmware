#ifndef UM2_DUAL_H
#define UM2_DUAL_H

#include "Configuration.h"

#if (EXTRUDERS > 1)

void Dual_ResetDefault();


extern float add_homeing_z2;


#ifdef EEPROM_SETTINGS
void Dual_StoreSettings();
void Dual_RetrieveSettings();
#else
FORCE_INLINE void Dual_StoreSettings() {}
FORCE_INLINE void Dual_RetrieveSettings() { Dual_ResetDefault(); }
#endif

#endif //EXTRUDERS

#endif //UM2_DUAL_H
