#include "konfig.h"
#include "device/lcd_ili9341.h"

// static Variablen

#ifdef KONFIG_USE_FONT
#include "device/font_22x40.c"
#endif

// Pinout Konfiguration

#define LCD_PORT  GPIOA
#define LCD_PORT_BIT GPIOA_BIT
#define LCD_SCK   GPIO_PIN1   // PA1
#define LCD_MOSI  GPIO_PIN3   // PA3
#define LCD_DC    GPIO_PIN2   // PA2
#define LCD_RESET GPIO_PIN5   // PA5
#define LCD_CS    GPIO_PIN7   // PA7

// Query Pinout

gpio_bit_t getportconfig_lcd(void)
{
   return GPIOA_BIT;
}

// Commands

#define LCD_CMD_SLEEP_OUT     0x11
#define LCD_CMD_DISPLAY_ON    0x29
#define LCD_CMD_SET_COL_ADDR  0x2a  // Parameter SC[15:8],SC[7:0],EC[15:8],EC[7:0]
#define LCD_CMD_SET_PAGE_ADDR 0x2b  // Parameter SP[15:8],SP[7:0],EP[15:8],EP[7:0]
#define LCD_CMD_MEMORY_WRITE  0x2C  // All Data-Bytes are seen as content of within coordinates
                                    // defined with LCD_CMD_SET_COL_ADDR and LCD_CMD_SET_PAGE_ADDR
#define LCD_CMD_MACCESSCTRL   0x36  // Memory Access Control; MY:1 MX:1 MV:1 ML:1 BGR:1 MH:1 X:1 X:1
                                    // MY=Flip Y, MX=Flip X, MV=Flip X<->Y

#define WRITE_BIT(NR) \
      bit = ((((uint32_t)data >> (NR)) & 1u) * LCD_MOSI); \
      write_gpio(LCD_PORT, (uint16_t) bit, (uint16_t) (bit^LCD_MOSI)); \
      write0_gpio(LCD_PORT, LCD_SCK); \
	   __asm("nop"); \
      write1_gpio(LCD_PORT, LCD_SCK); \
	   __asm("nop");

static void send_byte(uint8_t data)
{
   uint32_t bit;
   WRITE_BIT(7);
   WRITE_BIT(6);
   WRITE_BIT(5);
   WRITE_BIT(4);
   WRITE_BIT(3);
   WRITE_BIT(2);
   WRITE_BIT(1);
   WRITE_BIT(0);
}

static inline void start_transmission(void)
{
   write0_gpio(LCD_PORT, LCD_CS);
}

static inline void end_transmission(void)
{
   write1_gpio(LCD_PORT, LCD_CS);
}

static inline void select_data(void)
{
   write1_gpio(LCD_PORT, LCD_DC);
}

static inline void select_command(void)
{
   write0_gpio(LCD_PORT, LCD_DC);
}

void sendcmd_lcd(uint8_t data)
{
   select_command();
   start_transmission();
   send_byte(data);
   end_transmission();
}

void senddata_lcd(uint8_t data)
{
   select_data();
   start_transmission();
   send_byte(data);
   end_transmission();
}

void sendpixels_lcd(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
   sendcmd_lcd(LCD_CMD_SET_COL_ADDR);
   senddata_lcd(x1>>8);
   senddata_lcd(x1);
   senddata_lcd(x2>>8);
   senddata_lcd(x2);

   sendcmd_lcd(LCD_CMD_SET_PAGE_ADDR);
   senddata_lcd(y1>>8);
   senddata_lcd(y1);
   senddata_lcd(y2>>8);
   senddata_lcd(y2);

   sendcmd_lcd(LCD_CMD_MEMORY_WRITE);
   select_data();
}

static inline void setdefaultlayout_lcd(void)
{
   sendcmd_lcd(LCD_CMD_MACCESSCTRL);
   senddata_lcd(0x88);  // Flip Y (0x80) + BGR (0x8)
}

void init_lcd(void)
{
   const uint32_t HZ = getHZ_clockcntrl();

   config_output_gpio(LCD_PORT, LCD_SCK|LCD_MOSI|LCD_DC|LCD_RESET|LCD_CS);
   write1_gpio(LCD_PORT, LCD_SCK|LCD_MOSI|LCD_DC|LCD_RESET|LCD_CS);

   config_systick(HZ/(1000000/10)/*10µs*/, systickcfg_CORECLOCK);
   write0_gpio(LCD_PORT, LCD_RESET);
	start_systick();
   while (!isexpired_systick()) ;
	stop_systick();
   write1_gpio(LCD_PORT, LCD_RESET);

   config_systick(HZ/(1000/20)/*20ms*/, systickcfg_CORECLOCK);
	start_systick();
   while (!isexpired_systick()) ;
	stop_systick();

   setdefaultlayout_lcd();

   sendcmd_lcd(0x3A);   // Pixel Format Set
   senddata_lcd(0x55);  // X DPI[2:0] X DBI[2:0]   // 16Bit(0x5)

   sendcmd_lcd(0xB1);   // Frame Rate Control (in Normal Mode)
   senddata_lcd(0x00);  // X X X X X X DIVA[1:0]
   senddata_lcd(0x13);  // X X X RTNA[4:0]    79Hz(0x18) 100Hz(0x13)

   sendcmd_lcd(LCD_CMD_SLEEP_OUT);
   config_systick(HZ/(1000/60)/*60ms*/, systickcfg_CORECLOCK);
	start_systick();
   while (!isexpired_systick()) ;
	stop_systick();

   sendcmd_lcd(LCD_CMD_DISPLAY_ON);
}

void fillrect_lcd(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
   sendpixels_lcd(x1, y1, x2, y2);
   start_transmission();
   for (uint32_t nrpixels = (x2-x1+1) * (y2-y1+1); nrpixels > 0; --nrpixels) {
      send_byte((uint8_t)(color>>8));
      send_byte((uint8_t)color);
   }
   end_transmission();
}

void fillscreen_lcd(uint16_t color)
{
   fillrect_lcd(0, 0, 240-1, 320-1, color);
}

uint16_t color16_lcd(uint8_t r/*0..255*/, uint8_t g/*0..255*/, uint8_t b/*0..255*/)
{
  return ((r & 0xf8/*5Bit*/) << 8) | ((g & 0xfc/*6Bit*/) << 3) | (b >> 3/*5Bit*/);
}

void scrolly_lcd(uint16_t yoffset/*modulo 320*/)
{
   yoffset %= 320;
   sendcmd_lcd(0x33);
   senddata_lcd(0);
   senddata_lcd(0);
   senddata_lcd((uint8_t)(320>>8));
   senddata_lcd((uint8_t)320);
   senddata_lcd(0);
   senddata_lcd(0);
   sendcmd_lcd(0x37);
   senddata_lcd((uint8_t)(yoffset>>8));
   senddata_lcd((uint8_t)yoffset);
}

uint8_t fontwidth_lcd()
{
#ifdef KONFIG_USE_FONT
   return s_font_width;
#else
   return 0;
#endif
}

uint8_t fontheight_lcd()
{
#ifdef KONFIG_USE_FONT
   return s_font_height;
#else
   return 0;
#endif
}


void drawascii_lcd(uint16_t x, uint16_t y, char ascii, uint8_t scale, uint8_t rotate/*0..3*/)
{
#ifdef KONFIG_USE_FONT
   ascii -= 32;
   if ((unsigned char)ascii >= 95) {
      ascii = (char) ('?' - 32);
   }

   if (scale == 0) {
      scale = 1;
   } else if (scale > 16) {
      scale = 16;
   }

   if (rotate) {
      sendcmd_lcd(LCD_CMD_MACCESSCTRL);
      if (rotate == 1) {
         senddata_lcd(0xe8);  // Flip Y (0x80) + Flip X (0x40) + Swap X<->Y (0x20) + BGR (0x8)
      } else if (rotate == 2) {
         senddata_lcd(0x48);  // Flip X (0x40) + BGR (0x8)
      } else {
         senddata_lcd(0x28);  // Swap X<->Y (0x20) + BGR (0x8)
      }
   }

   const uint32_t *glyph = s_font_glyph + s_font_height * ascii;
   sendpixels_lcd(x, y, x + scale*s_font_width-1, y + scale*s_font_height-1);
   start_transmission();
   for (uint32_t y = s_font_height; y; --y) {
      const uint32_t bits = *glyph++;
      for (uint32_t sy = scale; sy; --sy) {
         for (uint32_t b = bits, x = s_font_width; x; --x, b >>= 1) {
            uint8_t color = (uint8_t) (0 - (b & 1));
            for (uint32_t sx = scale; sx; --sx) {
               send_byte(color);
               send_byte(color);
            }
         }
      }
   }
   end_transmission();

   if (rotate) {
      setdefaultlayout_lcd();
   }

#else
   (void) x;
   (void) y;
   (void) ascii;
   (void) scale;
   (void) rotate;
#endif
}
