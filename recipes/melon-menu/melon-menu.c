/* melon-menu: melon's start menu for XFCE (docs/xfce-theme.md, "The start menu").
 *
 * A search field, the pinned apps as a grid, the files opened last, and the user with lock and power at the bottom, in a
 * rounded card centred above the taskbar. "All apps" lists everything. The taskbar's melon button and the Super key run
 * `melon-menu`, which shows the menu or hides it; one process stays running (started hidden with --background at login),
 * so it opens at once.
 *
 * Pinned apps: desktop IDs in melon/menu.conf ([menu] pinned=a.desktop;b.desktop;), read from ~/.config first, then from
 * XDG_CONFIG_DIRS (melon-xfce ships /etc/xdg/melon-xfce/melon/menu.conf). Pinning or unpinning (right click) writes the
 * user's copy. Apps that aren't installed are skipped. */
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <string.h>

#define CARD_W 640
#define CARD_H 620
#define SHADOW 28          /* room around the card for its shadow, which the menu draws itself */
#define GAP 10             /* between the card and the taskbar */
#define COLUMNS 6
#define RECENT_MAX 4

static GtkApplication *app;
static GtkWidget *win, *card, *search, *stack, *pinned_box, *recent_box, *recent_empty, *all_list, *results_list;
static GtkWidget *power_pop, *app_pop, *hint;
static GList *apps;                 /* every app that should show, sorted by name */
static gchar **pinned;              /* desktop IDs */
static gint64 hidden_at;            /* when the menu was last hidden (a click on the taskbar button hides it first) */
static gboolean background;         /* --background: start without showing */
static GAppInfo *menu_target;       /* the app a right click opened the pin menu for */

static const char *CSS =
  "window.melon-menu { background: transparent; }\n"
  ".mm-card { background-color: alpha(#141a16, 0.97); border-radius: 14px; border: 1px solid alpha(white, 0.09);"
  "  box-shadow: 0 10px 28px alpha(black, 0.55), 0 0 0 1px alpha(black, 0.35); margin: " G_STRINGIFY(SHADOW) "px; }\n"
  ".mm-card entry.mm-search { min-height: 40px; border-radius: 10px; padding: 0 14px; font-size: 11pt;"
  "  background-color: alpha(white, 0.06); border: 1px solid alpha(white, 0.07); box-shadow: none; }\n"
  ".mm-card entry.mm-search:focus-within { border-color: @accent_bg_color; background-color: alpha(white, 0.08); }\n"
  ".mm-heading { font-weight: 600; font-size: 10.5pt; }\n"
  ".mm-card button.mm-link { padding: 3px 10px; border-radius: 7px; font-size: 9.5pt; min-height: 0;"
  "  background-color: alpha(white, 0.06); }\n"
  ".mm-card button.mm-link:hover { background-color: alpha(white, 0.11); }\n"
  ".mm-card flowbox, .mm-card list, .mm-card scrolledwindow, .mm-card viewport { background: transparent; }\n"
  ".mm-card flowboxchild { padding: 10px 2px 8px; border-radius: 10px; margin: 2px; }\n"
  ".mm-card flowboxchild:hover { background-color: alpha(white, 0.06); }\n"
  ".mm-card flowboxchild:selected, .mm-card flowboxchild:focus { background-color: alpha(white, 0.09); outline: none; }\n"
  ".mm-card flowboxchild label { font-size: 9.5pt; }\n"
  ".mm-card list row { padding: 6px 10px; border-radius: 9px; margin: 1px 0; }\n"
  ".mm-card list row:hover { background-color: alpha(white, 0.06); }\n"
  ".mm-card list row:selected, .mm-card list row:focus { background-color: alpha(white, 0.09); color: @window_fg_color;"
  "  outline: none; }\n"
  ".mm-dim { opacity: 0.6; font-size: 9pt; }\n"
  ".mm-hint { opacity: 0.55; font-size: 11pt; }\n"
  ".mm-letter { font-weight: 600; color: @accent_color; padding: 8px 10px 0; font-size: 9.5pt; }\n"
  ".mm-footer { background-color: alpha(black, 0.18); border-top: 1px solid alpha(white, 0.06);"
  "  border-radius: 0 0 14px 14px; padding: 10px 22px; }\n"
  ".mm-avatar { background-color: @accent_bg_color; color: @accent_fg_color; border-radius: 99px; font-weight: 700;"
  "  min-width: 32px; min-height: 32px; }\n"
  ".mm-card button.mm-round { min-width: 36px; min-height: 36px; padding: 0; border-radius: 9px;"
  "  background-color: alpha(white, 0.06); }\n"
  ".mm-card button.mm-round:hover { background-color: alpha(white, 0.12); }\n"
  "popover.mm-pop button { padding: 7px 14px; border-radius: 7px; font-weight: normal; border: none; box-shadow: none;"
  "  outline: none; background: transparent; min-width: 110px; }\n"
  "popover.mm-pop button:hover, popover.mm-pop button:focus:hover { background-color: alpha(white, 0.08); }\n"
  "popover.mm-pop button:focus { background-color: alpha(white, 0.05); }\n";

/* ---- launching ---- */

static void hide_menu(void)
{
  if (!gtk_widget_get_visible(win)) return;
  gtk_widget_hide(win);
  hidden_at = g_get_monotonic_time();
}

static void launch(GAppInfo *info)
{
  GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gtk_widget_get_display(win));
  GError *err = NULL;
  if (!g_app_info_launch(info, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err)) {
    g_warning("%s: %s", g_app_info_get_id(info), err->message);
    g_error_free(err);
  }
  g_object_unref(ctx);
  hide_menu();
}

static void run(const char *cmdline)
{
  GError *err = NULL;
  if (!g_spawn_command_line_async(cmdline, &err)) { g_warning("%s: %s", cmdline, err->message); g_error_free(err); }
  hide_menu();
}

static void open_uri(const char *uri)
{
  GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gtk_widget_get_display(win));
  g_app_info_launch_default_for_uri_async(uri, G_APP_LAUNCH_CONTEXT(ctx), NULL, NULL, NULL);
  g_object_unref(ctx);
  hide_menu();
}

/* ---- the apps ---- */

static gint by_name(gconstpointer a, gconstpointer b)
{
  return g_utf8_collate(g_app_info_get_name(G_APP_INFO(a)), g_app_info_get_name(G_APP_INFO(b)));
}

static void load_apps(void)
{
  g_list_free_full(apps, g_object_unref);
  apps = NULL;
  for (GList *l = g_app_info_get_all(), *all = l; ; l = l->next) {
    if (!l) { g_list_free(all); break; }
    GAppInfo *a = l->data;
    if (g_app_info_should_show(a) && g_strcmp0(g_app_info_get_id(a), "melon-menu.desktop") != 0)
      apps = g_list_prepend(apps, a);
    else
      g_object_unref(a);
  }
  apps = g_list_sort(apps, by_name);
}

static GAppInfo *find_app(const char *id)
{
  for (GList *l = apps; l; l = l->next)
    if (g_strcmp0(g_app_info_get_id(l->data), id) == 0) return l->data;
  return NULL;
}

static void load_pinned(void)
{
  g_strfreev(pinned);
  pinned = NULL;
  /* the user's own list wins; otherwise the first system config dir that has one (/etc/xdg/melon-xfce under XFCE) */
  gchar *user = g_build_filename(g_get_user_config_dir(), "melon", "menu.conf", NULL);
  GKeyFile *kf = g_key_file_new();
  if (g_key_file_load_from_file(kf, user, G_KEY_FILE_NONE, NULL)
      || g_key_file_load_from_dirs(kf, "melon/menu.conf", (const gchar **) g_get_system_config_dirs(), NULL,
                                   G_KEY_FILE_NONE, NULL))
    pinned = g_key_file_get_string_list(kf, "menu", "pinned", NULL, NULL);
  if (!pinned) pinned = g_new0(gchar *, 1);
  g_key_file_free(kf);
  g_free(user);
}

static void save_pinned(void)
{
  gchar *dir = g_build_filename(g_get_user_config_dir(), "melon", NULL);
  gchar *path = g_build_filename(dir, "menu.conf", NULL);
  g_mkdir_with_parents(dir, 0700);
  GKeyFile *kf = g_key_file_new();
  g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
  g_key_file_set_string_list(kf, "menu", "pinned", (const gchar * const *) pinned, g_strv_length(pinned));
  g_key_file_save_to_file(kf, path, NULL);
  g_key_file_free(kf);
  g_free(path);
  g_free(dir);
}

static gboolean is_pinned(const char *id)
{
  return id && g_strv_contains((const gchar * const *) pinned, id);
}

/* ---- right click: pin or unpin ---- */

static void fill_pinned(void);

static void toggle_pin(GtkWidget *item, gpointer data)
{
  (void) item; (void) data;
  const char *id = g_app_info_get_id(menu_target);
  GPtrArray *a = g_ptr_array_new();
  gboolean was = is_pinned(id);
  for (gchar **p = pinned; *p; p++)
    if (!was || g_strcmp0(*p, id) != 0) g_ptr_array_add(a, g_strdup(*p));
  if (!was) g_ptr_array_add(a, g_strdup(id));
  g_ptr_array_add(a, NULL);
  g_strfreev(pinned);
  pinned = (gchar **) g_ptr_array_free(a, FALSE);
  save_pinned();
  fill_pinned();
  gtk_widget_hide(app_pop);
}

static gboolean on_app_button(GtkWidget *w, GdkEventButton *ev, gpointer info)
{
  if (ev->type != GDK_BUTTON_PRESS || ev->button != GDK_BUTTON_SECONDARY) return FALSE;
  menu_target = info;
  GtkWidget *box = gtk_bin_get_child(GTK_BIN(app_pop));
  GList *kids = gtk_container_get_children(GTK_CONTAINER(box));
  gtk_button_set_label(GTK_BUTTON(kids->data), is_pinned(g_app_info_get_id(info)) ? "Unpin from start" : "Pin to start");
  g_list_free(kids);
  gtk_popover_set_relative_to(GTK_POPOVER(app_pop), w);
  gtk_popover_popup(GTK_POPOVER(app_pop));
  return TRUE;
}

/* ---- building the views ---- */

static GtkWidget *app_icon(GAppInfo *info, int size)
{
  GIcon *icon = g_app_info_get_icon(info);
  GtkWidget *img = icon ? gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG)
                        : gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size(GTK_IMAGE(img), size);
  return img;
}

static void clear(GtkWidget *container)
{
  GList *kids = gtk_container_get_children(GTK_CONTAINER(container));
  for (GList *l = kids; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(kids);
}

static void fill_pinned(void)
{
  clear(pinned_box);
  for (gchar **p = pinned; *p; p++) {
    GAppInfo *info = find_app(*p);
    if (!info) continue;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(box), app_icon(info, 40), FALSE, FALSE, 0);
    GtkWidget *label = gtk_label_new(g_app_info_get_name(info));
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_lines(GTK_LABEL(label), 2);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 11);
    gtk_label_set_width_chars(GTK_LABEL(label), 11);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    GtkWidget *ev = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ev), box);
    g_object_set_data(G_OBJECT(ev), "app", info);
    gtk_widget_set_tooltip_text(ev, g_app_info_get_description(info));
    g_signal_connect(ev, "button-press-event", G_CALLBACK(on_app_button), info);
    gtk_flow_box_insert(GTK_FLOW_BOX(pinned_box), ev, -1);
  }
  gtk_widget_show_all(pinned_box);
}

static GtkWidget *app_row(GAppInfo *info)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start(GTK_BOX(box), app_icon(info, 28), FALSE, FALSE, 0);
  GtkWidget *label = gtk_label_new(g_app_info_get_name(info));
  gtk_label_set_xalign(GTK_LABEL(label), 0);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
  GtkWidget *ev = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(ev), box);
  g_signal_connect(ev, "button-press-event", G_CALLBACK(on_app_button), info);
  GtkWidget *row = gtk_list_box_row_new();
  gtk_container_add(GTK_CONTAINER(row), ev);
  g_object_set_data(G_OBJECT(row), "app", info);
  return row;
}

static void fill_all(void)
{
  clear(all_list);
  gunichar last = 0;
  for (GList *l = apps; l; l = l->next) {
    const char *name = g_app_info_get_name(l->data);
    gunichar c = g_unichar_toupper(g_utf8_get_char(name));
    if (!g_unichar_isalpha(c)) c = '#';
    if (c != last) {
      char buf[8] = {0};
      g_unichar_to_utf8(c, buf);
      GtkWidget *letter = gtk_label_new(buf);
      gtk_label_set_xalign(GTK_LABEL(letter), 0);
      gtk_style_context_add_class(gtk_widget_get_style_context(letter), "mm-letter");
      GtkWidget *row = gtk_list_box_row_new();
      gtk_container_add(GTK_CONTAINER(row), letter);
      gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
      gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
      gtk_widget_set_can_focus(row, FALSE);
      gtk_container_add(GTK_CONTAINER(all_list), row);
      last = c;
    }
    gtk_container_add(GTK_CONTAINER(all_list), app_row(l->data));
  }
  gtk_widget_show_all(all_list);
}

static char *when(time_t t)
{
  GDateTime *now = g_date_time_new_now_local();
  GDateTime *local = g_date_time_new_from_unix_local(t);
  int days = g_date_time_get_day_of_year(now) - g_date_time_get_day_of_year(local)
           + 365 * (g_date_time_get_year(now) - g_date_time_get_year(local));
  char *s = days <= 0 ? g_date_time_format(local, "today, %H:%M")
          : days == 1 ? g_strdup("yesterday")
          : days < 7 ? g_date_time_format(local, "%A")
          : g_date_time_format(local, "%-d %B");
  g_date_time_unref(local);
  g_date_time_unref(now);
  return s;
}

static gint newest_first(gconstpointer a, gconstpointer b)
{
  time_t ta = gtk_recent_info_get_modified((GtkRecentInfo *) a), tb = gtk_recent_info_get_modified((GtkRecentInfo *) b);
  return ta < tb ? 1 : ta > tb ? -1 : 0;
}

static void on_recent_row(GtkListBox *list, GtkListBoxRow *row, gpointer data)
{
  (void) list; (void) data;
  open_uri(g_object_get_data(G_OBJECT(row), "uri"));
}

static void fill_recent(void)
{
  clear(recent_box);
  GList *items = g_list_sort(gtk_recent_manager_get_items(gtk_recent_manager_get_default()), newest_first);
  int n = 0;
  for (GList *l = items; l && n < RECENT_MAX; l = l->next) {
    GtkRecentInfo *ri = l->data;
    if (!gtk_recent_info_is_local(ri) || !gtk_recent_info_exists(ri)) continue;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GIcon *gi = gtk_recent_info_get_gicon(ri);
    GtkWidget *img = gi ? gtk_image_new_from_gicon(gi, GTK_ICON_SIZE_DND)
                        : gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 28);
    if (gi) g_object_unref(gi);
    gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *name = gtk_label_new(gtk_recent_info_get_display_name(ri));
    gtk_label_set_xalign(GTK_LABEL(name), 0);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_MIDDLE);
    gchar *path = g_filename_from_uri(gtk_recent_info_get_uri(ri), NULL, NULL);
    gchar *dir = path ? g_path_get_dirname(path) : g_strdup("");
    gchar *folder = g_path_get_basename(dir);
    char *w = when(gtk_recent_info_get_modified(ri));
    gchar *sub = g_strdup_printf("%s · %s", g_strcmp0(dir, g_get_home_dir()) == 0 ? "Home" : folder, w);
    GtkWidget *meta = gtk_label_new(sub);
    gtk_label_set_xalign(GTK_LABEL(meta), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(meta), "mm-dim");
    gtk_box_pack_start(GTK_BOX(text), name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), meta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), box);
    g_object_set_data_full(G_OBJECT(row), "uri", g_strdup(gtk_recent_info_get_uri(ri)), g_free);
    gtk_container_add(GTK_CONTAINER(recent_box), row);
    g_free(sub); g_free(w); g_free(folder); g_free(dir); g_free(path);
    n++;
  }
  g_list_free_full(items, (GDestroyNotify) gtk_recent_info_unref);
  gtk_widget_show_all(recent_box);
  gtk_widget_set_visible(recent_box, n > 0);
  gtk_widget_set_visible(recent_empty, n == 0);
}

/* ---- search ---- */

typedef struct { GAppInfo *info; int score; } Hit;

/* GLib ranks matches by where the words were found (name, keywords, ...); within that, an app whose name starts with
   the query, then one with a word that does, then the shorter name comes first ("term": Terminal before its settings) */
static int score(GAppInfo *info, const char *q, int group)
{
  gchar *name = g_utf8_casefold(g_app_info_get_name(info), -1), *fq = g_utf8_casefold(q, -1);
  gboolean starts = g_str_has_prefix(name, fq), word = starts;
  for (char *p = name; !word && (p = strchr(p, ' ')); p++) word = g_str_has_prefix(p + 1, fq);
  int s = (starts ? 0 : word ? 1 : 2) * 100000 + group * 1000 + (int) MIN(g_utf8_strlen(name, -1), 999);
  g_free(name); g_free(fq);
  return s;
}

static gint by_score(gconstpointer a, gconstpointer b) { return ((const Hit *) a)->score - ((const Hit *) b)->score; }

static void on_search_changed(GtkSearchEntry *entry, gpointer data)
{
  (void) data;
  const char *q = gtk_entry_get_text(GTK_ENTRY(entry));
  if (!*q) { gtk_stack_set_visible_child_name(GTK_STACK(stack), "home"); return; }
  clear(results_list);
  GArray *hits = g_array_new(FALSE, FALSE, sizeof(Hit));
  gchar ***groups = g_desktop_app_info_search(q);
  int gi = 0;
  for (gchar ***g = groups; *g; g++, gi++) {
    for (gchar **id = *g; *id; id++) {
      GAppInfo *info = find_app(*id);
      if (info) { Hit h = { info, score(info, q, gi) }; g_array_append_val(hits, h); }
    }
    g_strfreev(*g);
  }
  g_free(groups);
  g_array_sort(hits, by_score);
  int n = MIN(hits->len, 40u);
  for (int i = 0; i < n; i++) gtk_container_add(GTK_CONTAINER(results_list), app_row(g_array_index(hits, Hit, i).info));
  g_array_free(hits, TRUE);
  if (n == 0) {
    GtkWidget *none = gtk_label_new("Nothing found");
    gtk_style_context_add_class(gtk_widget_get_style_context(none), "mm-dim");
    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), none);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_container_add(GTK_CONTAINER(results_list), row);
  }
  gtk_widget_show_all(results_list);
  GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_list), 0);
  if (n && first) gtk_list_box_select_row(GTK_LIST_BOX(results_list), first);
  gtk_stack_set_visible_child_name(GTK_STACK(stack), "search");
}

static void on_search_activate(GtkEntry *entry, gpointer data)
{
  (void) data;
  if (!gtk_entry_get_text_length(entry)) return;
  /* Enter typed straight after the query, before the search field's short delay ran the search */
  on_search_changed(GTK_SEARCH_ENTRY(entry), NULL);
  GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(results_list));
  GAppInfo *info = row ? g_object_get_data(G_OBJECT(row), "app") : NULL;
  if (info) launch(info);
}

static void on_app_row(GtkListBox *list, GtkListBoxRow *row, gpointer data)
{
  (void) list; (void) data;
  GAppInfo *info = g_object_get_data(G_OBJECT(row), "app");
  if (info) launch(info);
}

static void on_pinned(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer data)
{
  (void) box; (void) data;
  GAppInfo *info = g_object_get_data(G_OBJECT(gtk_bin_get_child(GTK_BIN(child))), "app");
  if (info) launch(info);
}

/* ---- showing and hiding ---- */

static void update_hint(void)
{
  gtk_widget_set_visible(hint, gtk_entry_get_text_length(GTK_ENTRY(search)) == 0);
}

static void place(void)
{
  GdkDisplay *d = gdk_display_get_default();
  GdkSeat *seat = gdk_display_get_default_seat(d);
  int px = 0, py = 0;
  gdk_device_get_position(gdk_seat_get_pointer(seat), NULL, &px, &py);
  GdkMonitor *m = gdk_display_get_monitor_at_point(d, px, py);
  GdkRectangle wa;
  gdk_monitor_get_workarea(m, &wa);
  int w = CARD_W + 2 * SHADOW, h = MIN(CARD_H, wa.height - 2 * GAP) + 2 * SHADOW;
  gtk_widget_set_size_request(win, w, h);     /* the card is the window less its shadow margin */
  gtk_window_resize(GTK_WINDOW(win), w, h);
  gtk_window_move(GTK_WINDOW(win), wa.x + (wa.width - w) / 2, wa.y + wa.height - h - GAP + SHADOW);
}

static void show_menu(void)
{
  gtk_entry_set_text(GTK_ENTRY(search), "");
  gtk_stack_set_visible_child_name(GTK_STACK(stack), "home");
  fill_recent();
  place();
  guint32 t = GDK_CURRENT_TIME;
  gtk_widget_realize(win);
#ifdef GDK_WINDOWING_X11
  /* the menu is asked for by another process (the taskbar button, the Super key), so it has no user timestamp of its own;
     the X server's current time is newer than the last click, so the window manager hands it the keyboard focus */
  if (GDK_IS_X11_WINDOW(gtk_widget_get_window(win))) t = gdk_x11_get_server_time(gtk_widget_get_window(win));
#endif
  gtk_window_present_with_time(GTK_WINDOW(win), t);
  place();      /* again once mapped: the window manager may have placed it by its own rules */
  gtk_widget_grab_focus(search);
}

static void on_activate(GApplication *a, gpointer data)
{
  (void) a; (void) data;
  if (background) { background = FALSE; return; }
  if (gtk_widget_get_visible(win)) { hide_menu(); return; }
  /* the click on the taskbar button that should close the menu took its focus away first and hid it */
  if (g_get_monotonic_time() - hidden_at < 400000) return;
  show_menu();
}

static gboolean on_focus_out(GtkWidget *w, GdkEvent *ev, gpointer data)
{
  (void) w; (void) ev; (void) data;
  if (gtk_widget_get_visible(power_pop) || gtk_widget_get_visible(app_pop)) return FALSE;
  hide_menu();
  return FALSE;
}

static gboolean on_key(GtkWidget *w, GdkEventKey *ev, gpointer data)
{
  (void) w; (void) data;
  if (ev->keyval == GDK_KEY_Escape) {
    if (gtk_entry_get_text_length(GTK_ENTRY(search)) || g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(stack)), "home"))
      { gtk_entry_set_text(GTK_ENTRY(search), ""); gtk_stack_set_visible_child_name(GTK_STACK(stack), "home"); }
    else hide_menu();
    return TRUE;
  }
  if ((ev->keyval == GDK_KEY_Down || ev->keyval == GDK_KEY_Up) && gtk_widget_has_focus(search)
      && !g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(stack)), "search")) {
    GtkListBoxRow *r = gtk_list_box_get_selected_row(GTK_LIST_BOX(results_list));
    if (r) gtk_widget_grab_focus(GTK_WIDGET(r));
    return FALSE;
  }
  /* typing anywhere goes to the search field */
  if (!gtk_widget_has_focus(search) && ev->length > 0 && g_unichar_isprint(gdk_keyval_to_unicode(ev->keyval))) {
    gtk_widget_grab_focus(search);
    gtk_editable_set_position(GTK_EDITABLE(search), -1);
    return gtk_widget_event(search, (GdkEvent *) ev);
  }
  return FALSE;
}

static void on_apps_changed(GAppInfoMonitor *m, gpointer data)
{
  (void) m; (void) data;
  load_apps();
  fill_pinned();
  fill_all();
}

/* ---- the window ---- */

static GtkWidget *heading(const char *text, GtkWidget *extra)
{
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *l = gtk_label_new(text);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(l), "mm-heading");
  gtk_box_pack_start(GTK_BOX(row), l, TRUE, TRUE, 0);
  if (extra) gtk_box_pack_end(GTK_BOX(row), extra, FALSE, FALSE, 0);
  gtk_widget_set_margin_start(row, 8);
  gtk_widget_set_margin_end(row, 4);
  return row;
}

static GtkWidget *link_button(const char *text, GCallback cb)
{
  GtkWidget *b = gtk_button_new_with_label(text);
  gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "mm-link");
  gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
  g_signal_connect(b, "clicked", cb, NULL);
  return b;
}

static void go_all(void) { gtk_stack_set_visible_child_name(GTK_STACK(stack), "all"); }
static void go_home(void) { gtk_stack_set_visible_child_name(GTK_STACK(stack), "home"); }

static GtkWidget *list_in_scroller(GtkWidget **list)
{
  *list = gtk_list_box_new();
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(*list), TRUE);
  g_signal_connect(*list, "row-activated", G_CALLBACK(on_app_row), NULL);
  GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(sc), *list);
  gtk_widget_set_vexpand(sc, TRUE);
  return sc;
}

static void power_action(GtkWidget *b, gpointer cmd)
{
  (void) b;
  gtk_widget_hide(power_pop);
  run(cmd);
}

static GtkWidget *round_button(const char *icon, const char *tip)
{
  GtkWidget *b = gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(b, tip);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "mm-round");
  gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
  return b;
}

static void on_lock(GtkWidget *b, gpointer d) { (void) b; (void) d; run("xflock4"); }
static void on_power(GtkWidget *b, gpointer d) { (void) d; gtk_popover_set_relative_to(GTK_POPOVER(power_pop), b);
                                                  gtk_popover_popup(GTK_POPOVER(power_pop)); }

static GtkWidget *popover_with(const char *const *labels, const char *const *cmds, GCallback cb)
{
  GtkWidget *pop = gtk_popover_new(NULL);
  gtk_style_context_add_class(gtk_widget_get_style_context(pop), "mm-pop");
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_set_border_width(GTK_CONTAINER(box), 6);
  for (int i = 0; labels[i]; i++) {
    GtkWidget *b = gtk_button_new_with_label(labels[i]);
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(b)), GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "flat");
    g_signal_connect(b, "clicked", cb, cmds ? (gpointer) cmds[i] : NULL);
    gtk_container_add(GTK_CONTAINER(box), b);
  }
  gtk_widget_show_all(box);
  gtk_container_add(GTK_CONTAINER(pop), box);
  gtk_popover_set_position(GTK_POPOVER(pop), GTK_POS_TOP);
  return pop;
}

static void build(void)
{
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, CSS, -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  win = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(win), "melon menu");
  gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_MENU);  /* xfwm4 adds no frame or shadow */
  gtk_window_set_modal(GTK_WINDOW(win), TRUE);   /* as the Whisker menu: xfwm4 gives a modal menu window the focus */
  gtk_window_stick(GTK_WINDOW(win));
  gtk_style_context_add_class(gtk_widget_get_style_context(win), "melon-menu");
  GdkVisual *rgba = gdk_screen_get_rgba_visual(gdk_screen_get_default());
  if (rgba) gtk_widget_set_visual(win, rgba);      /* round corners and the shadow need a compositor (melon-xfce turns it on) */
  gtk_widget_set_app_paintable(win, TRUE);
  g_signal_connect(win, "focus-out-event", G_CALLBACK(on_focus_out), NULL);
  g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
  g_signal_connect(win, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(card), "mm-card");
  gtk_container_add(GTK_CONTAINER(win), card);

  GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_container_set_border_width(GTK_CONTAINER(body), 22);
  gtk_box_pack_start(GTK_BOX(card), body, TRUE, TRUE, 0);

  search = gtk_search_entry_new();
  gtk_style_context_add_class(gtk_widget_get_style_context(search), "mm-search");
  g_signal_connect(search, "search-changed", G_CALLBACK(on_search_changed), NULL);
  g_signal_connect(search, "activate", G_CALLBACK(on_search_activate), NULL);
  /* GTK 3 hides an entry's placeholder while it has the focus, and the search field always has it here */
  GtkWidget *over = gtk_overlay_new();
  gtk_container_add(GTK_CONTAINER(over), search);
  hint = gtk_label_new("Type to search apps and settings");
  gtk_style_context_add_class(gtk_widget_get_style_context(hint), "mm-hint");
  gtk_widget_set_halign(hint, GTK_ALIGN_START);
  gtk_widget_set_margin_start(hint, 38);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), hint);
  gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(over), hint, TRUE);
  g_signal_connect_swapped(search, "changed", G_CALLBACK(update_hint), NULL);
  gtk_box_pack_start(GTK_BOX(body), over, FALSE, FALSE, 0);

  stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration(GTK_STACK(stack), 120);
  gtk_box_pack_start(GTK_BOX(body), stack, TRUE, TRUE, 0);

  /* home: pinned and recent */
  GtkWidget *home = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(home), heading("Pinned", link_button("All apps  ›", G_CALLBACK(go_all))), FALSE, FALSE, 0);
  pinned_box = gtk_flow_box_new();
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(pinned_box), COLUMNS);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(pinned_box), COLUMNS);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(pinned_box), TRUE);
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(pinned_box), GTK_SELECTION_NONE);
  gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(pinned_box), TRUE);
  g_signal_connect(pinned_box, "child-activated", G_CALLBACK(on_pinned), NULL);
  GtkWidget *psc = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(psc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(psc), TRUE);
  gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(psc), 300);
  gtk_container_add(GTK_CONTAINER(psc), pinned_box);
  gtk_box_pack_start(GTK_BOX(home), psc, FALSE, FALSE, 0);
  GtkWidget *rh = heading("Recent", NULL);
  gtk_widget_set_margin_top(rh, 14);
  gtk_box_pack_start(GTK_BOX(home), rh, FALSE, FALSE, 0);
  recent_box = gtk_list_box_new();
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(recent_box), TRUE);
  g_signal_connect(recent_box, "row-activated", G_CALLBACK(on_recent_row), NULL);
  gtk_box_pack_start(GTK_BOX(home), recent_box, FALSE, FALSE, 0);
  recent_empty = gtk_label_new("Files you open will show up here.");
  gtk_label_set_xalign(GTK_LABEL(recent_empty), 0);
  gtk_widget_set_margin_start(recent_empty, 8);
  gtk_style_context_add_class(gtk_widget_get_style_context(recent_empty), "mm-dim");
  gtk_box_pack_start(GTK_BOX(home), recent_empty, FALSE, FALSE, 0);
  gtk_stack_add_named(GTK_STACK(stack), home, "home");

  /* all apps */
  GtkWidget *all = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(all), heading("All apps", link_button("‹  Back", G_CALLBACK(go_home))), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(all), list_in_scroller(&all_list), TRUE, TRUE, 0);
  gtk_stack_add_named(GTK_STACK(stack), all, "all");

  /* search results */
  GtkWidget *res = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start(GTK_BOX(res), heading("Best matches", NULL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(res), list_in_scroller(&results_list), TRUE, TRUE, 0);
  gtk_stack_add_named(GTK_STACK(stack), res, "search");

  /* footer: the user, lock and power */
  GtkWidget *foot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_style_context_add_class(gtk_widget_get_style_context(foot), "mm-footer");
  const char *real = g_get_real_name();
  const char *who = real && *real && strcmp(real, "Unknown") ? real : g_get_user_name();
  char initial[8] = {0};
  g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(who)), initial);
  GtkWidget *avatar = gtk_label_new(initial);
  gtk_style_context_add_class(gtk_widget_get_style_context(avatar), "mm-avatar");
  gtk_widget_set_valign(avatar, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(foot), avatar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(foot), gtk_label_new(who), FALSE, FALSE, 0);
  GtkWidget *power = round_button("system-shutdown-symbolic", "Power");
  g_signal_connect(power, "clicked", G_CALLBACK(on_power), NULL);
  gtk_box_pack_end(GTK_BOX(foot), power, FALSE, FALSE, 0);
  GtkWidget *lock = round_button("system-lock-screen-symbolic", "Lock");
  g_signal_connect(lock, "clicked", G_CALLBACK(on_lock), NULL);
  gtk_box_pack_end(GTK_BOX(foot), lock, FALSE, FALSE, 0);
  GtkWidget *settings = round_button("emblem-system-symbolic", "Settings");
  g_signal_connect_swapped(settings, "clicked", G_CALLBACK(run), "xfce4-settings-manager");
  gtk_box_pack_end(GTK_BOX(foot), settings, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(card), foot, FALSE, FALSE, 0);

  static const char *const plabels[] = { "Sleep", "Restart", "Shut down", "Log out", NULL };
  static const char *const pcmds[] = { "xfce4-session-logout --suspend", "xfce4-session-logout --reboot --fast",
                                       "xfce4-session-logout --halt --fast", "xfce4-session-logout --logout --fast" };
  power_pop = popover_with(plabels, pcmds, G_CALLBACK(power_action));
  static const char *const alabels[] = { "Pin to start", NULL };
  app_pop = popover_with(alabels, NULL, G_CALLBACK(toggle_pin));

  load_apps();
  load_pinned();
  fill_pinned();
  fill_all();
  g_signal_connect(g_app_info_monitor_get(), "changed", G_CALLBACK(on_apps_changed), NULL);
  gtk_widget_show_all(card);
}

static void on_startup(GApplication *a, gpointer data)
{
  (void) a; (void) data;
  build();
  g_application_hold(G_APPLICATION(app));     /* stay running while hidden */
}

static gint on_local_options(GApplication *a, GVariantDict *opts, gpointer data)
{
  (void) data;
  if (g_variant_dict_contains(opts, "background")) {
    /* at login: start hidden, or do nothing when the menu is already running */
    if (!g_application_register(a, NULL, NULL) || g_application_get_is_remote(a)) return 0;
    background = TRUE;
  }
  if (g_variant_dict_contains(opts, "version")) { g_print("melon-menu 0.1\n"); return 0; }
  return -1;
}

int main(int argc, char **argv)
{
  app = gtk_application_new("org.melon.Menu", G_APPLICATION_DEFAULT_FLAGS);
  g_application_add_main_option(G_APPLICATION(app), "background", 0, 0, G_OPTION_ARG_NONE,
                                "Start without showing the menu (at login)", NULL);
  g_application_add_main_option(G_APPLICATION(app), "version", 0, 0, G_OPTION_ARG_NONE, "Print the version", NULL);
  g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
  g_signal_connect(app, "startup", G_CALLBACK(on_startup), NULL);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
