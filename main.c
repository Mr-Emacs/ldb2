#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <windows.h>
#include <commdlg.h>
#include <GL/gl.h>

#define RGFW_IMPLEMENTATION
#define RGFW_OPENGL
#include "RGFW.h"

#define QUI_IMPLEMENTATION
#include "quickui.h"

#include "types.h"

#define FONT_BASE       3000
#define FONT_BASE_UI    4000

#define COLOR_BG_DARK_R       0x12
#define COLOR_BG_DARK_G       0x12
#define COLOR_BG_DARK_B       0x18

#define COLOR_BG_PANEL_R      0x1E
#define COLOR_BG_PANEL_G      0x1E
#define COLOR_BG_PANEL_B      0x2C

#define COLOR_BG_TOOLBAR_R    0x1A
#define COLOR_BG_TOOLBAR_G    0x1A
#define COLOR_BG_TOOLBAR_B    0x26

#define COLOR_BG_INPUT_R      0x14
#define COLOR_BG_INPUT_G      0x14
#define COLOR_BG_INPUT_B      0x1E

#define COLOR_BG_HOVER_R      0x2A
#define COLOR_BG_HOVER_G      0x2A
#define COLOR_BG_HOVER_B      0x36

#define COLOR_BG_ACTIVE_R     0x0A
#define COLOR_BG_ACTIVE_G     0x12
#define COLOR_BG_ACTIVE_B     0x1A

#define COLOR_TEXT_R          0xDC
#define COLOR_TEXT_G          0xDC
#define COLOR_TEXT_B          0xDC

#define COLOR_TEXT_DIM_R      0x88
#define COLOR_TEXT_DIM_G      0x88
#define COLOR_TEXT_DIM_B      0x88

#define COLOR_TEXT_BRIGHT_R   0xFF
#define COLOR_TEXT_BRIGHT_G   0xFF
#define COLOR_TEXT_BRIGHT_B   0xFF

#define COLOR_BORDER_R        0x32
#define COLOR_BORDER_G        0x32
#define COLOR_BORDER_B        0x48

#define COLOR_ACCENT_R        0x1C
#define COLOR_ACCENT_G        0x6E
#define COLOR_ACCENT_B        0xB8

#define COLOR_ACCENT_HOVER_R  0x2A
#define COLOR_ACCENT_HOVER_G  0x7A
#define COLOR_ACCENT_HOVER_B  0xC8

#define COLOR_SUCCESS_R       0x6A
#define COLOR_SUCCESS_G       0x99
#define COLOR_SUCCESS_B       0x5E

#define COLOR_WARNING_R       0xD9
#define COLOR_WARNING_G       0x5B
#define COLOR_WARNING_B       0x4A

#define COLOR_INFO_R          0x37
#define COLOR_INFO_G          0x9B
#define COLOR_INFO_B          0xD6

#define COLOR_BREAKPOINT_R    0xF4
#define COLOR_BREAKPOINT_G    0x87
#define COLOR_BREAKPOINT_B    0x71

#define COLOR_CURRENT_LINE_R  0x26
#define COLOR_CURRENT_LINE_G  0x4F
#define COLOR_CURRENT_LINE_B  0x78

#define SPLITTER_SIZE 4
#define SPLITTER_HOT_SIZE 8

static i32  s_fsz    = 22;
static i32  s_fw     = 0;
static i32  s_fh     = 0;
static i32  s_font_ready = 0;
static i32  s_ui_fw  = 7;
static i32  s_ui_fh  = 13;
static HDC  s_hdc    = NULL;
static HWND s_hwnd   = NULL;

static i32  s_refresh_pending = 0;
Debugger s_dbg = {0};
i32      s_dbg_live = 0;

static char s_cmd[512] = {0};
static char s_watch_new[128] = {0};
static char s_cli_args[256] = {0};

/* Text editor state */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    i32 cursor_pos;
    i32 selection_start;
    i32 selection_end;
    i32 has_selection;
} TextEditor;

static TextEditor s_cmd_editor = {
    .buffer = s_cmd,
    .capacity = 512,
    .length = 0,
    .cursor_pos = 0,
    .selection_start = -1,
    .selection_end = -1,
    .has_selection = 0
};

static TextEditor s_watch_editor = {
    .buffer = s_watch_new,
    .capacity = 128,
    .length = 0,
    .cursor_pos = 0,
    .selection_start = -1,
    .selection_end = -1,
    .has_selection = 0
};

static TextEditor s_args_editor = {
    .buffer = s_cli_args,
    .capacity = 256,
    .length = 0,
    .cursor_pos = 0,
    .selection_start = -1,
    .selection_end = -1,
    .has_selection = 0
};

/* Watch expression editing */
static i32 s_editing_watch_index = -1;
static char s_edit_watch_buffer[128] = {0};
static TextEditor s_edit_watch_editor = {
    .buffer = s_edit_watch_buffer,
    .capacity = 128,
    .length = 0,
    .cursor_pos = 0,
    .selection_start = -1,
    .selection_end = -1,
    .has_selection = 0
};

/* Tree view node for complex data structures */
typedef struct TreeNode {
    char name[128];
    char type[64];
    char value[256];
    i32 is_expanded;
    i32 child_count;
    struct TreeNode **children;
    i32 is_loaded;
    i32 depth;
} TreeNode;

/* Watch expression with tree support */
typedef struct {
    char expr[128];
    char value[256];
    TreeNode *root;
    i32 is_expanded;
    i32 is_complex;
    i32 pending_eval;
} WatchExpr;

/* Watch expressions array */
#define WATCH_MAX 16
static WatchExpr s_watches[WATCH_MAX];
static i32  s_wn   = 0;
static i32  s_wsc  = 0;
static i32  s_wpending = -1;
static i32  s_wqueue[WATCH_MAX];
static i32  s_wqlen = 0;

/* Thread info */
static ThreadInfo s_threads[32];
static i32 s_thread_count = 0;
static i32 s_thread_scroll = 0;
static u32 s_current_thread = 0;

/* Breakpoint info */
static BreakpointInfo s_breakpoints[128];
static i32 s_bp_count = 0;
static i32 s_bp_scroll = 0;

/* Register info */
typedef struct {
    char name[16];
    char value[64];
    i32 is_changed;
} RegisterInfo;

static RegisterInfo s_registers[64];
static i32 s_register_count = 0;
static i32 s_register_scroll = 0;

/* Splitter state */
typedef struct {
    i32 is_dragging;
    i32 splitter_index;
    i32 start_x;
    i32 start_y;
    i32 start_value1;
    i32 start_value2;
} SplitterState;

static SplitterState s_splitter = {0};
static i32 s_mouse_held = 0;

static i32 s_left_width       = 60;
static i32 s_source_height    = 65;
static i32 s_top_right_height = 25;
static i32 s_mid_right_height = 35;

static i32 s_threads_pct      = 33;
static i32 s_bp_pct           = 33;
static i32 s_locals_pct       = 50;

static void editor_init(TextEditor *ed, char *buffer, size_t capacity)
{
    ed->buffer = buffer;
    ed->capacity = capacity;
    ed->length = strlen(buffer);
    ed->cursor_pos = (i32)ed->length;
    ed->selection_start = -1;
    ed->selection_end = -1;
    ed->has_selection = 0;
}

static void editor_insert_char(TextEditor *ed, char c)
{
    if (ed->length >= ed->capacity - 1) return;

    if (ed->has_selection) {
        i32 start = ed->selection_start < ed->selection_end ? ed->selection_start : ed->selection_end;
        i32 end = ed->selection_start > ed->selection_end ? ed->selection_start : ed->selection_end;
        memmove(&ed->buffer[start], &ed->buffer[end], ed->length - end + 1);
        ed->length -= (end - start);
        ed->cursor_pos = start;
        ed->has_selection = 0;
    }

    memmove(&ed->buffer[ed->cursor_pos + 1], &ed->buffer[ed->cursor_pos], ed->length - ed->cursor_pos + 1);
    ed->buffer[ed->cursor_pos] = c;
    ed->length++;
    ed->cursor_pos++;
    ed->buffer[ed->length] = '\0';
}

static void editor_delete_char(TextEditor *ed)
{
    if (ed->has_selection) {
        i32 start = ed->selection_start < ed->selection_end ? ed->selection_start : ed->selection_end;
        i32 end = ed->selection_start > ed->selection_end ? ed->selection_start : ed->selection_end;
        memmove(&ed->buffer[start], &ed->buffer[end], ed->length - end + 1);
        ed->length -= (end - start);
        ed->cursor_pos = start;
        ed->has_selection = 0;
    } else if (ed->cursor_pos > 0) {
        memmove(&ed->buffer[ed->cursor_pos - 1], &ed->buffer[ed->cursor_pos], ed->length - ed->cursor_pos + 1);
        ed->length--;
        ed->cursor_pos--;
    }
    ed->buffer[ed->length] = '\0';
}

static void editor_delete_forward(TextEditor *ed)
{
    if (ed->has_selection) {
        editor_delete_char(ed);
    } else if (ed->cursor_pos < (i32)ed->length) {
        memmove(&ed->buffer[ed->cursor_pos], &ed->buffer[ed->cursor_pos + 1], ed->length - ed->cursor_pos);
        ed->length--;
        ed->buffer[ed->length] = '\0';
    }
}

static void editor_move_left(TextEditor *ed, i32 shift_pressed)
{
    if (ed->cursor_pos > 0) {
        if (shift_pressed) {
            if (!ed->has_selection) {
                ed->selection_start = ed->cursor_pos;
                ed->selection_end = ed->cursor_pos;
                ed->has_selection = 1;
            }
            ed->cursor_pos--;
            ed->selection_end = ed->cursor_pos;
        } else {
            ed->cursor_pos--;
            ed->has_selection = 0;
        }
    }
}

static void editor_move_right(TextEditor *ed, i32 shift_pressed)
{
    if (ed->cursor_pos < (i32)ed->length) {
        if (shift_pressed) {
            if (!ed->has_selection) {
                ed->selection_start = ed->cursor_pos;
                ed->selection_end = ed->cursor_pos;
                ed->has_selection = 1;
            }
            ed->cursor_pos++;
            ed->selection_end = ed->cursor_pos;
        } else {
            ed->cursor_pos++;
            ed->has_selection = 0;
        }
    }
}

static void editor_move_home(TextEditor *ed, i32 shift_pressed)
{
    if (shift_pressed) {
        if (!ed->has_selection) {
            ed->selection_start = ed->cursor_pos;
            ed->selection_end = ed->cursor_pos;
            ed->has_selection = 1;
        }
        ed->cursor_pos = 0;
        ed->selection_end = 0;
    } else {
        ed->cursor_pos = 0;
        ed->has_selection = 0;
    }
}

static void editor_move_end(TextEditor *ed, i32 shift_pressed)
{
    if (shift_pressed) {
        if (!ed->has_selection) {
            ed->selection_start = ed->cursor_pos;
            ed->selection_end = ed->cursor_pos;
            ed->has_selection = 1;
        }
        ed->cursor_pos = (i32)ed->length;
        ed->selection_end = (i32)ed->length;
    } else {
        ed->cursor_pos = (i32)ed->length;
        ed->has_selection = 0;
    }
}

static void editor_set_cursor(TextEditor *ed, i32 x, i32 y, i32 char_width, i32 start_x)
{
    (void)y;
    i32 char_index = (x - start_x) / char_width;
    if (char_index < 0) char_index = 0;
    if (char_index > (i32)ed->length) char_index = (i32)ed->length;
    ed->cursor_pos = char_index;
    ed->has_selection = 0;
}

static void editor_select_to_cursor(TextEditor *ed, i32 x, i32 y, i32 char_width, i32 start_x)
{
    (void)y;
    i32 char_index = (x - start_x) / char_width;
    if (char_index < 0) char_index = 0;
    if (char_index > (i32)ed->length) char_index = (i32)ed->length;

    if (!ed->has_selection) {
        ed->selection_start = ed->cursor_pos;
        ed->has_selection = 1;
    }
    ed->cursor_pos = char_index;
    ed->selection_end = ed->cursor_pos;
}

static void watch_send_next(void);

static void parse_lldb_value(const char *output, WatchExpr *watch)
{
    if (!output || !watch) return;

    strncpy(watch->value, output, 255);
    watch->value[255] = '\0';

    const char *ob = strchr(output, '{');
    if (ob && (strchr(ob, '}') || strstr(ob, "..."))) {
        watch->is_complex = 1;

        if (watch->root) {
            if (watch->root->children) {
                for (i32 ci = 0; ci < watch->root->child_count; ci++)
                    if (watch->root->children[ci]) free(watch->root->children[ci]);
                free(watch->root->children);
            }
            free(watch->root);
        }

        watch->root = calloc(1, sizeof(TreeNode));
        strncpy(watch->root->name,  watch->expr,  127);
        strncpy(watch->root->value, output,        255);
        watch->root->depth = 0;

        watch->root->children    = calloc(32, sizeof(TreeNode *));
        watch->root->child_count = 0;
        watch->root->is_loaded   = 1;

        const char *p = ob + 1;
        while (*p && *p != '}' && watch->root->child_count < 32) {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == '}') break;

            char fname[128] = {0};
            i32 fi = 0;
            while (*p && *p != '=' && *p != '}' && fi < 127) {
                if (*p != ' ') fname[fi++] = *p;
                else if (fi > 0) fname[fi++] = *p;
                p++;
            }

            while (fi > 0 && fname[fi-1] == ' ') fi--;
            fname[fi] = '\0';

            if (*p != '=') break;
            p++; /* skip '=' */
            while (*p == ' ') p++;

            char fval[256] = {0};
            i32 vi = 0, depth = 0;
            while (*p && vi < 255) {
                if (*p == '{') depth++;
                if (*p == '}') {
                    if (depth == 0) break;
                    depth--;
                }
                if (*p == ',' && depth == 0) { p++; break; }
                fval[vi++] = *p++;
            }

            while (vi > 0 && fval[vi-1] == ' ') vi--;
            fval[vi] = '\0';

            if (fname[0]) {
                TreeNode *child = calloc(1, sizeof(TreeNode));
                strncpy(child->name,  fname, 127);
                strncpy(child->value, fval,  255);
                child->depth = 1;
                watch->root->children[watch->root->child_count++] = child;
            }
        }
    } else {
        watch->is_complex = 0;
        if (watch->root) {
            if (watch->root->children) {
                for (i32 ci = 0; ci < watch->root->child_count; ci++)
                    if (watch->root->children[ci]) free(watch->root->children[ci]);
                free(watch->root->children);
            }
            free(watch->root);
            watch->root = NULL;
        }
    }
}

static void watch_add(const char *expr)
{
    if (!expr || !expr[0] || s_wn >= WATCH_MAX) return;

    for (i32 i = 0; i < s_wn; i++) {
        if (strcmp(s_watches[i].expr, expr) == 0) return;
    }

    strncpy(s_watches[s_wn].expr, expr, 127);
    strncpy(s_watches[s_wn].value, "(not yet evaluated)", 255);
    s_watches[s_wn].is_complex = 0;
    s_watches[s_wn].is_expanded = 0;
    s_watches[s_wn].root = NULL;
    s_watches[s_wn].pending_eval = 0;
    s_wn++;

    if (s_dbg_live) {
        s_wqueue[s_wqlen++] = s_wn - 1;
        if (s_wqlen == 1) watch_send_next();
    }
}

static void watch_update(i32 idx, const char *expr)
{
    if (idx < 0 || idx >= s_wn) return;
    strncpy(s_watches[idx].expr, expr, 127);
    strncpy(s_watches[idx].value, "(not yet evaluated)", 255);
    s_watches[idx].is_complex = 0;
    s_watches[idx].is_expanded = 0;
    if (s_watches[idx].root) {
        free(s_watches[idx].root);
        s_watches[idx].root = NULL;
    }

    if (s_dbg_live) {
        s_wqueue[s_wqlen++] = idx;
        if (s_wqlen == 1) watch_send_next();
    }
}

static void watch_remove(i32 idx)
{
    if (idx < 0 || idx >= s_wn) return;

    if (s_watches[idx].root) {
        free(s_watches[idx].root);
    }

    memmove(&s_watches[idx], &s_watches[idx + 1], (size_t)(s_wn - idx - 1) * sizeof(WatchExpr));
    s_wn--;

    for (i32 i = 0; i < s_wqlen; i++) {
        if (s_wqueue[i] == idx) {
            memmove(&s_wqueue[i], &s_wqueue[i + 1], (size_t)(s_wqlen - i - 1) * sizeof(i32));
            s_wqlen--;
            i--;
        } else if (s_wqueue[i] > idx) {
            s_wqueue[i]--;
        }
    }
}

static void watch_queue_all(void)
{
    s_wqlen = 0;
    for (i32 i = 0; i < s_wn; i++)
        s_wqueue[s_wqlen++] = i;
    if (s_wqlen > 0) watch_send_next();
}

static void watch_send_next(void)
{
    if (s_wqlen == 0) {
        s_wpending = -1;
        return;
    }
    s_wpending = s_wqueue[0];
    memmove(s_wqueue, s_wqueue + 1, (size_t)(s_wqlen - 1) * sizeof(i32));
    s_wqlen--;

    char cmd[150];
    snprintf(cmd, sizeof(cmd), "p %s\n", s_watches[s_wpending].expr);
    if (s_dbg_live) send_to_dbg(&s_dbg, cmd);
}

/* ═══════════════════════════════════════════════════════════════
 *  FONT MANAGEMENT
 * ═══════════════════════════════════════════════════════════════ */
static void font_build(i32 sz)
{
    if (sz < 8)  sz = 8;
    if (sz > 36) sz = 36;
    if (s_font_ready) glDeleteLists(FONT_BASE, 256);

    HFONT hf = CreateFontA(-sz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    if (!hf) hf = CreateFontA(-sz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");

    HFONT old = (HFONT)SelectObject(s_hdc, hf);
    wglUseFontBitmapsA(s_hdc, 0, 256, FONT_BASE);
    TEXTMETRICA tm; GetTextMetricsA(s_hdc, &tm);
    /* wglUseFontBitmapsA calls GetCharWidth internally for each glyph
       to determine the advance.  Measure the same way so cursor x
       matches rendered glyph positions exactly. */
    INT char_adv = 0;
    if (!GetCharWidth32A(s_hdc, (UINT)'M', (UINT)'M', &char_adv) || char_adv <= 0)
        char_adv = tm.tmAveCharWidth;
    s_fw = char_adv;
    s_fh = tm.tmHeight;
    s_fsz = sz;
    SelectObject(s_hdc, old);
    DeleteObject(hf);
    s_font_ready = 1;
}

static void font_build_ui(void)
{
    glDeleteLists(FONT_BASE_UI, 256);
    HFONT hf = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Segoe UI");

    if (!hf) hf = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Arial");

    HFONT old = (HFONT)SelectObject(s_hdc, hf);
    wglUseFontBitmapsA(s_hdc, 0, 256, FONT_BASE_UI);
    TEXTMETRICA tm; GetTextMetricsA(s_hdc, &tm);
    s_ui_fw = tm.tmAveCharWidth;
    s_ui_fh = tm.tmHeight;
    SelectObject(s_hdc, old);
    DeleteObject(hf);
}

static void text_at(const char *s, f32 x, f32 y, u8 r, u8 g, u8 b)
{
    if (!s || !*s) return;
    glColor3ub(r, g, b);
    glRasterPos2f(x, y + s_fh);
    glListBase(FONT_BASE);
    glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
}

static void text_at_len(const char *s, i32 len, f32 x, f32 y, u8 r, u8 g, u8 b)
{
    if (!s || len <= 0) return;
    glColor3ub(r, g, b);
    glRasterPos2f(x, y + s_fh);
    glListBase(FONT_BASE);
    glCallLists((GLsizei)len, GL_UNSIGNED_BYTE, s);
}

static void text_ui(const char *s, f32 x, f32 y, u8 r, u8 g, u8 b)
{
    if (!s || !*s) return;
    glColor3ub(r, g, b);
    glRasterPos2f(x, y + s_ui_fh);
    glListBase(FONT_BASE_UI);
    glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
}

static void rect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a)
{
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void rect_border(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b)
{
    glColor3ub(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void line_hline(f32 x0, f32 x1, f32 y, u8 r, u8 g, u8 b)
{
    glColor3ub(r, g, b);
    glBegin(GL_LINES);
    glVertex2f(x0, y);
    glVertex2f(x1, y);
    glEnd();
}

static void line_vline(f32 x, f32 y0, f32 y1, u8 r, u8 g, u8 b)
{
    glColor3ub(r, g, b);
    glBegin(GL_LINES);
    glVertex2f(x, y0);
    glVertex2f(x, y1);
    glEnd();
}

static void scissor(i32 x, i32 y, i32 w, i32 h, i32 wh)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, wh - y - h, w, h);
}

static void unscissor(void)
{
    glDisable(GL_SCISSOR_TEST);
}

/* ═══════════════════════════════════════════════════════════════
 *  SPLITTER HANDLING
 * ═══════════════════════════════════════════════════════════════ */
static i32 check_splitter_hit(i32 x, i32 y, i32 splitter_x, i32 splitter_y, i32 is_vertical)
{
    if (is_vertical) {
        return (abs(x - splitter_x) < SPLITTER_HOT_SIZE &&
                y >= splitter_y && y <= splitter_y + 1000); /* Rough height estimate*/
    } else {
        return (abs(y - splitter_y) < SPLITTER_HOT_SIZE &&
                x >= splitter_x && x <= splitter_x + 1000); /* Rough width estimate*/
    }
}

static void draw_splitter(i32 x, i32 y, i32 w, i32 h, i32 is_hot)
{
    rect((f32)x, (f32)y, (f32)w, (f32)h,
         is_hot ? COLOR_ACCENT_R : COLOR_BORDER_R,
         is_hot ? COLOR_ACCENT_G : COLOR_BORDER_G,
         is_hot ? COLOR_ACCENT_B : COLOR_BORDER_B,
         is_hot ? 255 : 180);
}

static void draw_panel(i32 px, i32 py, i32 pw, i32 ph, const char *title)
{
    (void)ph;

    rect((f32)(px + 2), (f32)(py + 2), (f32)pw, (f32)ph, 0, 0, 0, 40);

    rect((f32)px, (f32)py, (f32)pw, (f32)ph,
         COLOR_BG_PANEL_R, COLOR_BG_PANEL_G, COLOR_BG_PANEL_B, 255);

    rect_border((f32)px, (f32)py, (f32)pw, (f32)ph,
                COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    i32 title_h = s_ui_fh + 16;
    rect((f32)px, (f32)py, (f32)pw, (f32)title_h,
         COLOR_BG_TOOLBAR_R, COLOR_BG_TOOLBAR_G, COLOR_BG_TOOLBAR_B, 255);

    text_ui(title, (f32)(px + 8), (f32)(py + (title_h - s_ui_fh) / 2),
            COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);

    line_hline((f32)px, (f32)(px + pw), (f32)(py + title_h),
               COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
}

static void draw_scrollbar(i32 px, i32 py, i32 ph, i32 total, i32 vis, i32 sc)
{
    if (total <= vis || vis <= 0) return;

    f32 fph = (f32)ph;
    f32 ftot = (f32)total;
    f32 bh = fph * ((f32)vis / ftot);
    if (bh < 20) bh = 20;

    f32 by = (f32)py + fph * ((f32)sc / ftot);

    rect((f32)(px - 8), (f32)py, 4, (f32)ph, 0x2A, 0x2A, 0x2A, 255);
    rect((f32)(px - 8), by, 4, bh,
         COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 200);
}

static void cb_rect(qui_Context *ctx, f32 x, f32 y, f32 w, f32 h, qui_Color c)
{
    (void)ctx;
    rect(x, y, w, h, c.r, c.g, c.b, c.a);
}

static void cb_text(qui_Context *ctx, const char *s, f32 x, f32 y)
{
    (void)ctx;
    text_ui(s, x, y, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
}

static f32 cb_tw(qui_Context *ctx, const char *s)
{
    (void)ctx;
    return s ? (f32)((i32)strlen(s) * s_ui_fw) : 0;
}

static f32 cb_th(qui_Context *ctx, const char *s)
{
    (void)ctx;
    (void)s;
    return (f32)s_ui_fh;
}

#define INPUT_CMD    0
#define INPUT_WATCH  1
#define INPUT_ARGS   2
static i32  s_active_input = INPUT_CMD;

static TextEditor *get_active_editor(void);
static void clipboard_paste(void);
static void clipboard_copy(void);
static void editor_select_all(TextEditor *ed);
static i32  s_shift_pressed = 0;

static void input_char(char c)
{
    editor_insert_char(get_active_editor(), c);
}

static void input_backspace(void)
{
    editor_delete_char(get_active_editor());
}

static void input_delete(void)
{
    editor_delete_forward(get_active_editor());
}

static void input_left(i32 shift)
{
    editor_move_left(get_active_editor(), shift);
}

static void input_right(i32 shift)
{
    editor_move_right(get_active_editor(), shift);
}

static void input_home(i32 shift)
{
    editor_move_home(get_active_editor(), shift);
}

static void input_end(i32 shift)
{
    editor_move_end(get_active_editor(), shift);
}

#define LOG_CAP (1024 * 256)
static char   s_log[LOG_CAP];
static size_t s_log_len   = 0;
static i32    s_log_sc    = 0;

static void log_push(const char *d, size_t n)
{
    if (!d || !n) return;
    if (s_log_len + n >= (size_t)(LOG_CAP - 1)) {
        size_t h = LOG_CAP / 2;
        memmove(s_log, s_log + h, s_log_len - h);
        s_log_len -= h;
    }
    size_t room = (size_t)(LOG_CAP - 1) - s_log_len;
    if (n > room) n = room;
    memcpy(s_log + s_log_len, d, n);
    s_log_len += n;
    s_log[s_log_len] = '\0';
}

#define MAX_LOG_LINES 8192
static void draw_log(i32 px, i32 py, i32 pw, i32 ph, i32 wh)
{
    (void)wh;

    draw_panel(px, py, pw, ph, "OUTPUT");

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 3;
    i32 mv = (ch > 0) ? ch / lh : 1;

    static const char *ls[MAX_LOG_LINES];
    static i32 ll[MAX_LOG_LINES];
    i32 n = 0;
    ls[0] = s_log;
    const char *p = s_log;
    const char *e = s_log + s_log_len;

    while (p < e && n < MAX_LOG_LINES - 1) {
        if (*p == '\n') {
            ll[n] = (i32)(p - ls[n]);
            n++;
            ls[n] = p + 1;
        }
        p++;
    }
    ll[n] = (i32)(e - ls[n]);
    if (ll[n] > 0) n++;

    i32 msc = n - mv;
    if (msc < 0) msc = 0;
    if (s_log_sc > msc) s_log_sc = msc;
    if (s_log_sc < 0) s_log_sc = 0;

    i32 first = n - mv - s_log_sc;
    if (first < 0) first = 0;
    i32 last = first + mv;
    if (last > n) last = n;

    scissor(px + 1, cy, pw - 2, ch, wh);

    char buf[2048];
    for (i32 i = first; i < last; i++) {
        i32 l = ll[i];
        if (l <= 0) continue;
        if (l > (i32)sizeof(buf) - 1) l = (i32)sizeof(buf) - 1;

        memcpy(buf, ls[i], (size_t)l);
        buf[l] = '\0';

        u8 r = COLOR_TEXT_R, g = COLOR_TEXT_G, b = COLOR_TEXT_B;
        if (buf[0] == '>' && buf[1] == ' ') {
            r = COLOR_INFO_R;
            g = COLOR_INFO_G;
            b = COLOR_INFO_B;
        } else if (strstr(buf, "error:") || strstr(buf, "Error:")) {
            r = COLOR_WARNING_R;
            g = COLOR_WARNING_G;
            b = COLOR_WARNING_B;
        } else {
            r = COLOR_TEXT_R;
            g = COLOR_TEXT_G;
            b = COLOR_TEXT_B;
        }

        text_at(buf, (f32)(px + 8), (f32)(cy + (i - first) * lh + 2), r, g, b);
    }

    unscissor();
    draw_scrollbar(px + pw, cy, ch, n, mv, n - mv - s_log_sc);
}

/* ═══════════════════════════════════════════════════════════════
 *  SOURCE PANEL
 * ═══════════════════════════════════════════════════════════════ */
#define SRC_MAX  8192
#define SRC_W    512
static char  s_src_path[MAX_PATH] = {0};
static char  s_src[SRC_MAX][SRC_W];
static i32   s_src_n    = 0;
static i32   s_cur_line = 0;
static i32   s_src_sc   = 0;

#define BP_MAX 128
static i32 s_bp[BP_MAX], s_bp_n = 0;
static i32 bp_has(i32 ln) {
    for (i32 i = 0; i < s_bp_n; i++)
        if (s_bp[i] == ln) return 1;
    return 0;
}

static void bp_toggle(i32 ln)
{
    for (i32 i = 0; i < s_bp_n; i++) {
        if (s_bp[i] == ln) {
            s_bp[i] = s_bp[--s_bp_n];
            /* Remove from the panel's breakpoints array too */
            for (i32 j = 0; j < s_bp_count; j++) {
                if ((i32)s_breakpoints[j].line == ln) {
                    s_breakpoints[j] = s_breakpoints[--s_bp_count];
                    break;
                }
            }
            return;
        }
    }
    if (s_bp_n < BP_MAX) {
        s_bp[s_bp_n++] = ln;
        /* Also register in the panel's breakpoints array */
        if (s_bp_count < 128) {
            /* Use next sequential id, or 1-based count */
            s_breakpoints[s_bp_count].bp_id = (u32)(s_bp_count + 1);
            s_breakpoints[s_bp_count].line  = (u32)ln;
            /* Extract just the filename from the full path */
            const char *fname = s_src_path;
            const char *sl = strrchr(s_src_path, '\\');
            if (!sl) sl = strrchr(s_src_path, '/');
            if (sl) fname = sl + 1;
            strncpy(s_breakpoints[s_bp_count].file, fname[0] ? fname : "<unknown>", 255);
            s_breakpoints[s_bp_count].file[255] = '\0';
            s_breakpoints[s_bp_count].is_enabled  = 1;
            s_breakpoints[s_bp_count].hit_count   = 0;
            s_bp_count++;
        }
    }
}

static char s_lookup_pending[MAX_PATH];

static void src_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;

    s_src_n = 0;
    while (s_src_n < SRC_MAX) {
        char *ln = s_src[s_src_n];
        if (!fgets(ln, SRC_W, f)) break;

        i32 l = (i32)strlen(ln);
        while (l > 0 && (ln[l - 1] == '\n' || ln[l - 1] == '\r'))
            ln[--l] = '\0';

        char tmp[SRC_W];
        i32 ti = 0;
        for (i32 i = 0; i < l && ti < SRC_W - 1; i++) {
            if (ln[i] == '\t') {
                i32 spaces = 4 - (ti % 4);
                for (i32 s = 0; s < spaces && ti < SRC_W - 1; s++)
                    tmp[ti++] = ' ';
            } else {
                tmp[ti++] = ln[i];
            }
        }
        tmp[ti] = '\0';
        memcpy(ln, tmp, (size_t)ti + 1);

        s_src_n++;
    }
    fclose(f);
    strncpy(s_src_path, path, sizeof(s_src_path) - 1);
    s_lookup_pending[0] = '\0';
}

static void draw_source(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                       i32 *gx, i32 *cy_out, i32 *lh_out)
{
    char hdr[MAX_PATH + 32];
    if (s_src_path[0]) {
        const char *base = strrchr(s_src_path, '\\');
        if (!base) base = strrchr(s_src_path, '/');
        if (base) base++; else base = s_src_path;
        snprintf(hdr, sizeof(hdr), "SOURCE %s", base);
    } else {
        snprintf(hdr, sizeof(hdr), "SOURCE (no file loaded)");
    }

    draw_panel(px, py, pw, ph, hdr);

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 3;
    i32 gw = s_fw * 6;

    *gx = px + gw;
    *cy_out = cy;
    *lh_out = lh;

    if (!s_src_n) return;

    i32 mv = (ch > 0) ? ch / lh : 1;
    i32 msc = s_src_n - mv;
    if (msc < 0) msc = 0;
    if (s_src_sc > msc) s_src_sc = msc;
    if (s_src_sc < 0) s_src_sc = 0;

    scissor(px + 1, cy, pw - 2, ch, wh);

    i32 last = s_src_sc + mv;
    if (last > s_src_n) last = s_src_n;

    for (i32 i = s_src_sc; i < last; i++) {
        i32 ln = i + 1;
        f32 ly = (f32)(cy + (i - s_src_sc) * lh);

        if (ln == s_cur_line) {
            rect((f32)px + 1, ly - 1, (f32)pw - 2, (f32)lh,
                 COLOR_CURRENT_LINE_R, COLOR_CURRENT_LINE_G, COLOR_CURRENT_LINE_B, 80);
        }

        if (bp_has(ln)) {
            rect((f32)px + 1, ly - 1, (f32)gw - 1, (f32)lh,
                 COLOR_BREAKPOINT_R, COLOR_BREAKPOINT_G, COLOR_BREAKPOINT_B, 255);
        }

        if (ln == s_cur_line) {
            glColor3ub(COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
            glBegin(GL_TRIANGLES);
            glVertex2f((f32)(px + 4), ly + (f32)lh / 2 - 2);
            glVertex2f((f32)(px + 10), ly + (f32)lh / 2 - 2);
            glVertex2f((f32)(px + 7), ly + (f32)lh / 2 - 8);
            glEnd();
        }

        char nb[8];
        snprintf(nb, sizeof(nb), "%4d", ln);

        u8 r = COLOR_TEXT_R, g = COLOR_TEXT_G, b = COLOR_TEXT_B;
        if (ln == s_cur_line) {
            r = COLOR_SUCCESS_R;
            g = COLOR_SUCCESS_G;
            b = COLOR_SUCCESS_B;
        } else if (bp_has(ln)) {
            r = COLOR_BREAKPOINT_R;
            g = COLOR_BREAKPOINT_G;
            b = COLOR_BREAKPOINT_B;
        } else {
            r = COLOR_TEXT_DIM_R;
            g = COLOR_TEXT_DIM_G;
            b = COLOR_TEXT_DIM_B;
        }
        text_at(nb, (f32)(px + 4), ly, r, g, b);

        text_at(s_src[i], (f32)(px + gw + 8), ly,
                COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }

    unscissor();

    line_vline((f32)(px + gw + 1), (f32)cy, (f32)(cy + ch),
               COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    draw_scrollbar(px + pw, cy, ch, s_src_n, mv, s_src_sc);
}

/* ═══════════════════════════════════════════════════════════════
 *  TEXT PANELS (Locals, Stack)
 * ═══════════════════════════════════════════════════════════════ */
#define SIDE_CAP (32 * 1024)
static char s_locals[SIDE_CAP] = "(waiting - run and stop the program)\n";
static char s_stack[SIDE_CAP] = "(waiting - run and stop the program)\n";
static i32  s_locals_sc = 0, s_stack_sc = 0;

static void draw_textpanel(const char *title, const char *text,
                          i32 px, i32 py, i32 pw, i32 ph, i32 wh, i32 *sc)
{
    (void)wh;

    draw_panel(px, py, pw, ph, title);

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 3;
    i32 mv = (ch > 0) ? ch / lh : 1;

    static const char *ls[4096];
    static i32 ll[4096];
    i32 n = 0;
    ls[0] = text;
    const char *p = text;

    while (*p && n < 4095) {
        if (*p == '\n') {
            ll[n] = (i32)(p - ls[n]);
            n++;
            ls[n] = p + 1;
        }
        p++;
    }
    ll[n] = (i32)(p - ls[n]);
    if (ll[n] > 0 || n == 0) n++;

    i32 msc = n - mv;
    if (msc < 0) msc = 0;
    if (*sc > msc) *sc = msc;
    if (*sc < 0) *sc = 0;

    i32 first = *sc;
    i32 last = first + mv;
    if (last > n) last = n;

    scissor(px + 1, cy, pw - 2, ch, wh);

    char buf[1024];
    for (i32 i = first; i < last; i++) {
        i32 l = ll[i];
        if (l <= 0) continue;
        if (l > (i32)sizeof(buf) - 1) l = (i32)sizeof(buf) - 1;

        memcpy(buf, ls[i], (size_t)l);
        buf[l] = '\0';

        u8 r = COLOR_TEXT_R, g = COLOR_TEXT_G, b = COLOR_TEXT_B;
        if (strchr(buf, '=')) {
            r = 0xDD; g = 0xBB; b = 0x88;
        } else if (buf[0] == '*' || (buf[0] == ' ' && buf[1] == 'f')) {
            r = 0x88; g = 0xCC; b = 0xFF;
        } else {
            r = COLOR_TEXT_R;
            g = COLOR_TEXT_G;
            b = COLOR_TEXT_B;
        }

        text_at(buf, (f32)(px + 8), (f32)(cy + (i - first) * lh), r, g, b);
    }

    unscissor();
    draw_scrollbar(px + pw, cy, ch, n, mv, *sc);
}

/* ═══════════════════════════════════════════════════════════════
 *  REGISTERS PANEL
 * ═══════════════════════════════════════════════════════════════ */
static void update_registers_from_output(const char *output)
{
    const char *p = output;
    s_register_count = 0;

    while (*p && s_register_count < 64) {
        /* Skip leading whitespace on each line */
        while (*p == ' ' || *p == '\t') p++;

        if (isalpha(*p)) {
            char name[16] = {0};
            i32 i = 0;
            while (isalnum(*p) && i < 15) {
                name[i++] = *p++;
            }

            while (*p == ' ' || *p == '=') p++;

            if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                char value[64] = {0};
                i = 0;
                while ((isxdigit(*p) || *p == 'x' || *p == 'X') && i < 63) {
                    value[i++] = *p++;
                }

                strncpy(s_registers[s_register_count].name, name, 15);
                strncpy(s_registers[s_register_count].value, value, 63);
                s_registers[s_register_count].is_changed = 0;
                s_register_count++;
            }
        }
        p = strchr(p, '\n');
        if (!p) break;
        p++;
    }
}

static void draw_registers_panel(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                                 i32 mx, i32 my, i32 clicked)
{
    (void)mx;
    (void)my;
    (void)clicked;
    (void)wh;

    draw_panel(px, py, pw, ph, "REGISTERS");

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 3;
    i32 mv = (ch > 0) ? ch / lh : 1;

    scissor(px + 1, cy, pw - 2, ch, wh);

    i32 start = s_register_scroll;
    i32 end = start + mv;
    if (end > s_register_count) end = s_register_count;

    for (i32 i = start; i < end; i++) {
        f32 ry = (f32)(cy + (i - start) * lh);

        text_at(s_registers[i].name, (f32)(px + 8), ry + 2,
                COLOR_INFO_R, COLOR_INFO_G, COLOR_INFO_B);

        i32 name_width = (i32)strlen(s_registers[i].name) * s_fw;
        text_at(s_registers[i].value, (f32)(px + 8 + name_width + s_fw * 2), ry + 2,
                s_registers[i].is_changed ? COLOR_WARNING_R : COLOR_TEXT_R,
                s_registers[i].is_changed ? COLOR_WARNING_G : COLOR_TEXT_G,
                s_registers[i].is_changed ? COLOR_WARNING_B : COLOR_TEXT_B);
    }

    unscissor();

    if (s_register_count > mv) {
        f32 scroll_pos = (f32)s_register_scroll / (f32)(s_register_count - mv);
        f32 scroll_h = (f32)ch * ((f32)mv / (f32)s_register_count);
        f32 scroll_y = (f32)cy + scroll_pos * ((f32)ch - scroll_h);
        rect((f32)(px + pw - 8), scroll_y, 4, scroll_h,
             COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 200);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  CLI ARGUMENTS PANEL - Fixed text positioning
 * ═══════════════════════════════════════════════════════════════ */
static void draw_cli_args_panel(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                                i32 mx, i32 my, i32 clicked)
{
    (void)wh;

    draw_panel(px, py, pw, ph, "PROGRAM ARGUMENTS");

    i32 hh = s_ui_fh + 16 + 2;
    i32 input_y = py + hh + 4;
    i32 input_h = s_fh + 16;
    i32 input_w = pw - 24;

    rect((f32)px + 12, (f32)input_y, (f32)input_w, (f32)input_h,
         COLOR_BG_INPUT_R, COLOR_BG_INPUT_G, COLOR_BG_INPUT_B, 255);

    i32 focused = (s_active_input == INPUT_ARGS);
    rect((f32)px + 14, (f32)input_y + 2, (f32)input_w - 4, (f32)input_h - 4,
         focused ? COLOR_BG_ACTIVE_R : 0x2A,
         focused ? COLOR_BG_ACTIVE_G : 0x2A,
         focused ? COLOR_BG_ACTIVE_B : 0x2A, 255);

    i32 text_x = px + 18;
    i32 text_y = input_y + (input_h - s_fh) / 2;

    if (focused && s_args_editor.has_selection) {
        i32 sel_start = s_args_editor.selection_start < s_args_editor.selection_end ?
                        s_args_editor.selection_start : s_args_editor.selection_end;
        i32 sel_end = s_args_editor.selection_start > s_args_editor.selection_end ?
                      s_args_editor.selection_start : s_args_editor.selection_end;

        if (sel_start > 0) {
            text_at_len(s_cli_args, sel_start, (f32)text_x, (f32)text_y,
                        s_cli_args[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                        s_cli_args[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                        s_cli_args[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
        }

        rect((f32)(text_x + sel_start * s_fw), (f32)text_y,
             (f32)((sel_end - sel_start) * s_fw), (f32)s_fh,
             0x26, 0x4F, 0x78, 255);

        text_at_len(s_cli_args + sel_start, sel_end - sel_start,
                    (f32)(text_x + sel_start * s_fw), (f32)text_y,
                    COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);

        if (sel_end < (i32)s_args_editor.length) {
            text_at_len(s_cli_args + sel_end, s_args_editor.length - sel_end,
                        (f32)(text_x + sel_end * s_fw), (f32)text_y,
                        s_cli_args[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                        s_cli_args[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                        s_cli_args[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
        }
    } else {
        text_at(s_cli_args[0] ? s_cli_args : "enter program arguments...",
                (f32)text_x, (f32)text_y,
                s_cli_args[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                s_cli_args[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                s_cli_args[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
    }

    if (focused && (GetTickCount() / 500) % 2) {
        i32 cursor_x = text_x + s_args_editor.cursor_pos * s_fw;
        rect((f32)cursor_x, (f32)text_y, 2, (f32)s_fh,
             COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 200);
    }

    if (clicked && mx >= px + 14 && mx < px + 14 + input_w - 4 &&
        my >= input_y + 2 && my < input_y + 2 + input_h - 4) {
        s_active_input = INPUT_ARGS;
        editor_set_cursor(&s_args_editor, mx, my, s_fw, text_x);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  THREADS PANEL
 * ═══════════════════════════════════════════════════════════════ */
static void update_threads_from_output(const char *output)
{
    const char *p = output;
    s_thread_count = 0;

    while (*p && s_thread_count < 32) {
        if (strncmp(p, "* thread #", 10) == 0) {
            p += 10;
            char *end;
            u32 tid = (u32)strtoul(p, &end, 10);
            s_threads[s_thread_count].thread_id = tid;
            s_threads[s_thread_count].is_current = 1;
            s_current_thread = tid;
            snprintf(s_threads[s_thread_count].name, 63, "Thread %u", tid);
            s_thread_count++;
        } else if (strncmp(p, "  thread #", 10) == 0) {
            p += 10;
            char *end;
            u32 tid = (u32)strtoul(p, &end, 10);
            s_threads[s_thread_count].thread_id = tid;
            s_threads[s_thread_count].is_current = 0;
            snprintf(s_threads[s_thread_count].name, 63, "Thread %u", tid);
            s_thread_count++;
        }
        p = strchr(p, '\n');
        if (!p) break;
        p++;
    }
}

static void draw_threads_panel(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                               i32 mx, i32 my, i32 clicked)
{
    (void)wh;

    draw_panel(px, py, pw, ph, "THREADS");

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 6;
    i32 mv = (ch > 0) ? ch / lh : 1;

    scissor(px + 1, cy, pw - 2, ch, wh);

    i32 start = s_thread_scroll;
    i32 end = start + mv;
    if (end > s_thread_count) end = s_thread_count;

    for (i32 i = start; i < end; i++) {
        f32 ry = (f32)(cy + (i - start) * lh);

        if (s_threads[i].is_current) {
            rect((f32)px + 1, ry, (f32)pw - 2, (f32)lh,
                 COLOR_BG_ACTIVE_R, COLOR_BG_ACTIVE_G, COLOR_BG_ACTIVE_B, 80);
        }

        char line[256];
        snprintf(line, sizeof(line), "%s %s",
                 s_threads[i].is_current ? "→" : " ",
                 s_threads[i].name);

        text_at(line, (f32)(px + 8), ry + 2,
                s_threads[i].is_current ? COLOR_TEXT_BRIGHT_R : COLOR_TEXT_R,
                s_threads[i].is_current ? COLOR_TEXT_BRIGHT_G : COLOR_TEXT_G,
                s_threads[i].is_current ? COLOR_TEXT_BRIGHT_B : COLOR_TEXT_B);

        if (clicked && mx >= px + 1 && mx < px + pw - 1 &&
            my >= (i32)ry && my < (i32)ry + lh) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "thread select %u\n", s_threads[i].thread_id);
            send_to_dbg(&s_dbg, cmd);
        }
    }

    unscissor();

    if (s_thread_count > mv) {
        f32 scroll_pos = (f32)s_thread_scroll / (f32)(s_thread_count - mv);
        f32 scroll_h = (f32)ch * ((f32)mv / (f32)s_thread_count);
        f32 scroll_y = (f32)cy + scroll_pos * ((f32)ch - scroll_h);
        rect((f32)(px + pw - 8), scroll_y, 4, scroll_h,
             COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 200);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  BREAKPOINTS PANEL - Fixed to show breakpoints
 * ═══════════════════════════════════════════════════════════════ */
static void update_breakpoints_from_output(const char *output)
{
    /* Save locally-set breakpoints so they survive the re-parse */
    BreakpointInfo saved[128];
    i32 saved_count = s_bp_count;
    if (saved_count > 0)
        memcpy(saved, s_breakpoints, (size_t)saved_count * sizeof(BreakpointInfo));

    const char *p = output;
    s_bp_count = 0;

    while (*p && s_bp_count < 128) {
        /* Look for breakpoint patterns like "1: name = 'main', locations = 1" */
        if (isdigit(*p) && p[1] == ':') {
            s_breakpoints[s_bp_count].bp_id = (u32)(*p - '0');
            p += 2;

            /* Skip to file/line info */
            char *at = strstr(p, "at ");
            if (at) {
                at += 3;
                char *colon = strchr(at, ':');
                if (colon) {
                    i32 len = (i32)(colon - at);
                    if (len > 255) len = 255;
                    strncpy(s_breakpoints[s_bp_count].file, at, len);
                    s_breakpoints[s_bp_count].file[len] = '\0';

                    char *end;
                    s_breakpoints[s_bp_count].line = (u32)strtoul(colon + 1, &end, 10);
                    s_breakpoints[s_bp_count].is_enabled = 1;
                    s_bp_count++;
                }
            }
        }
        p = strchr(p, '\n');
        if (!p) break;
        p++;
    }

    for (i32 i = 0; i < saved_count; i++) {
        i32 already = 0;
        for (i32 j = 0; j < s_bp_count; j++) {
            if (s_breakpoints[j].line == saved[i].line) { already = 1; break; }
        }
        if (!already && s_bp_count < 128)
            s_breakpoints[s_bp_count++] = saved[i];
    }
}

static void draw_breakpoints_panel(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                                   i32 mx, i32 my, i32 clicked)
{
    (void)mx;
    (void)my;
    (void)clicked;
    (void)wh;

    draw_panel(px, py, pw, ph, "BREAKPOINTS");

    i32 hh = s_ui_fh + 16 + 2;
    i32 cy = py + hh;
    i32 ch = ph - hh - 2;
    i32 lh = s_fh + 6;
    i32 mv = (ch > 0) ? ch / lh : 1;

    scissor(px + 1, cy, pw - 2, ch, wh);

    i32 start = s_bp_scroll;
    i32 end = start + mv;
    if (end > s_bp_count) end = s_bp_count;

    for (i32 i = start; i < end; i++) {
        f32 ry = (f32)(cy + (i - start) * lh);

        char line[256];
        snprintf(line, sizeof(line), "#%u  %s:%u",
                 s_breakpoints[i].bp_id,
                 s_breakpoints[i].file,
                 s_breakpoints[i].line);

        text_at(line, (f32)(px + 8), ry + 2,
                s_breakpoints[i].is_enabled ? COLOR_BREAKPOINT_R : COLOR_TEXT_DIM_R,
                s_breakpoints[i].is_enabled ? COLOR_BREAKPOINT_G : COLOR_TEXT_DIM_G,
                s_breakpoints[i].is_enabled ? COLOR_BREAKPOINT_B : COLOR_TEXT_DIM_B);
    }

    if (s_bp_count == 0) {
        text_at("No breakpoints set", (f32)(px + 8), (f32)(cy + 2),
                COLOR_TEXT_DIM_R, COLOR_TEXT_DIM_G, COLOR_TEXT_DIM_B);
    }

    unscissor();

    if (s_bp_count > mv) {
        f32 scroll_pos = (f32)s_bp_scroll / (f32)(s_bp_count - mv);
        f32 scroll_h = (f32)ch * ((f32)mv / (f32)s_bp_count);
        f32 scroll_y = (f32)cy + scroll_pos * ((f32)ch - scroll_h);
        rect((f32)(px + pw - 8), scroll_y, 4, scroll_h,
             COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 200);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  WATCH PANEL with tree view - Fixed text positioning and expandable tree
 * ═══════════════════════════════════════════════════════════════ */
static void draw_watch(i32 px, i32 py, i32 pw, i32 ph, i32 wh,
                      i32 mx, i32 my, i32 clicked, i32 double_clicked,
                      i32 *add_btn_x, i32 *add_btn_y, i32 *add_btn_w, i32 *add_btn_h,
                      i32 rm_x[WATCH_MAX], i32 rm_y[WATCH_MAX])
{
    (void)wh;

    draw_panel(px, py, pw, ph, "WATCH");

    i32 hh = s_ui_fh + 16 + 2;

    i32 ir_y = py + hh + 4;
    i32 ir_h = s_fh + 16;
    i32 btn_w = s_fw * 6 + 16;
    i32 input_w = pw - btn_w - 20; /* input fills up to button */

    rect((f32)px + 8, (f32)ir_y, (f32)(pw - 16), (f32)ir_h,
         COLOR_BG_INPUT_R, COLOR_BG_INPUT_G, COLOR_BG_INPUT_B, 255);

    i32 focused = (s_active_input == INPUT_WATCH && s_editing_watch_index < 0);

    rect((f32)(px + 10), (f32)(ir_y + 2), (f32)input_w, (f32)ir_h - 4,
         focused ? COLOR_BG_ACTIVE_R : 0x2A,
         focused ? COLOR_BG_ACTIVE_G : 0x2A,
         focused ? COLOR_BG_ACTIVE_B : 0x2A, 255);

    const char *display_text = s_watch_new[0] ? s_watch_new : "enter expression...";
    i32 display_len = (i32)strlen(display_text);
    i32 text_x = px + 14;
    i32 text_y = ir_y + (ir_h - s_fh) / 2;

    if (focused && s_watch_editor.has_selection) {
        i32 sel_start = s_watch_editor.selection_start < s_watch_editor.selection_end ?
                        s_watch_editor.selection_start : s_watch_editor.selection_end;
        i32 sel_end = s_watch_editor.selection_start > s_watch_editor.selection_end ?
                      s_watch_editor.selection_start : s_watch_editor.selection_end;

        if (sel_start > 0) {
            text_at_len(display_text, sel_start, (f32)text_x, (f32)text_y,
                        s_watch_new[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                        s_watch_new[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                        s_watch_new[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
        }

        rect((f32)(text_x + sel_start * s_fw), (f32)text_y,
             (f32)((sel_end - sel_start) * s_fw), (f32)s_fh,
             0x26, 0x4F, 0x78, 255);

        text_at_len(display_text + sel_start, sel_end - sel_start,
                    (f32)(text_x + sel_start * s_fw), (f32)text_y,
                    COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);

        if (sel_end < display_len) {
            text_at_len(display_text + sel_end, display_len - sel_end,
                        (f32)(text_x + sel_end * s_fw), (f32)text_y,
                        s_watch_new[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                        s_watch_new[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                        s_watch_new[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
        }
    } else {
        text_at(display_text, (f32)text_x, (f32)text_y,
                s_watch_new[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                s_watch_new[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                s_watch_new[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
    }

    if (focused && (GetTickCount() / 500) % 2) {
        i32 cursor_x = text_x + s_watch_editor.cursor_pos * s_fw;
        rect((f32)cursor_x, (f32)text_y, 2, (f32)s_fh,
             COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 200);
    }

    i32 bx = px + pw - btn_w - 8;
    i32 by = ir_y;
    i32 bw = btn_w;
    i32 bh = ir_h;

    i32 hot = (mx >= bx && mx < bx + bw && my >= by && my < by + bh);
    rect((f32)bx, (f32)by, (f32)bw, (f32)bh,
         hot ? COLOR_ACCENT_HOVER_R : COLOR_ACCENT_R,
         hot ? COLOR_ACCENT_HOVER_G : COLOR_ACCENT_G,
         hot ? COLOR_ACCENT_HOVER_B : COLOR_ACCENT_B, 255);

    {
        const char *lbl = "Add";
        f32 lw = (f32)((i32)strlen(lbl) * s_fw);
        text_at(lbl, (f32)(bx + (bw - (i32)lw) / 2), (f32)(by + (bh - s_fh) / 2 - 2),
                hot ? COLOR_TEXT_BRIGHT_R : 0x88,
                hot ? COLOR_TEXT_BRIGHT_G : 0xFF,
                hot ? COLOR_TEXT_BRIGHT_B : 0x88);
    }

    *add_btn_x = bx;
    *add_btn_y = by;
    *add_btn_w = bw;
    *add_btn_h = bh;

    if (clicked && mx >= px + 10 && mx < px + 10 + input_w &&
        my >= ir_y && my < ir_y + ir_h) {
        s_active_input = INPUT_WATCH;
        s_editing_watch_index = -1;
        editor_set_cursor(&s_watch_editor, mx, my, s_fw, text_x);
    }

    i32 cy = ir_y + ir_h + 2;
    i32 ch = ph - hh - ir_h - 8;
    i32 lh = s_fh + 6;
    i32 indent = s_fw * 2;

    /* Calculate total lines for scrolling */
    i32 total_lines = 0;
    for (i32 i = 0; i < s_wn; i++) {
        total_lines++;
        if (s_watches[i].is_complex && s_watches[i].is_expanded && s_watches[i].root) {
            i32 nc = s_watches[i].root->child_count > 0 ? s_watches[i].root->child_count : 1;
            total_lines += nc;
        }
    }

    i32 mv = (ch > 0) ? ch / lh : 1;
    i32 msc = total_lines - mv;
    if (msc < 0) msc = 0;
    if (s_wsc > msc) s_wsc = msc;
    if (s_wsc < 0) s_wsc = 0;

    scissor(px + 1, cy, pw - 2, ch, wh);

    i32 line = 0;
    for (i32 i = 0; i < s_wn; i++) {
        f32 ry = (f32)(cy + (line - s_wsc) * lh);

        if (line >= s_wsc && line < s_wsc + mv) {
            if ((line - s_wsc) % 2 == 1) {
                rect((f32)px + 1, ry, (f32)pw - 2, (f32)lh, 0x16, 0x16, 0x20, 255);
            }

            if (s_editing_watch_index == i) {
                rect((f32)px + 1, ry, (f32)pw - 2, (f32)lh, COLOR_BG_ACTIVE_R, COLOR_BG_ACTIVE_G, COLOR_BG_ACTIVE_B, 60);
            }

            /* Compact remove button — just "x", right-aligned, small */
            i32 rx = px + pw - s_fw * 3 - 6;
            rm_x[i] = rx;
            rm_y[i] = (i32)ry;

            i32 hot_rm = (mx >= rx && mx < rx + s_fw * 3 + 2 &&
                          my >= (i32)ry + 1 && my < (i32)ry + lh - 1);

            rect((f32)rx, ry + 2, (f32)(s_fw * 3 + 2), (f32)(lh - 4),
                 hot_rm ? COLOR_WARNING_R : 0x38,
                 hot_rm ? 0x20          : 0x18,
                 hot_rm ? 0x20          : 0x18, 255);

            text_at(" x", (f32)(rx + 1), ry + (lh - s_fh) / 2,
                    hot_rm ? 0xFF : 0x88,
                    hot_rm ? 0x60 : 0x44,
                    hot_rm ? 0x60 : 0x44);

            /* Expand/collapse button for complex types */
            i32 expand_x = px + 8;
            if (s_watches[i].is_complex) {
                if (s_watches[i].is_expanded) {
                    text_at("▼", (f32)expand_x, ry + (lh - s_fh) / 2, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B);
                } else {
                    text_at("▶", (f32)expand_x, ry + (lh - s_fh) / 2, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B);
                }
            }

            /* Display the watch value — or an inline edit box */
            if (s_editing_watch_index == i) {
                /* ── Inline text editor ─────────────────────────── */
                i32 box_x  = px + 24;
                i32 box_w  = rx - box_x - 4;
                i32 box_y  = (i32)ry + (lh - s_fh - 8) / 2;
                i32 box_h  = s_fh + 8;

                /* Box background */
                rect((f32)box_x, (f32)box_y, (f32)box_w, (f32)box_h,
                     COLOR_BG_INPUT_R, COLOR_BG_INPUT_G, COLOR_BG_INPUT_B, 255);
                rect((f32)(box_x + 2), (f32)(box_y + 2), (f32)(box_w - 4), (f32)(box_h - 4),
                     COLOR_BG_ACTIVE_R, COLOR_BG_ACTIVE_G, COLOR_BG_ACTIVE_B, 255);
                rect_border((f32)box_x, (f32)box_y, (f32)box_w, (f32)box_h,
                            COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B);

                i32 tx = box_x + 4;
                i32 ty = box_y + (box_h - s_fh) / 2;

                /* Selection highlight */
                if (s_edit_watch_editor.has_selection) {
                    i32 ss = s_edit_watch_editor.selection_start < s_edit_watch_editor.selection_end
                           ? s_edit_watch_editor.selection_start : s_edit_watch_editor.selection_end;
                    i32 se = s_edit_watch_editor.selection_start > s_edit_watch_editor.selection_end
                           ? s_edit_watch_editor.selection_start : s_edit_watch_editor.selection_end;
                    if (ss > 0)
                        text_at_len(s_edit_watch_buffer, ss, (f32)tx, (f32)ty,
                                    COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
                    rect((f32)(tx + ss * s_fw), (f32)(box_y + 2),
                         (f32)((se - ss) * s_fw), (f32)(box_h - 4),
                         0x26, 0x4F, 0x78, 255);
                    text_at_len(s_edit_watch_buffer + ss, se - ss,
                                (f32)(tx + ss * s_fw), (f32)ty,
                                COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);
                    if (se < (i32)s_edit_watch_editor.length)
                        text_at_len(s_edit_watch_buffer + se,
                                    (i32)s_edit_watch_editor.length - se,
                                    (f32)(tx + se * s_fw), (f32)ty,
                                    COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
                } else {
                    text_at(s_edit_watch_buffer, (f32)tx, (f32)ty,
                            COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);
                }

                /* Blinking cursor */
                if ((GetTickCount() / 500) % 2) {
                    i32 cur_x = tx + s_edit_watch_editor.cursor_pos * s_fw;
                    rect((f32)cur_x, (f32)(box_y + 2), 2.0f, (f32)(box_h - 4),
                         COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 220);
                }

                /* Click inside the box sets cursor */
                if (clicked && mx >= box_x && mx < box_x + box_w &&
                    my >= box_y && my < box_y + box_h) {
                    editor_set_cursor(&s_edit_watch_editor, mx, my, s_fw, tx);
                }

            } else {
                /* Normal value display */
                char disp[384];
                if (s_watches[i].is_complex) {
                    const char *ptr = strstr(s_watches[i].value, "0x");
                    if (ptr) {
                        char ptr_val[32] = {0};
                        i32 j = 0;
                        while (ptr[j] && !isspace(ptr[j]) && j < 31) { ptr_val[j] = ptr[j]; j++; }
                        snprintf(disp, sizeof(disp), "%s = %s", s_watches[i].expr, ptr_val);
                    } else {
                        snprintf(disp, sizeof(disp), "%s = %s", s_watches[i].expr, s_watches[i].value);
                    }
                } else {
                    snprintf(disp, sizeof(disp), "%s = %s", s_watches[i].expr, s_watches[i].value);
                }

            /* Truncate if too long, but try to keep pointer visible */
            i32 maxw = (rx - px - 32) / s_fw;
            if (maxw < (i32)strlen(disp)) {
                if (maxw > 8) {
                    char *ptr_part = strstr(disp, "0x");
                    if (ptr_part && (ptr_part - disp) < maxw - 12) {
                        i32 keep_len = (i32)(ptr_part - disp) + 12;
                        if (keep_len < maxw) {
                            disp[keep_len] = '.';
                            disp[keep_len + 1] = '.';
                            disp[keep_len + 2] = '.';
                            disp[keep_len + 3] = '\0';
                        }
                    } else {
                        disp[maxw - 3] = '.';
                        disp[maxw - 2] = '.';
                        disp[maxw - 1] = '.';
                        disp[maxw] = '\0';
                    }
                }
            }

            text_at(disp, (f32)(px + 24), ry + (lh - s_fh) / 2, 0xCC, 0xDD, 0xAA);
            } /* end normal display else */

            /* Click on expand button */
            if (clicked && mx >= expand_x && mx < expand_x + s_fw * 2 &&
                my >= (i32)ry && my < (i32)ry + lh) {
                s_watches[i].is_expanded = !s_watches[i].is_expanded;
            }

            /* Double-click to edit */
            if (double_clicked && mx >= px + 24 && mx < rx &&
                my >= (i32)ry && my < (i32)ry + lh) {
                s_editing_watch_index = i;
                s_active_input = INPUT_WATCH;
                strncpy(s_edit_watch_buffer, s_watches[i].expr, 127);
                s_edit_watch_buffer[127] = '\0';
                editor_init(&s_edit_watch_editor, s_edit_watch_buffer, 128);
            }
        }

        line++;

        /* Draw children if expanded */
        if (s_watches[i].is_complex && s_watches[i].is_expanded &&
            s_watches[i].root && s_watches[i].root->child_count > 0) {
            for (i32 ci = 0; ci < s_watches[i].root->child_count; ci++) {
                TreeNode *child = s_watches[i].root->children[ci];
                if (!child) continue;
                if (line >= s_wsc && line < s_wsc + mv) {
                    f32 child_ry = (f32)(cy + (line - s_wsc) * lh);
                    if ((line - s_wsc) % 2 == 0)
                        rect((f32)px + 1, child_ry, (f32)pw - 2, (f32)lh, 0x12, 0x12, 0x1A, 255);
                    char cbuf[384];
                    snprintf(cbuf, sizeof(cbuf), "  .%s = %s", child->name, child->value);
                    i32 cmaxw = (px + pw - s_fw * 4 - px - 24) / s_fw;
                    if (cmaxw > 0 && (i32)strlen(cbuf) > cmaxw) {
                        cbuf[cmaxw - 3] = '.'; cbuf[cmaxw - 2] = '.';
                        cbuf[cmaxw - 1] = '.'; cbuf[cmaxw] = '\0';
                    }
                    text_at(cbuf, (f32)(px + 24 + indent), child_ry + (lh - s_fh) / 2,
                            COLOR_INFO_R, COLOR_INFO_G, COLOR_INFO_B);
                }
                line++;
            }
        } else if (s_watches[i].is_complex && s_watches[i].is_expanded &&
                   s_watches[i].root && s_watches[i].root->child_count == 0) {
            /* No parsed children — show raw value */
            if (line >= s_wsc && line < s_wsc + mv) {
                f32 child_ry = (f32)(cy + (line - s_wsc) * lh);
                char cbuf[256];
                snprintf(cbuf, sizeof(cbuf), "  %s", s_watches[i].root->value);
                i32 cmaxw = (pw - 32) / s_fw;
                if (cmaxw > 0 && (i32)strlen(cbuf) > cmaxw) {
                    cbuf[cmaxw-3] = '.'; cbuf[cmaxw-2] = '.'; cbuf[cmaxw-1] = '.'; cbuf[cmaxw] = '\0';
                }
                text_at(cbuf, (f32)(px + 24 + indent), child_ry + (lh - s_fh) / 2,
                        COLOR_INFO_R, COLOR_INFO_G, COLOR_INFO_B);
            }
            line++;
        }
    }

    unscissor();
    draw_scrollbar(px + pw, cy, ch, total_lines, mv, s_wsc);
}

/* ═══════════════════════════════════════════════════════════════
 *  CONTEXT MENU
 * ═══════════════════════════════════════════════════════════════ */
static i32 s_menu_open = 0;
static i32 s_menu_x = 0, s_menu_y = 0, s_menu_line = 0;

typedef enum {
    MENU_SET_BP = 0,
    MENU_CLR_BP,
    MENU_RUN_TO,
    MENU_ADD_WATCH,
    MENU_GOTO,
    MENU_COUNT
} MenuItem;

static const char *s_menu_labels[MENU_COUNT] = {
    "Set Breakpoint",
    "Clear Breakpoint",
    "Run to Line",
    "Add to Watch",
    "Go to Line"
};

static i32 draw_menu(i32 mx, i32 my, i32 clicked)
{
    if (!s_menu_open) return -1;

    i32 iw = s_fw * 22;
    i32 ih = s_fh + 8;
    i32 pad = 6;
    i32 mw = iw + pad * 2;
    i32 mh = ih * MENU_COUNT + pad * 2;

    rect((f32)s_menu_x, (f32)s_menu_y, (f32)mw, (f32)mh, 0x28, 0x28, 0x38, 230);
    rect_border((f32)s_menu_x, (f32)s_menu_y, (f32)mw, (f32)mh,
                COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    i32 hit = -1;
    for (i32 i = 0; i < MENU_COUNT; i++) {
        i32 iy = s_menu_y + pad + i * ih;
        i32 hot = (mx >= s_menu_x + pad && mx < s_menu_x + pad + iw &&
                   my >= iy && my < iy + ih);

        if (hot) {
            rect((f32)(s_menu_x + pad), (f32)iy, (f32)iw, (f32)ih,
                 0x30, 0x50, 0x80, 255);
        }

        text_at(s_menu_labels[i], (f32)(s_menu_x + pad + 6), (f32)(iy + 2),
                hot ? 0xFF : 0xCC, hot ? 0xFF : 0xCC, hot ? 0xFF : 0xCC);

        if (hot && clicked) hit = i;
    }

    return hit;
}

/* ═══════════════════════════════════════════════════════════════
 *  LLDB OUTPUT CLASSIFIER
 * ═══════════════════════════════════════════════════════════════ */
typedef enum {ROUTE_NONE, ROUTE_LOCALS, ROUTE_STACK, ROUTE_WATCH, ROUTE_LOOKUP} RouteMode;
static RouteMode s_route = ROUTE_NONE;
static char s_racc[SIDE_CAP];
static i32 s_rlen = 0;
static i32 s_suppress_log = 0;

static void route_flush(void)
{
    if (!s_rlen) return;
    s_racc[s_rlen] = '\0';
    if (s_route == ROUTE_LOCALS) {
        memcpy(s_locals, s_racc, (size_t)s_rlen + 1);
        s_locals_sc = 0;
    } else if (s_route == ROUTE_STACK) {
        memcpy(s_stack, s_racc, (size_t)s_rlen + 1);
        s_stack_sc = 0;
    }
    s_route = ROUTE_NONE;
    s_rlen = 0;
}

static void parse_location(const char *chunk);

static void classify(const char *chunk)
{
    s_suppress_log = 0;
    i32 in_lookup = 0;
    const char *p = chunk;

    if (strstr(chunk, "thread #")) {
        update_threads_from_output(chunk);
    }

    if (strstr(chunk, "Current breakpoints:") ||
        strstr(chunk, "breakpoint list")) {
        update_breakpoints_from_output(chunk);
    }

    /* Also parse individual "Breakpoint N: where = ... at file:line" lines
       that lldb emits immediately when a breakpoint is set or resolved.      */
    {
        const char *bscan = chunk;
        while ((bscan = strstr(bscan, "Breakpoint ")) != NULL) {
            /* Expect "Breakpoint <digits>: " */
            const char *bp_id_start = bscan + 11;
            if (isdigit(*bp_id_start)) {
                u32 bid = (u32)strtoul(bp_id_start, NULL, 10);
                /* Look for " at <file>:<line>" on this same line */
                const char *at = strstr(bscan, " at ");
                const char *nl = strchr(bscan, '\n');
                if (at && (!nl || at < nl)) {
                    at += 4;
                    const char *colon = strchr(at, ':');
                    if (colon && (!nl || colon < nl)) {
                        i32 flen = (i32)(colon - at);
                        if (flen > 0 && flen < 256) {
                            char fname[256]; memcpy(fname, at, (size_t)flen); fname[flen] = '\0';
                            char *eol; u32 lno = (u32)strtoul(colon + 1, &eol, 10);
                            if (lno > 0) {
                                /* Check if already tracked */
                                i32 found = 0;
                                for (i32 k = 0; k < s_bp_count; k++)
                                    if (s_breakpoints[k].line == lno) { found = 1; break; }
                                if (!found && s_bp_count < 128) {
                                    s_breakpoints[s_bp_count].bp_id      = bid;
                                    s_breakpoints[s_bp_count].line       = lno;
                                    s_breakpoints[s_bp_count].is_enabled = 1;
                                    s_breakpoints[s_bp_count].hit_count  = 0;
                                    /* Use just the filename portion */
                                    const char *sl = strrchr(fname, '\\');
                                    if (!sl) sl = strrchr(fname, '/');
                                    strncpy(s_breakpoints[s_bp_count].file,
                                            sl ? sl + 1 : fname, 255);
                                    s_bp_count++;
                                    /* Also ensure gutter dot is set */
                                    if (!bp_has((i32)lno) && s_bp_n < BP_MAX)
                                        s_bp[s_bp_n++] = (i32)lno;
                                }
                            }
                        }
                    }
                }
            }
            bscan++;
        }
    }

    if (strstr(chunk, "General Purpose Registers:")) {
        update_registers_from_output(chunk);
    }

    while (*p) {
        const char *nl = strchr(p, '\n');
        i32 ll2 = nl ? (i32)(nl - p) : (i32)strlen(p);
        char lb[1024];
        i32 cl = ll2 < 1023 ? ll2 : 1023;
        memcpy(lb, p, (size_t)cl);
        lb[cl] = '\0';

        if (!strncmp(lb, "(lldb)", 6)) {
            route_flush();
            const char *cmd = lb + 6;
            while (*cmd == ' ') cmd++;

            if (!strncmp(cmd, "frame variable", 14) || !strncmp(cmd, "fr v", 4)) {
                s_route = ROUTE_LOCALS;
                s_rlen = 0;
                /* Fire watch refresh once locals are done loading */
                if (s_refresh_pending > 0) {
                    s_refresh_pending--;
                    if (s_refresh_pending == 0) watch_queue_all();
                }
            } else if (!strncmp(cmd, "bt", 2) || !strncmp(cmd, "backtrace", 9)) {
                s_route = ROUTE_STACK;
                s_rlen = 0;
            } else if (!strncmp(cmd, "p ", 2) && s_wpending >= 0) {
                s_route = ROUTE_WATCH;
                s_rlen = 0;
            } else if (!strncmp(cmd, "register read", 13)) {
                s_route = ROUTE_NONE;
            } else if (!strncmp(cmd, "target modules lookup", 21) ||
                       !strncmp(cmd, "source info", 11)) {
                s_route = ROUTE_LOOKUP;
                s_rlen = 0;
                s_suppress_log = 1;
                in_lookup = 1;
            } else {
                s_route = ROUTE_NONE;
            }
        } else if (s_route == ROUTE_LOOKUP) {
            s_suppress_log = 1;
            in_lookup = 1;
            if (s_lookup_pending[0]) {
                char full[MAX_PATH] = {0};
                char *fp = strstr(lb, "file = \"");
                if (fp) {
                    fp += 8;
                    char *eq = strchr(fp, '"');
                    if (eq && eq > fp) {
                        i32 len = (i32)(eq - fp);
                        if (len > 0 && len < MAX_PATH) {
                            memcpy(full, fp, (size_t)len);
                            full[len] = '\0';
                        }
                    }
                }
                if (!full[0]) {
                    char *sep = strstr(lb, "]: ");
                    if (!sep) sep = strstr(lb, "): ");
                    if (sep) {
                        char *ps = sep + 3;
                        if (ps[1] == ':') {
                            char *col = strrchr(ps + 3, ':');
                            i32 len = col ? (i32)(col - ps) : (i32)strlen(ps);
                            if (len > 0 && len < MAX_PATH) {
                                memcpy(full, ps, (size_t)len);
                                full[len] = '\0';
                            }
                        }
                    }
                }
                if (full[0]) {
                    if (strcmp(full, s_src_path) != 0) src_load(full);
                    else s_lookup_pending[0] = '\0';
                }
            }
        } else if (s_route == ROUTE_WATCH) {
            if (cl > 0 && s_wpending >= 0) {
                parse_lldb_value(lb, &s_watches[s_wpending]);
                s_wpending = -1;
                watch_send_next();
            }
            s_route = ROUTE_NONE;
        } else if (s_route != ROUTE_NONE && s_rlen + cl + 1 < SIDE_CAP) {
            memcpy(s_racc + s_rlen, lb, (size_t)cl);
            s_rlen += cl;
            s_racc[s_rlen++] = '\n';
        }

        if (!nl) break;
        p = nl + 1;
    }

    if (!in_lookup) parse_location(chunk);
}

static void parse_location(const char *chunk)
{
    static const char *exts[] = {".c:", ".h:", ".cpp:", ".cc:", ".cxx:", NULL};
    const char *line = chunk;

    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t ll3 = nl ? (size_t)(nl - line) : strlen(line);
        char lb[1024];
        if (ll3 >= sizeof(lb)) ll3 = sizeof(lb) - 1;
        memcpy(lb, line, ll3);
        lb[ll3] = '\0';

        for (i32 e = 0; exts[e]; e++) {
            char *ep = strstr(lb, exts[e]);
            if (!ep) continue;

            i32 elen = (i32)strlen(exts[e]);
            char *dp = ep + elen;
            i32 lno = 0;
            while (*dp >= '0' && *dp <= '9') {
                lno = lno * 10 + (*dp - '0');
                dp++;
            }
            if (lno <= 0) continue;

            char *ns = ep;
            while (ns > lb && ns[-1] != ' ' && ns[-1] != '`' && ns[-1] != '"')
                ns--;

            size_t flen = (size_t)(ep - ns) + (size_t)(elen - 1);
            if (!flen || flen >= MAX_PATH) continue;

            char nf[MAX_PATH];
            memcpy(nf, ns, flen);
            nf[flen] = '\0';

            s_cur_line = lno;
            s_src_sc = lno - 10;
            if (s_src_sc < 0) s_src_sc = 0;

            i32 is_abs = ((i32)flen > 1 && nf[1] == ':') ||
                         (nf[0] == '\\') || (nf[0] == '/');

            if (is_abs) {
                if (strcmp(nf, s_src_path) != 0) src_load(nf);
            } else {
                if (strchr(nf, '\\') || strchr(nf, '/')) return;

                const char *loaded_base = s_src_path;
                const char *sl2 = strrchr(s_src_path, '\\');
                if (!sl2) sl2 = strrchr(s_src_path, '/');
                if (sl2) loaded_base = sl2 + 1;

                if (strcmp(nf, loaded_base) == 0) return;

                if (s_dbg_live && strcmp(nf, s_lookup_pending) != 0) {
                    strncpy(s_lookup_pending, nf, sizeof(s_lookup_pending) - 1);
                    char cmd2[MAX_PATH + 60];
                    snprintf(cmd2, sizeof(cmd2), "source info --file %s\n", nf);
                    send_to_dbg(&s_dbg, cmd2);
                }
            }
            return;
        }

        if (!nl) break;
        line = nl + 1;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  APP STATE
 * ═══════════════════════════════════════════════════════════════ */
static i32 s_mx = 0, s_my = 0;
static i32 s_need_refresh = 0;
static i32 s_thunk_recover = 0;
static i32 s_double_click_time = 0;
static i32 s_last_click_x = -1, s_last_click_y = -1;
static i32 s_last_click_time = 0;

static i32 is_double_click(i32 x, i32 y, i32 time)
{
    i32 double_click = 0;
    if (abs(x - s_last_click_x) < 5 && abs(y - s_last_click_y) < 5 &&
        time - s_last_click_time < 500) {
        double_click = 1;
    }
    s_last_click_x = x;
    s_last_click_y = y;
    s_last_click_time = time;
    return double_click;
}

/* ═══════════════════════════════════════════════════════════════
 *  CLIPBOARD HELPERS
 * ═══════════════════════════════════════════════════════════════ */
static TextEditor *get_active_editor(void)
{
    if (s_active_input == INPUT_CMD)   return &s_cmd_editor;
    if (s_active_input == INPUT_WATCH) {
        if (s_editing_watch_index >= 0) return &s_edit_watch_editor;
        return &s_watch_editor;
    }
    if (s_active_input == INPUT_ARGS)  return &s_args_editor;
    return &s_cmd_editor;
}

static void clipboard_paste(void)
{
    if (!OpenClipboard(s_hwnd)) return;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) {
        char *text = (char *)GlobalLock(h);
        if (text) {
            TextEditor *ed = get_active_editor();
            if (ed->has_selection) editor_delete_char(ed);
            while (*text) {
                char c = *text++;
                if (c == '\r') continue;
                if (c == '\n') break;
                if ((u8)c >= 32)
                    editor_insert_char(ed, c);
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

static void clipboard_copy(void)
{
    TextEditor *ed = get_active_editor();
    if (!ed->has_selection) return;

    i32 ss = ed->selection_start < ed->selection_end ? ed->selection_start : ed->selection_end;
    i32 se = ed->selection_start > ed->selection_end ? ed->selection_start : ed->selection_end;
    i32 len = se - ss;
    if (len <= 0) return;

    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(len + 1));
    if (!hg) return;
    char *dst = (char *)GlobalLock(hg);
    if (!dst) { GlobalFree(hg); return; }
    memcpy(dst, ed->buffer + ss, (size_t)len);
    dst[len] = '\0';
    GlobalUnlock(hg);

    if (OpenClipboard(s_hwnd)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hg);
        CloseClipboard();
    } else {
        GlobalFree(hg);
    }
}

static void editor_select_all(TextEditor *ed)
{
    ed->selection_start = 0;
    ed->selection_end   = (i32)ed->length;
    ed->cursor_pos      = (i32)ed->length;
    ed->has_selection   = (ed->length > 0) ? 1 : 0;
}

static void refresh_panels(void)
{
    if (!s_dbg_live) return;
    /* Send the panel refresh commands; watches are queued AFTER the
       frame variable response arrives (s_refresh_pending counts down
       to 0 on that echo) so the p commands don't interleave badly. */
    s_refresh_pending = 1;
    send_to_dbg(&s_dbg, "frame info\n");
    send_to_dbg(&s_dbg, "frame variable\n");
    send_to_dbg(&s_dbg, "bt\n");
    send_to_dbg(&s_dbg, "register read\n");
    send_to_dbg(&s_dbg, "breakpoint list\n");
    debugger_thread_info(&s_dbg);
}

static void try_load(char *path)
{
    if (s_dbg_live) {
        char cmd[MAX_PATH + 30];
        snprintf(cmd, sizeof(cmd), "target create \"%s\"\n", path);
        send_to_dbg(&s_dbg, cmd);

        if (s_cli_args[0]) {
            snprintf(cmd, sizeof(cmd), "settings set target.run-args %s\n", s_cli_args);
            send_to_dbg(&s_dbg, cmd);
        }
    } else {
        if (debugger_load(&s_dbg, path, s_cli_args) == DBG_OK) {
            s_dbg_live = 1;
            send_to_dbg(&s_dbg, "settings set use-color false\n");
            send_to_dbg(&s_dbg, "settings set stop-line-count-before 0\n");
            send_to_dbg(&s_dbg, "settings set stop-line-count-after 0\n");
            char msg[MAX_PATH + 32];
            snprintf(msg, sizeof(msg), "[ldb2] Loaded: %s\n", path);
            log_push(msg, strlen(msg));

            debugger_thread_info(&s_dbg);
        } else {
            log_push("[ldb2] ERROR: Failed to spawn lldb.exe\n", 40);
        }
    }
}

static void open_file_dialog(HWND hwnd)
{
    char path[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Executables\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Open Executable";

    if (GetOpenFileNameA(&ofn)) try_load(path);
}

static i32 s_do_send = 0;
static void do_send(void)
{
    if (!s_cmd[0]) return;

    if (s_cmd[0] == ':') {
        i32 ln = atoi(s_cmd + 1);
        if (ln > 0) {
            s_src_sc = ln - 10;
            if (s_src_sc < 0) s_src_sc = 0;
            s_cur_line = ln;
        }
        s_cmd[0] = '\0';
        editor_init(&s_cmd_editor, s_cmd, 512);
        return;
    }

    char echo[520];
    snprintf(echo, sizeof(echo), "> %s\n", s_cmd);
    log_push(echo, strlen(echo));

    if (s_dbg_live) {
        char full[514];
        snprintf(full, sizeof(full), "%s\n", s_cmd);
        send_to_dbg(&s_dbg, full);
    }

    s_cmd[0] = '\0';
    editor_init(&s_cmd_editor, s_cmd, 512);
    s_log_sc = 0;
    s_active_input = INPUT_CMD;
}

static i32 toolbar_btn(f32 *bx, f32 by, const char *lbl, i32 mx, i32 my, i32 clicked)
{
    f32 bw = (f32)((i32)strlen(lbl) * s_ui_fw) + 24;
    f32 bh = (f32)(s_ui_fh + 14);
    i32 hot = (mx >= (i32)*bx && mx < (i32)(*bx + bw) &&
               my >= (i32)by && my < (i32)(by + bh));

    rect(*bx, by, bw, bh,
         hot ? COLOR_BG_HOVER_R : COLOR_BG_TOOLBAR_R,
         hot ? COLOR_BG_HOVER_G : COLOR_BG_TOOLBAR_G,
         hot ? COLOR_BG_HOVER_B : COLOR_BG_TOOLBAR_B, 255);

    text_ui(lbl, *bx + (bw - (f32)((i32)strlen(lbl) * s_ui_fw)) / 2,
            by + (bh - (f32)s_ui_fh) / 2 - 3,
            hot ? COLOR_TEXT_BRIGHT_R : COLOR_TEXT_R,
            hot ? COLOR_TEXT_BRIGHT_G : COLOR_TEXT_G,
            hot ? COLOR_TEXT_BRIGHT_B : COLOR_TEXT_B);

    i32 r = hot && clicked;
    *bx += bw + 8;
    return r;
}

/* ═══════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════ */
int main(i32 argc, char **argv)
{
    RGFW_window *win = RGFW_createWindow("ldb2 - LLDB GUI Frontend",
                                         50, 50, 1800, 1000,
                                         RGFW_windowMaximize | RGFW_windowOpenGL);
    if (!win) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    s_hdc = win->src.hdc;
    font_build(s_fsz);
    font_build_ui();

    editor_init(&s_cmd_editor, s_cmd, 512);
    editor_init(&s_watch_editor, s_watch_new, 128);
    editor_init(&s_args_editor, s_cli_args, 256);
    editor_init(&s_edit_watch_editor, s_edit_watch_buffer, 128);

    qui_Context ui;
    qui_init(&ui, NULL);
    ui.draw_rect = cb_rect;
    ui.draw_text = cb_text;
    ui.text_width = cb_tw;
    ui.text_height = cb_th;

    if (argc > 1) try_load(argv[1]);
    else log_push("[ldb2] No executable loaded. Click Open or pass exe on command line. Type :N to jump to line.\n", 91);

    i32 s_new_fsz = -1;
    i32 pclick_x = -1, pclick_y = -1, pclick_btn = -1;
    i32 gutter_x = 0, src_cy = 0, src_lh = 1;
    i32 wadd_x = 0, wadd_y = 0, wadd_w = 0, wadd_h = 0;
    i32 wrm_x[WATCH_MAX] = {0}, wrm_y[WATCH_MAX] = {0};
    i32 double_clicked = 0;

    RGFW_event ev = {0};
    HWND hwnd = (HWND)(uintptr_t)win->src.window;
    s_hwnd = hwnd;

    while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
        i32 W = win->w, H = win->h;
        const i32 TB_H = 60;
        const i32 CMD_H = 50;
        const i32 WORK_H = H - TB_H - CMD_H;

        i32 left_width = (W * s_left_width) / 100;
        i32 right_width = W - left_width - SPLITTER_SIZE;

        i32 source_height = (WORK_H * s_source_height) / 100;
        i32 args_height = 80;
        i32 log_height = WORK_H - source_height - args_height - SPLITTER_SIZE;

        i32 top_right_height = (WORK_H * s_top_right_height) / 100;
        i32 mid_right_height = (WORK_H * s_mid_right_height) / 100;
        i32 bottom_right_height = WORK_H - top_right_height - mid_right_height - (SPLITTER_SIZE * 2);

        i32 THREADS_W     = right_width * s_threads_pct / 100;
        i32 BREAKPOINTS_W = right_width * s_bp_pct      / 100;
        i32 REGISTERS_W   = right_width - THREADS_W - BREAKPOINTS_W;
        i32 LOCALS_W      = right_width * s_locals_pct  / 100;
        i32 WATCH_W       = right_width - LOCALS_W;

        if (THREADS_W     < 80) THREADS_W     = 80;
        if (BREAKPOINTS_W < 80) BREAKPOINTS_W = 80;
        if (REGISTERS_W   < 80) REGISTERS_W   = right_width - THREADS_W - BREAKPOINTS_W;
        if (LOCALS_W      < 80) LOCALS_W      = 80;
        if (WATCH_W       < 80) { LOCALS_W = right_width - 80; WATCH_W = 80; }

        i32 splitter_left_right_x  = left_width;
        i32 splitter_source_log_y  = TB_H + source_height;
        i32 splitter_top_mid_y     = TB_H + top_right_height;
        i32 splitter_mid_bottom_y  = TB_H + top_right_height + mid_right_height + SPLITTER_SIZE;

        i32 splitter_left_right_hot  = check_splitter_hit(s_mx, s_my, splitter_left_right_x, TB_H, 1);
        i32 splitter_source_log_hot  = check_splitter_hit(s_mx, s_my, left_width, splitter_source_log_y, 0);
        i32 splitter_top_mid_hot     = check_splitter_hit(s_mx, s_my, left_width + SPLITTER_SIZE, splitter_top_mid_y, 0);
        i32 splitter_mid_bottom_hot  = check_splitter_hit(s_mx, s_my, left_width + SPLITTER_SIZE, splitter_mid_bottom_y, 0);

        i32 rx0              = left_width + SPLITTER_SIZE;
        i32 spl_th_x         = rx0 + THREADS_W;
        i32 spl_bp_x         = rx0 + THREADS_W + BREAKPOINTS_W;
        i32 spl_lw_x         = rx0 + LOCALS_W;
        i32 top_row_y0       = TB_H,  top_row_y1 = TB_H + top_right_height;
        i32 mid_row_y0       = TB_H + top_right_height + SPLITTER_SIZE;
        i32 mid_row_y1       = mid_row_y0 + mid_right_height;

        i32 spl_th_hot  = (s_mx >= spl_th_x - SPLITTER_HOT_SIZE && s_mx <= spl_th_x + SPLITTER_HOT_SIZE
                           && s_my >= top_row_y0 && s_my <= top_row_y1);
        i32 spl_bp_hot  = (s_mx >= spl_bp_x - SPLITTER_HOT_SIZE && s_mx <= spl_bp_x + SPLITTER_HOT_SIZE
                           && s_my >= top_row_y0 && s_my <= top_row_y1);
        i32 spl_lw_hot  = (s_mx >= spl_lw_x - SPLITTER_HOT_SIZE && s_mx <= spl_lw_x + SPLITTER_HOT_SIZE
                           && s_my >= mid_row_y0 && s_my <= mid_row_y1);

        while (RGFW_window_checkEvent(win, &ev)) {
            switch (ev.type) {
            case RGFW_quit: goto shutdown;
            case RGFW_mousePosChanged:
                s_mx = ev.mouse.x;
                s_my = ev.mouse.y;
                qui_mouse_move(&ui, s_mx, s_my);
                break;
            case RGFW_mouseButtonPressed:
                qui_mouse_down(&ui, s_mx, s_my);
                s_mouse_held = 1;
                pclick_x = s_mx;
                pclick_y = s_my;
                pclick_btn = (ev.button.value == RGFW_mouseRight) ? 1 : 0;

                double_clicked = is_double_click(s_mx, s_my, GetTickCount());

                if (s_menu_open) {
                    i32 mx2 = s_mx, my2 = s_my;
                    i32 iw = s_fw * 22 + 12;
                    i32 mh = (s_fh + 8) * MENU_COUNT + 12;
                    if (!(mx2 >= s_menu_x && mx2 < s_menu_x + iw &&
                          my2 >= s_menu_y && my2 < s_menu_y + mh))
                        s_menu_open = 0;
                }
                if (s_my > H - CMD_H) s_active_input = INPUT_CMD;
                break;
            case RGFW_mouseButtonReleased:
                qui_mouse_up(&ui, s_mx, s_my);
                s_mouse_held = 0;
                pclick_btn = -1;
                break;
            case RGFW_mouseScroll: {
                i32 d = ev.scroll.y > 0 ? -4 : 4;
                i32 mx2 = s_mx, my2 = s_my;
                i32 rx0 = left_width + SPLITTER_SIZE;
                if (mx2 < left_width && my2 >= TB_H && my2 < TB_H + source_height)
                    s_src_sc += d;
                else if (mx2 < left_width)
                    s_log_sc -= d;
                else if (my2 >= TB_H && my2 < TB_H + top_right_height) {
                    /* Top-right row: threads | breakpoints | registers */
                    i32 spl_th  = rx0 + THREADS_W;
                    i32 spl_bp  = rx0 + THREADS_W + BREAKPOINTS_W;
                    if (mx2 < spl_th)
                        s_thread_scroll += d;
                    else if (mx2 < spl_bp)
                        s_bp_scroll += d;
                    else
                        s_register_scroll += d;
                } else if (my2 < TB_H + top_right_height + mid_right_height + SPLITTER_SIZE) {
                    /* Mid row: locals | watch */
                    if (mx2 < rx0 + LOCALS_W)
                        s_locals_sc += d;
                    else
                        s_wsc += d;
                } else
                    s_stack_sc += d;
                break;
            }
            case RGFW_keyPressed:
                s_shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1 : 0;

                if (ev.key.value == RGFW_F5) {
                    if (s_dbg_live) {
                        debugger_continue(&s_dbg);
                        s_need_refresh = 1;
                    }
                    break;
                }
                if (ev.key.value == RGFW_F9) {
                    if (s_src_n && s_cur_line > 0) {
                        bp_toggle(s_cur_line);
                        if (s_dbg_live) {
                            char c[64];
                            snprintf(c, sizeof(c),
                                     bp_has(s_cur_line) ?
                                     "breakpoint set --line %d\n" :
                                     "breakpoint clear --line %d\n",
                                     s_cur_line);
                            send_to_dbg(&s_dbg, c);
                        }
                    }
                    break;
                }
                if (ev.key.value == RGFW_F10) {
                    if (s_dbg_live) {
                        debugger_step_over(&s_dbg);
                        s_need_refresh = 1;
                    }
                    break;
                }
                if (ev.key.value == RGFW_F11) {
                    if (s_dbg_live) {
                        debugger_step_into(&s_dbg);
                        s_need_refresh = 1;
                    }
                    break;
                }
                if (ev.key.value == RGFW_left) {
                    input_left(s_shift_pressed);
                    break;
                }
                if (ev.key.value == RGFW_right) {
                    input_right(s_shift_pressed);
                    break;
                }
                if (ev.key.value == RGFW_home) {
                    input_home(s_shift_pressed);
                    break;
                }
                if (ev.key.value == RGFW_end) {
                    input_end(s_shift_pressed);
                    break;
                }
                if (ev.key.value == RGFW_backSpace) {
                    qui_feed_key_backspace(&ui);
                    input_backspace();
                    break;
                }
                if (ev.key.value == RGFW_delete) {
                    input_delete();
                    break;
                }
                if (ev.key.value == RGFW_return) {
                    qui_feed_key_enter(&ui);
                    if (s_active_input == INPUT_CMD) {
                        s_do_send = 1;
                    } else if (s_active_input == INPUT_WATCH) {
                        if (s_editing_watch_index >= 0) {
                            watch_update(s_editing_watch_index, s_edit_watch_buffer);
                            s_editing_watch_index = -1;
                            s_edit_watch_buffer[0] = '\0';
                        } else {
                            watch_add(s_watch_new);
                            s_watch_new[0] = '\0';
                            editor_init(&s_watch_editor, s_watch_new, 128);
                        }
                    }
                    break;
                }
                if (ev.key.value == RGFW_escape) {
                    if (s_editing_watch_index >= 0) {
                        s_editing_watch_index = -1;
                        s_edit_watch_buffer[0] = '\0';
                    }
                    break;
                }
                /* ── Ctrl+C / Ctrl+V / Ctrl+A ────────────────────────── */
                if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    char sym = (char)(ev.key.sym);
                    /* RGFW may report the raw control code (^V=22, ^C=3, ^A=1)
                       OR the plain letter depending on platform — check both */
                    if (sym == 'v' || sym == 'V' || sym == 22) { clipboard_paste(); break; }
                    if (sym == 'c' || sym == 'C' || sym == 3)  { clipboard_copy();  break; }
                    if (sym == 'a' || sym == 'A' || sym == 1)  {
                        editor_select_all(get_active_editor());
                        break;
                    }
                    break; /* swallow all other Ctrl+key combos */
                }
                {
                    char c = (char)ev.key.sym;
                    if (c >= 32 && c < 127) {
                        input_char(c);
                    }
                }
                break;
            default:
                break;
            }
        }

        if (s_do_send) {
            do_send();
            s_do_send = 0;
        }
        if (s_new_fsz > 0) {
            font_build(s_new_fsz);
            s_new_fsz = -1;
        }

        if (s_dbg_live) {
            char chunk[16384];
            DWORD n = debugger_poll(&s_dbg, chunk, sizeof(chunk));
            if (n > 0) {
                classify(chunk);
                if (!s_suppress_log) {
                    log_push(chunk, (size_t)n);
                    s_log_sc = 0;
                }

                i32 stopped = strstr(chunk, "stop reason") != NULL ||
                              (strstr(chunk, "Process") && strstr(chunk, "stopped") != NULL);

                if (strstr(chunk, "step over failed") ||
                    strstr(chunk, "Could not create return address breakpoint")) {
                    if (s_thunk_recover == 0) {
                        s_thunk_recover = 1;
                        send_to_dbg(&s_dbg, "stepi\n");
                    }
                } else if (s_thunk_recover == 1 && stopped) {
                    s_thunk_recover = 2;
                    send_to_dbg(&s_dbg, "finish\n");
                } else if (s_thunk_recover == 2 && stopped) {
                    s_thunk_recover = 0;
                    s_need_refresh = 1;
                }

                /* Detect launch/resume FIRST so that if "launched" and
                   "stop reason" arrive in the same chunk (program breaks
                   immediately on run), s_need_refresh is already set when
                   the check below runs — otherwise locals never refresh. */
                if (strstr(chunk, "Process") && strstr(chunk, "launched"))
                    s_need_refresh = 1;
                if (strstr(chunk, "Process") && strstr(chunk, "resuming"))
                    s_need_refresh = 1;

                if (s_need_refresh && stopped && s_thunk_recover == 0) {
                    s_need_refresh = 0;
                    refresh_panels();
                }
            }
        }

        W = win->w;
        H = win->h;

        glViewport(0, 0, W, H);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, W, H, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        /* Toolbar */
        i32 TB_BTN_Y = (TB_H - s_ui_fh - 12) / 2;
        rect(0, 0, (f32)W, (f32)TB_H,
             COLOR_BG_TOOLBAR_R, COLOR_BG_TOOLBAR_G, COLOR_BG_TOOLBAR_B, 255);
        line_hline(0, (f32)W, (f32)TB_H,
                   COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

        f32 bx = 12.0f;
        i32 tb_clicked = (pclick_btn == 0 && pclick_x >= 0 && pclick_y < TB_H);

        i32 do_open  = toolbar_btn(&bx, (f32)TB_BTN_Y, "Open", s_mx, s_my, tb_clicked);
        bx += 20;

        text_ui("Font:", bx, (f32)TB_BTN_Y + 2,
                COLOR_TEXT_DIM_R, COLOR_TEXT_DIM_G, COLOR_TEXT_DIM_B);
        bx += (f32)(5 * s_ui_fw + 8);

        i32 fsz_dn = toolbar_btn(&bx, (f32)TB_BTN_Y, "-", s_mx, s_my, tb_clicked);
        char fszbuf[16];
        snprintf(fszbuf, sizeof(fszbuf), " %d ", s_fsz);
        text_ui(fszbuf, bx, (f32)TB_BTN_Y + 2,
                COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
        bx += (f32)((i32)strlen(fszbuf) * s_ui_fw + 2);
        i32 fsz_up = toolbar_btn(&bx, (f32)TB_BTN_Y, "+", s_mx, s_my, tb_clicked);

        text_ui("F9=BP  F10=Step  F11=Into  F5=Cont  :N=GoTo",
                (f32)(W - s_ui_fw * 44 - 16), (f32)TB_BTN_Y + 2,
                COLOR_TEXT_DIM_R, COLOR_TEXT_DIM_G, COLOR_TEXT_DIM_B);

        if (do_open) open_file_dialog(hwnd);
        if (fsz_dn && s_fsz > 8) s_new_fsz = s_fsz - 1;
        if (fsz_up && s_fsz < 36) s_new_fsz = s_fsz + 1;

        /* Left column */
        draw_source(0, TB_H, left_width, source_height, H, &gutter_x, &src_cy, &src_lh);

        draw_cli_args_panel(0, TB_H + source_height + SPLITTER_SIZE, left_width, args_height, H,
                            s_mx, s_my, (pclick_btn == 0 && pclick_x >= 0));

        draw_log(0, TB_H + source_height + args_height + SPLITTER_SIZE * 2,
                 left_width, log_height, H);

        /* Right column */
        i32 right_x = left_width + SPLITTER_SIZE;

        draw_threads_panel(right_x, TB_H, THREADS_W, top_right_height, H,
                           s_mx, s_my, (pclick_btn == 0 && pclick_x >= 0));

        draw_breakpoints_panel(right_x + THREADS_W + SPLITTER_SIZE, TB_H, BREAKPOINTS_W, top_right_height, H,
                               s_mx, s_my, (pclick_btn == 0 && pclick_x >= 0));

        draw_registers_panel(right_x + THREADS_W + SPLITTER_SIZE + BREAKPOINTS_W + SPLITTER_SIZE, TB_H,
                             REGISTERS_W - SPLITTER_SIZE, top_right_height, H,
                             s_mx, s_my, (pclick_btn == 0 && pclick_x >= 0));

        draw_textpanel("LOCALS", s_locals,
                       right_x, TB_H + top_right_height + SPLITTER_SIZE,
                       LOCALS_W, mid_right_height, H, &s_locals_sc);

        draw_watch(right_x + LOCALS_W + SPLITTER_SIZE, TB_H + top_right_height + SPLITTER_SIZE,
                   WATCH_W - SPLITTER_SIZE, mid_right_height, H,
                   s_mx, s_my,
                   (pclick_btn == 0 && pclick_x >= 0),
                   double_clicked,
                   &wadd_x, &wadd_y, &wadd_w, &wadd_h, wrm_x, wrm_y);

        draw_textpanel("CALL STACK", s_stack,
                       right_x, TB_H + top_right_height + mid_right_height + SPLITTER_SIZE * 2,
                       right_width, bottom_right_height, H, &s_stack_sc);

        /* Draw splitters */
        draw_splitter(left_width, TB_H, SPLITTER_SIZE, WORK_H, splitter_left_right_hot || (s_splitter.is_dragging && s_splitter.splitter_index == 0));
        draw_splitter(0, TB_H + source_height, left_width, SPLITTER_SIZE, splitter_source_log_hot);
        draw_splitter(right_x, TB_H + top_right_height, right_width, SPLITTER_SIZE, splitter_top_mid_hot);
        draw_splitter(right_x, TB_H + top_right_height + mid_right_height + SPLITTER_SIZE,
                     right_width, SPLITTER_SIZE, splitter_mid_bottom_hot);
        /* Vertical splitters inside right column */
        draw_splitter(spl_th_x, TB_H,                   SPLITTER_SIZE, top_right_height, spl_th_hot || (s_splitter.is_dragging && s_splitter.splitter_index == 4));
        draw_splitter(spl_bp_x, TB_H,                   SPLITTER_SIZE, top_right_height, spl_bp_hot || (s_splitter.is_dragging && s_splitter.splitter_index == 5));
        draw_splitter(spl_lw_x, mid_row_y0,             SPLITTER_SIZE, mid_right_height, spl_lw_hot || (s_splitter.is_dragging && s_splitter.splitter_index == 6));

        /* Main dividers */
        line_vline((f32)left_width, (f32)TB_H, (f32)(H - CMD_H),
                   COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
        line_hline(0, (f32)W, (f32)(TB_H + source_height),
                   COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
        line_hline((f32)right_x, (f32)W, (f32)(TB_H + top_right_height),
                   COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
        line_hline((f32)right_x, (f32)W, (f32)(TB_H + top_right_height + mid_right_height + SPLITTER_SIZE),
                   COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

        /* Context menu */
        {
            i32 hit = draw_menu(s_mx, s_my, pclick_btn == 0 && pclick_x >= 0);
            if (hit >= 0 && s_menu_open) {
                s_menu_open = 0;
                if (hit == MENU_SET_BP || hit == MENU_CLR_BP) {
                    bp_toggle(s_menu_line);
                    if (s_dbg_live) {
                        char c[64];
                        snprintf(c, sizeof(c),
                                 bp_has(s_menu_line) ?
                                 "breakpoint set --line %d\n" :
                                 "breakpoint clear --line %d\n",
                                 s_menu_line);
                        send_to_dbg(&s_dbg, c);
                    }
                } else if (hit == MENU_RUN_TO) {
                    if (s_dbg_live) {
                        char c[64];
                        snprintf(c, sizeof(c), "thread until %d\n", s_menu_line);
                        send_to_dbg(&s_dbg, c);
                    }
                } else if (hit == MENU_ADD_WATCH) {
                    s_watch_new[0] = '\0';
                    editor_init(&s_watch_editor, s_watch_new, 128);
                    s_active_input = INPUT_WATCH;
                } else if (hit == MENU_GOTO) {
                    s_src_sc = s_menu_line - 10;
                    if (s_src_sc < 0) s_src_sc = 0;
                }
            }
        }

        if (s_splitter.is_dragging) {
            if (!s_mouse_held) {
                s_splitter.is_dragging = 0;
            } else {
                i32 delta;
                switch (s_splitter.splitter_index) {
                    case 0:
                        delta = s_mx - s_splitter.start_x;
                        s_left_width = (s_splitter.start_value1 + delta) * 100 / W;
                        if (s_left_width < 20) s_left_width = 20;
                        if (s_left_width > 80) s_left_width = 80;
                        break;
                    case 1:
                        delta = s_my - s_splitter.start_y;
                        s_source_height = (s_splitter.start_value1 + delta) * 100 / WORK_H;
                        if (s_source_height < 20) s_source_height = 20;
                        if (s_source_height > 80) s_source_height = 80;
                        break;
                    case 2:
                        delta = s_my - s_splitter.start_y;
                        s_top_right_height = (s_splitter.start_value1 + delta) * 100 / WORK_H;
                        if (s_top_right_height < 10) s_top_right_height = 10;
                        if (s_top_right_height > 60) s_top_right_height = 60;
                        break;
                    case 3:
                        delta = s_my - s_splitter.start_y;
                        s_mid_right_height = (s_splitter.start_value1 + delta) * 100 / WORK_H;
                        if (s_mid_right_height < 10) s_mid_right_height = 10;
                        if (s_mid_right_height > 60) s_mid_right_height = 60;
                        break;
                    case 4: /* threads | breakpoints vertical splitter */
                        delta = s_mx - s_splitter.start_x;
                        s_threads_pct = (s_splitter.start_value1 + delta) * 100 / right_width;
                        if (s_threads_pct < 10) s_threads_pct = 10;
                        if (s_threads_pct > 70) s_threads_pct = 70;
                        break;
                    case 5: /* breakpoints | registers vertical splitter */
                        delta = s_mx - s_splitter.start_x;
                        /* start_value1 = threads_w+bp_w at drag start, keep threads fixed */
                        { i32 new_bp_right = s_splitter.start_value1 + delta;
                          i32 tw = right_width * s_threads_pct / 100;
                          i32 new_bp = new_bp_right - tw;
                          s_bp_pct = new_bp * 100 / right_width;
                          if (s_bp_pct < 10) s_bp_pct = 10;
                          if (s_bp_pct > 70) s_bp_pct = 70; }
                        break;
                    case 6: /* locals | watch vertical splitter */
                        delta = s_mx - s_splitter.start_x;
                        s_locals_pct = (s_splitter.start_value1 + delta) * 100 / right_width;
                        if (s_locals_pct < 10) s_locals_pct = 10;
                        if (s_locals_pct > 80) s_locals_pct = 80;
                        break;
                }
            }
        } else if (pclick_btn == 0 && pclick_x >= 0) {
            /* Start a drag only if the click landed on a splitter */
            i32 cx = pclick_x, cy = pclick_y;
            i32 on_lr  = (abs(cx - left_width) < SPLITTER_HOT_SIZE && cy >= TB_H);
            i32 on_sl  = (abs(cy - (TB_H + source_height)) < SPLITTER_HOT_SIZE && cx < left_width);
            i32 on_tm  = (abs(cy - (TB_H + top_right_height)) < SPLITTER_HOT_SIZE && cx >= left_width + SPLITTER_SIZE);
            i32 on_mb  = (abs(cy - (TB_H + top_right_height + mid_right_height + SPLITTER_SIZE)) < SPLITTER_HOT_SIZE && cx >= left_width + SPLITTER_SIZE);
            i32 on_th  = (abs(cx - spl_th_x) < SPLITTER_HOT_SIZE && cy >= top_row_y0 && cy <= top_row_y1);
            i32 on_bp  = (abs(cx - spl_bp_x) < SPLITTER_HOT_SIZE && cy >= top_row_y0 && cy <= top_row_y1);
            i32 on_lw  = (abs(cx - spl_lw_x) < SPLITTER_HOT_SIZE && cy >= mid_row_y0 && cy <= mid_row_y1);

            if (on_lr) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 0;
                s_splitter.start_x = cx;    s_splitter.start_value1  = left_width;
            } else if (on_sl) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 1;
                s_splitter.start_y = cy;    s_splitter.start_value1  = source_height;
            } else if (on_tm) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 2;
                s_splitter.start_y = cy;    s_splitter.start_value1  = top_right_height;
            } else if (on_mb) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 3;
                s_splitter.start_y = cy;    s_splitter.start_value1  = mid_right_height;
            } else if (on_th) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 4;
                s_splitter.start_x = cx;    s_splitter.start_value1  = THREADS_W;
            } else if (on_bp) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 5;
                s_splitter.start_x = cx;    s_splitter.start_value1  = THREADS_W + BREAKPOINTS_W;
            } else if (on_lw) {
                s_splitter.is_dragging = 1; s_splitter.splitter_index = 6;
                s_splitter.start_x = cx;    s_splitter.start_value1  = LOCALS_W;
            }
            /* If a drag started, consume the click so panels don't react */
            if (s_splitter.is_dragging) {
                pclick_x = -1; pclick_y = -1; pclick_btn = -1;
            }
        }

        /* Gutter and source panel click handling */
        if (pclick_x >= 0) {
            i32 cx = pclick_x, cy2 = pclick_y;

            i32 in_source_panel = (cx >= 0 && cx < left_width &&
                                   cy2 >= TB_H && cy2 < TB_H + source_height);

            /* Gutter click (left side of source panel) - toggle breakpoint */
            if (in_source_panel && cx < gutter_x && pclick_btn == 0) {
                if (s_src_n > 0) {
                    i32 row = (cy2 - src_cy) / (src_lh > 0 ? src_lh : 1);
                    i32 ln = s_src_sc + row + 1;
                    if (ln >= 1 && ln <= s_src_n) {
                        bp_toggle(ln);
                        if (s_dbg_live) {
                            char c[64];
                            snprintf(c, sizeof(c),
                                     bp_has(ln) ?
                                     "breakpoint set --line %d\n" :
                                     "breakpoint clear --line %d\n",
                                     ln);
                            send_to_dbg(&s_dbg, c);
                        }
                    }
                }
            }

            /* Right-click in source panel (not gutter) - context menu */
            if (in_source_panel && cx >= gutter_x && pclick_btn == 1) {
                i32 row = (cy2 - src_cy) / (src_lh > 0 ? src_lh : 1);
                s_menu_line = s_src_sc + row + 1;
                s_menu_x = cx;
                s_menu_y = cy2;

                i32 mw = s_fw * 22 + 12;
                i32 mh = (s_fh + 8) * MENU_COUNT + 12;
                if (s_menu_x + mw > W) s_menu_x = W - mw - 8;
                if (s_menu_y + mh > H) s_menu_y = H - mh - 8;
                s_menu_open = 1;
            }

            /* Watch panel button clicks */
            if (pclick_btn == 0 && cx >= wadd_x && cx < wadd_x + wadd_w &&
                cy2 >= wadd_y && cy2 < wadd_y + wadd_h) {
                watch_add(s_watch_new);
                s_watch_new[0] = '\0';
                editor_init(&s_watch_editor, s_watch_new, 128);
            }

            /* Watch remove buttons */
            for (i32 i = 0; i < s_wn; i++) {
                if (pclick_btn == 0 && cx >= wrm_x[i] && cx < wrm_x[i] + s_fw * 3 + 2 &&
                    cy2 >= wrm_y[i] && cy2 < wrm_y[i] + (s_fh + 6)) {
                    watch_remove(i);
                    break;
                }
            }

            pclick_x = -1;
            pclick_y = -1;
            pclick_btn = -1;
        }

        /* Command bar */
        {
            i32 ct = H - CMD_H;
            rect(0, (f32)ct, (f32)W, (f32)CMD_H,
                 COLOR_BG_INPUT_R, COLOR_BG_INPUT_G, COLOR_BG_INPUT_B, 255);
            line_hline(0, (f32)W, (f32)ct,
                       COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

            f32 iy = (f32)(ct + (CMD_H - s_fh) / 2);
            text_ui("(lldb)", 12, iy, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);

            f32 tx = 12.0f + (f32)(6 * s_ui_fw) + 12.0f;
            f32 sbw = (f32)((i32)strlen("   Send   ") * s_ui_fw) + 32;
            f32 tw2 = (f32)W - tx - 16.0f - sbw;

            i32 focused = (s_active_input == INPUT_CMD);
            rect(tx, iy - 2, tw2, (f32)(s_fh + 8),
                 focused ? COLOR_BG_ACTIVE_R : 0x2A,
                 focused ? COLOR_BG_ACTIVE_G : 0x2A,
                 focused ? COLOR_BG_ACTIVE_B : 0x2A, 255);

            i32 text_x = (i32)tx + 6;

            if (focused && s_cmd_editor.has_selection) {
                i32 sel_start = s_cmd_editor.selection_start < s_cmd_editor.selection_end ?
                                s_cmd_editor.selection_start : s_cmd_editor.selection_end;
                i32 sel_end = s_cmd_editor.selection_start > s_cmd_editor.selection_end ?
                              s_cmd_editor.selection_start : s_cmd_editor.selection_end;

                if (sel_start > 0) {
                    text_at_len(s_cmd, sel_start, (f32)text_x, iy,
                                s_cmd[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                                s_cmd[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                                s_cmd[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
                }

                rect((f32)(text_x + sel_start * s_fw), iy - 2,
                     (f32)((sel_end - sel_start) * s_fw), (f32)s_fh + 4,
                     0x26, 0x4F, 0x78, 255);

                text_at_len(s_cmd + sel_start, sel_end - sel_start,
                            (f32)(text_x + sel_start * s_fw), iy,
                            COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);

                if (sel_end < (i32)s_cmd_editor.length) {
                    text_at_len(s_cmd + sel_end, s_cmd_editor.length - sel_end,
                                (f32)(text_x + sel_end * s_fw), iy,
                                s_cmd[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                                s_cmd[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                                s_cmd[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
                }
            } else {
                text_at(s_cmd[0] ? s_cmd : "type command or :N to jump to line",
                        (f32)text_x, iy,
                        s_cmd[0] ? COLOR_TEXT_R : COLOR_TEXT_DIM_R,
                        s_cmd[0] ? COLOR_TEXT_G : COLOR_TEXT_DIM_G,
                        s_cmd[0] ? COLOR_TEXT_B : COLOR_TEXT_DIM_B);
            }

            if (focused && (GetTickCount() / 500) % 2) {
                i32 cursor_x = text_x + s_cmd_editor.cursor_pos * s_fw;
                rect((f32)cursor_x, iy - 2, 2, (f32)s_fh + 4,
                     COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 200);
            }

            f32 btn_x = tx + tw2 + 8;
            f32 btn_y = iy - 2;
            f32 btn_h = (f32)(s_fh + 8);
            i32 btn_hot = (s_mx >= (i32)btn_x && s_mx < (i32)(btn_x + sbw) &&
                           s_my >= (i32)btn_y && s_my < (i32)(btn_y + btn_h));
            rect(btn_x, btn_y, sbw, btn_h,
                 btn_hot ? COLOR_ACCENT_HOVER_R : COLOR_ACCENT_R,
                 btn_hot ? COLOR_ACCENT_HOVER_G : COLOR_ACCENT_G,
                 btn_hot ? COLOR_ACCENT_HOVER_B : COLOR_ACCENT_B, 255);
            {
                const char *lbl = "Send";
                f32 lw = (f32)((i32)strlen(lbl) * s_fw);
                text_at(lbl, btn_x + (sbw - lw) / 2, btn_y + (btn_h - s_fh) / 2 - 2,
                        COLOR_TEXT_BRIGHT_R, COLOR_TEXT_BRIGHT_G, COLOR_TEXT_BRIGHT_B);
            }
            if (btn_hot && s_mouse_held == 0 && pclick_btn == 0) s_do_send = 1;
        }

        RGFW_window_swapBuffers_OpenGL(win);
        glFlush();
    }

shutdown:
    qui_cleanup(&ui);
    RGFW_window_close(win);
    return 0;
}
