#include <avr/pgmspace.h>

#include "Configuration.h"
#ifdef ENABLE_ULTILCD2
#include "Marlin.h"
#include "cardreader.h"
#include "temperature.h"
#include "lifetime_stats.h"
#include "commandbuffer.h"
#include "UltiLCD2.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2_menu_print.h"
#include "UltiLCD2_menu_material.h"
#include "UltiLCD2_menu_maintenance.h"

#if EXTRUDERS > 1
#include "ConfigurationDual.h"
#include "UltiLCD2_menu_dual.h"
#endif // EXTRUDERS

// allowed tuning difference to the target temperature
#define MAX_TEMP_DIFF  25

uint8_t lcd_cache[LCD_CACHE_SIZE];
#define LCD_CACHE_NR_OF_FILES() lcd_cache[(LCD_CACHE_COUNT*(LONG_FILENAME_LENGTH+2))]
#define LCD_CACHE_TYPE(n) lcd_cache[LCD_CACHE_COUNT + (n)]
#define LCD_DETAIL_CACHE_START ((LCD_CACHE_COUNT*(LONG_FILENAME_LENGTH+2))+1)
#define LCD_DETAIL_CACHE_ID() lcd_cache[LCD_DETAIL_CACHE_START]
#define LCD_DETAIL_CACHE_TIME() (*(uint32_t*)&lcd_cache[LCD_DETAIL_CACHE_START+1])
#define LCD_DETAIL_CACHE_MATERIAL(n) (*(uint32_t*)&lcd_cache[LCD_DETAIL_CACHE_START+5+4*n])
#define LCD_DETAIL_CACHE_NOZZLE_DIAMETER(n) (*(float*)&lcd_cache[LCD_DETAIL_CACHE_START+5+4*EXTRUDERS+4*n])
#define LCD_DETAIL_CACHE_MATERIAL_TYPE(n) ((char*)&lcd_cache[LCD_DETAIL_CACHE_START+5+8*EXTRUDERS+8*n])

static void lcd_menu_print_heatup();
static void lcd_menu_print_printing();
static void lcd_menu_print_error_sd();
static void lcd_menu_print_error_position();
static void lcd_menu_print_classic_warning();
static void lcd_menu_print_material_warning();
static void lcd_menu_print_abort();
static void lcd_menu_print_ready();
static void lcd_menu_print_ready_cooled_down();
static void lcd_menu_print_tune();
static void lcd_menu_print_tune_retraction();
static void lcd_menu_print_pause();

uint8_t primed = 0;

static bool pauseRequested = false;


void lcd_clear_cache()
{
    for(uint8_t n=0; n<LCD_CACHE_COUNT; n++)
        LCD_CACHE_ID(n) = 0xFF;
    LCD_DETAIL_CACHE_ID() = 0;
    LCD_CACHE_NR_OF_FILES() = 0xFF;
}

void abortPrint(bool bQuickstop)
{
    postMenuCheck = NULL;
    clear_command_queue();

    if (bQuickstop)
    {
        quickStop();
    }
    else
    {
        st_synchronize();
    }

    // we're not printing any more
    card.sdprinting = false;

    // reset defaults
    feedmultiply = 100;
    for(uint8_t e=0; e<EXTRUDERS; e++)
    {
        extrudemultiply[e] = 100;
    }

    float minTemp = get_extrude_min_temp();
    set_extrude_min_temp(0.0f);

#if EXTRUDERS > 1
    if (!bQuickstop && active_extruder)
    {
        switch_extruder(0, true);
    }
#endif // EXTRUDERS

    // set up the end of print retraction
    if ((primed & ENDOFPRINT_RETRACT) && (primed & (EXTRUDER_PRIMED << active_extruder)))
    {
#if EXTRUDERS > 1
        if (!TOOLCHANGE_RETRACTED(active_extruder))
        {
            // add tool change retraction
            float retractlen = toolchange_retractlen[active_extruder]/volume_to_filament_length[active_extruder];
            if (EXTRUDER_RETRACTED(active_extruder))
            {
                retractlen -= retract_recover_length[active_extruder];
                if (retractlen < 0)
                {
                    retractlen = 0.0f;
                }
            }
            SET_TOOLCHANGE_RETRACT(active_extruder);
            toolchange_recover_length[active_extruder] = retractlen;

            // perform end-of-print retract
            plan_set_e_position(retractlen, true);
            current_position[E_AXIS] = 0.0f;
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], toolchange_retractfeedrate[active_extruder]/60, active_extruder);
        }
#else
        char buffer[32];
        sprintf_P(buffer, PSTR("G92 E%i"), int(((float)END_OF_PRINT_RETRACTION) / volume_to_filament_length[active_extruder]));
        enquecommand(buffer);
        // perform the retraction at the standard retract speed
        sprintf_P(buffer, PSTR("G1 F%i E0"), int(retract_feedrate));
        enquecommand(buffer);
        cmd_synchronize();
#endif
    }

    // no longer primed
    primed = 0;
    set_extrude_min_temp(minTemp);
    doCooldown();

#if EXTRUDERS > 1
    // move to a safe y position in dual mode
    CommandBuffer::move2SafeYPos();
#endif // EXTRUDERS
    if (current_position[Z_AXIS] > Z_MAX_POS - 30)
    {
        CommandBuffer::homeHead();
        CommandBuffer::homeBed();
    }
    else
    {
        CommandBuffer::homeAll();
    }

    // finish all queued commands
    cmd_synchronize();
    finishAndDisableSteppers();
    current_position[E_AXIS] = 0.0f;
    plan_set_e_position(current_position[E_AXIS], true);

    stoptime = millis();
    lifetime_stats_print_end();

    //If we where paused, make sure we abort that pause. Else strange things happen: https://github.com/Ultimaker/Ultimaker2Marlin/issues/32
    card.pause = false;
    pauseRequested = false;
    printing_state = PRINT_STATE_NORMAL;

    // reset defaults
    fanSpeedPercent = 100;
    for(uint8_t e=0; e<EXTRUDERS; ++e)
    {
        volume_to_filament_length[e] = 1.0;
    }
    axis_relative_state = 0;
}

static void checkPrintFinished()
{
    if (pauseRequested)
    {
        lcd_menu_print_pause();
    }

    if (!card.sdprinting && !is_command_queued())
    {
        // normal end of print
        abortPrint(false);
        currentMenu = lcd_menu_print_ready;
        SELECT_MAIN_MENU_ITEM(0);
    }else if (position_error)
    {
        abortPrint(true);
        currentMenu = lcd_menu_print_error_position;
        SELECT_MAIN_MENU_ITEM(0);
    }else if (card.errorCode())
    {
        abortPrint(true);
        currentMenu = lcd_menu_print_error_sd;
        SELECT_MAIN_MENU_ITEM(0);
    }
}

static void doStartPrint()
{
    // zero the extruder position
    current_position[E_AXIS] = 0.0;
    plan_set_e_position(0, true);
    primed = 0;
    position_error = false;

    // since we are going to prime the nozzle, forget about any G10/G11 retractions that happened at end of previous print
    reset_retractstate();
    for (uint8_t e=0; e<EXTRUDERS; ++e)
    {
        CLEAR_EXTRUDER_RETRACT(e);
        retract_recover_length[e] = 0.0f;
    }

    for(int8_t e = EXTRUDERS-1; (e>=0) && (printing_state < PRINT_STATE_ABORT); --e)
    {
#ifdef FWRETRACT
        // clear reheat flag
        retract_state &= ~(EXTRUDER_PREHEAT << e);
#endif

        if (!LCD_DETAIL_CACHE_MATERIAL(e))
        {
            // don't prime the extruder if it isn't used in the (Ulti)gcode
            // traditional gcode files typically won't have the Material lines at start, so we won't prime for those
            // Also, on dual/multi extrusion files, only prime the extruders that are used in the gcode-file.
            continue;
        }
        extruder_lastused[e] = millis();

        if (!primed)
        {
            // move to priming height
            char buffer[16] = {0};
            sprintf_P(buffer, PSTR("G1 Z%i F%i"), PRIMING_HEIGHT, (int)homing_feedrate[Z_AXIS]);
            enquecommand(buffer);
//            current_position[Z_AXIS] = PRIMING_HEIGHT;
//            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], homing_feedrate[Z_AXIS], e);
            // finish z-move
            cmd_synchronize();
            st_synchronize();
        }

        if (printing_state == PRINT_STATE_ABORT)
        {
            return;
        }

    #if (EXTRUDERS > 1)
        if (active_extruder != e)
        {
            // switch active extruder
            switch_extruder(e, true);
        }
        else
        {
            // move to heatup pos
            CommandBuffer::move2heatup();

            // wait for nozzle heatup
            reheatNozzle(active_extruder);

            if ((printing_state < PRINT_STATE_ABORT) && IS_WIPE_ENABLED)
            {
                // execute prime and wipe script
                cmdBuffer.processWipe(printing_state);
            }
        }
        if (!IS_WIPE_ENABLED && (printing_state < PRINT_STATE_ABORT))
        {
            // undo the tool change retraction
            float length = CommandBuffer::preparePriming(e) + PRIMING_MM3;
            // perform additional priming
            current_position[E_AXIS] = 0.0;
            plan_set_e_position(-length, true);
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], (PRIMING_MM3_PER_SEC * volume_to_filament_length[e]), e);

            CLEAR_TOOLCHANGE_RETRACT(e);
            CLEAR_EXTRUDER_RETRACT(e);
            retract_recover_length[e] = 0.0f;
            toolchange_recover_length[e] = 0.0f;

            // retract
            process_command_P(PSTR("G10"));
        }
    #else
        // undo the end-of-print retraction
        plan_set_e_position(current_position[E_AXIS] - (END_OF_PRINT_RETRACTION / volume_to_filament_length[e]), true);
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], END_OF_PRINT_RECOVERY_SPEED, e);
        // perform additional priming
        plan_set_e_position(current_position[E_AXIS]-PRIMING_MM3, true);
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], (PRIMING_MM3_PER_SEC * volume_to_filament_length[e]), e);
    #endif

        // finish priming moves and reset e-position
        set_current_position(E_AXIS, 0.0f);
        plan_set_e_position(current_position[E_AXIS], true);

        // note that we have primed, so that we know to de-prime at the end
        primed |= (EXTRUDER_PRIMED << e);
        primed |= ENDOFPRINT_RETRACT;
    }

#if (EXTRUDERS > 1)
    // recover wipe retract
    if (primed & (EXTRUDER_PRIMED << active_extruder))
    {
        // process_command_P(PSTR("G11"));
        current_position[E_AXIS] = 0.0;
        plan_set_e_position(0, true);
        enquecommand_P(PSTR("G1 E0"));
        // enquecommand_P(PSTR("G11"));
    }
#endif

    postMenuCheck = checkPrintFinished;
    card.startFileprint();
    lifetime_stats_print_start();
    stoptime = starttime = millis();
}

static void cardUpdir()
{
    card.updir();
}

static char* lcd_sd_menu_filename_callback(uint8_t nr)
{
    //This code uses the card.longFilename as buffer to store the filename, to save memory.
    if (nr == 0)
    {
        if (card.atRoot())
        {
            strcpy_P(card.longFilename, PSTR("< RETURN"));
        }else{
            strcpy_P(card.longFilename, PSTR("< BACK"));
        }
    }else{
        card.longFilename[0] = '\0';
        for(uint8_t idx=0; idx<LCD_CACHE_COUNT; idx++)
        {
            if (LCD_CACHE_ID(idx) == nr)
                strcpy(card.longFilename, LCD_CACHE_FILENAME(idx));
        }
        if (card.longFilename[0] == '\0')
        {
            card.getfilename(nr - 1);
            if (!card.longFilename[0])
                strcpy(card.longFilename, card.filename);
            if (!card.filenameIsDir)
            {
                if (strrchr(card.longFilename, '.')) strrchr(card.longFilename, '.')[0] = '\0';
            }

            uint8_t idx = nr % LCD_CACHE_COUNT;
            LCD_CACHE_ID(idx) = nr;
            strcpy(LCD_CACHE_FILENAME(idx), card.longFilename);
            LCD_CACHE_TYPE(idx) = card.filenameIsDir ? 1 : 0;
            if (card.errorCode() && card.sdInserted)
            {
                //On a read error reset the file position and try to keep going. (not pretty, but these read errors are annoying as hell)
                card.clearError();
                LCD_CACHE_ID(idx) = 255;
                card.longFilename[0] = '\0';
            }
        }
    }
    return card.longFilename;
}

void lcd_sd_menu_details_callback(uint8_t nr)
{
    if (nr == 0)
    {
        return;
    }
    for(uint8_t idx=0; idx<LCD_CACHE_COUNT; idx++)
    {
        if (LCD_CACHE_ID(idx) == nr)
        {
            if (LCD_CACHE_TYPE(idx) == 1)
            {
                lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Folder"));
            }else{
                char buffer[64];
                if (LCD_DETAIL_CACHE_ID() != nr)
                {
                    card.getfilename(nr - 1);
                    if (card.errorCode())
                    {
                        card.clearError();
                        return;
                    }
                    LCD_DETAIL_CACHE_ID() = nr;
                    LCD_DETAIL_CACHE_TIME() = 0;
                    for(uint8_t e=0; e<EXTRUDERS; e++)
                    {
                        LCD_DETAIL_CACHE_MATERIAL(e) = 0;
                        LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e) = 0.4;
                        LCD_DETAIL_CACHE_MATERIAL_TYPE(e)[0] = '\0';
                    }
                    card.openFile(card.filename, true);
                    if (card.isFileOpen())
                    {
                        for(uint8_t n=0;n<16;n++)
                        {
                            card.fgets(buffer, sizeof(buffer));
                            buffer[sizeof(buffer)-1] = '\0';
                            while (strlen(buffer) > 0 && buffer[strlen(buffer)-1] < ' ') buffer[strlen(buffer)-1] = '\0';
                            if (strncmp_P(buffer, PSTR(";TIME:"), 6) == 0)
                                LCD_DETAIL_CACHE_TIME() = atol(buffer + 6);
                            else if (strncmp_P(buffer, PSTR(";MATERIAL:"), 10) == 0)
                                LCD_DETAIL_CACHE_MATERIAL(0) = atol(buffer + 10);
                            else if (strncmp_P(buffer, PSTR(";NOZZLE_DIAMETER:"), 17) == 0)
                                LCD_DETAIL_CACHE_NOZZLE_DIAMETER(0) = strtod(buffer + 17, NULL);
                            else if (strncmp_P(buffer, PSTR(";MTYPE:"), 7) == 0)
                            {
                                strncpy(LCD_DETAIL_CACHE_MATERIAL_TYPE(0), buffer + 7, 8);
                                LCD_DETAIL_CACHE_MATERIAL_TYPE(0)[7] = '\0';
                            }
#if EXTRUDERS > 1
                            else if (strncmp_P(buffer, PSTR(";MATERIAL2:"), 11) == 0)
                                LCD_DETAIL_CACHE_MATERIAL(1) = atol(buffer + 11);
                            else if (strncmp_P(buffer, PSTR(";NOZZLE_DIAMETER2:"), 18) == 0)
                                LCD_DETAIL_CACHE_NOZZLE_DIAMETER(1) = strtod(buffer + 18, NULL);
                            else if (strncmp_P(buffer, PSTR(";MTYPE2:"), 8) == 0)
                            {
                                strncpy(LCD_DETAIL_CACHE_MATERIAL_TYPE(1), buffer + 8, 8);
                                LCD_DETAIL_CACHE_MATERIAL_TYPE(1)[7] = '\0';
                            }
#endif

                        }
                    }
                    if (card.errorCode())
                    {
                        //On a read error reset the file position and try to keep going. (not pretty, but these read errors are annoying as hell)
                        card.clearError();
                        LCD_DETAIL_CACHE_ID() = 255;
                    }
                }

                if (LCD_DETAIL_CACHE_TIME() > 0)
                {
                    char* c = buffer;
                    if (led_glow_dir)
                    {
                        if (led_glow < 63)
                        {
                            strcpy_P(c, PSTR("Time: ")); c += 6;
                            c = int_to_time_string(LCD_DETAIL_CACHE_TIME(), c);
                        }else{
#if EXTRUDERS > 1
                            strcpy_P(c, PSTR("Mat.: ")); c += 6;
#else
                            strcpy_P(c, PSTR("Material: ")); c += 10;
#endif
                            float length = float(LCD_DETAIL_CACHE_MATERIAL(0)) / (M_PI * (material[0].diameter / 2.0) * (material[0].diameter / 2.0));
                            if (length < 10000)
                                c = float_to_string(length / 1000.0, c, PSTR("m"));
                            else
                                c = int_to_string(length / 1000.0, c, PSTR("m"));
#if EXTRUDERS > 1
                            if (LCD_DETAIL_CACHE_MATERIAL(1))
                            {
                                *c++ = '/';
                                float length = float(LCD_DETAIL_CACHE_MATERIAL(1)) / (M_PI * (material[1].diameter / 2.0) * (material[1].diameter / 2.0));
                                if (length < 10000)
                                    c = float_to_string(length / 1000.0, c, PSTR("m"));
                                else
                                    c = int_to_string(length / 1000.0, c, PSTR("m"));
                            }
#endif
                        }
                    }else{
                        strcpy_P(c, PSTR("Nozzle: ")); c += 8;
                        c = float_to_string(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(0), c);
#if EXTRUDERS > 1
                        if (LCD_DETAIL_CACHE_MATERIAL(1))
                        {
                            *c++ = '/';
                            c = float_to_string(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(1), c);
                        }
#endif
                    }
                    lcd_lib_draw_string(3, BOTTOM_MENU_YPOS, buffer);
                }else{
                    lcd_lib_draw_stringP(3, BOTTOM_MENU_YPOS, PSTR("No info available"));
                }
            }
        }
    }
}

void lcd_menu_print_select()
{
    if (!card.sdInserted)
    {
        LED_GLOW();
        lcd_lib_encoder_pos = MAIN_MENU_ITEM_POS(0);
        lcd_info_screen(lcd_menu_main);
        lcd_lib_draw_string_centerP(15, PSTR("No SD-CARD!"));
        lcd_lib_draw_string_centerP(25, PSTR("Please insert card"));
        lcd_lib_update_screen();
        card.release();
        return;
    }
    if (!card.isOk())
    {
        lcd_info_screen(lcd_menu_main);
        lcd_lib_draw_string_centerP(16, PSTR("Reading card..."));
        lcd_lib_update_screen();
        lcd_clear_cache();
        card.initsd();
        return;
    }

    if (LCD_CACHE_NR_OF_FILES() == 0xFF)
        LCD_CACHE_NR_OF_FILES() = card.getnrfilenames();
    if (card.errorCode())
    {
        LCD_CACHE_NR_OF_FILES() = 0xFF;
        return;
    }
    uint8_t nrOfFiles = LCD_CACHE_NR_OF_FILES();
    if (nrOfFiles == 0)
    {
        if (card.atRoot())
            lcd_info_screen(lcd_menu_main, NULL, PSTR("OK"));
        else
            lcd_info_screen(lcd_menu_print_select, cardUpdir, PSTR("OK"));
        lcd_lib_draw_string_centerP(25, PSTR("No files found!"));
        lcd_lib_update_screen();
        lcd_clear_cache();
        return;
    }

    if (lcd_lib_button_pressed)
    {
        uint8_t selIndex = uint16_t(SELECTED_SCROLL_MENU_ITEM());
        if (selIndex == 0)
        {
            if (card.atRoot())
            {
                lcd_change_to_menu(lcd_menu_main);
                lcd_clear_cache();
                return;
            }else{
                lcd_clear_cache();
                lcd_lib_beep();
                card.updir();
            }
        }else{
            card.getfilename(selIndex - 1);
            if (!card.filenameIsDir)
            {
                //Start print
//#if (EXTRUDERS > 1)
//                switch_extruder(0, false);
//#else
//                active_extruder = 0;
//#endif
                card.openFile(card.filename, true);
                if (card.isFileOpen() && !is_command_queued())
                {
#ifndef DUAL_FAN
                    if (led_mode == LED_MODE_WHILE_PRINTING || led_mode == LED_MODE_BLINK_ON_DONE)
                        analogWrite(LED_PIN, 255 * int(led_brightness_level) / 100);
#endif
                    LCD_CACHE_ID(0) = 255;
                    if (card.longFilename[0])
                        strcpy(LCD_CACHE_FILENAME(0), card.longFilename);
                    else
                        strcpy(LCD_CACHE_FILENAME(0), card.filename);
                    LCD_CACHE_FILENAME(0)[20] = '\0';
                    if (strchr(LCD_CACHE_FILENAME(0), '.')) strchr(LCD_CACHE_FILENAME(0), '.')[0] = '\0';

                    char buffer[64];
                    card.fgets(buffer, sizeof(buffer));
                    buffer[sizeof(buffer)-1] = '\0';
                    while (strlen(buffer) > 0 && buffer[strlen(buffer)-1] < ' ') buffer[strlen(buffer)-1] = '\0';
                    if (strcmp_P(buffer, PSTR(";FLAVOR:UltiGCode")) != 0)
                    {
                        card.fgets(buffer, sizeof(buffer));
                        buffer[sizeof(buffer)-1] = '\0';
                        while (strlen(buffer) > 0 && buffer[strlen(buffer)-1] < ' ') buffer[strlen(buffer)-1] = '\0';
                    }
                    card.setIndex(0);

                    //reset the settings to defaults
                    fanSpeed = 0;
                    fanSpeedPercent = 100;
                    feedmultiply = 100;
                    target_temperature_bed_diff = 0;
                    axis_relative_state = 0;

                    for(uint8_t e=0; e<EXTRUDERS; ++e)
                    {
                        volume_to_filament_length[e] = 1.0f;
                        extrudemultiply[e] = 100;
                        target_temperature_diff[e] = 0;
                    }

                    if (strcmp_P(buffer, PSTR(";FLAVOR:UltiGCode")) == 0)
                    {
                        //New style GCode flavor without start/end code.
                        // Temperature settings, filament settings, fan settings, start and end-code are machine controlled.
                        lcd_change_to_menu(lcd_menu_print_heatup);

#if TEMP_SENSOR_BED != 0
                        target_temperature_bed = 0;
#endif
                        fanSpeedPercent = 0;
                        for(uint8_t e=0; e<EXTRUDERS; ++e)
                        {
                            volume_to_filament_length[e] = 1.0 / (M_PI * (material[e].diameter / 2.0) * (material[e].diameter / 2.0));
                            extrudemultiply[e] = material[e].flow;
                            retract_feedrate = material[e].retraction_speed[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e))];
                            retract_length = material[e].retraction_length[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e))];
                            target_temperature[e] = 0;

                            if (LCD_DETAIL_CACHE_MATERIAL(e) < 1)
                                continue;
#if TEMP_SENSOR_BED != 0
                            target_temperature_bed = max(target_temperature_bed, material[e].bed_temperature);
#endif
                            fanSpeedPercent = max(fanSpeedPercent, material[e].fan_speed);
                        }

                        if (strcasecmp(material[0].name, LCD_DETAIL_CACHE_MATERIAL_TYPE(0)) != 0)
                        {
                            if (strlen(material[0].name) > 0 && strlen(LCD_DETAIL_CACHE_MATERIAL_TYPE(0)) > 0)
                            {
                                currentMenu = lcd_menu_print_material_warning;
                            }
                        }

                        // move to heatup pos
                        process_command_P(PSTR("G28"));
#if EXTRUDERS < 2
                        CommandBuffer::move2heatup();
#endif
#if TEMP_SENSOR_BED != 0
                            if (target_temperature_bed > 0)
                                printing_state = PRINT_STATE_HEATING_BED;
                            else
#endif
                                printing_state = PRINT_STATE_HEATING;

                    }else{
                        //Classic gcode file
                        lcd_change_to_menu(lcd_menu_print_classic_warning, MAIN_MENU_ITEM_POS(0));
                    }
                }
            }else{
                lcd_lib_beep();
                lcd_clear_cache();
                card.chdir(card.filename);
                SELECT_SCROLL_MENU_ITEM(0);
            }
            return;//Return so we do not continue after changing the directory or selecting a file. The nrOfFiles is invalid at this point.
        }
    }
    lcd_scroll_menu(PSTR("SD CARD"), nrOfFiles+1, lcd_sd_menu_filename_callback, lcd_sd_menu_details_callback);
}

static void lcd_menu_print_heatup()
{
    if (printing_state == PRINT_STATE_ABORT)
    {
        return;
    }

    lcd_question_screen(lcd_menu_print_tune, NULL, PSTR("TUNE"), lcd_menu_print_abort, NULL, PSTR("ABORT"));

#if TEMP_SENSOR_BED != 0
    if (current_temperature_bed > degTargetBed() - TEMP_WINDOW)
    {
#endif
        printing_state = PRINT_STATE_HEATING;
        for(int8_t e=EXTRUDERS-1; e>=0; --e)
        {
            if (LCD_DETAIL_CACHE_MATERIAL(e) < 1)
                continue;
            if (target_temperature[e] <= 0)
                target_temperature[e] = material[e].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e))];
            // limit power consumption: pre-heat only one nozzle at the same time
            if (target_temperature[e] > 0)
                break;
        }

#if TEMP_SENSOR_BED != 0
        if (current_temperature_bed >= degTargetBed() - TEMP_WINDOW * 2 && !is_command_queued() && !blocks_queued())
#else
        if (!is_command_queued() && !blocks_queued())
#endif
        {
            bool ready = false;
            for(int8_t e=EXTRUDERS-1; e>=0; --e)
            {
                if ((target_temperature[e] > 0) && (current_temperature[e] >= degTargetHotend(e) - TEMP_WINDOW))
                {
                    ready = true;
                    // set target temperature for other used nozzles
                    for(int8_t e2=EXTRUDERS-1; e2>=0; --e2)
                    {
                        if ((LCD_DETAIL_CACHE_MATERIAL(e2) < 1) || (target_temperature[e2] > 0))
                            continue;
                        target_temperature[e2] = material[e2].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(e2))];
                    }
                    break;
                }
            }

            if (ready)
            {
                currentMenu = lcd_menu_print_printing;
                doStartPrint();
            }
            else
            {
                printing_state = PRINT_STATE_HEATING;
            }
        }
#if TEMP_SENSOR_BED != 0
    }
    else
    {
        printing_state = PRINT_STATE_HEATING_BED;
    }
#endif

    uint8_t progress = 125;
    for(int8_t e=EXTRUDERS-1; e>=0; --e)
    {
        if (LCD_DETAIL_CACHE_MATERIAL(e) < 1 || target_temperature[e] < 1)
            continue;
        if (current_temperature[e] > 20)
            progress = min(progress, (current_temperature[e] - 20) * 125 / (degTargetHotend(e) - 20 - TEMP_WINDOW));
        else
            progress = 0;
    }
#if TEMP_SENSOR_BED != 0
    if (current_temperature_bed > 20)
        progress = min(progress, (current_temperature_bed - 20) * 125 / (degTargetBed() - 20 - TEMP_WINDOW));
    else if (degTargetBed() > current_temperature_bed - 20)
        progress = 0;
#endif

    if (progress < minProgress)
        progress = minProgress;
    else
        minProgress = progress;

    lcd_lib_draw_string_centerP(10, PSTR("Heating up..."));
    lcd_lib_draw_string_centerP(20, PSTR("Preparing to print:"));
    lcd_lib_draw_string_center(30, LCD_CACHE_FILENAME(0));

    lcd_progressbar(progress);

    lcd_lib_update_screen();
}

static void lcd_change_to_menu_change_material_return()
{
    plan_set_e_position(current_position[E_AXIS], true);
    setTargetHotend(material[active_extruder].temperature[nozzleSizeToTemperatureIndex(LCD_DETAIL_CACHE_NOZZLE_DIAMETER(active_extruder))], active_extruder);
    currentMenu = lcd_menu_print_printing;
}

static void lcd_menu_print_printing()
{
    if (card.pause)
    {
        lcd_tripple_menu(PSTR("RESUME|PRINT"), PSTR("CHANGE|MATERIAL"), PSTR("TUNE"));
        if (lcd_lib_button_pressed)
        {
            if (IS_SELECTED_MAIN(0) && movesplanned() < 1)
            {
                card.pause = false;
                if (card.sdprinting)
                {
                    primed |= ENDOFPRINT_RETRACT;
                }
                lcd_lib_beep();
            }else if (IS_SELECTED_MAIN(1) && movesplanned() < 1)
                lcd_change_to_menu_change_material(lcd_change_to_menu_change_material_return, active_extruder);
            else if (IS_SELECTED_MAIN(2))
                lcd_change_to_menu(lcd_menu_print_tune);
        }
    }
    else if (pauseRequested)
    {
        lcd_lib_clear();
        lcd_lib_draw_string_centerP(20, PSTR("Pausing..."));
    }
    else
    {
        lcd_question_screen(lcd_menu_print_tune, NULL, PSTR("TUNE"), NULL, lcd_menu_print_pause, PSTR("PAUSE"));
        uint8_t progress = card.getFilePos() / ((card.getFileSize() + 123) / 124);
#if EXTRUDERS > 1
        char buffer[20];
#else
        char buffer[16];
#endif
        char* c;
        switch(printing_state)
        {
        default:
            lcd_lib_draw_string_centerP(20, PSTR("Printing:"));
            lcd_lib_draw_string_center(30, LCD_CACHE_FILENAME(0));
            break;
        case PRINT_STATE_HEATING:
            lcd_lib_draw_string_centerP(20, PSTR("Heating"));
#if EXTRUDERS > 1
            int_to_string(int(degTargetHotend(1)), int_to_string(int(dsp_temperature[1]), int_to_string(int(degTargetHotend(0)), int_to_string(int(dsp_temperature[0]), buffer, PSTR("C/")), PSTR("C ")), PSTR("C/")), PSTR("C"));
#else
            int_to_string(int(degTargetHotend(0)), int_to_string(int(dsp_temperature[0]), buffer, PSTR("C/")), PSTR("C"));
#endif // EXTRUDERS
            lcd_lib_draw_string_center(30, buffer);
            break;
        case PRINT_STATE_HEATING_BED:
            lcd_lib_draw_string_centerP(20, PSTR("Heating buildplate"));
            c = int_to_string(dsp_temperature_bed, buffer, PSTR("C"));
            *c++ = '/';
            c = int_to_string(degTargetBed(), c, PSTR("C"));
            lcd_lib_draw_string_center(30, buffer);
            break;
        }
        float printTimeMs = (millis() - starttime);
        float printTimeSec = printTimeMs / 1000L;
        float totalTimeMs = float(printTimeMs) * float(card.getFileSize()) / float(card.getFilePos());
        static float totalTimeSmoothSec;
        totalTimeSmoothSec = (totalTimeSmoothSec * 999L + totalTimeMs / 1000L) / 1000L;
        if (isinf(totalTimeSmoothSec))
            totalTimeSmoothSec = totalTimeMs;

        if (LCD_DETAIL_CACHE_TIME() == 0 && printTimeSec < 240)
        {
            totalTimeSmoothSec = totalTimeMs / 1000;
            // lcd_lib_draw_stringP(5, 10, PSTR("Time left unknown"));
        }
        else if (card.sdprinting)
        {
            unsigned long totalTimeSec;
            if (printTimeSec < LCD_DETAIL_CACHE_TIME() / 2)
            {
                float f = float(printTimeSec) / float(LCD_DETAIL_CACHE_TIME() / 2);
                if (f > 1.0)
                    f = 1.0;
                totalTimeSec = float(totalTimeSmoothSec) * f + float(LCD_DETAIL_CACHE_TIME()) * (1 - f);
            }else{
                totalTimeSec = totalTimeSmoothSec;
            }
            unsigned long timeLeftSec;
            if (printTimeSec > totalTimeSec)
            {
                timeLeftSec = 1;
            }
            else if ((progress > 2) && (printTimeSec > 240))
            {
                c = strcpy_P(buffer, PSTR("Time left "));
                c += 10;
                timeLeftSec = totalTimeSec - printTimeSec;
                int_to_time_min(timeLeftSec, c);
                lcd_lib_draw_string_center(10, buffer);
            }
        }

        lcd_progressbar(progress);
    }

    lcd_lib_update_screen();
}

static void lcd_menu_print_error_sd()
{
    LED_GLOW_ERROR();
    lcd_info_screen(lcd_menu_main, NULL, PSTR("RETURN TO MAIN"));

    lcd_lib_draw_string_centerP(10, PSTR("Error while"));
    lcd_lib_draw_string_centerP(20, PSTR("reading SD-card!"));
    lcd_lib_draw_string_centerP(30, PSTR("Go to:"));
    lcd_lib_draw_string_centerP(40, PSTR("ultimaker.com/ER08"));
    /*
    char buffer[12];
    strcpy_P(buffer, PSTR("Code:"));
    int_to_string(card.errorCode(), buffer+5);
    lcd_lib_draw_string_center(40, buffer);
    */

    lcd_lib_update_screen();
}

static void lcd_menu_print_error_position()
{
    LED_GLOW_ERROR();
    lcd_info_screen(lcd_menu_main, NULL, PSTR("RETURN TO MAIN"));

    lcd_lib_draw_string_centerP(15, PSTR("ERROR:"));
    lcd_lib_draw_string_centerP(25, PSTR("Tried printing out"));
    lcd_lib_draw_string_centerP(35, PSTR("of printing area"));

    lcd_lib_update_screen();
}

static void lcd_menu_print_classic_warning()
{
    lcd_question_screen(lcd_menu_print_printing, doStartPrint, PSTR("CONTINUE"), lcd_menu_print_select, NULL, PSTR("CANCEL"));

    lcd_lib_draw_string_centerP(10, PSTR("This file will"));
    lcd_lib_draw_string_centerP(20, PSTR("override machine"));
    lcd_lib_draw_string_centerP(30, PSTR("setting with setting"));
    lcd_lib_draw_string_centerP(40, PSTR("from the slicer."));

    lcd_lib_update_screen();
}

static void lcd_menu_print_material_warning()
{
    lcd_question_screen(lcd_menu_print_heatup, NULL, PSTR("CONTINUE"), lcd_menu_print_select, doCooldown, PSTR("CANCEL"));

    lcd_lib_draw_string_centerP(10, PSTR("This file is created"));
    lcd_lib_draw_string_centerP(20, PSTR("for a different"));
    lcd_lib_draw_string_centerP(30, PSTR("material."));
    char buffer[MATERIAL_NAME_SIZE * 2 + 5];
    sprintf_P(buffer, PSTR("%s vs %s"), material[0].name, LCD_DETAIL_CACHE_MATERIAL_TYPE(0));
    lcd_lib_draw_string_center(40, buffer);

    lcd_lib_update_screen();
}

static void lcd_menu_doabort()
{
    LED_GLOW();
    if (printing_state == PRINT_STATE_ABORT)
    {
        lcd_lib_clear();
        lcd_lib_draw_string_centerP(20, PSTR("Aborting..."));
        lcd_lib_update_screen();
    }
    else
    {
        lcd_change_to_menu(lcd_menu_print_ready);
    }
}

static void set_abort_state()
{
    printing_state = PRINT_STATE_ABORT;
    postMenuCheck = NULL;
}

static void lcd_menu_print_abort()
{
    LED_GLOW();
    lcd_question_screen(lcd_menu_doabort, set_abort_state, PSTR("YES"), previousMenu, NULL, PSTR("NO"));

    lcd_lib_draw_string_centerP(20, PSTR("Abort the print?"));

    lcd_lib_update_screen();
}

static void postPrintReady()
{
#ifndef DUAL_FAN
    if (led_mode == LED_MODE_BLINK_ON_DONE)
        analogWrite(LED_PIN, 0);
#endif
}

static void lcd_menu_print_ready()
{
#ifndef DUAL_FAN
    if (led_mode == LED_MODE_WHILE_PRINTING)
        analogWrite(LED_PIN, 0);
    else if (led_mode == LED_MODE_BLINK_ON_DONE)
        analogWrite(LED_PIN, (led_glow << 1) * int(led_brightness_level) / 100);
#endif
    lcd_info_screen(lcd_menu_main, postPrintReady, PSTR("BACK TO MENU"));

    char buffer[16] = {0};
    unsigned long t=(stoptime-starttime)/1000;

    if (t > 1)
    {
        char *c = buffer;
        strcpy_P(c, PSTR("Time ")); c += 5;
        c = int_to_time_min(t, c);
        if (t < 60)
        {
                strcat_P(c, PSTR("min"));
        }
        else
        {
                strcat_P(c, PSTR("h"));
        }
        lcd_lib_draw_string_center(5, buffer);
    }

    if (current_temperature[0] > 60 || current_temperature_bed > 40)
    {
        lcd_lib_draw_string_centerP(15, PSTR("Printer cooling down"));

        int16_t progress = 124 - (current_temperature[0] - 60);
        if (progress < 0) progress = 0;
        if (progress > 124) progress = 124;

        if (progress < minProgress)
            progress = minProgress;
        else
            minProgress = progress;

        lcd_progressbar(progress);
        char* c = buffer;
        for(uint8_t e=0; e<EXTRUDERS; e++)
            c = int_to_string(dsp_temperature[e], c, PSTR("C "));
#if TEMP_SENSOR_BED != 0
        int_to_string(dsp_temperature_bed, c, PSTR("C"));
#endif
        lcd_lib_draw_string_center(25, buffer);
    }else{
        currentMenu = lcd_menu_print_ready_cooled_down;
    }

    lcd_lib_update_screen();
}

static void lcd_menu_print_ready_cooled_down()
{
#ifndef DUAL_FAN
    if (led_mode == LED_MODE_WHILE_PRINTING)
        analogWrite(LED_PIN, 0);
    else if (led_mode == LED_MODE_BLINK_ON_DONE)
        analogWrite(LED_PIN, (led_glow << 1) * int(led_brightness_level) / 100);
#endif
    lcd_info_screen(lcd_menu_main, postPrintReady, PSTR("BACK TO MENU"));

    LED_GLOW();
    char buffer[16] = {0};
    unsigned long t=(stoptime-starttime)/1000;

    if (t > 1)
    {
        char *c = buffer;
        strcpy_P(c, PSTR("Time ")); c += 5;
        c = int_to_time_min(t, c);
        if (t < 60)
        {
            strcat_P(c, PSTR("min"));
        }
        else
        {
            strcat_P(c, PSTR("h"));
        }
        lcd_lib_draw_string_center(5, buffer);
    }
    lcd_lib_draw_string_centerP(15, PSTR("Print finished"));
    lcd_lib_draw_string_centerP(30, PSTR("You can remove"));
    lcd_lib_draw_string_centerP(40, PSTR("the print."));

    lcd_lib_update_screen();
}

static char* tune_item_callback(uint8_t nr)
{
    char* c = card.longFilename;
    if (nr == 0)
        strcpy_P(c, PSTR("< RETURN"));
    else if (nr == 1)
        strcpy_P(c, PSTR("Abort"));
    else if (nr == 2)
        strcpy_P(c, PSTR("Speed"));
#if EXTRUDERS > 1
    else if (nr == 3)
        strcpy_P(c, PSTR("Temperature 1"));
    else if (nr == 4)
        strcpy_P(c, PSTR("Temperature 2"));
#else
    else if (nr == 3)
        strcpy_P(c, PSTR("Temperature"));
#endif
#if TEMP_SENSOR_BED != 0
    else if (nr == 3 + EXTRUDERS)
        strcpy_P(c, PSTR("Buildplate temp."));
#endif
    else if (nr == 3 + BED_MENU_OFFSET + EXTRUDERS)
        strcpy_P(c, PSTR("Fan speed"));
#if EXTRUDERS > 1
    else if (nr == 4 + BED_MENU_OFFSET + EXTRUDERS)
        strcpy_P(c, PSTR("Material flow 1"));
    else if (nr == 5 + BED_MENU_OFFSET + EXTRUDERS)
        strcpy_P(c, PSTR("Material flow 2"));
#else
    else if (nr == 4 + BED_MENU_OFFSET + EXTRUDERS)
        strcpy_P(c, PSTR("Material flow"));
#endif
    else if (nr == 4 + BED_MENU_OFFSET + EXTRUDERS * 2)
        strcpy_P(c, PSTR("Retraction"));
#if EXTRUDERS > 1
    else if (nr == 5 + BED_MENU_OFFSET + EXTRUDERS * 2)
        strcpy_P(c, PSTR("Toolchange retract 1"));
    else if (nr == 6 + BED_MENU_OFFSET + EXTRUDERS * 2)
        strcpy_P(c, PSTR("Toolchange retract 2"));
    else if (nr == 7 + BED_MENU_OFFSET + EXTRUDERS * 2)
        strcpy_P(c, PSTR("Extruder offset"));
#endif
    else if (nr == 2 + BED_MENU_OFFSET + EXTRUDERS * 5)
        strcpy_P(c, PSTR("LED Brightness"));
    return c;
}

static void tune_item_details_callback(uint8_t nr)
{
    char* c = card.longFilename;
    if (nr == 2)
        c = int_to_string(feedmultiply, c, PSTR("%"));
    else if (nr == 3)
    {
        c = int_to_string(dsp_temperature[0], c, PSTR("C"));
        *c++ = '/';
        c = int_to_string(int(degTargetHotend(0)), c, PSTR("C"));
    }
#if EXTRUDERS > 1
    else if (nr == 4)
    {
        c = int_to_string(dsp_temperature[1], c, PSTR("C"));
        *c++ = '/';
        c = int_to_string(int(degTargetHotend(1)), c, PSTR("C"));
    }
#endif
#if TEMP_SENSOR_BED != 0
    else if (nr == 3 + EXTRUDERS)
    {
        c = int_to_string(dsp_temperature_bed, c, PSTR("C"));
        *c++ = '/';
        c = int_to_string(degTargetBed(), c, PSTR("C"));
    }
#endif
    else if (nr == 3 + BED_MENU_OFFSET + EXTRUDERS)
        c = int_to_string(int(fanSpeed) * 100 / 255, c, PSTR("%"));
    else if (nr == 4 + BED_MENU_OFFSET + EXTRUDERS)
        c = int_to_string(extrudemultiply[0], c, PSTR("%"));
#if EXTRUDERS > 1
    else if (nr == 5 + BED_MENU_OFFSET + EXTRUDERS)
        c = int_to_string(extrudemultiply[1], c, PSTR("%"));
#endif
    else if (nr == 2 + BED_MENU_OFFSET + 5*EXTRUDERS)
    {
        c = int_to_string(led_brightness_level, c, PSTR("%"));
#ifndef DUAL_FAN
        if (led_mode == LED_MODE_ALWAYS_ON ||  led_mode == LED_MODE_WHILE_PRINTING || led_mode == LED_MODE_BLINK_ON_DONE)
            analogWrite(LED_PIN, 255 * int(led_brightness_level) / 100);
#endif
    }
    else
        return;
    lcd_lib_draw_string(5, BOTTOM_MENU_YPOS, card.longFilename);
}

static void lcd_menu_print_tune_heatup_nozzle(uint8_t e, int16_t max_temp)
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        target_temperature_diff[e] = constrain(target_temperature_diff[e] + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM),
                                               max(-MAX_TEMP_DIFF, -target_temperature[e]),
                                               min(MAX_TEMP_DIFF, max_temp - target_temperature[e] - 15));
        lcd_lib_encoder_pos = 0;
    }
    if (lcd_lib_button_pressed)
        lcd_change_to_menu(previousMenu, previousEncoderPos);

    lcd_lib_clear();
    char buffer[24];
#if EXTRUDERS > 1
    lcd_lib_draw_string_centerP(10, PSTR("Temperature"));
    strcpy_P(buffer, PSTR("Nozzle "));
    int_to_string(e+1, buffer+7, 0);
    lcd_lib_draw_string_center(20, buffer);
#else
    lcd_lib_draw_string_centerP(20, PSTR("Nozzle temperature:"));
#endif
    lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Click to return"));
    char * c = int_to_string(int(dsp_temperature[e]), buffer, PSTR("C/"));
    c = int_to_string(int(degTargetHotend(e)), c, PSTR("C"));
    if (target_temperature_diff[e])
    {
        // append relative difference
        int_to_string(target_temperature_diff[e], c, PSTR(")"), PSTR(" ("), true);
    }
    lcd_lib_draw_string_center(30, buffer);
    lcd_lib_update_screen();
}

#if TEMP_SENSOR_BED != 0
static void lcd_menu_print_tune_heatup_bed()
{
    if (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM != 0)
    {
        target_temperature_bed_diff = constrain(target_temperature_bed_diff + (lcd_lib_encoder_pos / ENCODER_TICKS_PER_SCROLL_MENU_ITEM),
                                                max(-MAX_TEMP_DIFF, -target_temperature_bed),
                                                min(MAX_TEMP_DIFF, BED_MAXTEMP - target_temperature_bed - 15));
        lcd_lib_encoder_pos = 0;
    }
    if (lcd_lib_button_pressed)
        lcd_change_to_menu(previousMenu, previousEncoderPos);

    lcd_lib_clear();
    char buffer[24];
    lcd_lib_draw_string_centerP(10, PSTR("Temperature"));
    lcd_lib_draw_string_centerP(20, PSTR("Buildplate"));
    lcd_lib_draw_string_centerP(BOTTOM_MENU_YPOS, PSTR("Click to return"));
    char * c = int_to_string(int(dsp_temperature_bed), buffer, PSTR("C/"));
    c = int_to_string(int(degTargetBed()), c, PSTR("C"));
    if (target_temperature_bed_diff)
    {
        // append relative difference
        int_to_string(target_temperature_bed_diff, c, PSTR(")"), PSTR(" ("), true);
    }
    lcd_lib_draw_string_center(30, buffer);
    lcd_lib_update_screen();
}
#endif // TEMP_SENSOR_BED

static void lcd_menu_print_tune_heatup_nozzle0()
{
    lcd_menu_print_tune_heatup_nozzle(0, HEATER_0_MAXTEMP);
}

#if EXTRUDERS > 1
void lcd_menu_print_tune_heatup_nozzle1()
{
    lcd_menu_print_tune_heatup_nozzle(1, HEATER_1_MAXTEMP);
}
#endif


static void lcd_menu_print_tune()
{
    lcd_scroll_menu(PSTR("TUNE"), 2 + LED_MENU_OFFSET + BED_MENU_OFFSET + EXTRUDERS * 5, tune_item_callback, tune_item_details_callback);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(0))
        {
            if (card.sdprinting)
                lcd_change_to_menu(lcd_menu_print_printing);
            else
                lcd_change_to_menu(lcd_menu_print_heatup);
        }else if (IS_SELECTED_SCROLL(1))
        {
            lcd_change_to_menu(lcd_menu_print_abort);
        }else if (IS_SELECTED_SCROLL(2))
            LCD_EDIT_SETTING(feedmultiply, "Print speed", "%", 10, 1000);
        else if (IS_SELECTED_SCROLL(3))
            lcd_change_to_menu(lcd_menu_print_tune_heatup_nozzle0, 0);
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(4))
            lcd_change_to_menu(lcd_menu_print_tune_heatup_nozzle1, 0);
#endif
#if TEMP_SENSOR_BED != 0
        else if (IS_SELECTED_SCROLL(3 + EXTRUDERS))
            lcd_change_to_menu(lcd_menu_print_tune_heatup_bed, 0);
#endif
        else if (IS_SELECTED_SCROLL(3 + BED_MENU_OFFSET + EXTRUDERS))
            LCD_EDIT_SETTING_BYTE_PERCENT(fanSpeed, "Fan speed", "%", 0, 100);
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(4 + BED_MENU_OFFSET + EXTRUDERS))
            LCD_EDIT_SETTING(extrudemultiply[0], "Material flow 1", "%", 10, 1000);
        else if (IS_SELECTED_SCROLL(5 + BED_MENU_OFFSET + EXTRUDERS))
            LCD_EDIT_SETTING(extrudemultiply[1], "Material flow 2", "%", 10, 1000);
#else
        else if (IS_SELECTED_SCROLL(4 + BED_MENU_OFFSET + EXTRUDERS))
            LCD_EDIT_SETTING(extrudemultiply[0], "Material flow", "%", 10, 1000);
#endif
        else if (IS_SELECTED_SCROLL(4 + BED_MENU_OFFSET + EXTRUDERS * 2))
            lcd_change_to_menu(lcd_menu_print_tune_retraction);
#if EXTRUDERS > 1
        else if (IS_SELECTED_SCROLL(5 + BED_MENU_OFFSET + EXTRUDERS * 2))
        {
            menu_extruder = 0;
            lcd_change_to_menu(lcd_menu_tune_tcretract, MAIN_MENU_ITEM_POS(1));
        }
        else if (IS_SELECTED_SCROLL(6 + BED_MENU_OFFSET + EXTRUDERS * 2))
        {
            menu_extruder = 1;
            lcd_change_to_menu(lcd_menu_tune_tcretract, MAIN_MENU_ITEM_POS(1));
        }
        else if (IS_SELECTED_SCROLL(7 + BED_MENU_OFFSET + EXTRUDERS * 2))
        {
            lcd_init_extruderoffset();
            lcd_change_to_menu(lcd_menu_extruderoffset, MAIN_MENU_ITEM_POS(1));
        }
#endif
#ifndef DUAL_FAN
        else if (IS_SELECTED_SCROLL(2 + BED_MENU_OFFSET + EXTRUDERS * 5))
            LCD_EDIT_SETTING(led_brightness_level, "Brightness", "%", 0, 100);
#endif
    }
}

static char* lcd_retraction_item(uint8_t nr)
{
    if (nr == 0)
        strcpy_P(card.longFilename, PSTR("< RETURN"));
    else if (nr == 1)
        strcpy_P(card.longFilename, PSTR("Retract length"));
    else if (nr == 2)
        strcpy_P(card.longFilename, PSTR("Retract speed"));
#if EXTRUDERS > 1
    else if (nr == 3)
        strcpy_P(card.longFilename, PSTR("Extruder change len"));
#endif
    else
        strcpy_P(card.longFilename, PSTR("???"));
    return card.longFilename;
}

static void lcd_retraction_details(uint8_t nr)
{
    char buffer[16];
    if (nr == 0)
        return;
    else if(nr == 1)
        float_to_string(retract_length, buffer, PSTR("mm"));
    else if(nr == 2)
        int_to_string(retract_feedrate / 60 + 0.5, buffer, PSTR("mm/sec"));
    lcd_lib_draw_string(5, BOTTOM_MENU_YPOS, buffer);
}

static void lcd_menu_print_tune_retraction()
{
    lcd_scroll_menu(PSTR("RETRACTION"), 3, lcd_retraction_item, lcd_retraction_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(0))
            lcd_change_to_menu(lcd_menu_print_tune, SCROLL_MENU_ITEM_POS(6));
        else if (IS_SELECTED_SCROLL(1))
            LCD_EDIT_SETTING_FLOAT001(retract_length, "Retract length", "mm", 0, 50);
        else if (IS_SELECTED_SCROLL(2))
            LCD_EDIT_SETTING_SPEED(retract_feedrate, "Retract speed", "mm/sec", 0, max_feedrate[E_AXIS] * 60);
    }
}

static void lcd_menu_print_pause()
{
    if (card.sdprinting && !card.pause)
    {
        if (movesplanned() && (commands_queued() < BUFSIZE))
        {
            pauseRequested = false;
            card.pause = true;

            // move z up according to the current height - but minimum to z=70mm (above the gantry height)
            uint16_t zdiff = 0;
            if (current_position[Z_AXIS] < 70)
                zdiff = max(70 - floor(current_position[Z_AXIS]), 20);
            else if (current_position[Z_AXIS] < Z_MAX_POS - 60)
            {
                zdiff = 20;
            }
            else if (current_position[Z_AXIS] < Z_MAX_POS - 30)
            {
                zdiff = 2;
            }

            char buffer[32];
        #if (EXTRUDERS > 1)
            char buffer_len[10];
            float_to_string(toolchange_retractlen[active_extruder], buffer_len, NULL);
            uint16_t x = max(5, 5 + extruder_offset[X_AXIS][active_extruder]);
            uint16_t y = IS_DUAL_ENABLED ? 60 : 10;
            sprintf_P(buffer, PSTR("M601 X%u Y%u Z%u L%s"), x, y, zdiff, buffer_len);
        #else
            sprintf_P(buffer, PSTR("M601 X5 Y10 Z%u L%u"), zdiff, END_OF_PRINT_RETRACTION);
        #endif

            enquecommand(buffer);
            // clear flag for end of print retraction
            primed &= ~ENDOFPRINT_RETRACT;
        }
        else{
            pauseRequested = true;
        }
    }
}

#endif//ENABLE_ULTILCD2
