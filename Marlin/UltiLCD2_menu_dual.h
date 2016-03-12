#ifndef ULTILCD2_MENU_DUAL_H
#define ULTILCD2_MENU_DUAL_H

#include "Configuration.h"

#if EXTRUDERS > 1
extern uint8_t menu_extruder;

void lcd_menu_dual();
void switch_extruder(uint8_t newExtruder, bool moveZ);
void lcd_menu_tune_tcretract();
void lcd_menu_extruderoffset();
#endif // EXTRUDERS

#endif //ULTILCD2_MENU_DUAL_H
