#ifndef ULTILCD2_MENU_UTILS_H
#define ULTILCD2_MENU_UTILS_H

#include "UltiLCD2_low_lib.h"
#include "UltiLCD2_hi_lib.h"
#include "UltiLCD2_menu_print.h"

// menu item flags
#define MENU_NORMAL 0
#define MENU_INPLACE_EDIT 1
#define MENU_SCROLL_ITEM 2
#define MENU_SELECTED 4
#define MENU_ACTIVE   8
#define MENU_STATUSLINE   16

// text alignment
#define ALIGN_TOP 1
#define ALIGN_BOTTOM 2
#define ALIGN_VCENTER 4
#define ALIGN_LEFT 8
#define ALIGN_RIGHT 16
#define ALIGN_HCENTER 32
#define ALIGN_CENTER 36


// display constants
#define LCD_GFX_WIDTH 128
#define LCD_GFX_HEIGHT 64

// text position constants
#define LCD_LINE_HEIGHT 9
#define LCD_CHAR_MARGIN_LEFT 4
#define LCD_CHAR_MARGIN_RIGHT 4
#define LCD_CHAR_SPACING 6
#define LCD_CHAR_HEIGHT 7
#define BOTTOM_MENU_YPOS 53


#define UNIT_FLOW "mm\x1D/s"
#define UNIT_SPEED "mm/s"

FORCE_INLINE void lcd_lib_draw_string_leftP(uint8_t y, const char* pstr) { lcd_lib_draw_stringP(LCD_CHAR_MARGIN_LEFT, y, pstr); }


// --------------------------------------------------------------------------
// menu stack handling
// --------------------------------------------------------------------------
#define MAX_MENU_DEPTH 6

typedef void (*menuFunc_t)();

struct menu_t {
    menuFunc_t  initMenuFunc;
    menuFunc_t  postMenuFunc;
    menuFunc_t  processMenuFunc;
    int16_t     encoderPos;
    uint8_t     max_encoder_acceleration;
    uint8_t     flags;

    // constructor
    menu_t(menuFunc_t func=0, int16_t pos=ENCODER_NO_SELECTION, uint8_t accel=0, uint8_t fl=MENU_NORMAL)
     : initMenuFunc(0)
     , postMenuFunc(0)
     , processMenuFunc(func)
     , encoderPos(pos)
     , max_encoder_acceleration(accel)
     , flags(fl) {}
    // constructor
    menu_t(menuFunc_t initFunc, menuFunc_t eventFunc, menuFunc_t postFunc, int16_t pos=ENCODER_NO_SELECTION, uint8_t accel=0, uint8_t fl=MENU_NORMAL)
     : initMenuFunc(initFunc)
     , postMenuFunc(postFunc)
     , processMenuFunc(eventFunc)
     , encoderPos(pos)
     , max_encoder_acceleration(accel)
     , flags(fl) {}

    void setData(uint8_t fl, menuFunc_t func=0, uint8_t accel=0, int16_t pos=ENCODER_NO_SELECTION)
    {
        initMenuFunc = 0;
        postMenuFunc = 0;
        processMenuFunc = func;
        encoderPos = pos;
        max_encoder_acceleration = accel;
        flags = fl;
    }
    void setData(uint8_t fl, menuFunc_t initFunc, menuFunc_t eventFunc, menuFunc_t postFunc, uint8_t accel=0, int16_t pos=ENCODER_NO_SELECTION)
    {
        initMenuFunc = initFunc;
        postMenuFunc = postFunc;
        processMenuFunc = eventFunc;
        encoderPos = pos;
        max_encoder_acceleration = accel;
        flags = fl;
    }
};

typedef const menu_t & (*menuItemCallback_t) (uint8_t nr, menu_t &opt);
typedef void (*menuDrawCallback_t) (uint8_t nr, uint8_t &flags);
typedef void (*scrollDrawCallback_t) (uint8_t nr, uint8_t offsetY, uint8_t flags);

class LCDMenu
{
public:
    // constructor
    LCDMenu() : selectedSubmenu(-1) {}

    const uint8_t encoder_acceleration_factor() const
    {
        return (activeSubmenu.processMenuFunc ? activeSubmenu.max_encoder_acceleration : 0);
    }

    void process_submenu(menuItemCallback_t getMenuItem, uint8_t len);
    void reset_submenu();
    void set_selection(int8_t index);
    void set_active(menuItemCallback_t getMenuItem, int8_t index);
    void drawSubMenu(menuDrawCallback_t drawFunc, uint8_t nr, uint8_t &flags);
    void drawSubMenu(menuDrawCallback_t drawFunc, uint8_t nr);
    FORCE_INLINE bool isSubmenuSelected() const { return (selectedSubmenu >= 0); }
    FORCE_INLINE bool isSubmenuActive() { return ((selectedSubmenu >= 0) && activeSubmenu.processMenuFunc); }
    FORCE_INLINE bool isSelected(uint8_t nr) { return (selectedSubmenu == nr); }
    FORCE_INLINE bool isActive(uint8_t nr) { return ((selectedSubmenu == nr) && activeSubmenu.processMenuFunc); }

    // standard drawing functions (for convenience)
    static void drawMenuBox(uint8_t left, uint8_t top, uint8_t width, uint8_t height, uint8_t flags);
    static void drawMenuString(uint8_t left, uint8_t top, uint8_t width, uint8_t height, const char * str, uint8_t textAlign, uint8_t flags);
    static void drawMenuString_P(uint8_t left, uint8_t top, uint8_t width, uint8_t height, const char * str, uint8_t textAlign, uint8_t flags);
//    static void reset_selection();

private:
    // submenu item
    menu_t activeSubmenu;
    int8_t selectedSubmenu;

};

extern LCDMenu menu;

bool lcd_tune_value(int8_t &value, int8_t _min, int8_t _max);
bool lcd_tune_value(float &value, float _min, float _max, float _step);

#endif
