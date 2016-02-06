#include "Marlin.h"
#include "ConfigurationDual.h"

//Random number to verify that the dual settings are already written to the EEPROM
#define EEPROM_DUAL_MAGIC 0x218DE93C

// IMPORTANT:  Whenever there are changes made to the variables stored in EEPROM
// in the functions below, also increment the version number. This makes sure that
// the default values are used whenever there is a change to the data, to prevent
// wrong data being written to the variables.
#define STORE_DUAL_VERSION 0

#define EEPROM_DUAL_START      0x600  //  4 Byte Magic number
#define EEPROM_DUAL_VERSION    0x604  //  2 Byte
#define EEPROM_ADDHOMEING_Z2   0x606  //  4 Byte
#define EEPROM_EXTRUDER_OFFSET 0x60A  // 16 Byte
#define EEPROM_DUAL_RESERVED   0x61A  // next address

#if (EXTRUDERS > 1)
float add_homeing_z2 = 0.0f;

void Dual_ResetDefault()
{
    add_homeing_z2 = add_homeing[Z_AXIS];

    float default_offset[2][EXTRUDERS] = {
#if defined(EXTRUDER_OFFSET_X) && defined(EXTRUDER_OFFSET_Y)
        EXTRUDER_OFFSET_X, EXTRUDER_OFFSET_Y
#endif
    };
    memcpy(extruder_offset, default_offset, sizeof(extruder_offset));
}

#ifdef EEPROM_SETTINGS
void Dual_StoreSettings()
{
    eeprom_write_float((float*)EEPROM_ADDHOMEING_Z2, add_homeing_z2);
    eeprom_write_block(extruder_offset, (uint8_t*)EEPROM_EXTRUDER_OFFSET, sizeof(extruder_offset));
    // write version
    uint16_t version = STORE_DUAL_VERSION;
    eeprom_write_word((uint16_t*)EEPROM_DUAL_VERSION, version);
    // validate data
    eeprom_write_dword((uint32_t*)(EEPROM_DUAL_START), EEPROM_DUAL_MAGIC);
    // serial output
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Dual Settings Stored");
}

void Dual_RetrieveSettings()
{
    //read stored version
    uint32_t magic = eeprom_read_dword((uint32_t*)(EEPROM_DUAL_START));
    if (magic == EEPROM_DUAL_MAGIC)
    {
        // uint16_t version = eeprom_read_word((const uint16_t*)EEPROM_DUAL_VERSION)
        add_homeing_z2 = eeprom_read_float((const float*)EEPROM_ADDHOMEING_Z2);
        eeprom_read_block(extruder_offset, (uint8_t*)EEPROM_EXTRUDER_OFFSET, sizeof(extruder_offset));
    }
    else
    {
        Dual_ResetDefault();
    }
    Dual_PrintSettings();
}
#endif //EEPROM_SETTINGS

#ifdef EEPROM_CHITCHAT
void Dual_PrintSettings()
{  // Always have this function, even with EEPROM_SETTINGS disabled, the current values will be shown
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("Home offset z2 (mm): ", add_homeing_z2);
    SERIAL_ECHO_NEWLINE;
}
#endif //EEPROM_CHITCHAT

#endif //EXTRUDERS
