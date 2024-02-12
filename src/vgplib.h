#define VGP_VERSION 1.01f

#define GPIO_SWPORTA_DR   0x0000
#define GPIO_SWPORTA_DDR  0x0004
#define GPIO_EXT_PORTA    0x0050

#define PMUGRF  0xff320000
#define GRF     0xff770000

#define GPIO_INPUT    0
#define GPIO_OUTPUT   1


static const int GPIO_BASE[] = { 0xff720000, 0xff730000, 0xff780000, 0xff788000, 0xff790000 };

static const int GPIO_IOMUX[5][4] = {
    {0x00000, 0x00004, -1, -1},           // PMUGRF_GPIO0*
    {0x00010, 0x00014, 0x00018, 0x0001c}, // PMUGRF_GPIO1*
    {0x0e000, 0x0e004, 0x0e008, 0x0e00c}, // GRF_GPIO2*
    {0x0e010, 0x0e014, 0x0e018, 0x0e01c}, // GRF_GPIO3*
    {0x0e020, 0x0e024, 0x0e028, 0x0e02c}, // GRF_GPIO4*
};

static const char GPIO_GROUP[] = { 'A', 'B', 'C', 'D' };

static const char * GPIO_DIRECTION[] = { "IN", "OUT" };


int get_register(unsigned int address);

int set_register(unsigned int address, unsigned int value);

int get_chip_number(char *pin_name);

int get_line_number(char *pin_name);

int get_dir(int ch, int ln);

int set_dir(int ch, int ln, int dir);

int get_alt(int ch, int ln);

int set_alt(int ch, int ln, int alt);

int get(int ch, int ln);

int set(int ch, int ln, int val);

int get_adc(int a_pin);

float get_voltage_by_adc(int adc);


// 40-pin GPIO header
static const char *NAMES[] = { NULL,
  "3.3V", "5V", 
  "2A0",  "5V",
  "2A1",  "GND",
  "4D1",  "4C4",
  "GND",  "4C3",
  "4D6",  "4D2",
  "2D3",  "GND",
  "2A4",  "2A6",
  "3.3V", "2A3",
  "2B2",  "GND",
  "2B1",  "2A2",
  "2B3",  "2B4",
  "GND",  "2A5",
  "2A7",  "2B0",
  "1A4",  "GND",
  "1A2",  "1A1",
  "4B3",  "GND",
  "4B5",  "4B4",
  "4B0",  "4B1",
  "GND",  "4B2"
};


static const char * FUNCTIONS[][4] = { {NULL, NULL, NULL, NULL},
  /*3.3V*/{"","", "", ""},                            /*5V*/  {"", "", "", ""},
  /*2A0*/ {"I/O", "VOP_D0", "I2C2_SDA", "CIF_D0"},    /*5V*/  {"", "", "", ""},
  /*2A1*/ {"I/O", "VOP_D1", "I2C2_SCL", "CIF_D1"},    /*GND*/ {"", "", "", ""},
  /*4D1*/ {"I/O", "DP_HP", "", ""},                   /*4C4*/ {"I/O", "TXD", "HDCP_TX", ""},
  /*GND*/ {"", "", "", ""},                           /*4C3*/ {"I/O", "RXD", "HDCP_RX", ""},
  /*4D6*/ {"I/O", "", "", ""},                        /*4D2*/ {"I/O", "", "", ""},
  /*2D3*/ {"I/O", "SD_PWREN", "", ""},                /*GND*/ {"", "", "", ""},
  /*2A4*/ {"I/O", "VOP_D4", "JTAG_TDO", "CIF_D4"},    /*2A6*/ {"I/O", "VOP_D6", "JTAG_TMS", "CIF_D6"},
  /*3.3V*/{"", "", "", ""},                           /*2A3*/ {"I/O", "VOP_D3", "JTAG_TDI", "CIF_D3"},
  /*2B2*/ {"I/O", "MOSI", "I2C6_SCL", "CIF_CLKI"},    /*GND*/ {"", "", "", ""},
  /*2B1*/ {"I/O", "MISO", "I2C6_SDA", "CIF_HREF"},    /*2A2*/ {"I/O", "VOP_D2", "JTAG_TRS", "CIF_D2"},
  /*2B3*/ {"I/O", "CLK", "VOP_DEN", "CIF_CLKO"},      /*2B4*/ {"I/O", "CS", "", ""},
  /*GND*/ {"", "", "", ""},                           /*2A5*/ {"I/O", "VOP_D5", "JTAG_TCK", "CIF_D5"},
  /*2A7*/ {"I/O", "VOP_D7", "I2C7_SDA", "CIF_D7"},    /*2B0*/ {"I/O", "VOP_DCLK", "I2C7_SCL", "CIF_VSYN"},
  /*1A4*/ {"I/O", "ISP0_PLT", "ISP1_PLT", ""},        /*GND*/ {"", "", "", ""},
  /*1A2*/ {"I/O", "ISP0_FTI", "ISP1_FTI", ""},        /*1A1*/ {"I/O", "ISP0_ST", "ISP1_ST", "TCPD_CC"},
  /*4B3*/ {"I/O", "SDMMC_D3", "CJTAGTMS", "HJTAGTDO"},/*GND*/ {"", "", "", ""},
  /*4B5*/ {"I/O", "SDMMCCMD", "MJTAGTMS", "HJTAGTMS"},/*4B4*/ {"I/O", "SDMMCCLK", "MJTAGTCK", "HJTAGTCK"},
  /*4B0*/ {"I/O", "SDMMC_D0", "TXD2", ""},            /*4B1*/ {"I/O", "SDMMC_D1", "RXD2", "HJTAGTRS"},
  /*GND*/ {"", "", "", ""},                           /*4B2*/ {"I/O", "SDMMC_D2", "CJTAGTCK", "HJTAGTDI"}
};


bool is_power_pin(int pin);


// GPIO pin state monitor thread

#define GPIO_RISING_EDGE   1
#define GPIO_FALLING_EDGE  2
#define GPIO_BOTH_EDGES    3

#define MONITOR_THREADS    41

typedef struct {
  int pin;
  pthread_t thread;
  int delay;
  int wait_for;
  int latest_event;
  void (*callback)(void*);
  struct gpiod_chip * chip;
  struct gpiod_line * line;
} MonitorThread;

extern MonitorThread * monitor_threads[MONITOR_THREADS];

void thread_cleanup_handler(void *p);

void * monitor_pin(void *p);

void create_monitor_thread(int pin, int delay, int wait_for, void (*callback)(void*));

void init_monitor_threads(void (*callback)(void*));
