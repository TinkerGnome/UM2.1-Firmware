
#include "commandbuffer.h"
#include "cardreader.h"
#include "Configuration_adv.h"

#include "Marlin.h"
#include "temperature.h"
#include "stepper.h"
#include "lifetime_stats.h"
#include "UltiLCD2.h"

#define CONFIG_DIR  "config"
#define FILENAME_T0 "T0"
#define FILENAME_T1 "T1"
#define FILENAME_WIPE "wipe"

#if (EXTRUDERS > 1) && defined(SDSUPPORT)

CommandBuffer cmdBuffer;

CommandBuffer::~CommandBuffer()
{
    deleteScript(t0);
    deleteScript(t1);
    deleteScript(wipe);
}

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

#endif
