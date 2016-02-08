#include "Marlin.h"
#include "ConfigurationDual.h"

#if (EXTRUDERS > 1)

//Random number to verify that the dual settings are already written to the EEPROM
#define EEPROM_DUAL_MAGIC 0x218DE93C

// IMPORTANT:  Whenever there are changes made to the variables stored in EEPROM
// in the functions below, also increment the version number. This makes sure that
// the default values are used whenever there is a change to the data, to prevent
// wrong data being written to the variables.
#define STORE_DUAL_VERSION 1

#define EEPROM_DUAL_START      0x600  //  4 Byte Magic number
#define EEPROM_DUAL_VERSION    0x604  //  2 Byte
#define EEPROM_ADDHOMEING_Z2   0x606  //  4 Byte
#define EEPROM_EXTRUDER_OFFSET 0x60A  // 16 Byte
#define EEPROM_DOCK_POSITION   0x61A  //  8 Byte
#define EEPROM_WIPE_POSITION   0x622  //  8 Byte
#define EEPROM_DUAL_RESERVED   0x62A  // next address

#define DOCK_X_POSITION    218.0
#define DOCK_Y_POSITION     41.0

#define WIPE_X_POSITION     90.5
#define WIPE_Y_POSITION     24.0

float add_homeing_z2 = 0.0f;

float dock_position[2]   = { DOCK_X_POSITION, DOCK_Y_POSITION };
float wipe_position[2]   = { WIPE_X_POSITION, WIPE_Y_POSITION };


void Dual_ResetDefault()
{
    add_homeing_z2 = add_homeing[Z_AXIS];

    dock_position[X_AXIS] = DOCK_X_POSITION;
    dock_position[Y_AXIS] = DOCK_Y_POSITION;
    wipe_position[X_AXIS] = WIPE_X_POSITION;
    wipe_position[Y_AXIS] = WIPE_Y_POSITION;

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
    eeprom_write_block(dock_position,   (uint8_t*)EEPROM_DOCK_POSITION,   sizeof(dock_position));
    eeprom_write_block(wipe_position,   (uint8_t*)EEPROM_WIPE_POSITION,   sizeof(wipe_position));

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
        uint16_t version = eeprom_read_word((const uint16_t*)EEPROM_DUAL_VERSION);
        add_homeing_z2 = eeprom_read_float((const float*)EEPROM_ADDHOMEING_Z2);
        eeprom_read_block(extruder_offset, (uint8_t*)EEPROM_EXTRUDER_OFFSET, sizeof(extruder_offset));
        if (version > 0)
        {
            eeprom_read_block(dock_position, (uint8_t*)EEPROM_DOCK_POSITION, sizeof(dock_position));
            eeprom_read_block(wipe_position, (uint8_t*)EEPROM_WIPE_POSITION, sizeof(wipe_position));
        }
        else
        {
            dock_position[X_AXIS] = DOCK_X_POSITION;
            dock_position[Y_AXIS] = DOCK_Y_POSITION;
            wipe_position[X_AXIS] = WIPE_X_POSITION;
            wipe_position[Y_AXIS] = WIPE_Y_POSITION;
        }
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

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Dock position:");
    SERIAL_ECHOPAIR(" X", dock_position[X_AXIS]);
    SERIAL_ECHOPAIR(" Y", dock_position[Y_AXIS]);
    SERIAL_ECHO_NEWLINE;

    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Wipe position:");
    SERIAL_ECHOPAIR(" X", wipe_position[X_AXIS]);
    SERIAL_ECHOPAIR(" Y", wipe_position[Y_AXIS]);
    SERIAL_ECHO_NEWLINE;
}
#endif //EEPROM_CHITCHAT

#endif //EXTRUDERS
