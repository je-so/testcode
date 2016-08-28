/* title: Analog-to-Digital-Converter

   Gibt Zugriff auf

      o Analg-Digital-Wandler zugeordnet zu verschiednen Pins über eine Kanal-Nummer

   Jeder ADC ist ein 12-Bit schrittweise(skuzessiv) annähernder(approximierender) Analog zu Digital Konverter.
   Jeder ADC unterstützt 18 verschieden fest verdrahtete Kanäle.
   Der Wandler läuft mit mit der Taktfrequenz von Bus AHB (HCLK) oder mit PLLCLK.
   ADC 1 und 2 als auch ADC 3 und 4 können zu einem Dual-Wandler (Master-Slave-Konfiguration) zusammengeschalten werden.
   Die ADC können mit 7 internen Känalen (Temperatur, VBAT/2, OPAMP1-4 reference voltage output, VREFINT) verbudnen werden.

   Zuordnung der Kanale jedes ADC zu den externen Pins:

  /Pinout/ Kanäle:
   ADCx |  1    2    3    4    5    6   7    8    9    10   11   12  13  14   15   16   17   18
   -----|------------------------------------------------------------------------------------------
   ADC1 |  PA0  PA1  PA2  PA3  PF4  PC0 PC1  PC2  PC3  PF2                    Vop1 Vts  Vbat Vref
   ADC2 |  PA4  PA5  PA6  PA7  PC4  PC0 PC1  PC2  PC3  PF2  PC5  PB2                    Vop2 Vref
   ADC3 |  PB1  PE9  PE13 0V   PB13 PE8 PD10 PD11 PD12 PD13 PD14 PB0 PE7 PE10 PE11 PE12 Vop3 Vref
   ADC4 |  PE14 PE15 PB12 PB14 PB15 PE8 PD10 PD11 PD12 PD13 PD14 PD8 PD9                Vop4 Vref

   Wandlermodus:
   Der Wandler unterstützt zwei Modi pro Kanal.

   - Der Einfach- bzw. Einpinmodus (adc_channelmode_SINGLEMODE),
     wo die Spannung eines Kanal-Pins (siehe Pinout Tabelle) gegen Masse gemessen wird.
     Eigentlich wird gegen Vref- gemessen, dieser Pin ist auf dem Board aber gegen Masse gelegt.
     Bei einer 3.3V Spannung am Pin wird ein Wert von ca. 4095 gemessen.
     Bei einer 0V Spannung am Pin wird ein Wert von 0 gemessen.

   - Im Differenzmodus (adc_channelmode_DIFFMODE) wird die Spannung des dem chan Kanal zugeordneten Pins
     gegen die Spannung des Pins des Kanals chan+1 gemessen. Wird Kanal-Pin chan auf die Spannung 0V
     gesetzt und 3.3V an Kanal-Pin chan+1 angelegt, dann wird der Wert 0 gemessen.
     Liegt die Spannung 3.3V an Kanal-Pin chan und 0V an Kanal-Pin chan+1 an, wird ein Wert von
     ca. 4095 gemessen. Wird an beiden Pins dieselbe Spannung angelegt, ergibt sich in etwa
     der Wert 2046. Beim Testen zeigte der Wandler des Boards aber Probleme 0V vs. 0V korrekt als
     2046 zu ermitteln und ermittelte anstatt dessen den Wert 2536. Wurde die Spannung
     erhöht auf etwa 0.47V (0.47V vs. 0.47V), dann zeigte sich der korrekte Wert von 2046.
     Innerhalb des Spannungbereiches 0.47V an Kanal-Pin chan und 0V an Kanal-Pin chan+1 wurde
     ein nicht-monotones Verhalten des Wandlers festgestellt.

   == Vorsicht ==:
   o Nur ein ADC darf gleichzeitig Vref (Kanal 18) konvertieren.
   o Zusätzlich müssen die Kanäle Vts, Vbat, Vref per Bit im adc_dual_t->CCR Register eingeschaltet werden (nicht implem.).
   o Wenn Kanal "i" im Differenzmodus arbeitet, ist die zweite negative Vergleichseingabe V-
     verbunden mit dem Pin von Kanal "i+1", so dass dieser Kanal unter keinen Umständen verwendet werden darf.

   Spannungsregulator:
   Vor dem Einschalten des ADC muss der ADC Spannungsregulator (voltage regulator)
   mit einer bestimmten Sequenz aktiviert werden. Danach muss einmalig T(ADCVREG_STUP)
   gewartet werden, im schlimmsten Fall sind dies 10 μs, bevor eine automatische
   Eichung (calibration) durchgeführt werden kann oder der ADC eingeschaltet wird.

   Kanal-Sequenzen:
   Der ADC wandelt lest den Spannungswert nicht nur eines einzelnen Kanal, sondern kann bis zu
   16 Kanäle nacheinander bearbeiten. Die Kanalreihenfolge wird als Sequenz bezeichnet.
   Der Konverter unterstützt zwei Sequenzen. Die reguläre mit bis zu 16 Kanaälen Länge und die
   J-Sequenz mit bis zu 4. Die J-Sequenz hat die höhere Priorität und wird bei gleichzeitigem
   Eintreffen der Trigger für beide Sequenzen zuerst bearbeitet. Sie kann sogar die Abarbeitung
   der regulären Sequenz unterbrechen, die ihre Arbeit erst nach Beendigung der J-Sequenz wieder
   aufnimmt. Pro Kanal benötigt der AD-Wandler minimal 14 Bustakte.

   Master-&-Slave:
   Je zwei ADC sind zu einer Master und einem Slave verbunden. ADC1(master) und ADC2(slave)
   sind verbunden sowie ACD3(master) und ADC4(slave) sind verbunden.
   Eine Master&Slave Kombination können so zu einem 2-Kanal-ADC verbunden werden, so dass
   zwei 2 Pins synchron abgetastet werden können.

   Dieser Modus ist noch nicht implementiert!

   Trigger:
   Wurde der ADC mit einer der config_YYY_adc Funktionen eingeschaltet, so ist er bereit,
   liest und wandelt aber noch keine analoge Spannungen in digitale Werte. Dazu ist die
   Funktion start_adc aufzurufen, die den eigentlichen Wandlungsvorgang startet.

   Im Falle eines konfiguriertn Software-Triggers (ist momentan immer der Fall, da
   HW-Trigger noch nicht implementiert sind), startet die Konvertierung sofort mit dem
   Aufruf der start_adc Funktion. start_adc ist sozusagen der Software-Trigger, der
   die Wandlung startet. Bei Erreichen des Endes einer bis zu 16 Kanäle langen Sequenz
   wird das Startflag automatisch wieder zurückgesetzt und der ADC startet erst wieder
   mit dem erneuten Eintreffen eines Sw-Triggers bzw. Aufrufs der Funktion start_adc.

   Im Falle eines konfigurierten Hardware-Triggers, der 15 verschiedenen Timer-Events
   zugeordneten werden kann aber auch einem Flankenevent auf einem externen fest
   verdrahteten I/O-Pins, startet die Konvertierung der Kanäle nicht sofort nach dem Aufruf
   der Funktion start_adc. Diese setzt nur das Startflag und der AD-Konverter wartet auf
   das Eintreffen des konfigurierten HW-Events. Mit Eintreffen dieses Events startet die
   Wandlung. Bei Erreichen des Endes einer bis zu 16 Kanäle langen Sequenz wird das
   Startflag nicht zurückgesetzt, jedoch stoppt der AD-Konverter seine Aktivitäten und
   wartet auf ein neues HW-Event. Tritt während des Wandelns einer Sequenz ein erneutes
   HW-Event ein, so wird dieses ignoriert. Mit Aufruf der Funktion stop_adc wird das
   start_flag zurückgesetzt und weitere eintreffende HW-Events werden beständig ignoriert.

   Reguläre Sequenz:
   Der HW- oder Software-Trigger startet eine Messung von einer regulären Sequenz,
   die aus bis zu 16 verschiedenen Kanälen, die nacheinander gewandelt werden.
   Die regülare Sequenz kann um 4 erweitert, wenn die einfügbare (injected, abgekürzt 'J')
   Sequenz mittels (JAUTO=1) an die reguläre angefügt wird. Wird nach dem Konfigurieren
   der regulären Sequenz mit config_seq_adc die J-Sequenz mit config_autojseq_adc
   konfiguriert, startet diese automatisch am Ende der regulären Sequenz.

   Kontinuierliche Modus der regulären Sequenz:
   Nach dem Starten des ADC und im Fall eines HW-Trigger, nach dem Eintreffen eines HW-Events,
   beginnt die Wandlung der in der Sequenz definierten Kanäle der Reihe nach. Am Ende der Sequenz
   wird das EOS-Flag gesetzt und anstatt auf das Eintreffen des nächsten HW- oder SW-Events zu warten,
   wird die Wandlung wieder mit dem Beginn der Sequenz erneut gestartet. Und zwar solange, bis stop_adc
   aufgerufen wird. Dieser Modus wird mit config_contseq_adc konfiguriert.
   Wird nach dem Konfigurieren der regulären Sequenz die J-Sequenz mit config_autojseq_adc
   konfiguriert, startet diese automatisch am Ende der regulären Sequenz. Und zwar genauso kontinuierlich
   wie die reguläre Sequenz.

   Einfügbare Sequenz:
   Die einfügbare Sequenz (injected, abgekürzt 'J') besteht aus bis zu vier wählbaren Kanälen,
   die nacheinander gewandelt werden. Diese Sequenz hat aber zusätzlich die Eigenschaft,
   die reguläre Sequenz zu unterbrechen, wenn der Start-Impuls (HW oder Software) getriggert wird.
   Am Ende der J-Sequenz wird die reguläre Sequenz wieder aufgenommen, wenn diese unerbrochen wurde.
   Die J-Sequenz wird mittels config_jseq_adc konfiguriert. Der Startimpuls wird mittels
   startj_adc gegeben. Ist der Software-Trigger aktiviert (default, HW-Trigger nicht implementiert),
   startet das Wandeln sofort, im Falle eines Hw-Triggers wird auf das Eintreffen des konfigurierten
   HW-Events gewartet. Am Ende der Wandlung wird das J-Startflag (abfragbar mittels isjstarted_adc)
   im Falle eines SW-Triggers zurückgesetzt oder anbelassen im Falle eins HW-Triggers.

   Die Ergebnisse der Wandlung einer J-Sequenz werden in bis zu vier (je nach Länge der Sequenz)
   Ergebnisregister abgelegt. Alle Ergebnisse können so am Ende einer Sequenz gelesen werden.
   Dies steht im Gegensatz zur regulären Sequenz, die nur ein Ergebnisregister kennt und jeder Kanal
   nach der Wadnlung sofort ausgelesen werden muss.

   Autoeingefügte Sequenz:
   Soll die J-Sequenz automatisch gestartet werden, muss sie mit config_autojseq_adc nach der
   der regulären Sequenz konfiguriert werden. Danach wird mit dem Starten der regulären Sequenz
   (start_adc) auch die J-Sequenz gestartet. Die eigentliche Wandlung der J-Sequenz startet jedesmal,
   nachdem der letzte Kanal der regulären Sequenz gewandelt wurde automatisch. Ist die J-Sequenz zu Ende
   und die reguläre Sequenz kontinuierlich konfiguriert, wiederholt sich dieser Vorgang erneut.
   Die Funktionen startj_adc und stopj_adc dürfen nun nicht mehr aufgerufen werden, sondern das Starten
   und Stoppen ist immer mit start_adc und stop_adc über die reguläre Sequenz einzuleiten.

   Der Fehlerwert EAGAIN wird von config_autojseq_adc zurückgeliefert, falls die reguläre Sequenz
   in Partitionen aufgeteilt wurde (siehe nächsten Abschnitt) und nicht als eine einzige Sequenz
   abgearbeitet wird. Autoeingefügte J-Sequenzen sind nicht mit dem Partionsmodus vereinbar.

   Sequenzen in Partitionen zerlegen:
   Beide Sequenzen können in zusätzlich in Partitionen zerlegt werden. Jede einzelne Partition
   benötigt genau einen HW-oder SW-Trigger zum Starten. Der AD-Wandler verhält sich so, dass eine
   Sequenz in X-Sequenzen mit derselben Partitionslänge aufgeteilt wird. Der kontinuierliche Modus
   schliesst eine Partitionierung aus und umgekehrt.

   Die reguläre Sequenz unterstützt Partitionen mit einer Länge bis zu 8. Wobei die letzt Partition
   kleiner sein kann, um nicht die Länge der Gesamtsequenz zu überschreiten. Die J-Sequenz unterstützt
   nur Partitionen mit einer fixen Länge von 1.

   Wandelzeit:
   Der Wandler benötigt 12.5 Taktzyklen (12-Bit) zum Wandeln + eine einstellbare Zeit (Samplezeit)
   zum Aufladen des Kondensators auf die zu messende Spannung.

   Die ersten 5 Kanäle sind schnelle Kanäle und können 5.1 Millionen-Samples pro Sekunde wandeln
   (0.19 μs) mit 12-Bit. Die anderen Kanäle (Nummer >= 6) können nur 4.8 Ms/s wandeln (0.21 μs)
   mit 12-Bit Auflösung, d.h in etwa 14 Taktzyklen (12-Bit) zum Wandeln.

   Geringere Auflösungen sind schneller (siehe Datenblatt).

   Die Samplezeit kann pro Kanal eingestellt werden.
   Wenn eine Sequenz einen Kanal mehrfach wandelt oder Kanäle einer J-Sequenz sich mit einer regulären Sequenz
   überschneiden, ist die zuletzt eingestellte Samplezeit für diesen Kanal in allen Sequenzen an allen Positionen
   dieselbe.

   Autodelay:
   Falls die Zeit zwischen Konvertierungen zu knapp bemessen ist, kann mittels des Flags
   HW_REGISTER_BIT_ADC_CFGR_AUTDLY die Konvertierung des nächsten Kanals einer Seqeuenz solange
   unterdrückt werden, bis der vorherige Wert gelesen wurde.
   Diese Flag wird momentan immer gelöscht und das setzen mittels einer Funktion ist noch nicht implementiert.

   Initialisierungssequenz:

   >  enable_clock_adc(ADCXandY);
   >  enable_vreg_adc(ADCX_or_Y);
   >  delay_10µs(); // Warte 10µs
   >  calibrate_adc(ADCX_or_Y);
   >  config_single_adc(ADCX_or_Y, adc_chan_?, adc_SAMPLETIME_?|...);

   Busy-Wait Sampling eines Kanals:

   > function uint32_t read_next_value(adc_t *ADCX_or_Y) {
   >    start_adc(ADCX_or_Y);
   >    while (!isdata_adc(ADCX_or_Y)) ; // adc_sample_time_? + 12.5 Takte (+ 1.5 Takte Slow Channel) bei 72MHZ
   >    return read_adc(ADCX_or_Y); // Liest Wert des konf. Kanals
   > }

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: µC/adc.h
    Header file <Analog-to-Digital-Converter>.

   file: TODO: adc.c
    Implementation file <Analog-to-Digital-Converter impl>.
*/
#ifndef STM32F303xC_MC_ADC_HEADER
#define STM32F303xC_MC_ADC_HEADER

// == exported Peripherals/HW-Units
#define ADC1      ((volatile struct adc_t*)(HW_REGISTER_BASEADDR_ADC1))
#define ADC2      ((volatile struct adc_t*)(HW_REGISTER_BASEADDR_ADC1 + 0x100))
#define ADC3      ((volatile struct adc_t*)(HW_REGISTER_BASEADDR_ADC3))
#define ADC4      ((volatile struct adc_t*)(HW_REGISTER_BASEADDR_ADC3 + 0x100))
#define ADC1and2  ((volatile struct adc_dual_t*)(HW_REGISTER_BASEADDR_ADC1 + 0x300))
#define ADC3and4  ((volatile struct adc_dual_t*)(HW_REGISTER_BASEADDR_ADC3 + 0x300))

// == exported types
struct adc_t;        // Beschreibt Register des Analog-Digital-Converter
struct adc_dual_t;   // Master & Slave (1and2) oder (3and4) für Dual-Channel Abtastung

/* enums: adc_chan_e
 * Jeder ADC unterstützt bis zu 18 Kanäle. */
typedef enum adc_chan_e {
   adc_chan_1 = 1,
   adc_chan_2,
   adc_chan_3,
   adc_chan_4,
   adc_chan_5,
   adc_chan_6,
   adc_chan_7,
   adc_chan_8,
   adc_chan_9,
   adc_chan_10,
   adc_chan_11,
   adc_chan_12,
   adc_chan_13,
   adc_chan_14,
   adc_chan_15,
   adc_chan_16,
   adc_chan_17,
   adc_chan_18,
} adc_chan_e;

typedef enum adc_channelmode_e {
   adc_channelmode_SINGLEMODE,   // Pins chan(+) und Vref-
   adc_channelmode_DIFFMODE,     // Pins chan(+) und chan+1(-)
} adc_channelmode_e;

#define adc_config_POS_SAMPLETIME   0u
#define adc_config_BITS_SAMPLETIME  7u
#define adc_config_MASK_SAMPLETIME  (adc_config_BITS_SAMPLETIME << adc_config_POS_SAMPLETIME)
#define adc_config_POS_RESOLUTION   3u
#define adc_config_BITS_RESOLUTION  3u
#define adc_config_MASK_RESOLUTION  (adc_config_BITS_RESOLUTION << adc_config_POS_RESOLUTION)
#define adc_config_POS_TRIGGER      5u
#define adc_config_BITS_TRIGGER     3u
#define adc_config_MASK_TRIGOUT     (adc_config_BITS_TRIGGER << adc_config_POS_TRIGGER)
#define adc_config_POS_HWEVENT      7u
#define adc_config_BITS_HWEVENT     15u
#define adc_config_MASK_HWEVENT     (adc_config_BITS_HWEVENT << adc_config_POS_HWEVENT)

typedef enum adc_config_e {
   adc_config_SAMPLETIME_1_5  = 0 << adc_config_POS_SAMPLETIME, // default; 1.5 ADC clock cycles
   adc_config_SAMPLETIME_2_5  = 1 << adc_config_POS_SAMPLETIME, // 2.5 ADC clock cycles
   adc_config_SAMPLETIME_4_5  = 2 << adc_config_POS_SAMPLETIME, // 4.5 ADC clock cycles
   adc_config_SAMPLETIME_7_5  = 3 << adc_config_POS_SAMPLETIME, // 7.5 ADC clock cycles
   adc_config_SAMPLETIME_19_5 = 4 << adc_config_POS_SAMPLETIME, // 19.5 ADC clock cycles
   adc_config_SAMPLETIME_61_5 = 5 << adc_config_POS_SAMPLETIME, // 61.5 ADC clock cycles
   adc_config_SAMPLETIME_181_5 = 6 << adc_config_POS_SAMPLETIME, // 181.5 ADC clock cycles
   adc_config_SAMPLETIME_601_5 = 7 << adc_config_POS_SAMPLETIME, // 601.5 ADC clock cycles

   // Die Auflösung ist für alle Kanäle aller Sequenzen gültig, der zuletzt per config_???_adc gesetzte Wert zählt.
   adc_config_RESOLUTION_12BIT = 0 << adc_config_POS_RESOLUTION, // default
   adc_config_RESOLUTION_10BIT = 1 << adc_config_POS_RESOLUTION,
   adc_config_RESOLUTION_8BIT  = 2 << adc_config_POS_RESOLUTION,
   adc_config_RESOLUTION_6BIT  = 3 << adc_config_POS_RESOLUTION,

   // == not implemented from here on (always software trigger chosen) ==

   adc_config_TRIGGER_SOFTWARE    = 0 << adc_config_POS_TRIGGER, // default; Software-Trigger == start_adc
   adc_config_TRIGGER_RISINGEDGE  = 1 << adc_config_POS_TRIGGER, // Übergang 0V->3.3V
   adc_config_TRIGGER_FALLINGEDGE = 2 << adc_config_POS_TRIGGER, // Übergang 3.3V->0V
   adc_config_TRIGGER_BOTHEDGE    = 3 << adc_config_POS_TRIGGER, // Übergang 0V->3.3V oder 3.3V->0V

   adc_config_HWEVENT_0  = 0 << adc_config_POS_HWEVENT, // default
   adc_config_HWEVENT_1  = 1 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_2  = 2 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_3  = 3 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_4  = 4 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_5  = 5 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_6  = 6 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_7  = 7 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_8  = 8 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_9  = 9 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_10  = 10 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_11  = 11 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_12  = 12 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_13  = 13 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_14  = 14 << adc_config_POS_HWEVENT,
   adc_config_HWEVENT_15  = 15 << adc_config_POS_HWEVENT,

} adc_config_e;


// == exported functions

static inline void enable_clock_adc(volatile struct adc_dual_t *adc);
static inline void disable_clock_adc(volatile struct adc_dual_t *adc);
static inline void enable_vreg_adc(volatile struct adc_t *adc);
static inline void disable_vreg_adc(volatile struct adc_t *adc);
static inline int  calibrate_adc(volatile struct adc_t *adc);
static inline int  setchannelmode_adc(volatile struct adc_t *adc, adc_chan_e chan, adc_channelmode_e mode);
static inline adc_channelmode_e getchannelmode_adc(volatile struct adc_t *adc, adc_chan_e chan);
static inline void stopall_adc(volatile struct adc_t *adc);
static inline void enable_adc(volatile struct adc_t *adc);
static inline void disable_adc(volatile struct adc_t *adc);
static inline int  isenabled_adc(volatile struct adc_t *adc);
static inline void setsampletime_adc(volatile struct adc_t *adc, adc_chan_e chan, adc_config_e time/*time&adc_config_MASK_SAMPLETIME*/);
static inline adc_config_e getsampletime_adc(volatile struct adc_t *adc, adc_chan_e chan);
static inline int  isoverflow_adc(const volatile struct adc_t *adc);
static inline void clear_isoverflow_adc(volatile struct adc_t *adc);
// == reguläre Sequenz
static inline void start_adc(volatile struct adc_t *adc);
static inline void stop_adc(volatile struct adc_t *adc);
static inline int  isstarted_adc(const volatile struct adc_t *adc);
static inline int  iseos_adc(const volatile struct adc_t *adc);
static inline void clear_iseos_adc(volatile struct adc_t *adc);
static inline void config_single_adc(volatile struct adc_t *adc, adc_chan_e chan, adc_config_e config);
static inline int  config_seq_adc(volatile struct adc_t *adc, uint32_t size_part/*1..8 or size_seq*/, uint32_t size_seq/*1..16*/, adc_chan_e chan[size_seq], adc_config_e config);
static inline int  config_contseq_adc(volatile struct adc_t *adc, uint32_t size_seq/*1..16*/, adc_chan_e chan[size_seq], adc_config_e config);
static inline uint32_t lenseq_adc(const volatile struct adc_t *adc);
static inline int  isdata_adc(const volatile struct adc_t *adc);
static inline uint32_t read_adc(volatile struct adc_t *adc);
// == J-Sequenz, welche eine höhere Priorität besitzt und die reguläre Sequenz unterbricht
static inline void startj_adc(volatile struct adc_t *adc);
static inline void stopj_adc(volatile struct adc_t *adc);
static inline int  isjstarted_adc(const volatile struct adc_t *adc);
static inline int  config_autojseq_adc(volatile struct adc_t *adc, uint32_t size_jseq/*1..4*/, adc_chan_e chan[size_jseq], adc_config_e config);
static inline int  config_jseq_adc(volatile struct adc_t *adc, uint32_t size_part/*1 or size_jseq*/, uint32_t size_jseq/*1..4*/, adc_chan_e chan[size_jseq], adc_config_e config);
static inline uint32_t lenjseq_adc(const volatile struct adc_t *adc);
static inline int  isjdata_adc(const volatile struct adc_t *adc);
static inline int  isjeos_adc(const volatile struct adc_t *adc);
static inline void clear_isjdata_adc(volatile struct adc_t *adc);
static inline void clear_isjeos_adc(volatile struct adc_t *adc);
static inline uint32_t readj_adc(volatile struct adc_t *adc, uint32_t seq_pos/*0..3*/);

// == definitions

typedef volatile struct adc_t {
   /* ADC interrupt and status register; Address offset: 0x00; Reset value: 0x00000000
    * Bits werden gelöscht durch schreiben einer 1 an ihrer Position und gesetzt durch Hardware. */
   uint32_t    isr;
   /* ADC interrupt enable register; Address offset: 0x04; Reset value: 0x00000000
    * Bits werden gesetzt und gelöscht durch Software. Eine 1 schaltet den zum isr-flag zugehörigen Interrupt ein.
    * Bits dürfen nur geändert werden, wenn JADSTART=0 bzw. ADSTART=0. */
   uint32_t    ier;
   /* ADC control register; Address offset: 0x08; Reset value: 0x20000000 */
   uint32_t    cr;
   /* ADC configuration register; Address offset: 0x0C; Reset value: 0x000000000
    * Bits dürfen nur geändert werden, wenn JADSTART=0 bzw. ADSTART=0. */
   uint32_t    cfgr;
   uint32_t    _reservedx10;
   /* ADC sample time register 1; Address offset: 0x14; Reset value: 0x00000000
    * Kanal 1 bis 9 */
   uint32_t    smpr1;
   /* ADC sample time register 2; Address offset: 0x18; Reset value: 0x00000000
    * Kanal 10 bis 18 */
   uint32_t    smpr2;
   uint32_t    _reservedx1C;
   /* ADC watchdog threshold register 1; Address offset: 0x20; Reset value: 0x0FFF0000
    * Bits 27:16 HT1[11:0]: Analog watchdog 1 higher threshold; Bits 11:0 LT1[11:0]: Analog watchdog 1 lower threshold */
   uint32_t    tr1;
   /* ADC watchdog threshold register 2; Address offset: 0x24; Reset value: 0x00FF0000
    * Bits 23:16 HT2[7:0]: Analog watchdog 2 higher threshold; Bits 7:0 LT2[7:0]: Analog watchdog 2 lower threshold */
   uint32_t    tr2;
   /* ADC watchdog threshold register 3; Address offset: 0x28; Reset value: 0x00FF0000
    * Bits 23:16 HT3[7:0]: Analog watchdog 3 higher threshold; Bits 7:0 LT3[7:0]: Analog watchdog 3 lower threshold */
   uint32_t    tr3;
   uint32_t    _reservedx2C;
   /* ADC regular sequence register 1; Address offset: 0x30; Reset value: 0x00000000
    * Bits 3:0 L[3:0]: Regular channel sequence length; Bits 10:6 SQ1[4:0]: 1st conversion in regular sequence ... Bits 28:24 SQ4[4:0]: 4th conversion in regular sequence */
   uint32_t    sqr1;
   /* ADC regular sequence register 2; Address offset: 0x34; Reset value: 0x00000000
    * Bits 28:24 SQ9[4:0]: 9th conversion in regular sequence ... Bits 4:0 SQ5[4:0]: 5th conversion in regular sequence */
   uint32_t    sqr2;
   /* ADC regular sequence register 3; Address offset: 0x38; Reset value: 0x00000000
    * Bits 28:24 SQ14[4:0]: 14th conversion in regular sequence ... Bits 4:0 SQ10[4:0]: 10th conversion in regular sequence */
   uint32_t    sqr3;
   /* ADC regular sequence register 4; Address offset: 0x3C; Reset value: 0x00000000
    * Bits 10:6 SQ16[4:0]: 16th conversion in regular sequence; Bits 4:0 SQ15[4:0]: 15th conversion in regular sequence */
   uint32_t    sqr4;
   /* ADC regular Data Register; Address offset: 0x40; Reset value: 0x00000000
    * Bits 15:0 RDATA[15:0]: Regular Data converted (left or right aligned as configured) */
   uint32_t    dr;
   uint32_t    _reservedx44[2];
   /* ADC injected sequence register; Address offset: 0x4C; Reset value: 0x00000000
    * Bits 1:0 JL[1:0]: Injected channel sequence length; ... Bis zu vier Kanäle programmierbar. */
   uint32_t    jsqr;
   uint32_t    _reservedx50[4];
   /* ADC offset register; Address offset: 0x60, 0x64, 0x68, 0x6C; Reset value: 0x00000000
    * Der Wert von bis zu vier verschiedenen Kanälen kann um einen 12-Bit-Offset vermindert werden. */
   uint32_t    ofr[4];
   uint32_t    _reservedx70[4];
   /* ADC injected data register; Address offset: 0x80 - 0x8C; Reset value: 0x00000000
    * Bits 15:0 Injected data; Der gesampelte Wert von bis zu vier Kanälen der J-Sequenz (injected). */
   uint32_t    jdr[4];
   uint32_t    _reservedx90[4];
   /* ADC Analog Watchdog 2 Configuration Register; Address offset: 0xA0; Reset value: 0x00000000
    * Bits 18:1 Analog watchdog 2 channel selection; Wert 0 schaltet den Watchdog ab. Bit[4:2]=111 ==> Kanäle 2 bis 4 werden überwacht. */
   uint32_t    awd2cr;
   /* ADC Analog Watchdog 3 Configuration Register; Address offset: 0xA4; Reset value: 0x00000000
    * Bits 18:1 Analog watchdog 3 channel selection; Wert 0 schaltet den Watchdog ab. Bit[4:2]=111 ==> Kanäle 2 bis 4 werden überwacht. */
   uint32_t    awd3cr;
   uint32_t    _reservedxA8[2];
   /* ADC Differential Mode Selection Register; Address offset: 0xB0; Reset value: 0x00000000
    * Bits 18:16 Differential mode for channels 18 to 16 (read only); always set to 0 (single-ended mode)
    * Bits 15:1 Differential mode for channels 15 to 1; 0: Single-ended mode; 1: differential mode;
    * Software is allowed to write these bits only when the ADC is disabled */
   uint32_t    difsel;
   /* ADC Calibration Factors; Address offset: 0xB4; Reset value: 0x00000000
    * Bits 22:16 Calibration Factors in differential mode
    * Bits 6:0   Calibration Factors In Single-Ended mode
    * Software is allowed to write these bits only when ADEN=1, ADSTART=0 and JADSTART=0 */
   uint32_t    calfact;
} adc_t;


typedef volatile struct adc_dual_t {
   /* ADC common status register; Address offset: 0x00; Reset value: 0x00000000
    * Enhält Kopien der Flags aus Master&Slave ISR Register. */
   uint32_t csr;
   uint32_t _reservedx04;
   /* ADC common control register; Address offset: 0x08; Reset value: 0x00000000
    * Kontrollflags für ADC in Dual-Modus + Flags die immer für beide gemeinsam gelten. */
   uint32_t ccr;
   /* ADC common regular data register for dual mode; Address offset: 0x0C; Reset value: 0x00000000
    * Bits 31:16 Regular data of the slave ADC
    * Bits 15:0  Regular data of the master ADC. */
   uint32_t cdr;
} adc_dual_t;

// == Bit Definitionen

// Bits für ISR und auch für IER gültig
#define HW_REGISTER_BIT_ADC_ISR_JQOVF  (1u << 10)  // 1: Injected context queue overflow has occurred
#define HW_REGISTER_BIT_ADC_ISR_AWD3   (1u << 9)   // 1: Analog watchdog 3 event occurred
#define HW_REGISTER_BIT_ADC_ISR_AWD2   (1u << 8)   // 1: Analog watchdog 2 event occurred
#define HW_REGISTER_BIT_ADC_ISR_AWD1   (1u << 7)   // 1: Analog watchdog 1 event occurred
#define HW_REGISTER_BIT_ADC_ISR_JEOS   (1u << 6)   // 1: Injected conversions complete (end of injected sequence)
#define HW_REGISTER_BIT_ADC_ISR_JEOC   (1u << 5)   // 1: Injected channel conversion complete (end of injected conversion)
#define HW_REGISTER_BIT_ADC_ISR_OVR    (1u << 4)   // 1: ADC Overrun has occurred
#define HW_REGISTER_BIT_ADC_ISR_EOS    (1u << 3)   // 1: Regular Conversions sequence complete (end of regular sequence)
#define HW_REGISTER_BIT_ADC_ISR_EOC    (1u << 2)   // 1: Regular channel conversion complete (end of regular conversion)
#define HW_REGISTER_BIT_ADC_ISR_EOSMP  (1u << 1)   // 1: End of sampling phase reached (RC-circuit is precharged with Vin)
#define HW_REGISTER_BIT_ADC_ISR_ADRDY  (1u << 0)   // 1: ADC is ready to start conversion (set by hardware after ADEN=1)

#define HW_REGISTER_BIT_ADC_CR_ADCAL   (1u << 31)  // Write 1 to calibrate the ADC. Read at 1 means that a calibration in progress.
#define HW_REGISTER_BIT_ADC_CR_ADCALDIF (1u << 30) // 0: Single-ended inputs Mode Calibration. 1: Differential inputs Mode Calibration.
#define HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS  28u   // 00: Intermediate state; 01: ADC Voltage regulator enabled;
#define HW_REGISTER_BIT_ADC_CR_ADVREGEN_BITS 3u    // 10: ADC Voltage regulator disabled (Reset state); 11: reserved
#define HW_REGISTER_BIT_ADC_CR_ADVREGEN_MASK HW_REGISTER_MASK(ADC, CR, ADVREGEN)
#define HW_REGISTER_BIT_ADC_CR_JADSTP  (1u << 5)   // Write 1: stop injected conversions. Read 1: JADSTP command is in progress.
#define HW_REGISTER_BIT_ADC_CR_ADSTP   (1u << 4)   // Write 1: stop regular conversions. Read 1: ADSTP command is in progress.
#define HW_REGISTER_BIT_ADC_CR_JADSTART (1u << 3)  // Write 1: start injected conversions. Read 1: ADC operating and eventually converting injected channels
#define HW_REGISTER_BIT_ADC_CR_ADSTART (1u << 2)   // Write 1: start regular conversions. Read 1: ADC operating and eventually converting regular channels
#define HW_REGISTER_BIT_ADC_CR_ADDIS   (1u << 1)   // Write 1: disable the ADC. Read 1: an ADDIS command is in progress
#define HW_REGISTER_BIT_ADC_CR_ADEN    (1u << 0)   // 1: ADC enabled or enable ADC.

#define HW_REGISTER_BIT_ADC_CFGR_AWD1CH_POS  26u   // [4:0]: Analog watchdogHW_REGISTER_BIT_ADC_SQR1_SQ4_POS 1 channel selection
#define HW_REGISTER_BIT_ADC_CFGR_AWD1CH_BITS 0x1f
#define HW_REGISTER_BIT_ADC_CFGR_AWD1CH_MASK HW_REGISTER_MASK(ADC, CFGR, AWD1CH)
#define HW_REGISTER_BIT_ADC_CFGR_JAUTO (1u << 25)  // 1: enabled Automatic injected group conversion (after regular conversion)
#define HW_REGISTER_BIT_ADC_CFGR_JAWD1EN (1u << 24) // 1: enabled Analog watchdog 1 on injected channels
#define HW_REGISTER_BIT_ADC_CFGR_AWD1EN (1u << 23) // 1: enabled Analog watchdog 1 on regular channels
#define HW_REGISTER_BIT_ADC_CFGR_AWD1SGL (1u << 22) // 1: single channel Analog 1 watchdog (AWD1CH) 0: Analog watchdog 1 enabled on all channels
#define HW_REGISTER_BIT_ADC_CFGR_JQM   (1u << 21)  // JSQR queue mode 0: maintains last written state of JSQR 1: could get empty and conversion stops
#define HW_REGISTER_BIT_ADC_CFGR_JDISCEN (1u << 20) // 1: Discontinuous mode on injected channels enabled
#define HW_REGISTER_BIT_ADC_CFGR_DISCNUM_POS  17u  // [2:0]: Discontinuous mode channel count
#define HW_REGISTER_BIT_ADC_CFGR_DISCNUM_BITS 0x7  // Es werden nur DISCNUM Werte einer Sequenz konvertiert. Aus einer Seqeuenz von Werten werden so x Untergruppen mit DISCNUM Werten oder am Ende der Sequenz weniger.
#define HW_REGISTER_BIT_ADC_CFGR_DISCNUM_MASK HW_REGISTER_MASK(ADC, CFGR, DISCNUM)
#define HW_REGISTER_BIT_ADC_CFGR_DISCEN  (1u << 16) // 1: Discontinuous mode for regular channels enabled
#define HW_REGISTER_BIT_ADC_CFGR_AUTDLY  (1u << 14) // 1: Auto-delayed conversion mode on
#define HW_REGISTER_BIT_ADC_CFGR_CONT  (1u << 13)  // 1: Continuous conversion mode (sequence repeated) 0: Single conversion mode (stop at end of sequence)
#define HW_REGISTER_BIT_ADC_CFGR_OVRMOD (1u << 12) // 1: DR register is overwritten with the last conversion result when an overrun is detected 0: preserved with the old data
#define HW_REGISTER_BIT_ADC_CFGR_EXTEN_POS  10u    // [1:0]: External trigger enable and polarity selection for regular channels
#define HW_REGISTER_BIT_ADC_CFGR_EXTEN_BITS 0x3    // 00: Software Trigger 01: rising edge HW Trigger 10: falling edge HW Trigger 11: both edges HW Trigger
#define HW_REGISTER_BIT_ADC_CFGR_EXTEN_MASK HW_REGISTER_MASK(ADC, CFGR, EXTEN)
#define HW_REGISTER_BIT_ADC_CFGR_EXTSEL_POS  6u    // [3:0]: External trigger selection for regular group
#define HW_REGISTER_BIT_ADC_CFGR_EXTSEL_BITS 0xf   // Selektiert Event 0 bis Event 15, unterschiedliche für ADC12 und ADC34, meist interne Events von Timern, aber auch ein EXTI PIN
#define HW_REGISTER_BIT_ADC_CFGR_EXTSEL_MASK HW_REGISTER_MASK(ADC, CFGR, EXTSEL)
#define HW_REGISTER_BIT_ADC_CFGR_ALIGN (1u << 5)   // Data alignment of DR register 1: Left alignment 0: Right alignment
#define HW_REGISTER_BIT_ADC_CFGR_RES_POS  3u       // [1:0]: Data resolution
#define HW_REGISTER_BIT_ADC_CFGR_RES_BITS 0x3      // 00: 12-Bit 01: 10-Bit 10: 8-Bit 11: 6-Bit
#define HW_REGISTER_BIT_ADC_CFGR_RES_MASK HW_REGISTER_MASK(ADC, CFGR, RES)
#define HW_REGISTER_BIT_ADC_CFGR_RES_12BIT (0x0 << HW_REGISTER_BIT_ADC_CFGR_RES_POS)
#define HW_REGISTER_BIT_ADC_CFGR_RES_10BIT (0x1 << HW_REGISTER_BIT_ADC_CFGR_RES_POS)
#define HW_REGISTER_BIT_ADC_CFGR_RES_8BIT  (0x2 << HW_REGISTER_BIT_ADC_CFGR_RES_POS)
#define HW_REGISTER_BIT_ADC_CFGR_RES_6BIT  (0x3 << HW_REGISTER_BIT_ADC_CFGR_RES_POS)
#define HW_REGISTER_BIT_ADC_CFGR_DMACFG (1u << 1)  // 1: DMA Circular Mode selected 0: DMA One Shot Mode selected
#define HW_REGISTER_BIT_ADC_CFGR_DMAEN  (1u << 0)  // 1: DMA enabled

#define HW_REGISTER_BIT_ADC_SMPR_BITS        0x7
#define HW_REGISTER_BIT_ADC_SMPR_1_5_CYCLES  0     // 1.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_2_5_CYCLES  1     // 2.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_4_5_CYCLES  2     // 4.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_7_5_CYCLES  3     // 7.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_19_5_CYCLES 4     // 19.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_61_5_CYCLES 5     // 61.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_181_5_CYCLES 6    // 181.5 ADC clock cycles
#define HW_REGISTER_BIT_ADC_SMPR_601_5_CYCLES 7    // 601.5 ADC clock cycles

#define HW_REGISTER_BIT_ADC_SQRX_CHANNEL_LENGTH 6u    // Ein Feld in den Registern SQR1..SQR4 beginnt immer ab einer durch 6 teilbaren Bitposition
#define HW_REGISTER_BIT_ADC_SQRX_CHANNEL_BITS   0x1F  // Bits [4:0] enthalten die Kanalnummer 1 .. 18 (Wert 0 ist unverbunden)
                                                      // Bit [5] ist reserviert
#define HW_REGISTER_BIT_ADC_SQRX_NRCHANNEL      5u    // Die Register SQR2..SQR4 enthalten jeweils 5 Kanalfelder,
                                                      // Das Register SQR1 enthält die Länge der Sequenz und daher nur 4 Kanäle

#define HW_REGISTER_BIT_ADC_SQR1_SQ4_POS     24u   // [4:0]: 4nd conversion in regular sequence
#define HW_REGISTER_BIT_ADC_SQR1_SQ4_BITS    0x1F  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht definiert)
#define HW_REGISTER_BIT_ADC_SQR1_SQ4_MASK    HW_REGISTER_MASK(ADC, SQR1, SQ4)
#define HW_REGISTER_BIT_ADC_SQR1_SQ3_POS     18u   // [4:0]: 3nd conversion in regular sequence
#define HW_REGISTER_BIT_ADC_SQR1_SQ3_BITS    0x1F  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht definiert)
#define HW_REGISTER_BIT_ADC_SQR1_SQ3_MASK    HW_REGISTER_MASK(ADC, SQR1, SQ3)
#define HW_REGISTER_BIT_ADC_SQR1_SQ2_POS     12u   // [4:0]: 2nd conversion in regular sequence
#define HW_REGISTER_BIT_ADC_SQR1_SQ2_BITS    0x1F  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht definiert)
#define HW_REGISTER_BIT_ADC_SQR1_SQ2_MASK    HW_REGISTER_MASK(ADC, SQR1, SQ2)
#define HW_REGISTER_BIT_ADC_SQR1_SQ1_POS     6u    // [4:0]: 1st conversion in regular sequence
#define HW_REGISTER_BIT_ADC_SQR1_SQ1_BITS    0x1F  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht definiert)
#define HW_REGISTER_BIT_ADC_SQR1_SQ1_MASK    HW_REGISTER_MASK(ADC, SQR1, SQ1)
#define HW_REGISTER_BIT_ADC_SQR1_L_POS       0u    // [3:0]: Regular channel sequence length
#define HW_REGISTER_BIT_ADC_SQR1_L_BITS      0xF   // 0000: 1 conversion ... 1111: 16 conversions
#define HW_REGISTER_BIT_ADC_SQR1_L_MASK      HW_REGISTER_MASK(ADC, SQR1, L)

#define HW_REGISTER_BIT_ADC_JSQR_JSQ4_POS    26u   // [4:0]: 4th conversion in the injected sequence
#define HW_REGISTER_BIT_ADC_JSQR_JSQ4_BITS   0x1f  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht belegt)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ4_MASK   HW_REGISTER_MASK(ADC, JSQR, JSQ4)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ3_POS    20u   // [4:0]: 3th conversion in the injected sequence
#define HW_REGISTER_BIT_ADC_JSQR_JSQ3_BITS   0x1f  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht belegt)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ3_MASK   HW_REGISTER_MASK(ADC, JSQR, JSQ3)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ2_POS    14u   // [4:0]: 2nd conversion in the injected sequence
#define HW_REGISTER_BIT_ADC_JSQR_JSQ2_BITS   0x1f  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht belegt)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ2_MASK   HW_REGISTER_MASK(ADC, JSQR, JSQ2)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ1_POS    8u    // [4:0]: 1st conversion in the injected sequence
#define HW_REGISTER_BIT_ADC_JSQR_JSQ1_BITS   0x1f  // Enthält Kanalnummer 1 .. 18 (Wert 0 nicht belegt)
#define HW_REGISTER_BIT_ADC_JSQR_JSQ1_MASK   HW_REGISTER_MASK(ADC, JSQR, JSQ1)
#define HW_REGISTER_BIT_ADC_JSQR_JEXTEN_POS  6u    // [1:0]: External Trigger Enable and Polarity Selection for injected channels
#define HW_REGISTER_BIT_ADC_JSQR_JEXTEN_BITS 0x3   // 00: Software Trigger 01: rising edge HW Trigger 10: falling edge HW Trigger 11: both edges HW Trigger
#define HW_REGISTER_BIT_ADC_JSQR_JEXTEN_MASK HW_REGISTER_MASK(ADC, JSQR, JEXTEN)
#define HW_REGISTER_BIT_ADC_JSQR_JEXTSEL_POS 2u    // [3:0]: External Trigger Selection for injected group
#define HW_REGISTER_BIT_ADC_JSQR_JEXTSEL_BITS 0xf  // Die Eventnummer von 0 bis 15, falls HW-Trigger in JEXTEN selektiert
#define HW_REGISTER_BIT_ADC_JSQR_JEXTSEL_MASK HW_REGISTER_MASK(ADC, JSQR, JEXTSEL)
#define HW_REGISTER_BIT_ADC_JSQR_JL_POS      0u    // [1:0]: Injected channel sequence length
#define HW_REGISTER_BIT_ADC_JSQR_JL_BITS     0x3   // 00: 1 Konvertierung 01: 2 Konv. 10: 3 Konv. 11: 4 Konvertierungen
#define HW_REGISTER_BIT_ADC_JSQR_JL_MASK     HW_REGISTER_MASK(ADC, JSQR, JL)

#define HW_REGISTER_BIT_ADC_OFR_EN     (1u << 31)  // 1: Offset is enabled
#define HW_REGISTER_BIT_ADC_OFR_CH_POS       26u   // [4:0]: Channel selection for the data offset
#define HW_REGISTER_BIT_ADC_OFR_CH_BITS      0x1f  // Kanalnummer 1 bis 18, Wert 0 ist keinem Kanal zugeordnet
#define HW_REGISTER_BIT_ADC_OFR_CH_MASK      HW_REGISTER_MASK(ADC, OFR, CH)
#define HW_REGISTER_BIT_ADC_OFR_OFFSET_POS   0u    // [11:0]: Data offset for the channel programmed into bits CH[4:0]
#define HW_REGISTER_BIT_ADC_OFR_OFFSET_BITS  0xfff // Dieser Wert wird vom konvertierten Wert abgezogen
#define HW_REGISTER_BIT_ADC_OFR_OFFSET_MASK  HW_REGISTER_MASK(ADC, OFR, OFFSET)

#define HW_REGISTER_BIT_ADC_CCR_VBATEN (1u << 24)  // 1: V BAT enabled
#define HW_REGISTER_BIT_ADC_CCR_TSEN   (1u << 23)  // 1: Temperature sensor enabled
#define HW_REGISTER_BIT_ADC_CCR_VREFEN (1u << 22)  // 1: V REFINT enabled
#define HW_REGISTER_BIT_ADC_CCR_CKMODE_POS   16u   // [1:0]: ADC clock mode; 00: PLL Asynchronous clock mode,
#define HW_REGISTER_BIT_ADC_CCR_CKMODE_BITS  0x3   // 01: HCLK/1 (Synchronous clock mode), 10: HCLK/2 (Synchronous clock mode), 11: HCLK/4 (Synchronous clock mode)
#define HW_REGISTER_BIT_ADC_CCR_CKMODE_MASK  HW_REGISTER_MASK(ADC, CCR, CKMODE)



// section: inline implementation

/* Muss vor allen anderen Funktionen aufgerufen werden, damit das Interface aktiviert wird.
 * Der ADC muss ausgeschaltet sein, was nach RESET der Fall ist.
 * Der ADC läuft mit demselben Takt wie der AHB-Bus, mit dem auch die CPU läuft.
 *
 * Das Register adc_dual_t->ccr unterstützt auch die Taktung der ADC-Hardware
 * an die PLL-Clock, wobei das ADC-Interface immer an den AHB-Bus gekoppelt ist.
 * (Momentan gibt es keine Funktion zur Umschaltung der Source-Clock).
 *
 * Unchecked Precondition:
 * (isenabled_adc(adc) == 0) && "AHB clock prescaler is set to 1" */
static inline void enable_clock_adc(adc_dual_t *adc)
{
   enable_adc12_clockcntrl();
   // conversion clock is chosen to be synchronous to HCLK (AHB Bus == Core Clock)
   uint32_t ccr = adc->ccr;
   ccr &= ~HW_REGISTER_BIT_ADC_CCR_CKMODE_MASK;
   ccr |= (1u << HW_REGISTER_BIT_ADC_CCR_CKMODE_POS); // 01: HCLK/1 (Synchronous clock mode)
                                                      // only valid if AHB clock prescaler is set to 1
   adc->ccr = ccr;
}

/* Darf nur aufgerufen werden, wenn der ADC abgeschaltet wurde.
 *
 * Unchecked Precondition:
 * (isenabled_adc(adc) == 0) */
static inline void disable_clock_adc(adc_dual_t *adc)
{
   adc->ccr &= ~HW_REGISTER_BIT_ADC_CCR_CKMODE_MASK;
   disable_adc12_clockcntrl();
}

/* Schaltet Spannungsregulator ein. Dieser Schritt ist nach dem Einschalten des Taktes/Clock auszuführen.
 * Nach Aufruf der Funktion muss im schlimmsten Fall 10 μs per Software gewartet werden, bis die Regulatorspannung stabil ist
 * und bevor eine andere Funktion wie die Kalibrierung ausgeführt werden darf.
 *
 * Unchecked Precondition:
 * (isenabled_adc(adc) == 0) */
static inline void enable_vreg_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if (((cr >> HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS) & HW_REGISTER_BIT_ADC_CR_ADVREGEN_BITS) != 1) {
      // 00: Intermediate state
      cr &= ~HW_REGISTER_BIT_ADC_CR_ADVREGEN_MASK;
      adc->cr = cr;
      // 01: ADC Voltage regulator enabled;
      cr |= (1 << HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS);
      adc->cr = cr;
      // WAIT T ADCVREG_STUP is equal to 10 μs in the worst case process/temperature/power supply.
      // ==> Caller of this function must implement waiting
   }
}

static inline void disable_vreg_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if (((cr >> HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS) & HW_REGISTER_BIT_ADC_CR_ADVREGEN_BITS) != 2) {
      // 00: Intermediate state
      cr &= ~HW_REGISTER_BIT_ADC_CR_ADVREGEN_MASK;
      adc->cr = cr;
      // 10: ADC Voltage regulator disabled (Reset state);
      cr |= (2 << HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS);
      adc->cr = cr;
   }
}

/* Kalibriert den ADC für Differenz- und Einfach-Modus neu.
 * Vorher muss enable_vreg_adc aufgerufen worde sein und der ADC muss ausgeschaltet sein.
 *
 * Unchecked Precondition:
 * HW_REGISTER_BIT_ADC_CR_ADVREGEN == (1 << HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS)
 *
 * Returns:
 * 0     - OK
 * EBUSY - (isenabled_adc(adc) != 0) ==> disable_adc(adc) sollte aufgerufen werden! */
static inline int calibrate_adc(adc_t *adc)
{
   if ((adc->cr & HW_REGISTER_BIT_ADC_CR_ADEN) != 0) {
      return EBUSY;
   }

   adc->cr |= HW_REGISTER_BIT_ADC_CR_ADCALDIF; // 1: Differential inputs Mode Calibration
   adc->cr |= HW_REGISTER_BIT_ADC_CR_ADCAL;
   // wait for differential calibration to complete
   while ((adc->cr & HW_REGISTER_BIT_ADC_CR_ADCAL) != 0) ;

   adc->cr &= ~HW_REGISTER_BIT_ADC_CR_ADCALDIF; // 0: Calibration is for Single-ended inputs Mode
   adc->cr |= HW_REGISTER_BIT_ADC_CR_ADCAL;
   // wait for single-ended calibration to complete
   while ((adc->cr & HW_REGISTER_BIT_ADC_CR_ADCAL) != 0) ;

   return 0;
}

/* Setzt den Single-Pin-Modus oder den Differenz-Modus.
 * Der Single-Modus misst die Spannung eines Pins gegen Masse (0V).
 * Genauer gesagt wird gegen Vref- gemessen. Dieser Anschluss ist beim STM32F303xC aber
 * auf Masse gelegt.
 * Der Differenz-Modus dagegen misst die Spannung eines Pins gegen einen zweiten.
 * Der Kanal-Pin stellt die positive Spannung und der Pin des Kanals (chan+1) die negative.
 * Sollte der folgende Kanal nicht belegt sein, z.B Kanal 11 von ADC1, dann wird gegen Vref-
 * wie im Single-Modus gemessen.
 *
 * Achtung: Im Differenz-Modus darf der folgende Kanal (chan+1) nicht mehr verwendet werden.
 *
 * Vor Aufruf dieser Funktion muss der ADC ausgeschaltet sein (disable_adc).
 *
 * Unchecked Precondition:
 * HW_REGISTER_BIT_ADC_CR_ADVREGEN == (1 << HW_REGISTER_BIT_ADC_CR_ADVREGEN_POS)
 *
 * Returns:
 * 0     - OK
 * EBUSY - (isenabled_adc(adc) != 0) ==> disable_adc(adc) sollte aufgerufen werden! */
static inline int setchannelmode_adc(adc_t *adc, adc_chan_e chan, adc_channelmode_e mode)
{
   if (mode == adc_channelmode_DIFFMODE) {
      if (  chan > adc_chan_15/*16..18 always SINGLE*/
            || (chan == adc_chan_15 && adc == ADC1)/*15 of ADC1 is connected to an internal single ended channel*/) {
         return EINVAL;
      }
   }
   if (isenabled_adc(adc)) {
      return EBUSY;
   }
   uint32_t difsel = adc->difsel;
   difsel = (difsel & ~(1u << chan)) | ((uint32_t)mode << chan);
   adc->difsel = difsel;
   return 0;
}

static inline adc_channelmode_e getchannelmode_adc(adc_t *adc, adc_chan_e chan)
{
   return (adc->difsel >> chan) & 1;
}

/* Stoppt sowohl die Wandlung der regulären als auch der J-Sequenz.
 * Im Falle eines Software-Triggers ist diese Operation nicht nötig, außer
 * im kontinuierlichen Modus. Ist der adc gestoppt, wird das Eintreffen eines neuen
 * HW-Triggers ignoriert und keine Wandlung mehr vorgenommen.  */
static inline void stopall_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if ((cr & (HW_REGISTER_BIT_ADC_CR_JADSTART|HW_REGISTER_BIT_ADC_CR_ADSTART)) != 0) {
      if ((cr & HW_REGISTER_BIT_ADC_CR_ADSTART) != 0) {
         cr |= HW_REGISTER_BIT_ADC_CR_ADSTP;
      }
      if ((cr & HW_REGISTER_BIT_ADC_CR_JADSTART) != 0) {
         cr |= HW_REGISTER_BIT_ADC_CR_JADSTP;
      }
      adc->cr = cr;
      while ((adc->cr & (HW_REGISTER_BIT_ADC_CR_ADSTP|HW_REGISTER_BIT_ADC_CR_JADSTP)) != 0) ;
   }
}

static inline void enable_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if ((cr & HW_REGISTER_BIT_ADC_CR_ADEN) == 0) {
      cr |= HW_REGISTER_BIT_ADC_CR_ADEN;
      adc->cr = cr;
      // wait until adc is ready to start conversion
      while ((adc->isr & HW_REGISTER_BIT_ADC_ISR_ADRDY) == 0) ;
   }
}

static inline void disable_adc(adc_t *adc)
{
   stopall_adc(adc);
   uint32_t cr = adc->cr;
   if ((cr & HW_REGISTER_BIT_ADC_CR_ADEN) != 0) {
      if ((cr & HW_REGISTER_BIT_ADC_CR_ADDIS) == 0) {
         // stop is not in progress ==> start stop process
         cr |= HW_REGISTER_BIT_ADC_CR_ADDIS;
         adc->cr = cr;
      }
      do {
         cr = adc->cr;
      } while ((cr & HW_REGISTER_BIT_ADC_CR_ADEN) != 0) ;
   }
}

static inline int isenabled_adc(volatile struct adc_t *adc)
{
   return (adc->cr & HW_REGISTER_BIT_ADC_CR_ADEN) / HW_REGISTER_BIT_ADC_CR_ADEN;
}

/* Setzt die Zeit in Taktyklen, die zum Sampling verwendet werden.
 * Diese Zeit muss lang genug sein, damit der interne Kondensator(capacitor) auf die zu messende Spannung geladen werden kann.
 * Es darf keine Konvertierung stattfinden, wenn diese Funktion aufgerufen wird.
 *
 * Unchecked Precondition:
 * (HW_REGISTER_BIT_ADC_CR_ADSTART == 0) && (HW_REGISTER_BIT_ADC_CR_JADSTART == 0) */
static inline void setsampletime_adc(adc_t *adc, adc_chan_e chan, adc_config_e time/*time&adc_config_MASK_SAMPLETIME*/)
{
   static_assert( adc_config_SAMPLETIME_1_5 == HW_REGISTER_BIT_ADC_SMPR_1_5_CYCLES
                  && adc_config_SAMPLETIME_2_5 == HW_REGISTER_BIT_ADC_SMPR_2_5_CYCLES
                  && adc_config_SAMPLETIME_4_5 == HW_REGISTER_BIT_ADC_SMPR_4_5_CYCLES
                  && adc_config_SAMPLETIME_7_5 == HW_REGISTER_BIT_ADC_SMPR_7_5_CYCLES
                  && adc_config_SAMPLETIME_19_5 == HW_REGISTER_BIT_ADC_SMPR_19_5_CYCLES
                  && adc_config_SAMPLETIME_61_5 == HW_REGISTER_BIT_ADC_SMPR_61_5_CYCLES
                  && adc_config_SAMPLETIME_181_5 == HW_REGISTER_BIT_ADC_SMPR_181_5_CYCLES
                  && adc_config_SAMPLETIME_601_5 == HW_REGISTER_BIT_ADC_SMPR_601_5_CYCLES);
   uint32_t shift = chan;
   volatile uint32_t *smpr = &adc->smpr1;
   static_assert( &adc->smpr2 == &adc->smpr1 + 1);
   if (shift >= 10) {
      shift -= 10;
      ++ smpr;
   }
   uint32_t val = *smpr;
   shift *= 3;
   val &= ~ (HW_REGISTER_BIT_ADC_SMPR_BITS << shift);
   val |= ((time&adc_config_MASK_SAMPLETIME) << shift);
   *smpr = val;
}

static inline adc_config_e getsampletime_adc(volatile struct adc_t *adc, adc_chan_e chan)
{
   uint32_t shift = chan;
   volatile uint32_t *smpr = &adc->smpr1;
   if (shift >= 10) {
      shift -= 10;
      ++ smpr;
   }
   shift *= 3;
   return (*smpr >> shift) & HW_REGISTER_BIT_ADC_SMPR_BITS;
}

static inline int isoverflow_adc(const adc_t *adc)
{
   return (adc->isr & HW_REGISTER_BIT_ADC_ISR_OVR) / HW_REGISTER_BIT_ADC_ISR_OVR;
}

/* Löscht Overflowflag. */
static inline void clear_isoverflow_adc(adc_t *adc)
{
   adc->isr = HW_REGISTER_BIT_ADC_ISR_OVR;
}


/* Startet den ADC. Im Falle eines Software-Triggers beginnt die Wandlung sofort
 * und das Startflag wird am Ende der Sequenz(bis zu 16 Kanäle) zurückgesetzt.
 * Im Falle eines Hardware-Triggers beginnt die Wandlung erst, wenn dieser eintrifft.
 * Das Startflag bleibt gesetzt und mit Eintreffen des nächsten HW-Triggers wird
 * eine neue Wandlungssequenz gestartet.
 *
 * Im kontinuierlichen Modus:
 * Eine mit config_contseq_adc konfiguierte Samplingsequenz einer Reihe von bis zu 16 Kanälen,
 * zusätzlich am Ende erweiterbar durch bis zu 4 vier Kanäle einer J-Sequenz durch Aufruf
 * von config_autojseq_adc, wird durch Aufruf von start_adc gestartet (SW-Trigger) oder
 * nach Aufruf von start wird der nächste HW-Trigger Event diese starten.
 * Am Ende der Sequenz wird diese ohne erneuten HW- bzw. SW-Trigger wiederholt gestartet,
 * solange das Startflag nicht mittels stop_adc zurückgesetzt wird.
 *
 * Unchecked Precondition:
 * (isenabled_adc(adc) != 0) */
static inline void start_adc(adc_t *adc)
{
   adc->cr |= HW_REGISTER_BIT_ADC_CR_ADSTART;
}

static inline void stop_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if ((cr & HW_REGISTER_BIT_ADC_CR_ADSTART) != 0) {
      cr |= HW_REGISTER_BIT_ADC_CR_ADSTP;
      adc->cr = cr;
      while ((adc->cr & HW_REGISTER_BIT_ADC_CR_ADSTP) != 0) ;
   }
}

static inline int isstarted_adc(const adc_t *adc)
{
   return (adc->cr & HW_REGISTER_BIT_ADC_CR_ADSTART) / HW_REGISTER_BIT_ADC_CR_ADSTART;
}

/* Konfiguriert den ADC mit einem zu messenden Kanal.
 * Die Wandlung wird mit einem Software-Trigger durch Aufruf der Funktion start_adc(adc) gestartet.
 * Der ADC stoppt sich nach Ende der Wandlung selbst und legt das Ergebnis in sein Datenregister ab.
 * Mittels isdata_adc kann erfragt werden, wenn das Ergebnis verfügbar ist und mit read_adc
 * kann es gelesen werden. read_adc setzt das von isdata_adc ermittelte Flag zurück. */
static inline void config_single_adc(adc_t *adc, adc_chan_e chan, adc_config_e config)
{
   // config regular conversion sequence of length 1 with software trigger
   // ==> every call to start_adc samples one single value
   (void) config_seq_adc(adc, 1, 1, &chan, config);
}

static inline int config_seq_adc(adc_t *adc, uint32_t size_part/*1..8 or size_seq*/, uint32_t size_seq/*1..16*/, adc_chan_e chan[size_seq], adc_config_e config)
{
   const int isSeqPartitioned = (size_part != size_seq);
   -- size_seq;
   -- size_part;
   if (size_seq > 15u || (isSeqPartitioned && (size_part > 7u || size_part > size_seq))) {
      return EINVAL;
   }
   enable_adc(adc);
   stopall_adc(adc);
   uint32_t cfgr = adc->cfgr;
   static_assert(HW_REGISTER_BIT_ADC_CFGR_RES_12BIT == 0);
   // It is not possible to use both the auto-injected and discontinuous modes simultaneously
   // ==> clear JAUTO in case size_part != size_seq !!
   cfgr &= ~(  HW_REGISTER_BIT_ADC_CFGR_DISCNUM_MASK  // clear Discontinuous mode channel count
               | HW_REGISTER_BIT_ADC_CFGR_DISCEN      // full sequence, no discontinuous mode (sequence-partitions)
               | HW_REGISTER_BIT_ADC_CFGR_AUTDLY      // no auto delay (wait until dr is read)
               | HW_REGISTER_BIT_ADC_CFGR_CONT        // stop after full sequence
               | HW_REGISTER_BIT_ADC_CFGR_EXTEN_MASK  // disable Hardware External trigger (software trigger mode)
               | HW_REGISTER_BIT_ADC_CFGR_EXTSEL_MASK // clear External trigger selection for regular group
               | HW_REGISTER_BIT_ADC_CFGR_ALIGN       // data in dr is right aligned
               | HW_REGISTER_BIT_ADC_CFGR_RES_MASK    // 12-Bit Resolution
               | HW_REGISTER_BIT_ADC_CFGR_DMAEN       // DMA disabled
               // Disable Automatic injected group conversion ?
               | (isSeqPartitioned ? HW_REGISTER_BIT_ADC_CFGR_JAUTO : 0)
            );
   cfgr |=  (  HW_REGISTER_BIT_ADC_CFGR_OVRMOD // DR register contains latest conversion result even in case of an overrun
               // Enable Discontinuous mode and set Discontinuous mode channel count ?
               | (isSeqPartitioned ? HW_REGISTER_BIT_ADC_CFGR_DISCEN + (size_part << HW_REGISTER_BIT_ADC_CFGR_DISCNUM_POS) : 0)
               | ((config & adc_config_MASK_RESOLUTION)
               #if adc_config_POS_RESOLUTION > HW_REGISTER_BIT_ADC_CFGR_RES_POS
                  >> (adc_config_POS_RESOLUTION - HW_REGISTER_BIT_ADC_CFGR_RES_POS))
               #else
                  << (HW_REGISTER_BIT_ADC_CFGR_RES_POS - adc_config_POS_RESOLUTION))
               #endif
            );
   adc->cfgr = cfgr;
   adc->isr  = (  HW_REGISTER_BIT_ADC_ISR_OVR      // clear ADC overrun
                  | HW_REGISTER_BIT_ADC_ISR_EOS    // clear End of regular sequence flag
                  | HW_REGISTER_BIT_ADC_ISR_EOC    // clear End of conversion flag
                  | HW_REGISTER_BIT_ADC_ISR_EOSMP  // clear End of sampling flag
               );
   for (uint32_t i = 0; i <= size_seq; ++i) {
      setsampletime_adc(adc, chan[i], config);
   }
   uint32_t sqr = (size_seq << HW_REGISTER_BIT_ADC_SQR1_L_POS);
   volatile uint32_t *sqr1 = &adc->sqr1;
   for (uint32_t i = 0, pos = HW_REGISTER_BIT_ADC_SQR1_SQ1_POS; i <= size_seq; ++i, pos += HW_REGISTER_BIT_ADC_SQRX_CHANNEL_LENGTH) {
      if (pos > HW_REGISTER_BIT_ADC_SQR1_SQ4_POS) {
         *sqr1++ = sqr;
         sqr = 0;
         pos = 0;
      }
      sqr |= chan[i] << pos; // channel at position (i+1)
   }
   *sqr1 = sqr;
   return 0;
}

static inline int config_contseq_adc(adc_t *adc, uint32_t size_seq/*1..16*/, adc_chan_e chan[size_seq], adc_config_e config)
{
   // It is not possible to discontinuous mode and continuous conversion mode at the same time
   // ==> (size_part == size_seq)
   int err = config_seq_adc(adc, size_seq, size_seq, chan, config);
   if (err) return err;
   adc->cfgr |= HW_REGISTER_BIT_ADC_CFGR_CONT;  // Continuous conversion mode (sequence repeated) after first trigger
   return 0;
}

static inline uint32_t lenseq_adc(const adc_t *adc)
{
   return 1u + ((adc->sqr1 & HW_REGISTER_BIT_ADC_SQR1_L_MASK) >> HW_REGISTER_BIT_ADC_SQR1_L_POS);
}

/* Zeigt an, dass der nächste Wert einer Wandlung aus dem dr Register gelesen werden kann.
 * Falls eine Sequenz weitere ADC-Kanäle umfasst, wird im Hintergrund der nächste Kanal-Pin
 * gewandelt. Das Lesen mittels read_adc löscht dieses Flag. Erfolgt eine erneute Wandlung
 * noch bevor dieses Flag gelöscht wurde (durch Lesen z.B. oder Schreiben des Bits HW_REGISTER_BIT_ADC_ISR_EOC
 * in das adc->isr Register), dann gibt es einen Overflow. */
static inline int isdata_adc(const adc_t *adc)
{
   return (adc->isr & HW_REGISTER_BIT_ADC_ISR_EOC) / HW_REGISTER_BIT_ADC_ISR_EOC;
}

static inline int iseos_adc(const adc_t *adc)
{
   return (adc->isr & HW_REGISTER_BIT_ADC_ISR_EOS) / HW_REGISTER_BIT_ADC_ISR_EOS;
}

/* Löscht End-Of-Sequence-Flag, das gesetzt wird, wenn der letzte Wert einer Sequenz konvertiert wurde. */
static inline void clear_iseos_adc(adc_t *adc)
{
   adc->isr = HW_REGISTER_BIT_ADC_ISR_EOS;
}

/* Löscht Overflow-Flag, End-Of-Sequence-Flag und isdata-Flag. Alles Flags der regulären Sequenz. */
static inline void clear_flags_adc(adc_t *adc)
{
   adc->isr = (  HW_REGISTER_BIT_ADC_ISR_OVR      // clear ADC overrun
                 | HW_REGISTER_BIT_ADC_ISR_EOS    // clear End of regular sequence flag
                 | HW_REGISTER_BIT_ADC_ISR_EOC    // clear End of conversion flag
                 | HW_REGISTER_BIT_ADC_ISR_EOSMP  // clear End of sampling flag
               );
}

static inline uint32_t read_adc(adc_t *adc)
{
   return adc->dr;
}

/* Startet die Wandlung der J-Sequenz.
 * Im Falle eines Software-Triggers beginnt die Wandlung sofort
 * und das Startflag wird am Ende der Sequenz(bis zu 16 Kanäle) zurückgesetzt.
 * Im Falle eines Hardware-Triggers beginnt die Wandlung erst, wenn dieser eintrifft.
 * Das Startflag bleibt gesetzt und mit Eintreffen des nächsten HW-Triggers wird
 * eine neue Wandlungssequenz gestartet.
 *
 * Falls die J-Seq. mittels config_autojseq_adc konfiguriert wurde und daher automatisch
 * am Ende der regulären Sequenz angehängt wird, darf diese Funktion nicht aufgerufen
 * werden. Mit dem Starten der regulären Sequenz wird auch die J-Seq. gestartet.
 *
 * Unchecked Precondition:
 * (adc->cfgr |= HW_REGISTER_BIT_ADC_CFGR_JAUTO) == 0 // d.h nicht mit config_autojseq_adc konfiguriert
 */
static inline void startj_adc(adc_t *adc)
{
   adc->cr |= HW_REGISTER_BIT_ADC_CR_JADSTART;
}

/* Stoppt die Wandlung der J-Sequenz.
 * Im Falle eines Software-Triggers ist diese Operation nicht nötig
 * Ist die J-Sequenz gestoppt, wird das Eintreffen eines neuen
 * HW-Triggers für diese Sequenz ignoriert und keine Wandlung mehr vorgenommen.  */
static inline void stopj_adc(adc_t *adc)
{
   uint32_t cr = adc->cr;
   if ((cr & HW_REGISTER_BIT_ADC_CR_JADSTART) != 0) {
      cr |= HW_REGISTER_BIT_ADC_CR_JADSTP;
      adc->cr = cr;
      while ((adc->cr & HW_REGISTER_BIT_ADC_CR_JADSTP) != 0) ;
   }
}

static inline int isjstarted_adc(const adc_t *adc)
{
   return (adc->cr & HW_REGISTER_BIT_ADC_CR_JADSTART) / HW_REGISTER_BIT_ADC_CR_JADSTART;
}

static inline int config_autojseq_adc(adc_t *adc, uint32_t size_jseq/*1..4*/, adc_chan_e chan[size_jseq], adc_config_e config)
{
   // HW_REGISTER_BIT_ADC_CFGR_JAUTO
   // In this mode, external trigger on injected channels must be disabled.
   // It is not possible to use both the auto-injected and discontinuous modes simultaneously
   // the bits DISCEN and JDISCEN must be kept cleared by software when JAUTO is set.
   if ((adc->cfgr & HW_REGISTER_BIT_ADC_CFGR_DISCEN) != 0) {
      return EAGAIN;
   }
   // assert(DISCEN == 0);
   int err = config_jseq_adc(adc, size_jseq, size_jseq, chan, config); // chooses SW-Trigger and JDISCEN=0
   if (err) return err;
   // assert(DISCEN == 0 && JDISCEN == 0 && "SW-Trigger");
   adc->cfgr |= HW_REGISTER_BIT_ADC_CFGR_JAUTO; // Enable Automatic injected group conversion
   return 0;
}

static inline int config_jseq_adc(adc_t *adc, uint32_t size_part/*1 or size_jseq*/, uint32_t size_jseq/*1..4*/, adc_chan_e chan[size_jseq], adc_config_e config)
{
   -- size_jseq;
   -- size_part;
   static_assert(HW_REGISTER_BIT_ADC_JSQR_JL_MASK == 3u);
   if (size_jseq > 3u || (size_part != size_jseq && size_part != 0u)) {
      return EINVAL;
   }
   enable_adc(adc);
   disable_adc(adc); // stop ongoing conversions and flush jsqr-queue (2 entries)
   enable_adc(adc);
   uint32_t cfgr = adc->cfgr;
   static_assert(HW_REGISTER_BIT_ADC_CFGR_RES_12BIT == 0);
   cfgr &= ~(  HW_REGISTER_BIT_ADC_CFGR_JAUTO      // Disable Automatic injected group conversion
               | HW_REGISTER_BIT_ADC_CFGR_JQM      // Mode 0: The Queue is never empty and maintains the last written configuration into JSQR.
               | HW_REGISTER_BIT_ADC_CFGR_JDISCEN  // Disable Discontinuous mode on injected channels
               | HW_REGISTER_BIT_ADC_CFGR_AUTDLY   // no auto delay (wait until dr is read)
               | HW_REGISTER_BIT_ADC_CFGR_ALIGN    // data in dr is right aligned
               | HW_REGISTER_BIT_ADC_CFGR_RES_MASK // 12-Bit Resolution
               | HW_REGISTER_BIT_ADC_CFGR_DMAEN    // DMA disabled
            );
   cfgr |= (   HW_REGISTER_BIT_ADC_CFGR_OVRMOD     // DR register contains latest conversion result even in case of an overrun
               | (size_part != size_jseq ? HW_REGISTER_BIT_ADC_CFGR_JDISCEN : 0)  // Enable Discontinuous mode ?
               | ((config & adc_config_MASK_RESOLUTION)
               #if adc_config_POS_RESOLUTION > HW_REGISTER_BIT_ADC_CFGR_RES_POS
                  >> (adc_config_POS_RESOLUTION - HW_REGISTER_BIT_ADC_CFGR_RES_POS))
               #else
                  << (HW_REGISTER_BIT_ADC_CFGR_RES_POS - adc_config_POS_RESOLUTION))
               #endif
            );
   adc->cfgr = cfgr;
   adc->isr  = (  HW_REGISTER_BIT_ADC_ISR_JQOVF    // clear Injected context queue overflow
                  | HW_REGISTER_BIT_ADC_ISR_JEOS   // clear Injected channel end of sequence flag
                  | HW_REGISTER_BIT_ADC_ISR_JEOC   // clear Injected channel end of conversion flag
               );
   for (uint32_t i = 0; i <= size_jseq; ++i) {
      setsampletime_adc(adc, chan[i], config);
   }
   uint32_t jsqr = (size_jseq); // J-Seq-Length + Software-Trigger
   static_assert( HW_REGISTER_BIT_ADC_JSQR_JSQ2_POS - HW_REGISTER_BIT_ADC_JSQR_JSQ1_POS == 6
                  && HW_REGISTER_BIT_ADC_JSQR_JSQ3_POS - HW_REGISTER_BIT_ADC_JSQR_JSQ2_POS == 6
                  && HW_REGISTER_BIT_ADC_JSQR_JSQ4_POS - HW_REGISTER_BIT_ADC_JSQR_JSQ3_POS == 6);
   for (uint32_t i = 0, pos = HW_REGISTER_BIT_ADC_JSQR_JSQ1_POS; i <= size_jseq; ++i, pos += 6) {
      jsqr |= chan[i] << pos; // channel at position (i+1)
   }
   adc->jsqr = jsqr; // write newest value, queue is in flushed state
   return 0;
}

static inline uint32_t lenjseq_adc(const adc_t *adc)
{
   return 1u + ((adc->jsqr & HW_REGISTER_BIT_ADC_JSQR_JL_MASK) >> HW_REGISTER_BIT_ADC_JSQR_JL_POS);
}

static inline int isjdata_adc(const adc_t *adc)
{
   return (adc->isr & HW_REGISTER_BIT_ADC_ISR_JEOC) / HW_REGISTER_BIT_ADC_ISR_JEOC;
}

static inline int isjeos_adc(const adc_t *adc)
{
   return (adc->isr & HW_REGISTER_BIT_ADC_ISR_JEOS) / HW_REGISTER_BIT_ADC_ISR_JEOS;
}

static inline void clear_isjdata_adc(adc_t *adc)
{
   adc->isr = HW_REGISTER_BIT_ADC_ISR_JEOC;
}

static inline void clear_isjeos_adc(adc_t *adc)
{
   adc->isr = HW_REGISTER_BIT_ADC_ISR_JEOS;
}

static inline uint32_t readj_adc(adc_t *adc, uint32_t seq_pos/*0..3*/)
{
   return adc->jdr[seq_pos];
}

#endif
