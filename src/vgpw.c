#include <gtk/gtk.h>
#include <stdbool.h>
#include <fcntl.h>

#include "vgplib.h"
#include "style.h"

#define WINDOW_TITLE "Vivid GPIO Utility (V%.2f)"

#define LOCK_FILE "/var/lock/vgpw.lock"

#define GRID_WIDTH  60
#define GRID_HEIGHT 40

#define IN      "IN"
#define OUT     "OUT"
#define ALT1    "ALT1"
#define ALT2    "ALT2"
#define ALT3    "ALT3"
#define UNKNOWN "???"

#define ODD_PIN_MARKUP   "<span font_desc=\"Arial 14px bold\">%s</span>\n<span font_desc=\"Arial 9px\">%s</span>"
#define EVEN_PIN_MARKUP  "<span font_desc=\"Arial 9px\">%s</span>\n<span font_desc=\"Arial 14px bold\">%s</span>"
#define MARKUP_MAX_LENGTH 128


GtkWidget *grid;
GtkWidget *adc_label;

bool flipped = false;


void remove_lock_file() {
  remove(LOCK_FILE);
}


void make_sure_single_instance() {
  int fd = open(LOCK_FILE, O_RDWR | O_CREAT | O_EXCL, 0666);
  if (fd == -1)
  {
    exit(EXIT_FAILURE);
  }
  close(fd);
  atexit(remove_lock_file);
}


void load_css(void) 
{
  GtkCssProvider *provider = gtk_css_provider_new();
  GdkDisplay *display = gdk_display_get_default();
  GdkScreen *screen = gdk_display_get_default_screen(display);
  gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_css_provider_load_from_data(provider, style_css, -1, NULL);
  g_object_unref(provider);
}


void add_class(GtkWidget *widget, const char *cls)
{
  GtkStyleContext *context = gtk_widget_get_style_context(widget);
  gtk_style_context_add_class(context, cls);
}


gboolean refresh_pin_state(gpointer p) {
  MonitorThread * params = (MonitorThread *)p;
  int col = flipped ? ((params->pin - 1) / 2) : (19 - (params->pin - 1) / 2);
  int row = (params->pin % 2) ? 7 : 0;
  GtkWidget * value_button = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
  if (value_button != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(value_button), params->latest_event == GPIO_RISING_EDGE ? "1" : "0");
  }
  return FALSE;
}


gboolean refresh_adc_state(gpointer p) {
  int a0 = get_adc(0);
  float v0 = get_voltage_by_adc(a0);
  int a3 = get_adc(3);
  float v3 = get_voltage_by_adc(a3);
  int a4 = get_adc(4);
  float v4 = get_voltage_by_adc(a4);
  char buf[64];
  sprintf(buf, "A0 = %d (%.3fV),  A3 = %d (%.3fV),  A4 = %d (%.3fV)", a0, v0, a3, v3, a4, v4);
  gtk_label_set_text(GTK_LABEL(adc_label), buf);
  return TRUE;
}


void on_pin_state_changed(void *p)
{
  g_idle_add(refresh_pin_state, (gpointer)p);
}


void init_monitor_threads(void (*callback)(void*))
{
  monitor_threads[0] = NULL;
  for (int pin = 1; pin < MONITOR_THREADS; pin ++) {
    int col = flipped ? ((pin - 1) / 2) : (19 - (pin - 1) / 2);
    int row = (pin % 2) ? 6 : 1;
    GtkWidget * mode_button = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
    const char* mode = gtk_button_get_label(GTK_BUTTON(mode_button));
    if (strcmp(mode, IN) == 0)
    {
      create_monitor_thread(pin, 0, GPIO_BOTH_EDGES, callback);
    }
    else
    {
      monitor_threads[pin] = NULL;
    }
  }
}


void update_pin_info(int pin, bool flipped)
{
  char * pin_name = (char *)NAMES[pin];
  int ch = get_chip_number(pin_name);
  int ln = get_line_number(pin_name);
  char mode[5];
  int alt = get_alt(ch, ln);
  int dir = 0;
  if (alt == 0)
  {
    dir = get_dir(ch, ln);
    if (dir == 0)
    {
      strcpy(mode, IN);
    }
    else if (dir == 1)
    {
      strcpy(mode, OUT);
    }
    else
    {
      strcpy(mode, UNKNOWN);
    }
  }
  else
  {
    sprintf(mode, "ALT%d", alt);
  }
  
  int col = flipped ? ((pin - 1) / 2) : (19 - (pin - 1) / 2);
  int row = (pin % 2) ? 5 : 2;
   
  GtkWidget * pin_label = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
  if (pin_label != NULL)
  {
    char markup[MARKUP_MAX_LENGTH];
    if (pin % 2)
    {
      sprintf(markup, ODD_PIN_MARKUP, NAMES[pin], FUNCTIONS[pin][alt]);
    }
    else
    {
      sprintf(markup, EVEN_PIN_MARKUP, FUNCTIONS[pin][alt], NAMES[pin]);
    }
    gtk_label_set_markup(GTK_LABEL(pin_label), markup);
  }
  
  row = (pin % 2) ? 6 : 1;
  GtkWidget * mode_button = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
  if (mode_button != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(mode_button), mode);
  }
  
  int val = get(ch, ln);
  char value[3];
  sprintf(value, "%d", val);
  
  row = (pin % 2) ? 7 : 0;
  GtkWidget * value_button = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
  if (value_button != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(value_button), value);
    gtk_widget_set_sensitive(value_button, (dir == 1));
  }
}


void load_all_pin_info()
{
  for (int pin = 1; pin <= 40; pin ++)
  {
    if (!is_power_pin(pin))
    {
      update_pin_info(pin, flipped);
    }
  }
}


const char* get_next_mode(const char *mode)
{
  if (strcmp(mode, IN) == 0)
  {
    return OUT;
  } else if (strcmp(mode, OUT) == 0)
  {
    return ALT1;
  } else if (strcmp(mode, ALT1) == 0)
  {
    return ALT2;
  } else if (strcmp(mode, ALT2) == 0)
  {
    return ALT3;
  } else if (strcmp(mode, ALT3) == 0)
  {
    return IN;
  }
  return NULL;
}


int get_alt_by_mode(const char *mode)
{
  if (strcmp(mode, IN) == 0 || strcmp(mode, OUT) == 0) 
  {
    return 0;  
  }
  else if (strcmp(mode, ALT1) == 0)
  {
    return 1;
  }
  else if (strcmp(mode, ALT2) == 0)
  {
    return 2;
  }
  else if (strcmp(mode, ALT3) == 0)
  {
    return 3;
  }
  return -1;
}


int get_dir_by_mode(const char *mode)
{
  if (strcmp(mode, IN) == 0) 
  {
    return 0;  
  }
  else if (strcmp(mode, OUT) == 0)
  {
    return 1;
  }
  return -1;
}


void mode_button_clicked(GtkButton *button, gpointer data)
{ 
  const char *name = gtk_widget_get_name(GTK_WIDGET(button));
  int pin = atoi(&name[1]);
  char * pin_name = (char *)NAMES[pin];
  int ch = get_chip_number(pin_name);
  int ln = get_line_number(pin_name);
  const char* mode = gtk_button_get_label(button);
  const char* new_mode = get_next_mode(mode);
  if (new_mode != NULL)
  {
    gtk_button_set_label(button, new_mode);
    int alt = get_alt(ch, ln);
    int new_alt = get_alt_by_mode(new_mode);
    if (new_alt != alt)
    {
      set_alt(ch, ln, new_alt);
    }
    if (new_alt == 0)
    {
      int dir = get_dir(ch, ln);
      int new_dir = get_dir_by_mode(new_mode);
      if (new_dir == 0)
      {
        // ALT3->IN: create monitor thread
        create_monitor_thread(pin, 1, GPIO_BOTH_EDGES, on_pin_state_changed);
      }
      else if (new_dir == 1)
      {
        // IN->OUT: cancel monitor thread
        if (monitor_threads[pin] != NULL)
        {
          pthread_cancel(monitor_threads[pin]->thread);
          pthread_join(monitor_threads[pin]->thread, NULL);
          free(monitor_threads[pin]);
          monitor_threads[pin] = NULL;
        }
      }
      if (new_dir != dir)
      {
        set_dir(ch, ln, new_dir); 
      }
    }
  }
  update_pin_info(pin, flipped);
}


void value_button_clicked(GtkButton *button, gpointer data)
{
  const char *name = gtk_widget_get_name(GTK_WIDGET(button));
  int pin = atoi(&name[1]);
  char * pin_name = (char *)NAMES[pin];
  int ch = get_chip_number(pin_name);
  int ln = get_line_number(pin_name);
  int alt = get_alt(ch, ln);
  if (alt == 0)
  {
    int dir = get_dir(ch, ln);
    if (dir == 1)
    {
      int value = atoi(gtk_button_get_label(button));
      int new_value = (value == 1 ? 0 : 1);
      gtk_button_set_label(button, new_value ? "1" : "0");
      set(ch, ln, new_value);
    }
  }
  update_pin_info(pin, flipped);
}


void flip_view()
{
  flipped = !flipped;
  for (int row = 0; row < 8; row ++)
  {
    for (int col = 0; col < 10; col ++)
    {
      int mirrored_col = 19 - col;
      
      GtkWidget* child = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
      g_object_ref(child);
      gtk_container_remove(GTK_CONTAINER(grid), child);
      
      GtkWidget* peer = gtk_grid_get_child_at(GTK_GRID(grid), mirrored_col, row);
      g_object_ref(peer);
      gtk_container_remove(GTK_CONTAINER(grid), peer);

      gtk_grid_attach(GTK_GRID(grid), peer, col, row, 1, 1);
      g_object_unref(peer);
      
      gtk_grid_attach(GTK_GRID(grid), child, mirrored_col, row, 1, 1);
      g_object_unref(child);
    }
  }
  gtk_widget_show_all(grid);
}


int main(int argc, char *argv[])
{
  make_sure_single_instance();
  
  gtk_init(&argc, &argv);

  load_css();

  // main window with grid layout
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  char title[32];
  sprintf(title, WINDOW_TITLE, VGP_VERSION);
  
  gtk_window_set_title(GTK_WINDOW(window), title);
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
  gtk_container_set_border_width(GTK_CONTAINER(window), 10);
  GdkPixbuf *icon = gdk_pixbuf_new_from_file("/usr/share/icons/hicolor/48x48/apps/icon48.png", NULL);
  if(icon)
  {
    gtk_window_set_icon(GTK_WINDOW(window), icon);
  }
  grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(window), grid);

  int num_rows = 8;
  int num_columns = 20;
  GtkWidget *button;
  GtkWidget *label;
  int pin;
  char name[4];

  for (int j = 0; j < num_columns; j ++)
  {
    pin = (num_columns - j) * 2;
    
    bool power_pin = is_power_pin(pin);    

    // value buttons for pins with even number
    button = gtk_button_new_with_label(power_pin ? "" : "1");
    gtk_widget_set_size_request(button, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "v%d", pin);
    gtk_widget_set_name(button, name);
    gtk_widget_set_sensitive (button, !power_pin);
    g_signal_connect(button, "clicked", G_CALLBACK(value_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, j, 0, 1, 1);

    // mode buttons for pins with even number
    button = gtk_button_new_with_label(power_pin ? "" : "IN");
    gtk_widget_set_size_request(button, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "m%d", pin);
    gtk_widget_set_name(button, name);
    gtk_widget_set_sensitive (button, !power_pin);
    g_signal_connect(button, "clicked", G_CALLBACK(mode_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, j, 1, 1, 1);

    // labels for pins with even number
    label = gtk_label_new(NULL);
    char markup[MARKUP_MAX_LENGTH];
    sprintf(markup, EVEN_PIN_MARKUP, FUNCTIONS[pin][0], NAMES[pin]);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_size_request(label, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "l%d", pin);
    gtk_widget_set_name(label, name);
    add_class(label, "gpio-label");
    gtk_grid_attach(GTK_GRID(grid), label, j, 2, 1, 1);

    // 2x20 pin GPIO header
    for (int i = 3; i < 5; i ++)
    {
      pin -= (i == 3 ? 0 : 1);
      sprintf(name, "%d", pin);
      label = gtk_label_new(name);
      gtk_widget_set_size_request(label, GRID_WIDTH, GRID_HEIGHT);
      sprintf(name, "p%d", pin);
      gtk_widget_set_name(label, name);
      add_class(label, "header");
      gtk_grid_attach(GTK_GRID(grid), label, j, i, 1, 1);
    }
    
    power_pin = is_power_pin(pin);

    // labels for pins with odd number
    label = gtk_label_new(NULL);
    sprintf(markup, ODD_PIN_MARKUP, NAMES[pin], FUNCTIONS[pin][0]);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_size_request(label, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "l%d", pin);
    gtk_widget_set_name(label, name);
    add_class(label, "gpio-label");
    gtk_grid_attach(GTK_GRID(grid), label, j, 5, 1, 1);

    // mode buttons for pins with odd number
    button = gtk_button_new_with_label(power_pin ? "" : "IN");
    gtk_widget_set_size_request(button, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "m%d", pin);
    gtk_widget_set_name(button, name);
    gtk_widget_set_sensitive (button, !power_pin);
    g_signal_connect(button, "clicked", G_CALLBACK(mode_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, j, 6, 1, 1);

    // value buttons for pins with odd number
    button = gtk_button_new_with_label(power_pin ? "" : "1");
    gtk_widget_set_size_request(button, GRID_WIDTH, GRID_HEIGHT);
    sprintf(name, "v%d", pin);
    gtk_widget_set_name(button, name);
    gtk_widget_set_sensitive (button, !power_pin);
    g_signal_connect(button, "clicked", G_CALLBACK(value_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button, j, 7, 1, 1);
  }
  
  load_all_pin_info();
  
  init_monitor_threads(on_pin_state_changed);
  
  // bottom bar
  label = gtk_label_new(NULL);
  gtk_widget_set_size_request(label, 5, 5);
  gtk_grid_attach(GTK_GRID(grid), label, 0, 8, 20, 1);
  
  adc_label = gtk_label_new("A0 = 0 (0.000V),  A3 = 0 (0.000V),  A4 = 0 (0.000V)");
  gtk_label_set_xalign(GTK_LABEL(adc_label), 0.0);
  gtk_widget_set_size_request(adc_label, GRID_WIDTH, GRID_HEIGHT);
  gtk_grid_attach(GTK_GRID(grid), adc_label, 0, 9, 19, 1);
  g_timeout_add(1000, refresh_adc_state, NULL);
  
  button = gtk_button_new_with_label("Flip");
  gtk_widget_set_size_request(button, GRID_WIDTH, GRID_HEIGHT);
  g_signal_connect(button, "clicked", G_CALLBACK(flip_view), grid);
  gtk_grid_attach(GTK_GRID(grid), button, 19, 9, 1, 1);
  
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}
