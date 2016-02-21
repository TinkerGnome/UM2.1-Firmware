#include "commandbuffer.h"
#include "cardreader.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"

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

void CommandBuffer::processT0()
{
#ifdef SDSUPPORT
    if (t0)
    {
        processScript(t0);
    }
    else
#endif // SDSUPPORT
    {
        char buffer[30] = {0};

        sprintf_P(buffer, PSTR("G1 X170 Y51 F%i"), 200*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y%.2f"), dock_position[Y_AXIS]);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X%.2f F%i"), dock_position[X_AXIS], 50*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y55 F%i"), 100*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X171 F%i"), 200*60);
        process_command(buffer);

    }
}

void CommandBuffer::processT1()
{
#ifdef SDSUPPORT
    if (t1)
    {
        processScript(t1);
    }
    else
#endif // SDSUPPORT
    {
        char buffer[30] = {0};

        sprintf_P(buffer, PSTR("G1 X170 Y55 F%i"), 200*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X%.2f"), dock_position[X_AXIS]);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y%.2f F%i"), dock_position[Y_AXIS], 100*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X170 F%i"), 50*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y55 F%i"), 200*60);
        process_command(buffer);

    }
}

void CommandBuffer::processWipe()
{
#ifdef SDSUPPORT
    if (wipe)
    {
        processScript(wipe);
    }
    else
#endif // SDSUPPORT
    {
        char buffer[30] = {0};

        // undo the toolchange retraction
        plan_set_e_position((current_position[E_AXIS] - extruder_swap_retract_length) / volume_to_filament_length[active_extruder]);
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], END_OF_PRINT_RECOVERY_SPEED, active_extruder);

        // prime nozzle
        plan_set_e_position((current_position[E_AXIS] - (extruder_swap_retract_length*0.5f)) / volume_to_filament_length[active_extruder]);
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], 0.7f, active_extruder);

        // wipe moves
        sprintf_P(buffer, PSTR("G1 X%.2f Y60 F%i"), wipe_position[X_AXIS], 200*60);
        process_command(buffer);
        process_command_P(PSTR("G1 Y30"));
        sprintf_P(buffer, PSTR("G1 Y%.2f F%i"), wipe_position[Y_AXIS]-3.0f, 100*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X%.2f"), wipe_position[X_AXIS]+5.5f);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X%.2f Y%.2f"), wipe_position[X_AXIS]-5.5f, wipe_position[Y_AXIS]);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 X%.2f Y%.2f"), wipe_position[X_AXIS]+5.5f, wipe_position[Y_AXIS]+4.0f);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y35 F%i"), 100*60);
        process_command(buffer);
        sprintf_P(buffer, PSTR("G1 Y60 F%i"), 200*60);
        process_command(buffer);

    }
}

#endif
