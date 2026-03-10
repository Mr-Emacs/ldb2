/* quickui.h - Modernized version with cleaner styling */

#ifndef QUICK_UI_H
#define QUICK_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================================================
 * CORE TYPES AND CONSTANTS
 * ================================================================================================ */

typedef uint32_t qui_Id;

typedef enum {
    QUI_OK = 0,
    QUI_ERROR_NULL_POINTER,
    QUI_ERROR_INVALID_VALUE,
    QUI_ERROR_NOT_INITIALIZED,
    QUI_ERROR_BUFFER_TOO_SMALL,
    QUI_ERROR_INVALID_STATE
} qui_Result;

typedef struct { int x; int y; } qui_Vec2;
typedef struct { int r, g, b, a; } qui_Color;

typedef struct {
    void *data;
    int width;
    int height;
    int channels;
} qui_Image;

typedef struct { int width; int height; int pos_x; int pos_y; } qui_Rect;

typedef enum {
    QUI_COLOR_BACKGROUND = 0,
    QUI_COLOR_FOREGROUND,
    QUI_COLOR_HOT,
    QUI_COLOR_ACTIVE,
    QUI_COLOR_TEXT,
    QUI_COLOR_TEXT_DIM,
    QUI_COLOR_BORDER,
    QUI_COLOR_COUNT
} qui_ColorType;

/* ================================================================================================
 * MAIN CONTEXT STRUCTURE
 * ================================================================================================ */

typedef struct qui_Context {
    /* Display properties */
    int width;
    int height;
    bool is_running;

    /* UI state */
    qui_Id active_id;
    qui_Id hot_id;
    qui_Id keyboard_focus_id;
    qui_Id last_id;

    /* Input state */
    qui_Vec2 mouse_pos;
    int mouse_down;
    int mouse_pressed;
    int mouse_released;
    int key_pressed;
    int key_backspace;
    int key_enter;

    /* Color scheme */
    qui_Color colors[QUI_COLOR_COUNT];

    /* Layout state */
    float cursor_x;
    float cursor_y;
    float spacing_x;
    float spacing_y;
    float layout_offset_x;
    float layout_offset_y;

    float saved_cursor_x;
    float saved_cursor_y;
    float saved_offset_x;
    float saved_offset_y;

    float drag_offset_x;
    float drag_offset_y;

    /* Font properties */
    void *font;
    float font_size;
    float font_spacing;

    /* Window state */
    bool popup_open;
    qui_Vec2 popup_pos;
    qui_Vec2 popup_size;

    /* User data */
    void *userdata;

    /* Rendering callbacks */
    void (*draw_rect)(struct qui_Context* ctx, float x, float y, float w, float h, qui_Color col);
    void (*draw_text)(struct qui_Context* ctx, const char *text, float x, float y);
    float (*text_width)(struct qui_Context* ctx, const char *text);
    float (*text_height)(struct qui_Context* ctx, const char *text);
    void (*draw_image)(struct qui_Context* ctx, qui_Image *img, float x, float y, float w, float h);
    void (*draw_rect_border)(struct qui_Context* ctx, float x, float y, float w, float h, qui_Color col);
} qui_Context;

/* ================================================================================================
 * CORE FUNCTIONS
 * ================================================================================================ */

qui_Result qui_init(qui_Context *ctx, void *user_data);
qui_Result qui_cleanup(qui_Context *ctx);
qui_Result qui_begin(qui_Context *ctx, float start_x, float start_y);
qui_Result qui_end(qui_Context *ctx);
const char* qui_get_error_string(qui_Result result);

/* ================================================================================================
 * INPUT HANDLING
 * ================================================================================================ */

qui_Result qui_mouse_down(qui_Context *ctx, int x, int y);
qui_Result qui_mouse_up(qui_Context *ctx, int x, int y);
qui_Result qui_mouse_move(qui_Context *ctx, int x, int y);
qui_Result qui_feed_mouse_button(qui_Context *ctx, int pressed);
qui_Result qui_feed_key_backspace(qui_Context *ctx);
qui_Result qui_feed_key_enter(qui_Context *ctx);

/* ================================================================================================
 * UTILITY FUNCTIONS
 * ================================================================================================ */

qui_Vec2 qui_vec2(int x, int y);
qui_Rect qui_rect(int width, int height, int pos_x, int pos_y);
qui_Color qui_color(int r, int g, int b, int a);
qui_Result qui_set_color(qui_Context *ctx, qui_ColorType type, qui_Color color);
qui_Result qui_get_color(qui_Context *ctx, qui_ColorType type, qui_Color *color);
qui_Result qui_set_font(qui_Context *ctx, void *font, float font_size, float font_spacing);

/* ================================================================================================
 * MODERN UI ELEMENTS
 * ================================================================================================ */

/* Button with modern styling - flat design with subtle hover effect */
int qui_button(qui_Context *ctx, const char *label);

/* Secondary button (less prominent) */
int qui_button_secondary(qui_Context *ctx, const char *label);

/* Checkbox with modern styling */
int qui_checkbox(qui_Context *ctx, const char *label, int *value);

/* Slider with modern track and handle */
int qui_slider(qui_Context *ctx, const char *label, float *value, float min_val, float max_val, float width);

/* Text input with modern styling */
int qui_textbox(qui_Context *ctx, char *buffer, size_t capacity, float width);

/* Horizontal separator line */
void qui_separator(qui_Context *ctx);

/* Label with optional dim styling */
void qui_label(qui_Context *ctx, const char *text, bool dim);

#ifdef __cplusplus
}
#endif

#endif /* QUICK_UI_H */

/* ================================================================================================
 * IMPLEMENTATION
 * ================================================================================================ */

#ifdef QUI_IMPLEMENTATION

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ================================================================================================
 * INTERNAL CONSTANTS
 * ================================================================================================ */

/* Modern VS Code inspired color palette */
static const qui_Color QUI_DEFAULT_COLORS[QUI_COLOR_COUNT] = {
    {30,  30,  30,  255}, /* QUI_COLOR_BACKGROUND - dark background */
    {45,  45,  48,  255}, /* QUI_COLOR_FOREGROUND - panel background */
    {55,  55,  60,  255}, /* QUI_COLOR_HOT - hover state */
    {0,   122, 204, 255}, /* QUI_COLOR_ACTIVE - accent blue */
    {204, 204, 204, 255}, /* QUI_COLOR_TEXT - light gray */
    {110, 110, 110, 255}, /* QUI_COLOR_TEXT_DIM - dimmed text */
    {62,  62,  66,  255}, /* QUI_COLOR_BORDER - border color */
};

#define QUI_DEFAULT_SPACING_X 8.0f
#define QUI_DEFAULT_SPACING_Y 8.0f
#define QUI_FALLBACK_CHAR_WIDTH 8.0f
#define QUI_FALLBACK_TEXT_HEIGHT 16.0f
#define QUI_CORNER_RADIUS 3.0f

#define QUI_VALIDATE_CTX(ctx) do { if (!(ctx)) return QUI_ERROR_NULL_POINTER; } while(0)
#define QUI_VALIDATE_PTR(ptr) do { if (!(ptr)) return QUI_ERROR_NULL_POINTER; } while(0)

/* ================================================================================================
 * INTERNAL HELPERS
 * ================================================================================================ */

static qui_Id qui_gen_id(qui_Context *ctx) {
    if (!ctx) return 0;
    ctx->last_id += 1;
    if (ctx->last_id == 0) ctx->last_id = 1;
    return ctx->last_id;
}

static bool qui_hit_test(qui_Context *ctx, float x, float y, float w, float h) {
    if (!ctx) return false;

    float ox = ctx->layout_offset_x;
    float oy = ctx->layout_offset_y;
    int mx = ctx->mouse_pos.x;
    int my = ctx->mouse_pos.y;

    return (mx >= x + ox && mx <= x + ox + w &&
            my >= y + oy && my <= y + oy + h);
}

static float qui_get_text_width(qui_Context *ctx, const char *text) {
    if (!ctx || !text) return 0.0f;
    if (ctx->text_width) return ctx->text_width(ctx, text);
    return (float)strlen(text) * QUI_FALLBACK_CHAR_WIDTH;
}

static float qui_get_text_height(qui_Context *ctx, const char *text) {
    if (!ctx || !text) return 0.0f;
    if (ctx->text_height) return ctx->text_height(ctx, text);
    return QUI_FALLBACK_TEXT_HEIGHT;
}

static void qui_draw_rect_safe(qui_Context *ctx, qui_Rect *rect, qui_Color color) {
    if (!ctx || !rect || !ctx->draw_rect) return;
    float ox = ctx->layout_offset_x;
    float oy = ctx->layout_offset_y;
    ctx->draw_rect(ctx, rect->pos_x + ox, rect->pos_y + oy,
                   rect->width, rect->height, color);
}

static void qui_draw_rect_border_safe(qui_Context *ctx, qui_Rect *rect, qui_Color color) {
    if (!ctx || !rect || !ctx->draw_rect_border) return;
    float ox = ctx->layout_offset_x;
    float oy = ctx->layout_offset_y;
    ctx->draw_rect_border(ctx, rect->pos_x + ox, rect->pos_y + oy,
                          rect->width, rect->height, color);
}

static void qui_draw_text_safe(qui_Context *ctx, const char *text, float x, float y) {
    if (!ctx || !text || !ctx->draw_text) return;
    float ox = ctx->layout_offset_x;
    float oy = ctx->layout_offset_y;
    ctx->draw_text(ctx, text, x + ox, y + oy);
}

/* ================================================================================================
 * CORE FUNCTIONS
 * ================================================================================================ */

qui_Result qui_init(qui_Context *ctx, void *user_data) {
    QUI_VALIDATE_CTX(ctx);

    memset(ctx, 0, sizeof(*ctx));
    ctx->userdata = user_data;
    ctx->spacing_x = QUI_DEFAULT_SPACING_X;
    ctx->spacing_y = QUI_DEFAULT_SPACING_Y;
    memcpy(ctx->colors, QUI_DEFAULT_COLORS, sizeof(QUI_DEFAULT_COLORS));
    ctx->last_id = 0;

    return QUI_OK;
}

qui_Result qui_cleanup(qui_Context *ctx) {
    QUI_VALIDATE_CTX(ctx);
    return QUI_OK;
}

qui_Result qui_begin(qui_Context *ctx, float start_x, float start_y) {
    QUI_VALIDATE_CTX(ctx);
    if (!isfinite(start_x) || !isfinite(start_y))
        return QUI_ERROR_INVALID_VALUE;

    ctx->cursor_x = start_x;
    ctx->cursor_y = start_y;
    ctx->last_id = 0;
    ctx->hot_id = 0;

    return QUI_OK;
}

qui_Result qui_end(qui_Context *ctx) {
    QUI_VALIDATE_CTX(ctx);
    ctx->mouse_pressed = 0;
    ctx->mouse_released = 0;
    ctx->key_backspace = 0;
    ctx->key_enter = 0;
    return QUI_OK;
}

const char* qui_get_error_string(qui_Result result) {
    switch (result) {
        case QUI_OK: return "Success";
        case QUI_ERROR_NULL_POINTER: return "Null pointer argument";
        case QUI_ERROR_INVALID_VALUE: return "Invalid parameter value";
        case QUI_ERROR_NOT_INITIALIZED: return "Context not initialized";
        case QUI_ERROR_BUFFER_TOO_SMALL: return "Buffer capacity too small";
        case QUI_ERROR_INVALID_STATE: return "Invalid operation for current state";
        default: return "Unknown error";
    }
}

/* ================================================================================================
 * INPUT HANDLING
 * ================================================================================================ */

qui_Result qui_mouse_down(qui_Context *ctx, int x, int y) {
    QUI_VALIDATE_CTX(ctx);
    ctx->mouse_pos.x = x;
    ctx->mouse_pos.y = y;
    ctx->mouse_down = 1;
    ctx->mouse_pressed = 1;
    return QUI_OK;
}

qui_Result qui_mouse_up(qui_Context *ctx, int x, int y) {
    QUI_VALIDATE_CTX(ctx);
    ctx->mouse_pos.x = x;
    ctx->mouse_pos.y = y;
    ctx->mouse_down = 0;
    ctx->mouse_released = 1;
    return QUI_OK;
}

qui_Result qui_mouse_move(qui_Context *ctx, int x, int y) {
    QUI_VALIDATE_CTX(ctx);
    ctx->mouse_pos.x = x;
    ctx->mouse_pos.y = y;
    return QUI_OK;
}

qui_Result qui_feed_mouse_button(qui_Context *ctx, int pressed) {
    QUI_VALIDATE_CTX(ctx);
    ctx->mouse_pressed = pressed ? 1 : 0;
    return QUI_OK;
}

qui_Result qui_feed_key_backspace(qui_Context *ctx) {
    QUI_VALIDATE_CTX(ctx);
    ctx->key_backspace = 1;
    return QUI_OK;
}

qui_Result qui_feed_key_enter(qui_Context *ctx) {
    QUI_VALIDATE_CTX(ctx);
    ctx->key_enter = 1;
    return QUI_OK;
}

/* ================================================================================================
 * UTILITY FUNCTIONS
 * ================================================================================================ */

qui_Vec2 qui_vec2(int x, int y) {
    qui_Vec2 v = {x, y};
    return v;
}

qui_Rect qui_rect(int width, int height, int pos_x, int pos_y) {
    qui_Rect r = {width, height, pos_x, pos_y};
    return r;
}

qui_Color qui_color(int r, int g, int b, int a) {
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    if (a < 0) a = 0; else if (a > 255) a = 255;
    qui_Color color = {r, g, b, a};
    return color;
}

qui_Result qui_set_color(qui_Context *ctx, qui_ColorType type, qui_Color color) {
    QUI_VALIDATE_CTX(ctx);
    if (type >= QUI_COLOR_COUNT) return QUI_ERROR_INVALID_VALUE;
    ctx->colors[type] = color;
    return QUI_OK;
}

qui_Result qui_get_color(qui_Context *ctx, qui_ColorType type, qui_Color *color) {
    QUI_VALIDATE_CTX(ctx);
    QUI_VALIDATE_PTR(color);
    if (type >= QUI_COLOR_COUNT) return QUI_ERROR_INVALID_VALUE;
    *color = ctx->colors[type];
    return QUI_OK;
}

qui_Result qui_set_font(qui_Context *ctx, void *font, float font_size, float font_spacing) {
    QUI_VALIDATE_CTX(ctx);
    if (font_size < 0.0f || !isfinite(font_size) ||
        font_spacing < 0.0f || !isfinite(font_spacing)) {
        return QUI_ERROR_INVALID_VALUE;
    }
    ctx->font = font;
    ctx->font_size = font_size;
    ctx->font_spacing = font_spacing;
    return QUI_OK;
}

/* ================================================================================================
 * MODERN UI ELEMENTS
 * ================================================================================================ */

int qui_button(qui_Context *ctx, const char *label) {
    if (!ctx || !label) return -1;

    qui_Id id = qui_gen_id(ctx);
    float text_width = qui_get_text_width(ctx, label);
    float text_height = qui_get_text_height(ctx, label);
    float padding_x = 14.0f;
    float padding_y = 6.0f;

    float w = text_width + (padding_x * 2);
    float h = text_height + (padding_y * 2);
    float x = ctx->cursor_x;
    float y = ctx->cursor_y;

    /* Hit testing */
    bool hot = qui_hit_test(ctx, x, y, w, h);
    if (hot) {
        ctx->hot_id = id;
        if (ctx->mouse_pressed) {
            ctx->active_id = id;
        }
    }

    /* Determine colors based on state */
    qui_Color bg_color = ctx->colors[QUI_COLOR_FOREGROUND];
    qui_Color text_color = ctx->colors[QUI_COLOR_TEXT];

    if (ctx->active_id == id) {
        bg_color = ctx->colors[QUI_COLOR_ACTIVE];
        text_color = qui_color(255, 255, 255, 255);
    } else if (ctx->hot_id == id) {
        bg_color = ctx->colors[QUI_COLOR_HOT];
    }

    /* Draw button background with subtle border */
    qui_Rect bg_rect = qui_rect((int)w, (int)h, (int)x, (int)y);
    qui_draw_rect_safe(ctx, &bg_rect, bg_color);

    /* Draw border for non-active states */
    if (ctx->active_id != id) {
        qui_Rect border_rect = qui_rect((int)w, (int)h, (int)x, (int)y);
        qui_draw_rect_border_safe(ctx, &border_rect, ctx->colors[QUI_COLOR_BORDER]);
    }

    /* Draw text centered */
    float text_x = x + (w - text_width) / 2.0f;
    float text_y = y + (h - text_height) / 2.0f;
    qui_draw_text_safe(ctx, label, text_x, text_y);

    /* Check for click */
    int clicked = 0;
    if (ctx->mouse_released && ctx->active_id == id) {
        if (hot) clicked = 1;
        ctx->active_id = 0;
    }

    /* Update layout */
    ctx->cursor_y += h + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;

    return clicked;
}

int qui_button_secondary(qui_Context *ctx, const char *label) {
    if (!ctx || !label) return -1;

    qui_Id id = qui_gen_id(ctx);
    float text_width = qui_get_text_width(ctx, label);
    float text_height = qui_get_text_height(ctx, label);
    float padding_x = 12.0f;
    float padding_y = 4.0f;

    float w = text_width + (padding_x * 2);
    float h = text_height + (padding_y * 2);
    float x = ctx->cursor_x;
    float y = ctx->cursor_y;

    /* Hit testing */
    bool hot = qui_hit_test(ctx, x, y, w, h);
    if (hot) {
        ctx->hot_id = id;
        if (ctx->mouse_pressed) {
            ctx->active_id = id;
        }
    }

    /* Secondary button has transparent background, just border */
    qui_Color border_color = ctx->colors[QUI_COLOR_BORDER];
    qui_Color text_color = ctx->colors[QUI_COLOR_TEXT_DIM];

    if (ctx->hot_id == id) {
        border_color = ctx->colors[QUI_COLOR_ACTIVE];
        text_color = ctx->colors[QUI_COLOR_TEXT];
    }

    /* Draw border */
    qui_Rect border_rect = qui_rect((int)w, (int)h, (int)x, (int)y);
    qui_draw_rect_border_safe(ctx, &border_rect, border_color);

    /* Draw text centered */
    float text_x = x + (w - text_width) / 2.0f;
    float text_y = y + (h - text_height) / 2.0f;
    qui_draw_text_safe(ctx, label, text_x, text_y);

    /* Check for click */
    int clicked = 0;
    if (ctx->mouse_released && ctx->hot_id == id && hot) {
        clicked = 1;
    }
    if (ctx->mouse_released && ctx->active_id == id) {
        ctx->active_id = 0;
    }

    /* Update layout */
    ctx->cursor_y += h + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;

    return clicked;
}

int qui_checkbox(qui_Context *ctx, const char *label, int *value) {
    if (!ctx || !label || !value) return -1;

    qui_Id id = qui_gen_id(ctx);
    float box_size = 18.0f;
    float text_width = qui_get_text_width(ctx, label);
    float text_height = qui_get_text_height(ctx, label);
    float spacing = 8.0f;

    float total_width = box_size + spacing + text_width;
    float h = fmaxf(box_size, text_height);
    float x = ctx->cursor_x;
    float y = ctx->cursor_y;

    /* Hit testing */
    bool hot = qui_hit_test(ctx, x, y, total_width, h);
    if (hot) {
        ctx->hot_id = id;
        if (ctx->mouse_pressed) {
            ctx->active_id = id;
        }
    }

    /* Draw checkbox box */
    qui_Color box_color = ctx->colors[QUI_COLOR_FOREGROUND];
    if (ctx->hot_id == id) {
        box_color = ctx->colors[QUI_COLOR_HOT];
    }

    qui_Rect box_rect = qui_rect((int)box_size, (int)box_size, (int)x, (int)y);
    qui_draw_rect_safe(ctx, &box_rect, box_color);
    qui_draw_rect_border_safe(ctx, &box_rect, ctx->colors[QUI_COLOR_BORDER]);

    /* Draw checkmark if checked */
    if (*value) {
        glColor3ub(0, 122, 204);  /* Accent blue for checkmark */
        glBegin(GL_LINES);
        glVertex2f(x + 4, y + box_size / 2);
        glVertex2f(x + box_size / 2 - 2, y + box_size - 4);
        glVertex2f(x + box_size / 2 - 2, y + box_size - 4);
        glVertex2f(x + box_size - 4, y + 4);
        glEnd();
    }

    /* Draw label */
    qui_draw_text_safe(ctx, label, x + box_size + spacing, y + (h - text_height) / 2.0f);

    /* Check for click */
    int changed = 0;
    if (ctx->mouse_released && ctx->active_id == id) {
        if (hot) {
            *value = !(*value);
            changed = 1;
        }
        ctx->active_id = 0;
    }

    /* Update layout */
    ctx->cursor_y += h + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;

    return changed;
}

int qui_slider(qui_Context *ctx, const char *label, float *value, float min_val, float max_val, float width) {
    if (!ctx || !label || !value) return -1;
    if (min_val >= max_val || !isfinite(min_val) || !isfinite(max_val) || !isfinite(*value) || width < 0.0f)
        return -1;

    qui_Id id = qui_gen_id(ctx);
    float label_width = qui_get_text_width(ctx, label);
    float text_height = qui_get_text_height(ctx, label);
    float slider_width = (width > 0.0f) ? width : 200.0f;
    float slider_height = 4.0f;
    float knob_size = 14.0f;
    float spacing = 12.0f;

    float x = ctx->cursor_x;
    float y = ctx->cursor_y;
    float slider_x = x + label_width + spacing;
    float slider_y = y + text_height / 2.0f - slider_height / 2.0f;
    float h = text_height + 8.0f;

    /* Clamp value */
    if (*value < min_val) *value = min_val;
    if (*value > max_val) *value = max_val;

    /* Hit testing on slider area (including knob) */
    if (qui_hit_test(ctx, slider_x - knob_size/2, slider_y - knob_size/2,
                     slider_width + knob_size, knob_size)) {
        ctx->hot_id = id;
        if (ctx->mouse_pressed) {
            ctx->active_id = id;
        }
    }

    /* Draw label */
    qui_draw_text_safe(ctx, label, x, y);

    /* Draw slider track */
    qui_Rect track_rect = qui_rect((int)slider_width, (int)slider_height, (int)slider_x, (int)slider_y);
    qui_draw_rect_safe(ctx, &track_rect, ctx->colors[QUI_COLOR_BORDER]);

    /* Calculate knob position */
    float t = (*value - min_val) / (max_val - min_val);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float knob_x = slider_x + t * slider_width - knob_size / 2.0f;
    float knob_y = slider_y - (knob_size - slider_height) / 2.0f;

    /* Draw filled portion of track */
    qui_Rect fill_rect = qui_rect((int)(t * slider_width), (int)slider_height, (int)slider_x, (int)slider_y);
    qui_draw_rect_safe(ctx, &fill_rect, ctx->colors[QUI_COLOR_ACTIVE]);

    /* Draw knob */
    qui_Color knob_color = (ctx->active_id == id || ctx->hot_id == id) ?
                           ctx->colors[QUI_COLOR_ACTIVE] :
                           ctx->colors[QUI_COLOR_TEXT];
    qui_Rect knob_rect = qui_rect((int)knob_size, (int)knob_size, (int)knob_x, (int)knob_y);
    qui_draw_rect_safe(ctx, &knob_rect, knob_color);

    /* Handle dragging */
    if (ctx->active_id == id && ctx->mouse_down) {
        float local_x = (float)ctx->mouse_pos.x - (slider_x + ctx->layout_offset_x);
        float new_t = local_x / slider_width;
        if (new_t < 0.0f) new_t = 0.0f;
        if (new_t > 1.0f) new_t = 1.0f;
        *value = min_val + new_t * (max_val - min_val);
    }

    if (ctx->mouse_released && ctx->active_id == id) {
        ctx->active_id = 0;
    }

    /* Draw value */
    char value_buffer[32];
    snprintf(value_buffer, sizeof(value_buffer), "%.2f", *value);
    qui_draw_text_safe(ctx, value_buffer, slider_x + slider_width + 8.0f, y);

    /* Update layout */
    ctx->cursor_y += h + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;

    return 1;
}

int qui_textbox(qui_Context *ctx, char *buffer, size_t capacity, float width) {
    if (!ctx || !buffer || capacity == 0) return -1;

    qui_Id id = qui_gen_id(ctx);
    float text_height = qui_get_text_height(ctx, "A");
    float box_width = (width > 0.0f) ? width : 200.0f;
    float padding_x = 8.0f;
    float padding_y = 4.0f;
    float h = text_height + padding_y * 2;

    float x = ctx->cursor_x;
    float y = ctx->cursor_y;

    /* Hit testing */
    bool hot = qui_hit_test(ctx, x, y, box_width, h);
    if (hot && ctx->mouse_pressed) {
        ctx->keyboard_focus_id = id;
    } else if (ctx->mouse_pressed && ctx->keyboard_focus_id == id && !hot) {
        ctx->keyboard_focus_id = 0;
    }

    /* Determine colors */
    qui_Color bg_color = ctx->colors[QUI_COLOR_FOREGROUND];
    qui_Color border_color = ctx->colors[QUI_COLOR_BORDER];

    if (ctx->keyboard_focus_id == id) {
        border_color = ctx->colors[QUI_COLOR_ACTIVE];
    } else if (ctx->hot_id == id) {
        bg_color = ctx->colors[QUI_COLOR_HOT];
    }

    /* Draw background */
    qui_Rect rect = qui_rect((int)box_width, (int)h, (int)x, (int)y);
    qui_draw_rect_safe(ctx, &rect, bg_color);
    qui_draw_rect_border_safe(ctx, &rect, border_color);

    /* Draw text content */
    qui_draw_text_safe(ctx, buffer, x + padding_x, y + padding_y);

    /* Draw cursor if focused */
    if (ctx->keyboard_focus_id == id) {
        float text_width = qui_get_text_width(ctx, buffer);
        qui_Rect cursor_rect = qui_rect(2, (int)text_height,
                                         (int)(x + padding_x + text_width),
                                         (int)(y + padding_y));
        qui_draw_rect_safe(ctx, &cursor_rect, ctx->colors[QUI_COLOR_TEXT]);
    }

    /* Handle keyboard input */
    if (ctx->keyboard_focus_id == id && ctx->key_backspace) {
        size_t len = strlen(buffer);
        if (len > 0) buffer[len - 1] = '\0';
    }

    /* Update layout */
    ctx->cursor_y += h + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;

    return (ctx->keyboard_focus_id == id) ? 1 : 0;
}

void qui_separator(qui_Context *ctx) {
    if (!ctx) return;

    float x = ctx->cursor_x;
    float y = ctx->cursor_y;
    float width = 200.0f; /* Could be parameter */
    float height = 1.0f;

    qui_Rect sep_rect = qui_rect((int)width, (int)height, (int)x, (int)y);
    qui_draw_rect_safe(ctx, &sep_rect, ctx->colors[QUI_COLOR_BORDER]);

    ctx->cursor_y += height + ctx->spacing_y;
}

void qui_label(qui_Context *ctx, const char *text, bool dim) {
    if (!ctx || !text) return;

    float text_width = qui_get_text_width(ctx, text);
    float text_height = qui_get_text_height(ctx, text);
    float x = ctx->cursor_x;
    float y = ctx->cursor_y;

    qui_Color color = dim ? ctx->colors[QUI_COLOR_TEXT_DIM] : ctx->colors[QUI_COLOR_TEXT];
    qui_draw_text_safe(ctx, text, x, y);

    ctx->cursor_y += text_height + ctx->spacing_y;
    ctx->cursor_x = ctx->spacing_x;
}

#endif /* QUI_IMPLEMENTATION */
