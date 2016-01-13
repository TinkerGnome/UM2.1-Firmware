#include "Configuration.h"
#if (EXTRUDERS > 1) && defined(ENABLE_ULTILCD2) && defined(SDSUPPORT)
#include "ConfigurationStore.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"
#include "language.h"
#include "machinesettings.h"
#include "commandbuffer.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2_menu_utils.h"
#include "UltiLCD2_menu_maintenance.h"
#include "UltiLCD2_menu_dual.h"

void lcd_menu_dual();

static void lcd_store_extruderoffset()
{
    Dual_StoreSettings();
    lcd_change_to_previous_menu();
}

static void lcd_extruderoffset_x()
{
    lcd_tune_value(extruder_offset[X_AXIS][1], -99.99f, 99.99f, 0.01f);
}

static void lcd_extruderoffset_y()
{
    lcd_tune_value(extruder_offset[Y_AXIS][1], -99.99f, 99.99f, 0.01f);
}

static void lcd_extruderoffset_z()
{
    float zoffset = add_homeing[Z_AXIS] - add_homeing_z2;
    if (lcd_tune_value(zoffset, -10.0f, 10.0f, 0.01f))
    {
        add_homeing_z2 = add_homeing[Z_AXIS] - zoffset;
    }
}

// create menu options for "axis steps/mm"
static const menu_t & get_extruderoffset_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_extruderoffset);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // x steps
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_x, 5);
    }
    else if (nr == index++)
    {
        // y steps
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_y, 5);
    }
    else if (nr == index++)
    {
        // z steps
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_z, 5);
    }
    return opt;
}

static void drawExtruderOffsetSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32] = {0};
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store offsets"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , PSTR("STORE")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // RETURN
        LCDMenu::drawMenuBox(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , flags);
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , PSTR("RETURN")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // x offset
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("X offset"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(17, PSTR("X"));
        float_to_string(extruder_offset[X_AXIS][1], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 17
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // y offset
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Y offset"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(28, PSTR("Y"));
        float_to_string(extruder_offset[Y_AXIS][1], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 28
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // z offset
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Z offset"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(39, PSTR("Z"));
        float_to_string(add_homeing[Z_AXIS] - add_homeing_z2, buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 39
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

static void lcd_menu_extruderoffset()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_extruderoffset_menuoption, 5);

    uint8_t flags = 0;
    for (uint8_t index=0; index<5; ++index) {
        menu.drawSubMenu(drawExtruderOffsetSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Extruder offset"));
    }

    lcd_lib_update_screen();
}

static char* lcd_dual_item(uint8_t nr)
{
    uint8_t index(0);
    if (nr == index++)
        strcpy_P(card.longFilename, PSTR("< RETURN"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Change extruder"));
    else if (nr == index++)
    {
        strcpy_P(card.longFilename, PSTR("Adjust Z (nozzle "));
        int_to_string(active_extruder+1, card.longFilename+strlen(card.longFilename), PSTR(")"));
    }
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Extruder offset"));
    else
        strcpy_P(card.longFilename, PSTR("???"));

    return card.longFilename;
}

static void lcd_dual_details(uint8_t nr)
{
//    char buffer[32] = {0};
//    buffer[0] = '\0';
//    if (nr == 4)
//    {
//        // extruders swapped?
//        uint8_t xpos = (swapExtruders() ? LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-7*LCD_CHAR_SPACING : LCD_CHAR_MARGIN_LEFT);
//        lcd_lib_draw_stringP(xpos, BOTTOM_MENU_YPOS, PSTR("PRIMARY"));
//
//        xpos = (swapExtruders() ? LCD_CHAR_MARGIN_LEFT : LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-6*LCD_CHAR_SPACING);
//        lcd_lib_draw_stringP(xpos, BOTTOM_MENU_YPOS, PSTR("SECOND"));
//
//        lcd_lib_draw_stringP(LCD_GFX_WIDTH/2-LCD_CHAR_MARGIN_RIGHT-LCD_CHAR_SPACING, BOTTOM_MENU_YPOS, PSTR("<->"));
//        return;
//    }
//    lcd_lib_draw_string_left(BOTTOM_MENU_YPOS, buffer);
}

static void lcd_switch_extruder()
{
    if (tmp_extruder != active_extruder)
    {
        if ((tmp_extruder && cmdBuffer.hasScriptT1()) || cmdBuffer.hasScriptT0())
        {
            st_synchronize();
//            if (!(homestate & (HOMESTATE_X | HOMESTATE_Y)))
//            {
                enquecommand_P(PSTR("G28 X0 Y0"));
                st_synchronize();
//            }
        }
        changeExtruder(tmp_extruder, false);
        SERIAL_ECHO_START;
        SERIAL_ECHOPGM(MSG_ACTIVE_EXTRUDER);
        SERIAL_PROTOCOLLN((int)active_extruder);
    }
    lcd_change_to_menu(previousMenu, previousEncoderPos);
}

static bool endstop_reached(AxisEnum axis, int8_t direction)
{
    if (axis == X_AXIS)
    {
        #if defined(X_MIN_PIN) && X_MIN_PIN > -1
        if ((direction < 0) && (READ(X_MIN_PIN) != X_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        #if defined(X_MAX_PIN) && X_MAX_PIN > -1
        if ((direction > 0) && (READ(X_MAX_PIN) != X_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    else if (axis == Y_AXIS)
    {
        #if defined(Y_MIN_PIN) && Y_MIN_PIN > -1
        if ((direction < 0) && (READ(Y_MIN_PIN) != Y_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        // check max endstop
        #if defined(Y_MAX_PIN) && Y_MAX_PIN > -1
        if ((direction > 0) && (READ(Y_MAX_PIN) != Y_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    else if (axis == Z_AXIS)
    {
        #if defined(Z_MIN_PIN) && Z_MIN_PIN > -1
        if ((direction < 0) && (READ(Z_MIN_PIN) != Z_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
        // check max endstop
        #if defined(Z_MAX_PIN) && Z_MAX_PIN > -1
        if ((direction > 0) && (READ(Z_MAX_PIN) != Z_ENDSTOPS_INVERTING))
        {
            return true;
        }
        #endif
    }
    return false;
}

FORCE_INLINE static void lcd_dual_switch_extruder()
{
    lcd_select_nozzle(NULL, lcd_switch_extruder, NULL);
}

static void lcd_simple_buildplate_quit()
{
    // home z-axis
    enquecommand_P(PSTR("G28 Z0"));
    // home head
    enquecommand_P(PSTR("G28 X0 Y0"));
    enquecommand_P(PSTR("M84 X0 Y0"));
}

static void lcd_simple_buildplate_cancel()
{
    // reload settings
    Config_RetrieveSettings();
#if (EXTRUDERS > 1)
    Dual_RetrieveSettings();
#endif
    lcd_simple_buildplate_quit();
    lcd_change_to_previous_menu();
}

static void lcd_simple_buildplate_store()
{
#if (EXTRUDERS > 1)
    if (active_extruder)
    {
        add_homeing_z2 -= current_position[Z_AXIS];
        Dual_StoreSettings();
    }
    else
    {
        add_homeing[Z_AXIS] -= current_position[Z_AXIS];
        Config_StoreSettings();
    }
#else
    add_homeing[Z_AXIS] -= current_position[Z_AXIS];
    Config_StoreSettings();
#endif
    current_position[Z_AXIS] = 0;
    plan_set_position(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
    lcd_simple_buildplate_quit();
    lcd_change_to_previous_menu();
}

static void plan_move(AxisEnum axis, float newPos)
{
    if (!is_command_queued() && !blocks_queued())
    {
        // enque next move
        if ((abs(newPos - current_position[axis])>0.005) && !endstop_reached(axis, (newPos>current_position[axis]) ? 1 : -1))
        {
            current_position[axis] = newPos;
            plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], homing_feedrate[axis]/800, active_extruder);
        }
    }
}

static void lcd_position_z_axis()
{
    float zpos = current_position[Z_AXIS];
    if (lcd_tune_value(zpos, min_pos[Z_AXIS], max_pos[Z_AXIS], 0.01f))
    {
        plan_move(Z_AXIS, zpos);
    }
}

static void init_value_tuning()
{
    lcd_lib_encoder_pos = 0;
}

// create menu options for "move axes"
static const menu_t & get_simple_buildplate_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // Store new homeing offset
        opt.setData(MENU_NORMAL, lcd_simple_buildplate_store);
    }
    else if (nr == index++)
    {
        // Cancel
        opt.setData(MENU_NORMAL, lcd_simple_buildplate_cancel);
    }
    else if (nr == index++)
    {
        // z position
        opt.setData(MENU_INPLACE_EDIT, init_value_tuning, lcd_position_z_axis, NULL, 3);
    }
    return opt;
}

static void drawSimpleBuildplateSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // Store
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+3
                                , BOTTOM_MENU_YPOS
                                , 52
                                , LCD_CHAR_HEIGHT
                                , PSTR("STORE")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // Cancel
        LCDMenu::drawMenuString_P(LCD_GFX_WIDTH/2 + 2*LCD_CHAR_MARGIN_LEFT
                           , BOTTOM_MENU_YPOS
                           , 52
                           , LCD_CHAR_HEIGHT
                           , PSTR("CANCEL")
                           , ALIGN_CENTER
                           , flags);
    }
    else if (nr == index++)
    {
        // z position
        char buffer[32] = {0};
        lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING, 38, PSTR("Z"));
        float_to_string(st_get_position(Z_AXIS) / axis_steps_per_unit[Z_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+7*LCD_CHAR_SPACING
                              , 38
                              , 48
                              , LCD_CHAR_HEIGHT
                              , buffer
                              , ALIGN_RIGHT | ALIGN_VCENTER
                              , flags);
    }
}

static void lcd_simple_buildplate_init()
{
    menu.set_active(get_simple_buildplate_menuoption, 2);
}

static void lcd_menu_simple_buildplate()
{
    lcd_basic_screen();
    // lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_simple_buildplate_menuoption, 3);

    uint8_t flags = 0;
    for (uint8_t index=0; index<3; ++index) {
        menu.drawSubMenu(drawSimpleBuildplateSubmenu, index, flags);
    }

    lcd_lib_draw_string_centerP(5, PSTR("Move Z until the"));
    lcd_lib_draw_string_centerP(15, PSTR("nozzle touches"));
    lcd_lib_draw_string_centerP(25, PSTR("the buildplate"));

    lcd_lib_update_screen();
}

static void lcd_prepare_buildplate_adjust()
{
    Config_RetrieveSettings();
#if (EXTRUDERS > 1)
    if (active_extruder)
    {
        add_homeing_z2 = 0;
    }
    else
    {
        add_homeing[Z_AXIS] = 0;
    }
#else
    add_homeing[Z_AXIS] = 0;
#endif
    st_synchronize();
    enquecommand_P(PSTR("G28"));
    char buffer[32] = {0};
    sprintf_P(buffer, PSTR("G1 F%i Z%i X%i Y%i"), int(homing_feedrate[0]), 35, AXIS_CENTER_POS(X_AXIS), AXIS_CENTER_POS(Y_AXIS));
    enquecommand(buffer);
    enquecommand_P(PSTR("M84 X0 Y0"));
}

void lcd_menu_simple_buildplate_init()
{
    lcd_lib_clear();

    float zPos = st_get_position(Z_AXIS) / axis_steps_per_unit[Z_AXIS];
    if ((commands_queued() < 1) && (zPos < 35.01f))
    {
        currentMenu = lcd_menu_simple_buildplate;
        lcd_simple_buildplate_init();
    }

    lcd_lib_draw_string_centerP(5, PSTR("Move Z until the"));
    lcd_lib_draw_string_centerP(15, PSTR("nozzle touches"));
    lcd_lib_draw_string_centerP(25, PSTR("the buildplate"));

    char buffer[32] = {0};
    lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT+5*LCD_CHAR_SPACING, 38, PSTR("Z"));
    float_to_string(zPos, buffer, PSTR("mm"));
    LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+7*LCD_CHAR_SPACING
                          , 38
                          , 48
                          , LCD_CHAR_HEIGHT
                          , buffer
                          , ALIGN_RIGHT | ALIGN_VCENTER
                          , 0);

    lcd_lib_update_screen();
}

void lcd_menu_dual()
{
    lcd_scroll_menu(PSTR("Dual extrusion"), 4, lcd_dual_item, lcd_dual_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(0))
            lcd_change_to_menu(lcd_menu_maintenance_advanced);
        else if (IS_SELECTED_SCROLL(1))
            lcd_change_to_menu(lcd_dual_switch_extruder, MAIN_MENU_ITEM_POS(active_extruder ? 1 : 0));
        else if (IS_SELECTED_SCROLL(2))
        {
            lcd_prepare_buildplate_adjust();
            lcd_change_to_menu(lcd_menu_simple_buildplate_init, ENCODER_NO_SELECTION);
        }
        else if (IS_SELECTED_SCROLL(3))
            lcd_change_to_menu(lcd_menu_extruderoffset);
    }
    lcd_lib_update_screen();
}


#endif//ENABLE_ULTILCD2
