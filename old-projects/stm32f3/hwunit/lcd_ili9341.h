
/* Konfiguriert die Pins des verwendeten Port (siehe LCD_PORT in Implementierungsfile)
 * als Ausgänge und initialisiert das LCD Display. Oben ist dort wo die Steckkontakte sind.
 * Das Display hat eine Auflösung von 320 vertikal und 240 Pixel horizontal.
 * Der Systick-Timer wird zur Initialisierung verwendet, so dass er nach Aufruf diese Funktion
 * neu initialisiert werden muss. */
void init_lcd(void);

/* Gibt die Nummer des verwendeten Ports zurück. Dieser Port muss im vom Hautpprogramm aus
 * mittels enable_gpio_clockcntrl aktiviert werden. */
gpio_bit_t getportconfig_lcd(void);

/* Füllt den ein Rechteckbereich des Bildschirms (maximal 240(x)*320(y)) mit der Farbe color.
 * Der Füllbereich geht von horizontal von x1 bis x2 und vertikal von y1 bis y2 inklusive.
 * Der Farbwert color kann mit color15_lcd berechnet werden. */
void fillrect_lcd(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* Füllt den gesamten 240(x)*320(y) Bildschirm mit der Farbe color.
 * Der Farbwert color kann mit color15_lcd berechnet werden. */
void fillscreen_lcd(uint16_t color);

/* Rechnet die Farbkodierung eines Pixels um von 24-Bit RGB in 16-Bit RGB (5-6-5). */
uint16_t color16_lcd(uint8_t r/*0..255*/, uint8_t g/*0..255*/, uint8_t b/*0..255*/);

/* Verschiebt die Darstellung des Displays um yoffset Zeilen. Ein Vielfaches von 320 hat denselben
 * Effekt wie der Wert 0 – nämlich keinen. */
void scrolly_lcd(uint16_t yoffset/*modulo 320*/);

uint8_t fontwidth_lcd();
uint8_t fontheight_lcd();

/*
 * (x,y) == (left,top)
 *
 * Preconditions:
 * (rotate == 0 || rotate == 2) ==> (x <= 240-fontwidth_lcd() && y <= 320-fontheight_lcd())
 * (rotate == 1 || rotate == 3) ==> (x <= 320-fontwidth_lcd() && y <= 240-fontheight_lcd())
 * (32 <= ascii && ascii <= 126)
 */
void drawascii_lcd(uint16_t x, uint16_t y, char ascii, uint8_t scale/*0..16*/, uint8_t rotate/*0..3*/);
