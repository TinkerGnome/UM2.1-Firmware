#include "Configuration.h"
#if (EXTRUDERS > 1) && defined(ENABLE_ULTILCD2)
#include "ConfigurationStore.h"
#include "ConfigurationDual.h"
#include "planner.h"
#include "stepper.h"
#include "language.h"
#include "machinesettings.h"
#include "commandbuffer.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2_menu_maintenance.h"
#include "UltiLCD2_menu_print.h"
#include "UltiLCD2_menu_dual.h"

// Use the lcd_cache memory to store manual moving positions
#define TARGET_POS(n)   (*(float*)&lcd_cache[(n) * sizeof(float)])
#define TARGET_MIN(n)   (*(float*)&lcd_cache[(n) * sizeof(float)])
#define TARGET_MAX(n)   (*(float*)&lcd_cache[sizeof(min_pos) + (n) * sizeof(float)])
#define OLD_FEEDRATE    (*(float*)&lcd_cache[NUM_AXIS * sizeof(float)])
#define OLD_ACCEL       (*(float*)&lcd_cache[(NUM_AXIS+1) * sizeof(float)])
#define OLD_JERK        (*(float*)&lcd_cache[(NUM_AXIS+2) * sizeof(float)])

uint8_t menu_extruder = 0;

void lcd_menu_dual();

static void lcd_store_dualstate()
{
    Dual_StoreState();
    lcd_change_to_previous_menu();
}

static void lcd_store_dockposition()
{
    Dual_StoreDockPosition();
    lcd_change_to_previous_menu();
}

static void lcd_store_wipeposition()
{
    Dual_StoreWipePosition();
    lcd_change_to_previous_menu();
}

static void lcd_return_extruderoffset()
{
    lcd_calc_extruderoffset();
    lcd_change_to_previous_menu();
}

static void lcd_store_extruderoffset()
{
    lcd_calc_extruderoffset();
    Dual_StoreExtruderOffset();
    Dual_StoreAddHomeingZ2();
    lcd_change_to_previous_menu();
}

static void lcd_store_tcretract()
{
    Dual_StoreRetract();
    lcd_change_to_previous_menu();
}

//////////////////

static void lcd_extruderoffset_x()
{
    lcd_tune_value(extruder_offset[X_AXIS][1], -99.99f, 99.99f, 0.01f);
}

static void lcd_extruderoffset_y()
{
    lcd_tune_value(extruder_offset[Y_AXIS][1], -99.99f, 99.99f, 0.01f);
}

static void lcd_extruderruler_x()
{
    int8_t index = (int8_t)LCD_CACHE_ID(X_AXIS);
    if (lcd_tune_value(index, -20, 20))
    {
        LCD_CACHE_ID(X_AXIS) = (uint8_t)index;
    }
}

static void lcd_extruderruler_y()
{
    int8_t index = (int8_t)LCD_CACHE_ID(Y_AXIS);
    if (lcd_tune_value(index, -20, 20))
    {
        LCD_CACHE_ID(Y_AXIS) = (uint8_t)index;
    }
}

static void lcd_extruderoffset_z()
{
    float zoffset = add_homeing[Z_AXIS] - add_homeing_z2;
    if (lcd_tune_value(zoffset, -10.0f, 10.0f, 0.01f))
    {
        add_homeing_z2 = add_homeing[Z_AXIS] - zoffset;
    }
}

// create menu options for "extruder offset"
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
        opt.setData(MENU_NORMAL, lcd_return_extruderoffset);
    }
    else if (nr == index++)
    {
        // x offset
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_x, 8);
    }
    else if ((printing_state == PRINT_STATE_NORMAL) && !(card.sdprinting || commands_queued() || movesplanned()) && nr == index++)
    {
        // x calibration line
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderruler_x, 8);
    }
    else if (nr == index++)
    {
        // y offset
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_y, 8);
    }
    else if ((printing_state == PRINT_STATE_NORMAL) && !(card.sdprinting || commands_queued() || movesplanned()) && nr == index++)
    {
        // y calibration line
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderruler_y, 8);
    }
    else if (nr == index++)
    {
        // z offset
        opt.setData(MENU_INPLACE_EDIT, lcd_extruderoffset_z, 8);
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
        float_to_string(extruder_offset[X_AXIS][1]-((int8_t)LCD_CACHE_ID(X_AXIS)*0.04), buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 17
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if ((printing_state == PRINT_STATE_NORMAL) && !(card.sdprinting || commands_queued() || movesplanned()) && nr == index++)
    {
        // x offset ruler
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("X offset line no."));
            flags |= MENU_STATUSLINE;
        }
        int_to_string((int8_t)LCD_CACHE_ID(X_AXIS), buffer, PSTR(")"), PSTR("("), true);
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*13
                                , 17
                                , LCD_CHAR_SPACING*5
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
        float_to_string(extruder_offset[Y_AXIS][1]-((int8_t)LCD_CACHE_ID(Y_AXIS)*0.04f), buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 28
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if ((printing_state == PRINT_STATE_NORMAL) && !(card.sdprinting || commands_queued() || movesplanned()) && nr == index++)
    {
        // y offset ruler
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Y offset line no."));
            flags |= MENU_STATUSLINE;
        }
        int_to_string((int8_t)LCD_CACHE_ID(Y_AXIS), buffer, PSTR(")"), PSTR("("), true);
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*13
                                , 28
                                , LCD_CHAR_SPACING*5
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
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

void lcd_init_extruderoffset()
{
    // use the lcd cache for calibration line numbers
    LCD_CACHE_ID(X_AXIS) = 0;
    LCD_CACHE_ID(Y_AXIS) = 0;
}

void lcd_calc_extruderoffset()
{
    extruder_offset[X_AXIS][1] -= ((int8_t)LCD_CACHE_ID(X_AXIS)*0.04);
    extruder_offset[Y_AXIS][1] -= ((int8_t)LCD_CACHE_ID(Y_AXIS)*0.04);
    LCD_CACHE_ID(X_AXIS) = 0;
    LCD_CACHE_ID(Y_AXIS) = 0;
}

void lcd_menu_extruderoffset()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    uint8_t len = (card.sdprinting || commands_queued() || movesplanned() || (printing_state != PRINT_STATE_NORMAL) ? 5 : 7);

    menu.process_submenu(get_extruderoffset_menuoption, len);

    uint8_t flags = 0;
    for (uint8_t index=0; index<len; ++index) {
        menu.drawSubMenu(drawExtruderOffsetSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Extruder offset"));
    }

    lcd_lib_update_screen();
}

//////////////////

static void init_head_position()
{
    st_synchronize();
    if (!(position_state & (KNOWNPOS_X | KNOWNPOS_Y)))
    {
        // home head
        CommandBuffer::homeHead();
        cmd_synchronize();
        st_synchronize();
    }
}

static void lcd_dockmove_init()
{
    // save current variables
    TARGET_POS(X_AXIS) = dock_position[X_AXIS];
    TARGET_POS(Y_AXIS) = dock_position[Y_AXIS];
    OLD_ACCEL = acceleration;
    OLD_JERK = max_xy_jerk;
    // home head if necessary
    init_head_position();
    // move to dock position
    CommandBuffer::move2dock(false);
    acceleration = 250;
    max_xy_jerk = 5;
}

static void lcd_dockmove_quit()
{
    // reset variables
    acceleration = OLD_ACCEL;
    max_xy_jerk = OLD_JERK ;
    // home head
    CommandBuffer::move2SafeYPos();
    CommandBuffer::moveHead(min_pos[X_AXIS], max_pos[Y_AXIS], 200);
    enquecommand_P(PSTR("M84 X Y"));
    lcd_change_to_previous_menu();
}

static void lcd_store_dockmove()
{
    dock_position[X_AXIS] = TARGET_POS(X_AXIS);
    dock_position[Y_AXIS] = TARGET_POS(Y_AXIS);
    Dual_StoreDockPosition();
    lcd_dockmove_quit();
}

static void lcd_dockmove_x()
{
    if (!movesplanned())
    {
        lcd_tune_value(TARGET_POS(X_AXIS), 0.0f, max_pos[X_AXIS], 0.1f);
        CommandBuffer::moveHead(TARGET_POS(X_AXIS), current_position[Y_AXIS], 10);
    }
}

static void lcd_dockmove_y()
{
    if (!movesplanned())
    {
        lcd_tune_value(TARGET_POS(Y_AXIS), 0.0f, max_pos[Y_AXIS], 0.1f);
        CommandBuffer::moveHead(current_position[X_AXIS], TARGET_POS(Y_AXIS), 10);
    }
}

// create menu options for "dock position"
static const menu_t & get_dockmove_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_dockmove);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_dockmove_quit);
    }
    else if (nr == index++)
    {
        // x pos
        opt.setData(MENU_INPLACE_EDIT, lcd_dockmove_x, 2);
    }
    else if (nr == index++)
    {
        // y pos
        opt.setData(MENU_INPLACE_EDIT, lcd_dockmove_y, 2);
    }
    return opt;
}

static void drawDockMoveSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32] = {0};
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store dock position"));
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
            lcd_lib_draw_string_leftP(5, PSTR("Abort function"));
            flags |= MENU_STATUSLINE;
        }
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
        // x position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("dock position x"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(20, PSTR("X"));
        float_to_string(st_get_position(X_AXIS) / axis_steps_per_unit[X_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 20
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // y position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("dock position y"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(32, PSTR("Y"));
        float_to_string(st_get_position(Y_AXIS) / axis_steps_per_unit[Y_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 32
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

static void lcd_menu_dockmove()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_dockmove_menuoption, 4);

    uint8_t flags = 0;

    if (printing_state == PRINT_STATE_HOMING)
    {
        lcd_lib_draw_string_centerP(25, PSTR("Preparing..."));
    }
    else
    {
        for (uint8_t index=0; index<4; ++index) {
            menu.drawSubMenu(drawDockMoveSubmenu, index, flags);
        }
    }

    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Docking position"));
    }

    lcd_lib_update_screen();
}

//////////////////

static void lcd_dockposition_x()
{
    lcd_tune_value(dock_position[X_AXIS], 0.0f, max_pos[X_AXIS], 0.1f);
}

static void lcd_dockposition_y()
{
    lcd_tune_value(dock_position[Y_AXIS], 0.0f, max_pos[Y_AXIS], 0.1f);
}

static void lcd_dockposition_move()
{
    lcd_replace_menu(lcd_menu_dockmove, MAIN_MENU_ITEM_POS(1));
    lcd_dockmove_init();
}

static void lcd_menu_dockmove_prepare()
{
    lcd_question_screen(NULL, lcd_dockposition_move, PSTR("CONTINUE"), NULL, lcd_change_to_previous_menu, PSTR("CANCEL"));

    lcd_lib_draw_string_centerP( 8, PSTR("Remove the second"));
    lcd_lib_draw_string_centerP(18, PSTR("printhead from the"));
    lcd_lib_draw_string_centerP(28, PSTR("dock to start"));
    lcd_lib_draw_string_centerP(38, PSTR("calibration."));

    lcd_lib_update_screen();
}

static void lcd_dockmove_prepare()
{
    lcd_replace_menu(lcd_menu_dockmove_prepare, MAIN_MENU_ITEM_POS(1));
}

// create menu options for "dock position"
static const menu_t & get_dockposition_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_dockposition);
    }
    else if (!active_extruder && nr == index++)
    {
        // MOVE
        opt.setData(MENU_NORMAL, lcd_dockmove_prepare);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // x pos
        opt.setData(MENU_INPLACE_EDIT, lcd_dockposition_x, 2);
    }
    else if (nr == index++)
    {
        // y pos
        opt.setData(MENU_INPLACE_EDIT, lcd_dockposition_y, 2);
    }
    return opt;
}

static void drawDockPositionSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32] = {0};
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store dock position"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT
                                , BOTTOM_MENU_YPOS
                                , 38
                                , LCD_CHAR_HEIGHT
                                , PSTR("STORE")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (!active_extruder && nr == index++)
    {
        // move printhead to dock position
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Move head"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(2*LCD_CHAR_MARGIN_LEFT + 38
                                , BOTTOM_MENU_YPOS
                                , 32
                                , LCD_CHAR_HEIGHT
                                , PSTR("MOVE")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // RETURN
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Click to return"));
            flags |= MENU_STATUSLINE;
        }
        LCDMenu::drawMenuString_P(LCD_GFX_WIDTH - LCD_CHAR_MARGIN_RIGHT - 44
                                , BOTTOM_MENU_YPOS
                                , 44
                                , LCD_CHAR_HEIGHT
                                , PSTR("RETURN")
                                , ALIGN_CENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // x position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("dock position x"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(20, PSTR("X"));
        float_to_string(dock_position[X_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 20
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // y position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("dock position y"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(32, PSTR("Y"));
        float_to_string(dock_position[Y_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 32
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

static void lcd_menu_dockposition()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    uint8_t len = 5;
    if (active_extruder)
    {
        --len;
    }

    menu.process_submenu(get_dockposition_menuoption, len);

    uint8_t flags = 0;
    for (uint8_t index=0; index<len; ++index) {
        menu.drawSubMenu(drawDockPositionSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Docking position"));
    }

    lcd_lib_update_screen();
}

//////////////////

static void lcd_wipeposition_x()
{
    lcd_tune_value(wipe_position[X_AXIS], 0.0f, max_pos[X_AXIS], 0.1f);
}

static void lcd_wipeposition_y()
{
    lcd_tune_value(wipe_position[Y_AXIS], 0.0f, max_pos[Y_AXIS], 0.1f);
}

// create menu options for "axis steps/mm"
static const menu_t & get_wipeposition_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_wipeposition);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // x pos
        opt.setData(MENU_INPLACE_EDIT, lcd_wipeposition_x, 2);
    }
    else if (nr == index++)
    {
        // y pos
        opt.setData(MENU_INPLACE_EDIT, lcd_wipeposition_y, 2);
    }
    return opt;
}

static void drawWipePositionSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32] = {0};
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store wipe position"));
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
        // x position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("wipe position x"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(20, PSTR("X"));
        float_to_string(wipe_position[X_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 20
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // y position
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("wipe position y"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(32, PSTR("Y"));
        float_to_string(wipe_position[Y_AXIS], buffer, PSTR("mm"));
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*3
                                , 32
                                , LCD_CHAR_SPACING*8
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

static void lcd_menu_wipeposition()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    menu.process_submenu(get_wipeposition_menuoption, 4);

    uint8_t flags = 0;
    for (uint8_t index=0; index<4; ++index) {
        menu.drawSubMenu(drawWipePositionSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Wipe position"));
    }

    lcd_lib_update_screen();
}

//////////////////

static void lcd_tune_tcretractlen()
{
    lcd_tune_value(toolchange_retractlen[menu_extruder], 0.0f, 50.0, 0.1f);
}

static void lcd_tune_tcretractfeed()
{
    lcd_tune_value(toolchange_retractfeedrate[menu_extruder], 0.0f, max_feedrate[E_AXIS]*60, 60.0f);
}

static void lcd_tune_tcprime()
{
    lcd_tune_value(toolchange_prime[menu_extruder], -20.0f, 20.0f, 0.1f);
}

// create menu options for "toolchange retract"
static const menu_t & get_tcretract_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_tcretract);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // retract len
        opt.setData(MENU_INPLACE_EDIT, lcd_tune_tcretractlen, 2);
    }
    else if (nr == index++)
    {
        // retract feedrate
        opt.setData(MENU_INPLACE_EDIT, lcd_tune_tcretractfeed, 2);
    }
    else if (nr == index++)
    {
        // extra priming len
        opt.setData(MENU_INPLACE_EDIT, lcd_tune_tcprime, 2);
    }
    return opt;
}

static void drawTCRetractSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    char buffer[32] = {0};
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store retract values"));
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
        // retract len
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Retract len (mm)"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(17, PSTR("Len"));
        float_to_string(toolchange_retractlen[menu_extruder], buffer, NULL);
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*8
                                , 17
                                , LCD_CHAR_SPACING*5
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // retract feedrate
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Retract speed (mm/s)"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(27, PSTR("Speed"));
        float_to_string(toolchange_retractfeedrate[menu_extruder]/60, buffer, NULL);
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*8
                                , 27
                                , LCD_CHAR_SPACING*5
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // extra prime len
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Extra priming (mm)"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(37, PSTR("Prime"));
        float_to_string(toolchange_prime[menu_extruder], buffer, NULL);
        LCDMenu::drawMenuString(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*7
                                , 37
                                , LCD_CHAR_SPACING*6
                                , LCD_CHAR_HEIGHT
                                , buffer
                                , ALIGN_RIGHT | ALIGN_VCENTER
                                , flags);
    }
}

void lcd_menu_tune_tcretract()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    char buffer[4] = {0};
    strcpy_P(buffer, PSTR("("));
    int_to_string(menu_extruder+1, buffer+1, PSTR(")"));
    lcd_lib_draw_string(LCD_GFX_WIDTH-LCD_CHAR_MARGIN_RIGHT-LCD_CHAR_SPACING*3, 17, buffer);


    menu.process_submenu(get_tcretract_menuoption, 5);

    uint8_t flags = 0;
    for (uint8_t index=0; index<5; ++index) {
        menu.drawSubMenu(drawTCRetractSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Toolchange retract"));
    }

    lcd_lib_update_screen();
}

//////////////////

static void lcd_toggle_dual()
{
    dual_state ^= DUAL_ENABLED;
}

static void lcd_toggle_toolchange()
{
    dual_state ^= DUAL_TOOLCHANGE;
}

static void lcd_toggle_wipe()
{
    dual_state ^= DUAL_WIPE;
}

// create menu options for "dual mode"
static const menu_t & get_dualstate_menuoption(uint8_t nr, menu_t &opt)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // STORE
        opt.setData(MENU_NORMAL, lcd_store_dualstate);
    }
    else if (nr == index++)
    {
        // RETURN
        opt.setData(MENU_NORMAL, lcd_change_to_previous_menu);
    }
    else if (nr == index++)
    {
        // enable dual mode
        opt.setData(MENU_NORMAL, lcd_toggle_dual);
    }
    else if (nr == index++)
    {
        // enable toolchange scripts
        opt.setData(MENU_NORMAL, lcd_toggle_toolchange);
    }
    else if (nr == index++)
    {
        // enable wipe script
        opt.setData(MENU_NORMAL, lcd_toggle_wipe);
    }
    return opt;
}

static void drawDualStateSubmenu(uint8_t nr, uint8_t &flags)
{
    uint8_t index(0);
    if (nr == index++)
    {
        // Store
        if (flags & MENU_SELECTED)
        {
            lcd_lib_draw_string_leftP(5, PSTR("Store dual mode"));
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
        // dual mode
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Dual mode"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(17, PSTR("Dual mode:"));
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*13
                                , 17
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , IS_DUAL_ENABLED ? PSTR("enabled") : PSTR("off")
                                , ALIGN_LEFT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // wipe device
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Toolchange script"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(28, PSTR("Toolchange:"));
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*13
                                , 28
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , IS_TOOLCHANGE_ENABLED ? PSTR("enabled") : PSTR("off")
                                , ALIGN_LEFT | ALIGN_VCENTER
                                , flags);
    }
    else if (nr == index++)
    {
        // wipe device
        if ((flags & MENU_ACTIVE) | (flags & MENU_SELECTED))
        {
            lcd_lib_draw_string_leftP(5, PSTR("Wipe device"));
            flags |= MENU_STATUSLINE;
        }
        lcd_lib_draw_string_leftP(39, PSTR("Wipe device:"));
        LCDMenu::drawMenuString_P(LCD_CHAR_MARGIN_LEFT+LCD_CHAR_SPACING*13
                                , 39
                                , LCD_CHAR_SPACING*7
                                , LCD_CHAR_HEIGHT
                                , IS_WIPE_ENABLED ? PSTR("enabled") : PSTR("off")
                                , ALIGN_LEFT | ALIGN_VCENTER
                                , flags);
    }
}

static void lcd_menu_dualstate()
{
    lcd_basic_screen();
    lcd_lib_draw_hline(3, 124, 13);

    uint8_t len = IS_DUAL_ENABLED ? 5 : 3;
    menu.process_submenu(get_dualstate_menuoption, len);

    uint8_t flags = 0;
    for (uint8_t index=0; index<len; ++index) {
        menu.drawSubMenu(drawDualStateSubmenu, index, flags);
    }
    if (!(flags & MENU_STATUSLINE))
    {
        lcd_lib_draw_string_leftP(5, PSTR("Dual state"));
    }

    lcd_lib_update_screen();
}

//////////////////

void switch_extruder(uint8_t newExtruder, bool moveZ)
{
    if (newExtruder != active_extruder)
    {
        // home head if necessary
        init_head_position();
        // tool change
        changeExtruder(newExtruder, moveZ);
    }
}

static void lcd_switch_extruder()
{
    switch_extruder(menu_extruder, false);
    printing_state = PRINT_STATE_NORMAL;
    lcd_change_to_previous_menu();
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
    lcd_select_nozzle(NULL, lcd_switch_extruder, lcd_change_to_previous_menu);
}

//////////////////

static void lcd_simple_buildplate_quit()
{
    // home z-axis
    CommandBuffer::homeBed();
    // home head
    CommandBuffer::homeHead();
    enquecommand_P(PSTR("M84"));
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
        Dual_StoreAddHomeingZ2();
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
    st_synchronize();
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
    // remove homing offset
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
    char buffer[32] = {0};
    // home axis first
    strcpy_P(buffer, PSTR("G28"));
    if (!(position_state & KNOWNPOS_X))
    {
        strcat_P(buffer, PSTR(" X0"));
    }
    if (!(position_state & KNOWNPOS_Y))
    {
        strcat_P(buffer, PSTR(" Y0"));
    }
    strcat_P(buffer, PSTR(" Z0"));
    enquecommand(buffer);

    sprintf_P(buffer, PSTR("G1 F%i Z%i X%i Y%i"), int(homing_feedrate[0]), 20, AXIS_CENTER_POS(X_AXIS), AXIS_CENTER_POS(Y_AXIS));
    enquecommand(buffer);
    enquecommand_P(PSTR("M84 X0 Y0"));
}

void lcd_menu_simple_buildplate_init()
{
    lcd_lib_clear();

    float zPos = st_get_position(Z_AXIS) / axis_steps_per_unit[Z_AXIS];
    if ((commands_queued() < 1) && (zPos < 20.01f))
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

//////////////////

static char* lcd_dual_item(uint8_t nr)
{
    uint8_t index(0);
    if (nr == index++)
        strcpy_P(card.longFilename, PSTR("< RETURN"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Dual mode"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Change extruder"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Toolchange retract"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Extruder offset"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Docking position"));
    else if (nr == index++)
        strcpy_P(card.longFilename, PSTR("Wipe position"));
    else if (nr == index++)
    {
        strcpy_P(card.longFilename, PSTR("Adjust Z (nozzle "));
        int_to_string(active_extruder+1, card.longFilename+strlen(card.longFilename), PSTR(")"));
    }
    else
        strcpy_P(card.longFilename, PSTR("???"));

    return card.longFilename;
}

static void lcd_dual_details(uint8_t nr)
{
    if (nr == 1)
    {
        // dual mode
        lcd_lib_draw_string_leftP(BOTTOM_MENU_YPOS, IS_DUAL_ENABLED ? PSTR("enabled") : PSTR("off"));
    }
}

static void start_menu_tcretract()
{
    lcd_change_to_menu(lcd_menu_tune_tcretract, MAIN_MENU_ITEM_POS(1), false);
}

static void lcd_menu_tcretraction()
{
    lcd_select_nozzle(lcd_menu_dual, start_menu_tcretract, lcd_change_to_previous_menu);
}

void lcd_menu_dual()
{
    lcd_scroll_menu(PSTR("Dual extrusion"), 8, lcd_dual_item, lcd_dual_details);
    if (lcd_lib_button_pressed)
    {
        if (IS_SELECTED_SCROLL(0))
            lcd_change_to_menu(lcd_menu_maintenance_advanced);
        else if (IS_SELECTED_SCROLL(1))
            lcd_change_to_menu(lcd_menu_dualstate, MAIN_MENU_ITEM_POS(1));
        else if (IS_SELECTED_SCROLL(2))
            lcd_change_to_menu(lcd_dual_switch_extruder, MAIN_MENU_ITEM_POS(active_extruder ? 1 : 0));
        else if (IS_SELECTED_SCROLL(3))
            lcd_change_to_menu(lcd_menu_tcretraction, MAIN_MENU_ITEM_POS(menu_extruder));
        else if (IS_SELECTED_SCROLL(4))
        {
            lcd_init_extruderoffset();
            lcd_change_to_menu(lcd_menu_extruderoffset, MAIN_MENU_ITEM_POS(1));
        }
        else if (IS_SELECTED_SCROLL(5))
            lcd_change_to_menu(lcd_menu_dockposition, MAIN_MENU_ITEM_POS(2-active_extruder));
        else if (IS_SELECTED_SCROLL(6))
            lcd_change_to_menu(lcd_menu_wipeposition, MAIN_MENU_ITEM_POS(1));
        else if (IS_SELECTED_SCROLL(7))
        {
            lcd_prepare_buildplate_adjust();
            lcd_change_to_menu(lcd_menu_simple_buildplate_init, ENCODER_NO_SELECTION);
        }
    }
}

void lcd_select_nozzle(menuFunc_t nextMenu, menuFunc_t callbackOnSelect, menuFunc_t callbackOnAbort)
{
    lcd_tripple_menu(PSTR("EXTRUDER|1"), PSTR("EXTRUDER|2"), PSTR("RETURN"));

    if (lcd_lib_button_pressed)
    {
        uint8_t index(SELECTED_MAIN_MENU_ITEM());
        if (nextMenu)
        {
            lcd_replace_menu(nextMenu, previousEncoderPos, true);
        }
        if (index < 2)
        {
            menu_extruder = index;
            if (callbackOnSelect) callbackOnSelect();
        }
        else
        {
            if (callbackOnAbort) callbackOnAbort();
        }
    }
    else
    {
        lcd_lib_update_screen();
    }
}


#endif //EXTRUDERS
