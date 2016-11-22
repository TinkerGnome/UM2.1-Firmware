#include "commandbuffer.h"
#include "cardreader.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"
#include "machinesettings.h"
#include "UltiLCD2_low_lib.h"
#include "UltiLCD2_menu_print.h" // use lcd_cache as char buffer

#define CONFIG_DIR  "config"
#define FILENAME_T0 "T0"
#define FILENAME_T1 "T1"
#define FILENAME_WIPE "wipe"

#define TOOLCHANGE_STARTX 171
#define TOOLCHANGE_STARTY DUAL_Y_MIN_POS
#define WIPE_STARTX 60
#define WIPE_DISTANCEX 37
#define WIPE_DISTANCEY 2

CommandBuffer cmdBuffer;

#if (EXTRUDERS > 1)

FORCE_INLINE void relative_e_move(const float eDiff, const float feedrate, uint8_t e)
{
    current_position[E_AXIS] += eDiff;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, e);
}

static void toolchange_retract(float x, float y, int feedrate, uint8_t e)
{
    if (!TOOLCHANGE_RETRACTED(e))
    {
        float length = toolchange_retractlen[e] / volume_to_filament_length[e];
#ifdef FWRETRACT
        if (EXTRUDER_RETRACTED(e))
        {
            length = max(0.0, length-retract_recover_length[e]);
        }
        retract_recover_length[e] = toolchange_retractlen[e] / volume_to_filament_length[e];
        CLEAR_EXTRUDER_RETRACT(e);
        SET_TOOLCHANGE_RETRACT(e);
#endif // FWRETRACT
        current_position[E_AXIS] -= length;
    }
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, e);
}

void CommandBuffer::processT0(bool bRetract, bool bWipe)
{
    float ypos = (current_position[Y_AXIS] > TOOLCHANGE_STARTY) ? TOOLCHANGE_STARTY : current_position[Y_AXIS];
    if (bRetract)
    {
        toolchange_retract(TOOLCHANGE_STARTX, ypos, 200, 1);
    }
    else
    {
        CommandBuffer::moveHead(TOOLCHANGE_STARTX, ypos, 200);
    }

    CommandBuffer::moveHead(current_position[X_AXIS], dock_position[Y_AXIS], 100);
    idle();
    CommandBuffer::moveHead(dock_position[X_AXIS], current_position[Y_AXIS], 50);
    CommandBuffer::moveHead(dock_position[X_AXIS], TOOLCHANGE_STARTY, 100);
    CommandBuffer::moveHead(TOOLCHANGE_STARTX, TOOLCHANGE_STARTY, 200);
    idle();
}

void CommandBuffer::processT1(bool bRetract, bool bWipe)
{
    if (current_position[Y_AXIS] < TOOLCHANGE_STARTY)
    {
        CommandBuffer::moveHead(current_position[X_AXIS], TOOLCHANGE_STARTY, 200);
    }
    if (bRetract)
    {
        toolchange_retract(dock_position[X_AXIS], TOOLCHANGE_STARTY, 200, 0);
    }
    else
    {
        CommandBuffer::moveHead(dock_position[X_AXIS], TOOLCHANGE_STARTY, 200);
    }
    idle();
    CommandBuffer::moveHead(dock_position[X_AXIS], dock_position[Y_AXIS], 100);
    CommandBuffer::moveHead(TOOLCHANGE_STARTX, dock_position[Y_AXIS], 50);

    if (!bWipe | !bRetract)
    {
        CommandBuffer::moveHead(TOOLCHANGE_STARTX, TOOLCHANGE_STARTY, 200);
    }
    idle();
}

void CommandBuffer::processWipe()
{
    // limit fan speed during priming
    uint8_t old_printstate = printing_state;
    printing_state = PRINT_STATE_PRIMING;
    check_axes_activity();
#ifdef FWRETRACT
    float length = TOOLCHANGE_RETRACTED(active_extruder) ? retract_recover_length[active_extruder] : toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
    CLEAR_TOOLCHANGE_RETRACT(active_extruder);
    CLEAR_EXTRUDER_RETRACT(active_extruder);
    retract_recover_length[active_extruder] = 0.0f;
#else
    float length = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
#endif // FWRETRACT

    // undo the toolchange retraction
    relative_e_move(length*0.8, toolchange_retractfeedrate[active_extruder]/60, active_extruder);

    // prime nozzle
    relative_e_move((length*0.2)+toolchange_prime[active_extruder]/volume_to_filament_length[active_extruder], (PRIMING_MM3_PER_SEC * volume_to_filament_length[active_extruder]), active_extruder);

    // retract before wipe
    length = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
    relative_e_move(length*-0.4f, toolchange_retractfeedrate[active_extruder]/60, active_extruder);

    // wait a short moment
    st_synchronize();
    sprintf_P(LCD_CACHE_FILENAME(2), PSTR("G4 P%i"), 750);
    process_command(LCD_CACHE_FILENAME(2));

    // switch fan speed back to normal
    printing_state = old_printstate;
    check_axes_activity();

    // wipe start position
    CommandBuffer::moveHead(WIPE_STARTX, current_position[Y_AXIS], 200);

    // slow wipe move
    CommandBuffer::moveHead(WIPE_STARTX-WIPE_DISTANCEX, current_position[Y_AXIS], 40);

    // snip move
    CommandBuffer::moveHead(current_position[X_AXIS], current_position[Y_AXIS]+(WIPE_DISTANCEY*2), 125);
    // diagonal move
    CommandBuffer::moveHead(current_position[X_AXIS]+WIPE_DISTANCEY, current_position[Y_AXIS]-WIPE_DISTANCEY, 125);

    // back to start pos
    CommandBuffer::moveHead(wipe_position[X_AXIS]+extruder_offset[X_AXIS][active_extruder], current_position[Y_AXIS], 125);
    CommandBuffer::moveHead(current_position[X_AXIS], TOOLCHANGE_STARTY, 200);

    // small retract after wipe
    relative_e_move(length*-0.1, toolchange_retractfeedrate[active_extruder]/60, active_extruder);
#ifdef FWRETRACT
    retract_recover_length[active_extruder] = 0.5*length;
    SET_EXTRUDER_RETRACT(active_extruder);
#endif // FWRETRACT
}
#endif // EXTRUDERS


void CommandBuffer::moveHead(float x, float y, int feedrate)
{
    current_position[X_AXIS] = x;
    current_position[Y_AXIS] = y;
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, active_extruder);
}

void CommandBuffer::move2heatup()
{
    float x, y;
#if (EXTRUDERS > 1)
    if IS_DUAL_ENABLED
    {
        x = wipe_position[X_AXIS]+extruder_offset[X_AXIS][active_extruder];
        if (current_position[Y_AXIS] > TOOLCHANGE_STARTY)
        {
        // y = 65.0f;
            CommandBuffer::moveHead(x, TOOLCHANGE_STARTY, 200);
        }
        y = wipe_position[Y_AXIS];
    }
    else
    {
        x = max(5.0f, extruder_offset[X_AXIS][active_extruder] + 5);
        y = 10.0;
    }
#else
    x = 5.0f;
    y = 10.0f;
#endif
    CommandBuffer::moveHead(x, y, 200);
}

void CommandBuffer::move2front()
{
    float x = (X_MAX_POS/2);
#if (EXTRUDERS > 1)
    float y = IS_DUAL_ENABLED ? DUAL_Y_MIN_POS+5 : 10;
#else
    float y = 10;
#endif
    CommandBuffer::moveHead(x, y, 200);
}

// move to a safe y position in dual mode
void CommandBuffer::move2SafeYPos()
{
    if (IS_DUAL_ENABLED && current_position[Y_AXIS] < DUAL_Y_MIN_POS)
    {
        moveHead(current_position[X_AXIS], DUAL_Y_MIN_POS, 120);
    }
}

void CommandBuffer::homeHead()
{
    enquecommand_P(PSTR("G28 X0 Y0"));
}

void CommandBuffer::homeBed()
{
    enquecommand_P(PSTR("G28 Z0"));
}

void CommandBuffer::homeAll()
{
    enquecommand_P(PSTR("G28"));
}
