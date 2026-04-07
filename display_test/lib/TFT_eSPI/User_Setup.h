#define USER_SETUP_INFO "XIAO ESP32-S3 Round Display"

#define GC9A01_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_MOSI 9
#define TFT_SCLK 7
#define TFT_CS   2
#define TFT_DC   4
#define TFT_RST  -1
#define TFT_BL   6

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  20000000

#define USE_FSPI_PORT
