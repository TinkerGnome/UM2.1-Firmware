#include "commandbuffer.h"
#include "cardreader.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"
#include "UltiLCD2_low_lib.h"
#include "UltiLCD2_menu_print.h" // use lcd_cache as char buffer

#if (EXTRUDERS > 1)

#define CONFIG_DIR  "config"
#define FILENAME_T0 "T0"
#define FILENAME_T1 "T1"
#define FILENAME_WIPE "wipe"

CommandBuffer cmdBuffer;

CommandBuffer::~CommandBuffer()
{
    deleteScript(t0);
    deleteScript(t1);
    deleteScript(wipe);
}

#ifdef SDSUPPORT
uint8_t CommandBuffer::initScripts()
{
    // clear all
    deleteScript(t0);
    deleteScript(t1);
    deleteScript(wipe);
    t0=0;
    t1=0;
    wipe=0;

    uint8_t cmdCount(0);

    if(!card.isOk())
    {
        card.initsd();
        if (!card.isOk())
        {
            return cmdCount;
        }
    }

    card.setroot();
    {
        // change to config dir
        char filename[16];
        strcpy_P(filename, PSTR(CONFIG_DIR));
        card.chdir(filename);
        // read scripts from sd card
        strcpy_P(filename, PSTR(FILENAME_T0));
        if ((t0 = readScript(filename))) ++cmdCount;
        strcpy_P(filename, PSTR(FILENAME_T1));
        if ((t1 = readScript(filename))) ++cmdCount;
        strcpy_P(filename, PSTR(FILENAME_WIPE));
        if ((wipe = readScript(filename))) ++cmdCount;
    }
    card.setroot();

    return cmdCount;
}

struct CommandBuffer::t_cmdline* CommandBuffer::readScript(const char *filename)
{
    struct t_cmdline* script(0);

    card.openFile(filename, true);
    if (card.isFileOpen())
    {
        struct t_cmdline* cmd = script = createScript();
        char buffer[MAX_CMD_SIZE] = {0};

        while( !card.eof() )
        {
            // read next line from file
            int16_t len = card.fgets(buffer, sizeof(buffer)-1);
            if (len <= 0) break;
            SERIAL_ECHO_START;
            SERIAL_ECHOLN(buffer);
            // remove trailing spaces
            while (len > 0 && buffer[len-1] <= ' ') buffer[--len] = '\0';
            if (len > 0)
            {
                if (cmd->str)
                {
                    // append line
                    cmd->next = createScript();
                    cmd = cmd->next;
                }
                cmd->str = new char[len+1];
                strncpy(cmd->str, buffer, len);
                cmd->str[len] = '\0';
            }
        }
        card.closefile();
    }
    if (script && !script->str && !script->next)
    {
        // no need to buffer empty files
        delete script;
        script = 0;
    }
    return script;
}

struct CommandBuffer::t_cmdline* CommandBuffer::createScript()
{
    struct t_cmdline* script = new t_cmdline;
    script->str = 0;
    script->next = 0;
    return script;
}

void CommandBuffer::deleteScript(struct t_cmdline *script)
{
    struct t_cmdline *cmd(script);
    while (cmd)
    {
        script = cmd->next;
        delete cmd->str;
        delete cmd;
        cmd = script;
    }
}

uint8_t CommandBuffer::processScript(struct t_cmdline *script)
{
    uint8_t cmdCount(0);
    while (script)
    {
        process_command(script->str);
        script = script->next;
        ++cmdCount;
        // update loop
        idle();
        checkHitEndstops();
    }
    return cmdCount;
}

#endif // SDSUPPORT

static char * toolchange_retract(char *buffer, uint8_t e)
{
    float length = toolchange_retractlen[e] / volume_to_filament_length[e];
#ifdef FWRETRACT
    if (EXTRUDER_RETRACTED(e))
    {
        length = max(0.0, length-retract_recover_length[e]);
        // retract_recover_length[e] += length;
        CLEAR_EXTRUDER_RETRACT(e);
        SET_TOOLCHANGE_RETRACT(e);
    }
    else
    {
        CLEAR_TOOLCHANGE_RETRACT(e);
    }
#endif // FWRETRACT
    // dtostrf(-length, 4, 2, LCD_CACHE_FILENAME(3));
    float_to_string(-length, LCD_CACHE_FILENAME(3), NULL);
    sprintf_P(buffer, PSTR("G1 E%s F%i"), LCD_CACHE_FILENAME(3), (int)toolchange_retractfeedrate[e]);
    return buffer;
}

void CommandBuffer::processT0(bool bRetract)
{
#ifdef SDSUPPORT
    if (t0)
    {
        processScript(t0);
    }
    else
#endif // SDSUPPORT
    {
        // use relative e coordinates
        uint8_t old_relative_state = axis_relative_state;
        axis_relative_state = (1 << E_AXIS);

        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X170 Y51 F%i"), 200*60);
        process_command(LCD_CACHE_FILENAME(2));
        if (bRetract)
        {
            process_command(toolchange_retract(LCD_CACHE_FILENAME(2), 1));
        }
        float_to_string(dock_position[Y_AXIS], LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y%s F%i"), LCD_CACHE_FILENAME(3), 100*60);
        process_command(LCD_CACHE_FILENAME(2));
        idle();
        float_to_string(dock_position[X_AXIS], LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s F%i"), LCD_CACHE_FILENAME(3), 50*60);
        process_command(LCD_CACHE_FILENAME(2));
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y55 F%i"), 100*60);
        process_command(LCD_CACHE_FILENAME(2));
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X171 F%i"), 200*60);
        process_command(LCD_CACHE_FILENAME(2));
        idle();
        axis_relative_state = old_relative_state;
    }
}

void CommandBuffer::processT1(bool bRetract)
{
#ifdef SDSUPPORT
    if (t1)
    {
        processScript(t1);
    }
    else
#endif // SDSUPPORT
    {
        // use relative e coordinates
        uint8_t old_relative_state = axis_relative_state;
        axis_relative_state = (1 << E_AXIS);

        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X170 Y55 F%i"), 200*60);
        process_command(LCD_CACHE_FILENAME(2));
        if (bRetract)
        {
            process_command(toolchange_retract(LCD_CACHE_FILENAME(2), 0));
        }
        float_to_string(dock_position[X_AXIS], LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s F%i"), LCD_CACHE_FILENAME(3), 100*60);
        process_command(LCD_CACHE_FILENAME(2));
        float_to_string(dock_position[Y_AXIS], LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y%s F%i"), LCD_CACHE_FILENAME(3), 100*60);
        process_command(LCD_CACHE_FILENAME(2));
        idle();
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X170 F%i"), 50*60);
        process_command(LCD_CACHE_FILENAME(2));
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y55 F%i"), 200*60);
        process_command(LCD_CACHE_FILENAME(2));
        idle();
        axis_relative_state = old_relative_state;
    }
}

void CommandBuffer::processWipe()
{
    // use relative e coordinates
    uint8_t old_relative_state = axis_relative_state;
    axis_relative_state = (1 << E_AXIS);

#ifdef FWRETRACT
    float length = TOOLCHANGE_RETRACTED(active_extruder) ? retract_recover_length[active_extruder] : toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
    CLEAR_TOOLCHANGE_RETRACT(active_extruder);
    CLEAR_EXTRUDER_RETRACT(active_extruder);
#else
    float length = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
#endif // FWRETRACT

    // undo the toolchange retraction
    float_to_string(length*0.8, LCD_CACHE_FILENAME(3), NULL);
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G1 E%s F%i"), LCD_CACHE_FILENAME(3), (int)toolchange_retractfeedrate[active_extruder]);
    process_command(LCD_CACHE_FILENAME(2));

    // prime nozzle
    float_to_string(toolchange_prime[active_extruder]+(length*0.2), LCD_CACHE_FILENAME(3), NULL);
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G1 E%s F%i"), LCD_CACHE_FILENAME(3), 40);
    process_command(LCD_CACHE_FILENAME(2));

    // retract before wipe
    length = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
    float_to_string(length*-0.4, LCD_CACHE_FILENAME(3), NULL);
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G1 E%s F%i"), LCD_CACHE_FILENAME(3), (int)toolchange_retractfeedrate[active_extruder]);
    process_command(LCD_CACHE_FILENAME(2));

    // wait a second
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G4 P%i"), 1000);
    process_command(LCD_CACHE_FILENAME(2));

#ifdef SDSUPPORT
    if (wipe)
    {
        processScript(wipe);
    }
    else
#endif // SDSUPPORT
    {
        // wipe moves
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y70 F%i"), 200*60);
        process_command(LCD_CACHE_FILENAME(2));
        float_to_string(wipe_position[X_AXIS], LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s"), LCD_CACHE_FILENAME(3));
        process_command(LCD_CACHE_FILENAME(2));
        process_command_P(PSTR("G0 Y30"));
        float_to_string(wipe_position[Y_AXIS]-3.0f, LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y%s F%i"), LCD_CACHE_FILENAME(3), 80*60);
        process_command(LCD_CACHE_FILENAME(2));
        float_to_string(wipe_position[X_AXIS]+5.5f, LCD_CACHE_FILENAME(3), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s"), LCD_CACHE_FILENAME(3));
        process_command(LCD_CACHE_FILENAME(2));
        float_to_string(wipe_position[X_AXIS]-5.5f, LCD_CACHE_FILENAME(3), NULL);
        float_to_string(wipe_position[Y_AXIS], LCD_CACHE_FILENAME(1), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s Y%s"), LCD_CACHE_FILENAME(3), LCD_CACHE_FILENAME(1));
        process_command(LCD_CACHE_FILENAME(2));
        float_to_string(wipe_position[X_AXIS]+5.5f, LCD_CACHE_FILENAME(3), NULL);
        float_to_string(wipe_position[Y_AXIS]+4.0f, LCD_CACHE_FILENAME(1), NULL);
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 X%s Y%s"), LCD_CACHE_FILENAME(3), LCD_CACHE_FILENAME(1));
        process_command(LCD_CACHE_FILENAME(2));
        sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y35 F%i"), 50*60);
        process_command(LCD_CACHE_FILENAME(2));

    }
    // small retract after wipe
    float_to_string(length*-0.1, LCD_CACHE_FILENAME(3), NULL);
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G1 E%s F%i"), LCD_CACHE_FILENAME(3), (int)toolchange_retractfeedrate[active_extruder]);
    process_command(LCD_CACHE_FILENAME(2));
#ifdef FWRETRACT
    retract_recover_length[active_extruder] = 0.5*length;
    SET_TOOLCHANGE_RETRACT(active_extruder);
#endif // FWRETRACT
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G0 Y70 F%i"), 100*60);
    process_command(LCD_CACHE_FILENAME(2));
    axis_relative_state = old_relative_state;
}

void CommandBuffer::move2heatup()
{
#if (EXTRUDERS > 1)
    uint16_t x = max(5, 5 + (int)extruder_offset[X_AXIS][active_extruder]);
    uint16_t y = IS_DUAL_ENABLED ? 70 : 10;
#else
    uint8_t x = 5;
    uint8_t y = 10;
#endif
    sprintf_P(LCD_CACHE_FILENAME(3), PSTR(HEATUP_POSITION_COMMAND), x, y);
    process_command(LCD_CACHE_FILENAME(3));
}

#endif
