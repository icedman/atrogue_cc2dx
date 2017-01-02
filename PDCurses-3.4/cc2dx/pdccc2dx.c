#include "pdccc2dx.h"

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CHTYPE_LONG

# define A(x) ((chtype)x | A_ALTCHARSET)

chtype acs_map[128] =
{
    A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8), A(9),
    A(10), A(11), A(12), A(13), A(14), A(15), A(16), A(17), A(18),
    A(19), A(20), A(21), A(22), A(23), A(24), A(25), A(26), A(27),
    A(28), A(29), A(30), A(31), ' ', '!', '"', '#', '$', '%', '&',
    '\'', '(', ')', '*',
    
# ifdef PDC_WIDE
    0x2192, 0x2190, 0x2191, 0x2193,
# else
    A(0x1a), A(0x1b), A(0x18), A(0x19),
# endif
    
    '/',
    
# ifdef PDC_WIDE
    0x2588,
# else
    0xdb,
# endif
    
    '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=',
    '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    
# ifdef PDC_WIDE
    0x2666, 0x2592,
# else
    A(0x04), 0xb1,
# endif
    
    'b', 'c', 'd', 'e',
    
# ifdef PDC_WIDE
    0x00b0, 0x00b1, 0x2591, 0x00a4, 0x2518, 0x2510, 0x250c, 0x2514,
    0x253c, 0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524,
    0x2534, 0x252c, 0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3,
    0x00b7,
# else
    0xf8, 0xf1, 0xb0, A(0x0f), 0xd9, 0xbf, 0xda, 0xc0, 0xc5, 0x2d,
    0x2d, 0xc4, 0x2d, 0x5f, 0xc3, 0xb4, 0xc1, 0xc2, 0xb3, 0xf3,
    0xf2, 0xe3, 0xd8, 0x9c, 0xf9,
# endif
    
    A(127)
};

# undef A

#endif

int ESCDELAY = 0;
unsigned long pdc_key_modifiers = 0;
#define PDC_rows 25
#define PDC_cols 80

int pdc_last_key = -1;
int pdc_update_count = 1;
#define SET_SCREEN_DIRTY() pdc_update_count = 1;

char screenData[PDC_cols*PDC_rows];

char *getScreenData()
{
    return screenData;
}

void pushKey(int k) {
    pdc_last_key = k;
}

bool isScreenDirty()
{
    int i = pdc_update_count;
    if (pdc_update_count > 0)
        pdc_update_count--;
    return i > 0;
}

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp);

void PDC_beep(void)
{
    fprintf(stdout, "void PDC_beep(void)\n");
}

bool PDC_can_change_color(void)
{
    fprintf(stdout, "bool PDC_can_change_color(void)\n");
    return 0;
}

bool PDC_check_key(void)
{
    //    fprintf(stdout, "bool PDC_check_key(void)\n");
    return (pdc_last_key != -1);
}

int PDC_color_content(short p1, short * p2, short * p3, short * p4)
{
    fprintf(stdout, "int PDC_color_content(short, short *, short *, short *)\n");
    return 0;
}

int PDC_curs_set(int p1)
{
    fprintf(stdout, "int PDC_curs_set(int)\n");
    return 0;
}

void PDC_flushinp(void)
{
    fprintf(stdout, "void PDC_flushinp(void)\n");
    
}

int PDC_get_columns(void)
{
    //    fprintf(stdout, "int PDC_get_columns(void)\n");
    return PDC_cols;
}

int PDC_get_cursor_mode(void)
{
    fprintf(stdout, "int PDC_get_cursor_mode(void)\n");
    return 0;
}

int PDC_get_key(void)
{
    //    fprintf(stdout, "int PDC_get_key(void)\n");
    SP->key_code = 0;
    if (pdc_last_key != -1) {
        int k = pdc_last_key;
        pdc_last_key = -1;
        return k;
    }
    
    return -1;
}

int PDC_get_rows(void)
{
    //    fprintf(stdout, "int PDC_get_rows(void)\n");
    return PDC_rows;
}

void PDC_gotoyx(int row, int col)
{
    //    fprintf(stdout, "void PDC_gotoyx(int, int)\n");
    
    chtype ch;
    int oldrow, oldcol;
    
    SET_SCREEN_DIRTY()
    
    oldrow = SP->cursrow;
    oldcol = SP->curscol;
    
    //    screenData[(oldrow*SP->cols) + oldcol] = 0;
    PDC_transform_line(oldrow, oldcol, 1, curscr->_y[oldrow] + oldcol);
    
    if (!SP->visibility)
        return;
    
    /* draw a new cursor by overprinting the existing character in
     reverse, either the full cell (when visibility == 2) or the
     lowest quarter of it (when visibility == 1) */
    
    ch = curscr->_y[row][col] ^ A_REVERSE;
#ifdef CHTYPE_LONG
    if (ch & A_ALTCHARSET && !(ch & 0xff80))
        ch = acs_map[ch & 0x7f];
#endif
    
    screenData[(row*SP->cols) + col] = ch; //(ch & 0xff);
    
    // _set_attr(ch);
    
    if (oldrow != row || oldcol != col) {
        
    }
}

int PDC_init_color(short p1, short p2, short p3, short p4)
{
    fprintf(stdout, "int PDC_init_color(short, short, short, short)\n");
    return 0;
}

void PDC_init_pair(short p1, short p2, short p3)
{
//    fprintf(stdout, "void PDC_init_pair(short, short, short)\n");
    
}

int PDC_modifiers_set(void)
{
    fprintf(stdout, "int PDC_modifiers_set(void)\n");
    return 0;
}

int PDC_mouse_set(void)
{
    fprintf(stdout, "int PDC_mouse_set(void)\n");
    return 0;
}

void PDC_napms(int p1)
{
    //    fprintf(stdout, "void PDC_napms(int)\n");
    usleep(p1 * 50);
    
}

int PDC_pair_content(short p1, short * p2, short * p3)
{
    fprintf(stdout, "int PDC_pair_content(short, short *, short *)\n");
    return 0;
}

void PDC_reset_prog_mode(void)
{
    fprintf(stdout, "void PDC_reset_prog_mode(void)\n");
    
}

void PDC_reset_shell_mode(void)
{
    fprintf(stdout, "void PDC_reset_shell_mode(void)\n");
    
}

int PDC_resize_screen(int p1, int p2)
{
    fprintf(stdout, "int PDC_resize_screen(int, int)\n");
    return 0;
}

void PDC_restore_screen_mode(int p1)
{
    fprintf(stdout, "void PDC_restore_screen_mode(int)\n");
    
}

void PDC_save_screen_mode(int p1)
{
    fprintf(stdout, "void PDC_save_screen_mode(int)\n");
    
}

void PDC_scr_close(void)
{
    fprintf(stdout, "void PDC_scr_close(void)\n");
    
}

void PDC_scr_free(void)
{
    fprintf(stdout, "void PDC_scr_free(void)\n");
    
}

int PDC_scr_open(int p1, char ** p2)
{
    //    fprintf(stdout, "int PDC_scr_open(int, char **)\n");
    SP = calloc(1, sizeof(SCREEN));
    
    if (!SP)
        return ERR;
    
    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();
    memset(screenData,0,sizeof(screenData));
    
    SET_SCREEN_DIRTY()
    
    return OK;
}

int PDC_set_blink(bool p1)
{
    fprintf(stdout, "int PDC_set_blink(bool)\n");
    return 0;
}

void PDC_set_keyboard_binary(bool p1)
{
    fprintf(stdout, "void PDC_set_keyboard_binary(bool)\n");
    
}

const char *PDC_sysname(void)
{
    fprintf(stdout, "const char *PDC_sysname(void)\n");
    return "";
}

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    //    fprintf(stdout, "void PDC_transform_line(int, int, int, const chtype *)\n");
    SET_SCREEN_DIRTY()
    
    int j;
    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];
#ifdef CHTYPE_LONG
        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];
#endif
        // _set_attr(ch);
        screenData[(lineno * SP->cols) + x + j] = ch;
        
    }
    
}

