#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>
#include "vgplib.h"


#define COMMAND_BUFFER_SIZE 512
#define OUTPUT_BUFFER_SIZE  1024

char command_buffer[COMMAND_BUFFER_SIZE];
char output_buffer[OUTPUT_BUFFER_SIZE];

char chip_name[] = "/dev/gpiochipn";

int run_command(const char * cmd)
{
  FILE *fp;
  if ((fp = popen(cmd, "r")) == NULL)
  {
      return -1;
  }
  int length = 0;
  if (fgets(output_buffer, OUTPUT_BUFFER_SIZE, fp) != NULL)
  {
      length = strlen(output_buffer);
  }
  if (pclose(fp))
  {
      return -2;
  }
  return length;
}


int get_register(unsigned int address)
{
    sprintf(command_buffer, "sudo io -4 -r 0x%x", address);
    int length = run_command(command_buffer);
    if (length < 12)
    {
      printf("get_register returned an error\n");
      return -1;
    }
    strcpy(output_buffer, &output_buffer[11]);
    return strtol(output_buffer, NULL, 16);
}


int set_register(unsigned int address, unsigned int value)
{
    sprintf(command_buffer, "sudo io -4 -w 0x%x 0x%x", address, value);
    int length = run_command(command_buffer);
    if (length < 0)
    {
      printf("set_register returned an error\n");
    }
    return length;
}


int get_chip_number(char *pin_name)
{
  return pin_name[0] - 0x30;
}


int get_line_number(char *pin_name)
{
  return ((pin_name[1] - 0x41) << 3) + (pin_name[2] - 0x30);
}


int get_dir(int ch, int ln)
{
  int group = ln / 8;
  int index = ln % 8;
  int gpio_directions = get_register(GPIO_BASE[ch] + GPIO_SWPORTA_DDR);
  int directions = ((gpio_directions >> (group << 3)) & 0xff);
  return ((directions & (0x01 << index)) >> index);
}


int set_dir(int ch, int ln, int dir)
{
  int group = ln / 8;
  int index = ln % 8;
  int gpio_directions = get_register(GPIO_BASE[ch] + GPIO_SWPORTA_DDR);
  int directions = ((gpio_directions >> (group << 3)) & 0xff);
  if (dir == GPIO_INPUT)
  {
    directions &= ~(0x01 << index);
  }
  else if (dir == GPIO_OUTPUT)
  {
    directions |= (0x01 << index);
  }
  else
  {
    fprintf(stderr, "Unknown direction %d\n", dir);
    return -3;
  }
  gpio_directions &= ~(0xFF << (group << 3));
  gpio_directions |= (directions << (group << 3));
  set_register(GPIO_BASE[ch] + GPIO_SWPORTA_DDR, gpio_directions);
  return 0;
}


int get_alt(int ch, int ln)
{
  int group = ln / 8;
  int index = ln % 8;
  int iomux = (GPIO_IOMUX[ch][group] == -1) ? -1 : get_register((ch < 2 ? PMUGRF : GRF) + GPIO_IOMUX[ch][group]);
  if (iomux != -1)
  {
    return ((iomux >> (index << 1)) & 0x03);
  }
  return -1;
}


int set_alt(int ch, int ln, int alt)
{
  int group = ln / 8;
  int index = ln % 8;
  if (alt < 0 || alt > 3)
  {
    fprintf(stderr, "Unsupported ALT value %d\n", alt);
    return -2;
  }
  int iomux = (GPIO_IOMUX[ch][group] == -1) ? -1 : get_register((ch < 2 ? PMUGRF : GRF) + GPIO_IOMUX[ch][group]);
  if (iomux != -1)
  {
    iomux &= ~(0x03 << (index << 1));
    iomux |= (alt << (index << 1));
    iomux |= (0x03 << ((index << 1) + 16));
    set_register((ch < 2 ? PMUGRF : GRF) + GPIO_IOMUX[ch][group], iomux);
    return 0;
  }
  return -1;
}


int get(int ch, int ln)
{
  struct gpiod_chip *chip;
	struct gpiod_line *line;
	struct gpiod_line_request_config cfg;
	int req, value, ret = 0;
  chip_name[13] = ch + 0x30;
	chip = gpiod_chip_open(chip_name);
	if (chip)
	{
	  line = gpiod_chip_get_line(chip, ln);
  	if (line)
  	{
  	  memset(&cfg, 0, sizeof(cfg));
    	cfg.consumer = "vgp";
    	cfg.request_type = GPIOD_LINE_REQUEST_DIRECTION_AS_IS;
    	cfg.flags = 0;
    	req = gpiod_line_request(line, &cfg, 0);
    	if (req >= 0)
    	{
    	  value = gpiod_line_get_value(line);
    	}
    	else
    	{
    		ret = -3;
    	}
  	}
  	else
  	{
  		ret = -2;
  	}
	}
	else
	{
		ret = -1;
  }
  gpiod_line_release(line);
  gpiod_chip_close(chip);
  return ret == 0 ? value : ret;
}


int set(int ch, int ln, int val)
{
  struct gpiod_chip *chip;
	struct gpiod_line *line;
	int req, ret = 0;
  chip_name[13] = ch + 0x30;
	chip = gpiod_chip_open(chip_name);
	if (chip)
	{
	  line = gpiod_chip_get_line(chip, ln);
  	if (line)
  	{
  	  req = gpiod_line_request_output(line, "vgp", 0);
    	if (req >= 0)
    	{
        gpiod_line_set_value(line, val);
    	}
    	else
    	{
    		ret -3;
    	}
  	}
  	else
  	{
  		ret -2;
  	}
	}
	else
	{
		ret -1;
  }
  gpiod_line_release(line);
  gpiod_chip_close(chip);
  return ret;
}


int get_adc(int a_pin)
{
  sprintf(command_buffer, "cat /sys/bus/iio/devices/iio:device0/in_voltage%d_raw", a_pin);
  int length = run_command(command_buffer);
  if (length < 0)
  {
    printf("get_adc returned an error\n");
    return -1;
  }
  return atoi(output_buffer);
}


float get_voltage_by_adc(int adc)
{
  return 5.0f * (float)adc / 1024;
}


bool is_power_pin(int pin)
{
  if (pin == 1 || pin == 2 || pin == 4 || pin == 6 || pin == 9 || pin == 14 
    || pin == 17 || pin == 20 || pin == 25 || pin == 30 || pin == 34 || pin == 39) {
    return true;
  }
  return false;  
}


MonitorThread * monitor_threads[MONITOR_THREADS];


void thread_cleanup_handler(void *p)
{
  MonitorThread * params = (MonitorThread *)p;
  gpiod_line_release(params->line);
  gpiod_chip_close(params->chip);
}


void * monitor_pin(void *p)
{
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  
  MonitorThread * params = (MonitorThread *)p;
  
  sleep(params->delay);
  
  char * pin_name = (char *)NAMES[params->pin];
  int ch = get_chip_number(pin_name);
  int ln = get_line_number(pin_name);
  
  char chipname[15];
  sprintf(chipname, "/dev/gpiochip%d", ch);
  params->chip = gpiod_chip_open(chipname);
  if (!params->chip)
  {
    perror("Error opening GPIO chip");
    return NULL;
  }
  
  params->line = gpiod_chip_get_line(params->chip, ln);
  if (!params->line)
  {
    perror("Error getting GPIO line");
    gpiod_chip_close(params->chip);
    return NULL;
  }

  pthread_cleanup_push(thread_cleanup_handler, p);

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  
  // request event
  int ret;
  switch (params->wait_for)
  {
    case GPIO_RISING_EDGE: 
      ret = gpiod_line_request_rising_edge_events(params->line, "vgplib");
      break;
    case GPIO_FALLING_EDGE:
      ret = gpiod_line_request_falling_edge_events(params->line, "vgplib");
      break;
    case GPIO_BOTH_EDGES:
      ret = gpiod_line_request_both_edges_events(params->line, "vgplib");
      break;
    default:
      ret = -1;
  }
  if (ret < 0)
  {
    perror("Error requesting GPIO line events");
    gpiod_line_release(params->line);
    gpiod_chip_close(params->chip);
    return NULL;
  }
  
  // wait for event
  while (1)
  {
    pthread_testcancel();
    
    struct gpiod_line_event event;
    ret = gpiod_line_event_wait(params->line, NULL);
    if (ret < 0) {
        perror("Error waiting for GPIO event");
        gpiod_line_release(params->line);
        gpiod_chip_close(params->chip);
        return NULL;
    }
    ret = gpiod_line_event_read(params->line, &event);
    if (ret < 0) {
        perror("Error reading GPIO event");
        gpiod_line_release(params->line);
        gpiod_chip_close(params->chip);
        return NULL;
    }
    params->latest_event = (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE ? GPIO_RISING_EDGE : GPIO_FALLING_EDGE);
    params->callback(p);
  }
    
  // release GPIO resources
  gpiod_line_release(params->line);
  gpiod_chip_close(params->chip);
  
  pthread_cleanup_pop(0);
  
  return NULL;
}


void create_monitor_thread(int pin, int delay, int wait_for, void (*callback)(void*))
{
  monitor_threads[pin] = (MonitorThread *)malloc(sizeof(MonitorThread));
  monitor_threads[pin]->pin = pin;
  monitor_threads[pin]->delay = delay;
  monitor_threads[pin]->wait_for = wait_for;
  monitor_threads[pin]->callback = callback;
  int err = pthread_create(&(monitor_threads[pin]->thread), NULL, monitor_pin, (void*)monitor_threads[pin]);
  if (err != 0)
  {
    printf("Can't create monitor thread :[%s]", strerror(err));
  }
}
