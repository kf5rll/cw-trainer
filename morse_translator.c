/*
 * Morse Code Translator — GTK3 GUI application for Linux
 *
 * Build:
 *   gcc morse_translator.c -o morse_translator $(pkg-config --cflags --libs gtk+-3.0)
 *
 * Install dependencies (pick your distro):
 *   sudo apt install gcc libgtk-3-dev        (Debian / Ubuntu / Mint)
 *   sudo dnf install gcc gtk3-devel           (Fedora / RHEL / CentOS)
 *   sudo pacman -S gcc gtk3                   (Arch / Manjaro)
 *
 * Run:
 *   ./morse_translator
 */

#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

/* ── Morse table ────────────────────────────────────────────────────────── */

typedef struct { char ch; const char *code; } MorseEntry;

static const MorseEntry MORSE_TABLE[] = {
    {'A',".-"},  {'B',"-..."}, {'C',"-.-."}, {'D',"-.."},  {'E',"."},
    {'F',"..-."}, {'G',"--."}, {'H',"...."}, {'I',".."},   {'J',".---"},
    {'K',"-.-"},  {'L',".-.."}, {'M',"--"},  {'N',"-."},   {'O',"---"},
    {'P',".--."}, {'Q',"--.-"}, {'R',".-."}, {'S',"..."},  {'T',"-"},
    {'U',"..-"},  {'V',"...-"}, {'W',".--"}, {'X',"-..-"}, {'Y',"-.--"},
    {'Z',"--.."},
    {'0',"-----"}, {'1',".----"}, {'2',"..---"}, {'3',"...--"}, {'4',"....-"},
    {'5',"....."}, {'6',"-...."}, {'7',"--..."}, {'8',"---.."}, {'9',"----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'!', "-.-.--"},
    {'/', "-..-."}, {'@', ".--.-."}, {'=', "-...-"},   {'+', ".-.-."},
    {'(', "-.--."},
    {'\0', NULL}
};

static const char *char_to_morse(char c) {
    c = (char)toupper((unsigned char)c);
    for (int i = 0; MORSE_TABLE[i].ch != '\0'; i++)
        if (MORSE_TABLE[i].ch == c)
            return MORSE_TABLE[i].code;
    return NULL;
}

/* ── App state ──────────────────────────────────────────────────────────── */

typedef struct {
    GtkWidget *window;
    GtkWidget *text_view;
    GtkWidget *drawing_area;
    GtkWidget *copy_btn;
    GtkWidget *stats_label;
    char       morse_plain[8192];
} AppState;

/* ── Drawing constants ──────────────────────────────────────────────────── */

#define DOT_R     6
#define DASH_W    34
#define DASH_H    12
#define SYM_GAP   5
#define CHAR_GAP  16
#define WORD_GAP  30
#define ROW_H     52
#define PAD_X     16
#define PAD_Y     16
#define LABEL_OFF 34   /* y below row_top for letter label */
#define CORNER_R  5.0

/* Width of one morse code string in pixels */
static int code_pixel_width(const char *code) {
    int w = 0;
    for (int i = 0; code[i]; i++) {
        if (i > 0) w += SYM_GAP;
        w += (code[i] == '.') ? DOT_R * 2 : DASH_W;
    }
    return w;
}

/* Draw a rounded rectangle using cairo */
static void rounded_rect(cairo_t *cr, double x, double y,
                          double w, double h, double r) {
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, x + w - r, y);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2.0, 0);
    cairo_line_to(cr, x + w, y + h - r);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI / 2.0);
    cairo_line_to(cr, x + r, y + h);
    cairo_arc(cr, x + r, y + h - r, r, G_PI / 2.0, G_PI);
    cairo_line_to(cr, x, y + r);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3.0 * G_PI / 2.0);
    cairo_close_path(cr);
}

/*
 * Render the morse output into cr (width = area_width).
 * Returns the total height consumed (for widget sizing).
 */
static int render_morse(cairo_t *cr, const char *input,
                        int area_width, gboolean measure_only) {
    int x = PAD_X;
    int row_top = PAD_Y;
    int total_h = PAD_Y + ROW_H + PAD_Y;
    gboolean prev_space = FALSE;

    for (int i = 0; input[i]; i++) {
        char c = input[i];

        /* ── word space ── */
        if (c == ' ') {
            if (!prev_space) {
                if (!measure_only) {
                    cairo_set_source_rgb(cr, 0.80, 0.80, 0.80);
                    cairo_set_line_width(cr, 1.0);
                    double mid = x + WORD_GAP / 2.0;
                    cairo_move_to(cr, mid, row_top + 6);
                    cairo_line_to(cr, mid, row_top + 28);
                    cairo_stroke(cr);
                }
                x += WORD_GAP;
            }
            prev_space = TRUE;
            continue;
        }
        prev_space = FALSE;

        const char *code = char_to_morse(c);
        if (!code) continue;

        int cw = code_pixel_width(code);

        /* wrap row */
        if (x + cw > area_width - PAD_X && x > PAD_X) {
            x = PAD_X;
            row_top += ROW_H;
            total_h = row_top + ROW_H + PAD_Y;
        }

        /* ── draw symbols ── */
        int sx = x;
        for (int j = 0; code[j]; j++) {
            if (j > 0) sx += SYM_GAP;
            if (!measure_only) {
                cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
                if (code[j] == '.') {
                    cairo_arc(cr, sx + DOT_R, row_top + DOT_R,
                              DOT_R, 0, 2.0 * G_PI);
                    cairo_fill(cr);
                } else {
                    rounded_rect(cr, sx, row_top, DASH_W, DASH_H, CORNER_R);
                    cairo_fill(cr);
                }
            }
            sx += (code[j] == '.') ? DOT_R * 2 : DASH_W;
        }

        /* ── draw letter label ── */
        if (!measure_only) {
            cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
            cairo_select_font_face(cr, "Sans",
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 11.0);
            char lbl[2] = {(char)toupper((unsigned char)c), '\0'};
            cairo_text_extents_t te;
            cairo_text_extents(cr, lbl, &te);
            double lx = x + cw / 2.0 - te.width / 2.0 - te.x_bearing;
            cairo_move_to(cr, lx, row_top + LABEL_OFF);
            cairo_show_text(cr, lbl);
        }

        x += cw + CHAR_GAP;
    }

    return total_h;
}

/* ── Draw callback ──────────────────────────────────────────────────────── */

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    AppState *app = (AppState *)data;
    int w = gtk_widget_get_allocated_width(widget);

    /* background */
    cairo_set_source_rgb(cr, 0.98, 0.98, 0.97);
    cairo_paint(cr);

    GtkTextBuffer *buf =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->text_view));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    gchar *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);

    if (text && *text) {
        render_morse(cr, text, w, FALSE);
    } else {
        cairo_set_source_rgb(cr, 0.70, 0.70, 0.70);
        cairo_select_font_face(cr, "Sans",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 13.0);
        cairo_move_to(cr, PAD_X, PAD_Y + 16);
        cairo_show_text(cr, "Output will appear here");
    }

    g_free(text);
    return FALSE;
}

/* ── Resize drawing area to fit content ─────────────────────────────────── */

static void sync_drawing_height(AppState *app) {
    GtkTextBuffer *buf =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->text_view));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    gchar *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);

    int area_w = gtk_widget_get_allocated_width(app->drawing_area);
    if (area_w < 32) area_w = 640;

    int h = 80;
    if (text && *text) {
        cairo_surface_t *surf =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, area_w, 4000);
        cairo_t *cr = cairo_create(surf);
        h = render_morse(cr, text, area_w, FALSE);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        if (h < 80) h = 80;
    }

    gtk_widget_set_size_request(app->drawing_area, -1, h);
    g_free(text);
}

/* ── Update stats bar and plain-text morse ──────────────────────────────── */

static void update_stats(AppState *app, const char *text) {
    int chars = 0, dots = 0, dashes = 0;
    size_t rem = sizeof(app->morse_plain) - 1;
    app->morse_plain[0] = '\0';
    gboolean first_in_word = TRUE;

    for (int i = 0; text[i]; i++) {
        char c = text[i];
        if (c == ' ') {
            if (rem > 3) { strncat(app->morse_plain, " / ", rem); rem -= 3; }
            first_in_word = TRUE;
            continue;
        }
        const char *code = char_to_morse(c);
        if (!code) continue;
        chars++;
        for (int j = 0; code[j]; j++) {
            if (code[j] == '.') dots++;
            else              dashes++;
        }
        if (!first_in_word && rem > 1) {
            strncat(app->morse_plain, " ", rem); rem -= 1;
        }
        size_t cl = strlen(code);
        if (cl <= rem) { strncat(app->morse_plain, code, rem); rem -= cl; }
        first_in_word = FALSE;
    }

    char stats[160];
    snprintf(stats, sizeof(stats),
             "characters: %d   dots: %d   dashes: %d", chars, dots, dashes);
    gtk_label_set_text(GTK_LABEL(app->stats_label), stats);
}

/* ── Text-changed signal ────────────────────────────────────────────────── */

static void on_text_changed(GtkTextBuffer *buf, gpointer data) {
    AppState *app = (AppState *)data;
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    gchar *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);

    update_stats(app, text ? text : "");
    sync_drawing_height(app);
    gtk_widget_queue_draw(app->drawing_area);

    g_free(text);
}

/* ── Copy button ────────────────────────────────────────────────────────── */

static gboolean reset_copy_label(gpointer data) {
    gtk_button_set_label(GTK_BUTTON(data), "Copy Morse");
    return G_SOURCE_REMOVE;
}

static void on_copy_clicked(GtkButton *btn, gpointer data) {
    AppState *app = (AppState *)data;
    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(cb, app->morse_plain, -1);
    gtk_button_set_label(btn, "Copied!");
    g_timeout_add(1500, reset_copy_label, btn);
}

/* ── Build the window ───────────────────────────────────────────────────── */

static void build_ui(AppState *app) {
    /* Window */
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Morse Code Translator");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 720, 580);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 20);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);

    /* Heading */
    GtkWidget *heading = gtk_label_new("MORSE CODE TRANSLATOR");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    {
        PangoAttrList *al = pango_attr_list_new();
        pango_attr_list_insert(al, pango_attr_scale_new(0.82));
        pango_attr_list_insert(al,
            pango_attr_foreground_new(0x8888, 0x8888, 0x8888));
        gtk_label_set_attributes(GTK_LABEL(heading), al);
        pango_attr_list_unref(al);
    }
    gtk_box_pack_start(GTK_BOX(vbox), heading, FALSE, FALSE, 0);

    /* Input label */
    GtkWidget *in_lbl = gtk_label_new("Type your message:");
    gtk_widget_set_halign(in_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), in_lbl, FALSE, FALSE, 0);

    /* Text input (scrolled) */
    GtkWidget *in_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(in_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(in_scroll),
                                        GTK_SHADOW_IN);
    gtk_widget_set_size_request(in_scroll, -1, 80);

    app->text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->text_view),
                                GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->text_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->text_view), 6);
    gtk_container_add(GTK_CONTAINER(in_scroll), app->text_view);
    gtk_box_pack_start(GTK_BOX(vbox), in_scroll, FALSE, FALSE, 0);

    GtkTextBuffer *buf =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->text_view));
    g_signal_connect(buf, "changed", G_CALLBACK(on_text_changed), app);

    /* Output label */
    GtkWidget *out_lbl = gtk_label_new("Morse output:");
    gtk_widget_set_halign(out_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), out_lbl, FALSE, FALSE, 0);

    /* Scrollable drawing area */
    GtkWidget *out_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(out_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(out_scroll),
                                        GTK_SHADOW_IN);
    gtk_widget_set_size_request(out_scroll, -1, 160);

    app->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->drawing_area, -1, 80);
    g_signal_connect(app->drawing_area, "draw", G_CALLBACK(on_draw), app);

    gtk_container_add(GTK_CONTAINER(out_scroll), app->drawing_area);
    gtk_box_pack_start(GTK_BOX(vbox), out_scroll, TRUE, TRUE, 0);

    /* Stats */
    app->stats_label = gtk_label_new(
        "characters: 0   dots: 0   dashes: 0");
    gtk_widget_set_halign(app->stats_label, GTK_ALIGN_START);
    {
        PangoAttrList *al = pango_attr_list_new();
        pango_attr_list_insert(al, pango_attr_scale_new(0.85));
        pango_attr_list_insert(al,
            pango_attr_foreground_new(0x7777, 0x7777, 0x7777));
        gtk_label_set_attributes(GTK_LABEL(app->stats_label), al);
        pango_attr_list_unref(al);
    }
    gtk_box_pack_start(GTK_BOX(vbox), app->stats_label, FALSE, FALSE, 0);

    /* Copy button */
    app->copy_btn = gtk_button_new_with_label("Copy Morse");
    gtk_widget_set_halign(app->copy_btn, GTK_ALIGN_START);
    g_signal_connect(app->copy_btn, "clicked",
                     G_CALLBACK(on_copy_clicked), app);
    gtk_box_pack_start(GTK_BOX(vbox), app->copy_btn, FALSE, FALSE, 0);

    /* Reference section */
    GtkWidget *ref_lbl = gtk_label_new("Reference — all morse codes:");
    gtk_widget_set_halign(ref_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), ref_lbl, FALSE, FALSE, 4);

    GtkWidget *ref_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ref_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(ref_scroll, -1, 110);

    GtkWidget *ref_flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(ref_flow),
                                    GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(ref_flow), 5);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(ref_flow), 20);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(ref_flow), 2);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(ref_flow), 6);

    for (int i = 0; MORSE_TABLE[i].ch != '\0'; i++) {
        char entry[24];
        snprintf(entry, sizeof(entry), "%c  %s",
                 MORSE_TABLE[i].ch, MORSE_TABLE[i].code);
        GtkWidget *lbl = gtk_label_new(entry);
        PangoAttrList *al = pango_attr_list_new();
        pango_attr_list_insert(al, pango_attr_family_new("Monospace"));
        pango_attr_list_insert(al, pango_attr_scale_new(0.80));
        gtk_label_set_attributes(GTK_LABEL(lbl), al);
        pango_attr_list_unref(al);
        gtk_widget_set_margin_start(lbl, 4);
        gtk_widget_set_margin_end(lbl, 4);
        gtk_widget_set_margin_top(lbl, 1);
        gtk_widget_set_margin_bottom(lbl, 1);
        gtk_flow_box_insert(GTK_FLOW_BOX(ref_flow), lbl, -1);
    }

    gtk_container_add(GTK_CONTAINER(ref_scroll), ref_flow);
    gtk_box_pack_start(GTK_BOX(vbox), ref_scroll, FALSE, FALSE, 0);

    gtk_widget_show_all(app->window);
}

/* ── Entry point ────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    AppState app;
    memset(&app, 0, sizeof(app));
    build_ui(&app);

    gtk_main();
    return 0;
}
