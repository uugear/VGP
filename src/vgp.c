#include <gpiod.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "vgplib.h"


char pin_name[] = "0A0";

bool pin_changed = false;

// vgp command parameters...
// vpg help
// vgp list
// vgp all
// vgp mode 4C2
// vgp mode 4C2 in/out
// vgp alt 4C2
// vgp alt 4C2 0/1/2/3
// vgp get 4C2
// vgp set 4C2 0
// vgp wfi 4C2 rising/falling/both
// vgp adc 0/3/4 [v/V]
void do_help(int argc, char *const *argv)
{
  printf("------------------------------------------------------------\n");
  printf("Vivid GPIO Utility (V%.2f)\n", VGP_VERSION);
  printf("------------------------------------------------------------\n");
  printf("\n");
  printf("This program can be used to manage the GPIOs in Vivid Unit.\n");
  printf("Usage: %s <command> <pin> <parameter>...\n", argv[0]);
  printf("\n");
  printf("  You may specify the physical pin number, or the GPIO pin name\n");
  printf("  For example: 4D2 means GPIO4_D2 (physical pin 12).\n");
  printf("\n");
  printf("[Supported Commands]\n");
  printf("  list: display 40-pin GPIO information.\n");
  printf("  all: print information for all GPIO pins.\n");
  printf("  mode: get/set the mode of the pin, could be input or output.\n");
  printf("  alt: get/set the ALT of the pin, could be 0, 1, 2 or 3.\n");
  printf("  get: get the value of the pin, could be 0 or 1.\n");
  printf("  set: set the value of the pin, could be 0 or 1.\n");
  printf("  wfi: wait until the pin status change. Parameter could be rising/falling/both\n");
  printf("  adc: get the ADC value or voltage at A0, A3 or A4.\n");
  printf("  help: print these information.\n");
  printf("  version: print the version information.\n");  
  printf("\n");
  printf("[Examples]\n");
  printf("  vpg list\n");
  printf("  vpg all\n");
  printf("  vpg mode 4D1 (same as \"vgp mode 7\")\n");
  printf("  vpg mode 4D1 out (same as \"vgp mode 7 out\")\n");
  printf("  vpg alt 2A0 (same as \"vgp alt 3\")\n");
  printf("  vpg alt 2A0 0 (same as \"vgp alt 3 0\")\n");
  printf("  vpg get 4D6 (same as \"vgp get 11\")\n");
  printf("  vpg set 4D6 1 (same as \"vgp set 11 1\")\n");
  printf("  vpg wfi 2D3 falling (same as \"vgp wfi 13 falling\")\n");
  printf("  vpg adc 0 (will print adc value in range 0~1023)\n");
  printf("  vpg adc 3 v (will print voltage instead)\n");
  printf("  vpg adc 4 V (will print unit after voltage value)\n");
  printf("  vpg help\n");
  printf("  vpg version\n");
  printf("\n");
  printf("------------------------------------------------------------\n");
}


bool get_pin_name(const char* pin)
{
  // TODO: support lower case letter (4c2 == 4C2)
  if (strlen(pin) == 3 && pin[0] >= '0' && pin[0] <= '4' && pin[1] >= 'A' && pin[1] <= 'D' && pin[2] >= '0' && pin[2] <= '7')
  {
    strcpy(pin_name, pin);
    return true;
  }
  else
  {
    // get pin name by physical pin number
    int p = atoi(pin);
    if (p == 0 || p > 40)
    {
      fprintf(stderr, "Incorrect pin: %s\n", pin);
    }
    else if (is_power_pin(p))
    {
      fprintf(stderr, "Pin %d (%s) is not an IO pin\n", p, NAMES[p]);
    }
    else
    {
      strcpy(pin_name, NAMES[p]);
      return true;
    }
  }
  return false;
}


void do_all()
{
  int gpio_set_values[5];
  int gpio_get_values[5];
  int gpio_directions[5];
  int i, j, k;
  int iomux[5][4];

  for (i = 0; i < 5; i ++)
  {
    gpio_set_values[i] = get_register(GPIO_BASE[i] + GPIO_SWPORTA_DR);
    gpio_get_values[i] = get_register(GPIO_BASE[i] + GPIO_EXT_PORTA);
    gpio_directions[i] = get_register(GPIO_BASE[i] + GPIO_SWPORTA_DDR);

    for (j = 0; j < 4; j ++)
    {
      iomux[i][j] = (GPIO_IOMUX[i][j] == -1) ? -1 : get_register((i < 2 ? PMUGRF : GRF) + GPIO_IOMUX[i][j]);
      int set_values = ((gpio_set_values[i] >> (j << 3)) & 0xff);
      int get_values = ((gpio_get_values[i] >> (j << 3)) & 0xff);
      int directions = ((gpio_directions[i] >> (j << 3)) & 0xff);

      for (k = 0; k < 8; k ++)
      {
        int alt = (iomux[i][j] == -1) ? 0 : ((iomux[i][j] >> (k << 1)) & 0x03);
        int direction = ((directions >> k) & 0x01);
        int value = (((direction == 0 ? get_values : set_values) >> k) & 0x01);
        printf("GPIO%d_%c%d: ALT=%d, V=%d, %s\n", i, GPIO_GROUP[j], k, alt, value, GPIO_DIRECTION[direction]);
      }
    } 
  }
}


void align_string(char* string, int length, int align)
{
  int string_length = strlen(string);
  if (string_length >= length)
  {
    return;
  }
  int num_spaces = length - string_length;
  if (align == -1)
  {
    for (int i = string_length; i < length; i++) {
        string[i] = ' ';
    }
    string[length] = '\0';
  } 
  else if (align == 0)
  {
    int left_spaces = num_spaces / 2;
    int right_spaces = num_spaces - left_spaces;
    for (int i = string_length; i >= 0; i--) {
        string[i + left_spaces] = string[i];
    }
    for (int i = 0; i < left_spaces; i++) {
        string[i] = ' ';
    }
    for (int i = string_length + left_spaces; i < length; i++) {
        string[i] = ' ';
    }
    string[length] = '\0';
  }
  else if (align == 1) 
  {
    for (int i = length - 1; i >= num_spaces; i--)
    {
      string[i] = string[i - num_spaces];
    }
    for (int i = 0; i < num_spaces; i++)
    {
        string[i] = ' ';
    }
    string[length] = '\0';
  }
}


void do_list()
{
  printf("+------+----------+------+---+----------+---+------+----------+------+\n");
  printf("| GPIO |   Name   | Mode | V | Physical | V | Mode |   Name   | GPIO |\n");
  printf("+------+----------+------+---+----++----+---+------+----------+------+\n");
  for (int i = 0; i < 20; i ++)
  {
    int left = i * 2 + 1;
    bool power_pin_left = is_power_pin(left);
    int chip_left = -1;
    int line_left = -1;
    int alt_left = -1;
    int dir_left = -1;
    if (!power_pin_left)
    {
      chip_left = get_chip_number((char *)NAMES[left]);
      line_left = get_line_number((char *)NAMES[left]);
      alt_left = get_alt(chip_left, line_left);
      dir_left = get_dir(chip_left, line_left);
    }

    char gpio_left[6];
    strcpy(gpio_left, power_pin_left ? "" : NAMES[left]);
    align_string(gpio_left, 5, 1);
    
    char name_left[10];
    strcpy(name_left, power_pin_left ? NAMES[left] : FUNCTIONS[left][alt_left]);
    align_string(name_left, 9, 1);
    
    char mode_left[7];
    if (power_pin_left)
    {
      strcpy(mode_left, "");
    }
    else if (alt_left == 0)
    {
      strcpy(mode_left, GPIO_DIRECTION[dir_left]);
    }
    else
    {
      sprintf(mode_left, "ALT%d", alt_left);
    }
    align_string(mode_left, 6, 0);
    
    char value_left[2];
    strcpy(value_left, " ");
    if (!power_pin_left)
    {
      value_left[0] = 0x30 + get(chip_left, line_left);
    }
    
    char pin_left[4];
    sprintf(pin_left, "%d", left);
    align_string(pin_left, 3, 1);
    
    int right = i * 2 + 2;
    bool power_pin_right = is_power_pin(right);
    int chip_right = -1;
    int line_right = -1;
    int alt_right = -1;
    int dir_right = -1;
    if (!power_pin_right)
    {
      chip_right = get_chip_number((char *)NAMES[right]);
      line_right = get_line_number((char *)NAMES[right]);
      alt_right = get_alt(chip_right, line_right);
      dir_right = get_dir(chip_right, line_right);
    }

    char gpio_right[6];
    strcpy(gpio_right, power_pin_right ? "" : NAMES[right]);
    align_string(gpio_right, 5, -1);
    
    char name_right[10];
    strcpy(name_right, power_pin_right ? NAMES[right] : FUNCTIONS[right][alt_right]);
    align_string(name_right, 9, -1);
    
    char mode_right[7];
    if (power_pin_right)
    {
      strcpy(mode_right, "");
    }
    else if (alt_right == 0)
    {
      strcpy(mode_right, GPIO_DIRECTION[dir_right]);
    }
    else
    {
      sprintf(mode_right, "ALT%d", alt_right);
    }
    align_string(mode_right, 6, 0);
    
    char value_right[2];
    strcpy(value_right, " ");
    if (!power_pin_right)
    {
      value_right[0] = 0x30 + get(chip_right, line_right);
    }
    
    char pin_right[4];
    sprintf(pin_right, "%d", right);
    align_string(pin_right, 3, -1);
    
    printf("|%s |%s |%s| %s |%s || %s| %s |%s| %s| %s|\n", gpio_left, name_left, mode_left, value_left, pin_left, pin_right, value_right, mode_right, name_right, gpio_right);
  }
  printf("+------+----------+------+---+----++----+---+------+----------+------+\n");
  printf("| GPIO |   Name   | Mode | V | Physical | V | Mode |   Name   | GPIO |\n");
  printf("+------+----------+------+---+----++----+---+------+----------+------+\n");
  
  int a0 = get_adc(0);
  float v0 = get_voltage_by_adc(a0);
  int a3 = get_adc(3);
  float v3 = get_voltage_by_adc(a3);
  int a4 = get_adc(4);
  float v4 = get_voltage_by_adc(a4);
  char buf[64];
  sprintf(buf, "A0 = %d (%.3fV),  A3 = %d (%.3fV),  A4 = %d (%.3fV)", a0, v0, a3, v3, a4, v4);
  printf("\n%s\n\n", buf);
}


void do_mode(int argc, char *const *argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s mode <pin> in/out\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (get_pin_name(argv[2]))
  {
    int dir;
    int ch = get_chip_number(pin_name);
    int ln = get_line_number(pin_name);
    //printf("ch=%d, ln=%d\n", ch, ln); // todo: verify ch and ln
    if (argc >= 4)
    {
      if (strcasecmp(argv[3], "in") == 0 || strcasecmp(argv[3], "input") == 0)
      {
        set_dir(ch, ln, GPIO_INPUT);
      }
      else if (strcasecmp(argv[3], "out") == 0 || strcasecmp(argv[3], "output") == 0)
      {
        set_dir(ch, ln, GPIO_OUTPUT);
      }
      else
      {
        fprintf(stderr, "Unknown mode: %s \n", argv[3]);
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      dir = get_dir(ch, ln);
      printf(dir == GPIO_INPUT ? "IN\n" : "OUT\n");
    }
  }
  else
  {
    exit(EXIT_FAILURE);
  }
}


void do_alt(int argc, char *const *argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s alt <pin> 0/1/2/3\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (get_pin_name(argv[2]))
  {
    int ch = get_chip_number(pin_name);
    int ln = get_line_number(pin_name);
    //printf("ch=%d, ln=%d\n", ch, ln); // todo: verify ch and ln
    if (argc >= 4)
    {
      int alt = atoi(argv[3]);
      if (alt >= 0 && alt <= 3)
      {
        set_alt(ch, ln, alt);
      }
      else
      {
        fprintf(stderr, "Unsupported ALT value: %s\n", argv[3]);
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      int alt = get_alt(ch, ln);
      printf("%d\n", alt);
    }
  }
  else
  {
    exit(EXIT_FAILURE);
  }
}


void do_get(int argc, char *const *argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s get <pin>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (get_pin_name(argv[2]))
  {
    int v = get(get_chip_number(pin_name), get_line_number(pin_name));
    printf("%d\n", v);
  }
  else
  {
    exit(EXIT_FAILURE);
  }
}


void do_set(int argc, char *const *argv)
{
  if (argc < 4)
  {
    fprintf(stderr, "Usage: %s set <pin> 1/0\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (get_pin_name(argv[2]))
  {
    int v = atoi(argv[3]);
    printf("Pin name is: %s, set its value to %d\n", pin_name, v);
    set(get_chip_number(pin_name), get_line_number(pin_name), v);
  }
  else
  {
    exit(EXIT_FAILURE);
  }
}


int get_io_pin(const char* p)
{
  for (int i = 1; i <= 40; i ++)
  {
    if (!is_power_pin(i))
    {
      if (strcasecmp(p, NAMES[i]) == 0)
      {
        return i;
      }
    }
  }
  int pin = atoi(p);
  if (pin > 0 && pin <= 40 && !is_power_pin(pin))
  {
    return pin;
  }
  return -1;
}


void on_pin_state_changed(void *p)
{
  pin_changed = true;
}


void do_wfi(int argc, char *const *argv)
{
  if (argc < 4)
  {
    fprintf(stderr, "Usage: %s wfi <pin> rising/falling/both\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  int pin = get_io_pin(argv[2]);
  if (pin != -1)
  {
    int wait_for = 0;
    if (strcasecmp(argv[3], "rising") == 0)
    {
      wait_for = GPIO_RISING_EDGE;
    }
    else if (strcasecmp(argv[3], "falling") == 0)
    {
      wait_for = GPIO_FALLING_EDGE;
    }
    else if (strcasecmp(argv[3], "both") == 0)
    {
      wait_for = GPIO_BOTH_EDGES;
    }
    else
    {
      fprintf(stderr, "Unknown edge: %s (should be rising/falling/both)\n", argv[3]);
      exit(EXIT_FAILURE);
    }
    create_monitor_thread(pin, 0, wait_for, on_pin_state_changed);
    while (!pin_changed)
    {
      usleep(200000);
    }
  }
  else
  {
    fprintf(stderr, "Incorrect pin: %s\n", argv[2]);
    exit(EXIT_FAILURE);
  }
}

void do_adc(int argc, char *const *argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s adc <analog-pin> [v/V]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  int p = atoi(argv[2]);
  if (p == 0 || p == 3 || p == 4)
  {
    int adc = get_adc(p);
    if (argc == 3)
    {
      printf("%d\n", adc);
    }
    else if (strcmp(argv[3], "v") == 0)
    {
      printf("%.3f\n", get_voltage_by_adc(adc));
    }
    else if (strcmp(argv[3], "V") == 0)
    {
      printf("%.3fV\n", get_voltage_by_adc(adc));
    }
    else
    {
      fprintf(stderr, "Incorrect option: %s (must be v or V)\n", argv[3]);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    fprintf(stderr, "Incorrect pin: %s (must be 0, 3 or 4)\n", argv[2]);
    exit(EXIT_FAILURE);
  }
}

void do_version(int argc, char *const *argv)
{
   printf("Vivid GPIO utility version: %.2f\n", VGP_VERSION);
}

// vgp command parameters...
// vpg help
// vgp list
// vgp all
// vgp mode 4C2
// vgp mode 4C2 in/out
// vgp alt 4C2
// vgp alt 4C2 0/1/2/3
// vgp get 4C2
// vgp set 4C2 0
// vgp wfi 4C2 rising/falling/both
// vgp adc 0/3/4 [v/V]

int main(int argc, char *const *argv)
{
  if (argc == 1)
  {
    fprintf(stderr, "Usage: %s <command> <pin> <parameter> ...\n", argv[0]);
    fprintf(stderr, "Run \"%s --help\" for more information.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (strcasecmp(argv[1], "all") == 0)
  {
    do_all();
  }
  else if (strcasecmp(argv[1], "list") == 0)
  {
    do_list();
  }
  else if (strcasecmp(argv[1], "mode") == 0)
  {
    do_mode(argc, argv);
  }
  else if (strcasecmp(argv[1], "alt") == 0)
  {
    do_alt(argc, argv);
  }
  else if (strcasecmp(argv[1], "get") == 0)
  {
    do_get(argc, argv);
  }
  else if (strcasecmp(argv[1], "set") == 0)
  {
    do_set(argc, argv);
  }
  else if (strcasecmp(argv[1], "wfi") == 0)
  {
    do_wfi(argc, argv);
  }
  else if (strcasecmp(argv[1], "adc") == 0)
  {
    do_adc(argc, argv);
  }
  else if (strcasecmp(argv[1], "-h") == 0 || strcasecmp(argv[1], "--help") == 0 || strcasecmp(argv[1], "help") == 0)
  {
    do_help(argc, argv);
  }
  else if (strcasecmp(argv[1], "-v") == 0 || strcasecmp(argv[1], "--version") == 0 || strcasecmp(argv[1], "version") == 0)
  {
    do_version(argc, argv);
  }
}