/* title: 16Bit-Basic-Timer

   Gibt Zugriff auf

      o     !6-Bit inkrementierenden Timer mit 16-Bit Prescaler, maximale Periode 2<<32
      o     Gibt Zugriff auf zwei unabhängige Timer-units TIMER6 und TIMER7
      o     Erlaubt die Ansteuerung der Digital-to-Analog Converter (DAC)

   Timer-Clock:
   Die Timer sind über den APB1 Bus (langsamere Peripherie, maximal 36MHz)
   angeschlossen und werden über dessen Takt(Clock) PCLK1 angesteuert.
   Falls der APB1-Prescaler einen Wert ungleich 1 hat, wenn also der Prozessor
   z.B. mit 72MHz läuft (HCLK==72MHz) und der APB1-Prescaler auf 2 gesetzt ist,
   dann wird PCLK1 mit 2 multipliziert:
   > TIMER-CLOCK = (APB1-Prescaler == 1 ? PCKL1 : 2 * PCKL1)

   Arbeitsweise:
   Der Zähler des Timers hat eine Präzision von 16-Bit und zählt immer von 0
   bis maximal 65535. Bei Erreichen eines von 1 bis 65535 frei definierbaren
   Höchststandes, wird der Zähler beim nächsten Takt wieder auf 0 zurückgesetzt.

   In diesem Moment wird ein Update-Event ausgelöst. Dieses setzt das Interrupt-Flag
   des Timers, das zur Erkennung eines Überlaufs und des damit Zusammenhängenden Zurücksetzens
   auf 0 dient. Dieses Flag muss nach dem Lesen explizit mit clear_expired_basictimer
   zurückgesetzt werden.

   Wurde auch die Interruptunterstützung eingeschaltet mittels enable_interrupt_basictimer
   oder mit gesetztem Flag basictimercfg_INTERRUPT im Aufruf von config_basictimer,
   dann wird dieses Flag weitergemeldet an den NVIC-Baustein (Nested Vectored Interrupt Controller)
   des Cortex-M4 µControllers und setzt dessen Interruptflag interrupt_TIMER6_DAC bzw. interrupt_TIMER7.

   Der Interrupt kann mittels den Funktionen timer6_dac_interrupt bzw. timer7_interrupt behandelt werden.
   Innerhalb dieser Funktionen *muss* spätestens mit clear_expired_basictimer der Interrupt bestätigt
   oder mindestens disable_interrupt_basictimer aufgerufen werden, damit es zu keiner Endlosinterrupt-
   schleife kommt.

   Periode:
   Da der Zähler mit jedem Takt eins weiter zählt ist bei Setzen des Höchstwertes auf 3 folgender
   Zählerablauf zu erwarten: 0, 1, 2, 3, (Update-Event) -> 0, 1, 2, 3, ... Die Periode ist also 4.
   Die gezählte Takte können noch durch einen Prescaler vorgeteilt werden, so dass nur jede 2te oder
   3te Takt gezählt wird. Der Prescaler ist ein 16-Bit Register, das den Takt durch (Registerwert+1)
   dividiert. Ein Registerwert von 0 einstpricht einem Prescalerdivisor von 1 und ein Registerwert von
   maximal 65535 einem Takt-Divisor von 65536.

   Damit ergibt sich eine Timerperiode von (Timerhöchstwert+1) * (Prescalerwert+1) und somit maximal
   65536 * 65536 oder 2<<32.

   Bei Aufrif von config_basictimer bzw. von update_basictimer sind der eigentlichen Perioden- und
   Prescalerwert anzugeben, von diesen wird dann 1 abgezogen, bevor diese in das arr (auto-reload-register)
   bzw. psc (Prescaler) Register übertragen werden.

   Timer Trigger Output:
   Jeder Timer hat ein zugeordnetes µC internes Timer-Ausgangs-Signal TRGO (TIM6_TRGO, TIM7_TRGO).
   Dieses Signal dient wieder anderen internen Peripheriebausteinen als Ansteurungssignal.
   So kann mittels eines Triggers die Ausgabe des DAC bzw. die Wandlereinheit eines ADC angestossen
   werden. Bei Aufruf von config_basictimer muss genau einer der Werte
   basictimercfg_TRIGOUT_RESET, basictimercfg_TRIGOUT_START oder basictimercfg_TRIGOUT_UPDATE
   mitgegeben werden. Dieser bestimmt, wann das TRGO-Signal gemeldet wird.

   Typische Verwendung am Beispiel von TIMER6:

   > enable_timer_clockcntrl(timernr_6);  // schalte Taktung
   > int err = config_basictimer(timer, 65536, 1, basictimercfg_ONCE);
   > if (err) { ... }
   > start_basictimer(TIMER6);
   > while (!isexpired_basictimer()) ; // Warte 65536 Buszyklen
   > // Timer hat sich selbst abgeschaltet
   > clear_expired_basictimer();     // Muss manuall gelöscht werden

   > enable_timer_clockcntrl(timernr_6);  // schalte Taktung
   > int err = config_basictimer(timer, 65536, 1, basictimercfg_REPEAT);
   > if (err) { ... }
   > start_basictimer(TIMER6);
   > while (!isexpired_basictimer()) ; // Warte 65536 Buszyklen
   > // Timer läuft weiter
   > ...
   > stop_basictimer();                // Muss manuell gestoppt werden
   > // expired-flag wird von stop_basictimer zusätzlich gelöscht

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: µC/timer.h
    Header file <Common-Timer-Interface>.

   file: µC/basictimer.h
    Header file <16Bit-Basic-Timer>.

   file: TODO: basictimer.c
    Implementation file <16Bit-Basic-Timer impl>.
*/
#ifndef STM32F303xC_MC_BASICTIMER_HEADER
#define STM32F303xC_MC_BASICTIMER_HEADER

// == exported Peripherals/HW-Units
#define TIMER6    ((basictimer_t*) HW_BASEADDR_TIM6)  // TIM6
#define TIMER7    ((basictimer_t*) HW_BASEADDR_TIM7)  // TIM7

// == exported types
struct basictimer_t;

typedef enum basictimer_bits_e {
   TIMER6_BIT = 1 << 4,
   TIMER7_BIT = 1 << 5,
} basictimer_bits_e;

/* enums: basictimercfg_e
 * Konfigurationswert des Timers. Diese werden mit config_basictimer
 * gesetzt und können mit getconfig_basictimer erfragt werden.
 *
 * basictimercfg_REPEAT          - Defaultwert, kann weggelassen werden. Der Timer setzt seinen Zählerstand nach Ablauf
 *                                 seiner Periode auf 0 zurück und startet erneut. Das »Expiration«-Flag wird gesetzt beim
 *                                 Zurücksetzen auf den Stand 0. Dieses Flag muss manuell zurückgesetzt werden, um eine
 *                                 erneute Ablaufperiode zu erkennen.
 * basictimercfg_ONCE            - Zusätzlich verhält sich wie basictimercfg_REPEAT, im Gegensatz dazu aber schaltet er
 *                                 sich nach dem ersten Überlauf und Zurücksetzen auf 0 selbst ab. Das expired-flag muss
 *                                 danach manuell gelöscht werden.
 * basictimercfg_MASK_REPEATONCE - Bitmaske um basictimercfg_ONCE bzw. basictimercfg_REPEAT aus einem zusammengesetzten
 *                                 Konfigurationswert herauszufiltern.
 * basictimercfg_TRIGOUT_RESET   - Der interne (Trigger-) Signalausgang wird aktiv gesetzt, wenn ein durch Software
 *                                 initiiertes Zurücksetzen ausgelöst wird. Zum Zurücksetzen vorgesehene Funktionen
 *                                 sind reset_basictimer und resetandexpire_basictimer. Dies funktioniert auch dann,
 *                                 wenn der Timer zwar konfiguriert aber noch nicht gestartet wurde.
 * basictimercfg_TRIGOUT_START   - Der interne (Trigger-) Signalausgang wird aktiv gesetzt, wenn der Timer gestartet
 *                                 wird. Dies erlaubt es, mehrere Timer (als Master&Slave) gleichzeitig zu starten.
 * basictimercfg_TRIGOUT_UPDATE  - Der interne (Trigger-) Signalausgang wird aktiv gesetzt, wenn der zurückgesetzt wird,
 *                                 d.h. er abgelaufen ist oder zurückgesetzt wird und zusätzlich nach dem ersten Einschalten.
 *                                 Nicht das Expirationflag selbst ist der Auslöser, sondern ein internes Updateevent, das
 *                                 zum Setzen des Expirationflags führt. Somit wird bei Ablauf nur ein Signal gegeben, auch
 *                                 wenn das Expirationflag längere Zeit aktiv bleibt.
 *                                 Der Timer kann so für andere Timer als Prescaler dienen oder die Ausgabe das DAC
 *                                 periodisch anstossen.
 * basictimercfg_MASK_TRIGOUT    - Bitmaske um einen die gesetzte Triggerkonfiguration aus einem zusammengesetzten
 *                                 Konfigurationswert herauszufiltern.
 * basictimercfg_INTERRUPT       - Ist das Expirationflag gesetzt, wird an NVIC des Arm-Prozessors ein Interrupt gemeldet.
 *                                 Wenn der entsprechende Interrupt am NVIC angeschaltet wurde (interrupt_TIMER6_DAC bzw.
 *                                 interrupt_TIMER7) und die Priorität ausreichend hoch ist, wird die Interruptserviceroutine
 *                                 (timer6_dac_interrupt bzw. timer7_interrupt) aufgerufen. Diese darf nicht vergessen,
 *                                 vor ihrem Ende clear_expired_basictimer aufzurufen, sonst wird nach ihrem verlassen,
 *                                 der Interrupt sofort wieder am NVIC aktiv gesetzt.
 * basictimercfg_DMA             - Meldet das interne Updateevent an den DMA-Controller, z.B. für Memmory-Memory Transfers.
 *                                 Zuvor muss der DMA-Controller initialisiert worden sein.
 */
typedef enum basictimercfg_e {
   // -- 1. choose one of --
   basictimercfg_REPEAT = 0,           // (default), begins counting from 0 after expiration
   basictimercfg_ONCE   = 1,           // stops automatically after expiration flag was set
   // -- 2. choose one of --
   basictimercfg_TRIGOUT_RESET   = 0,  // (default) reset_basictimer or resetandexpire_basictimer generates TRGO
   basictimercfg_TRIGOUT_START   = 4,  // start_basictimer, continue_basictimer generates TRGO (if previously stopped)
   basictimercfg_TRIGOUT_UPDATE  = 8,  // isexpired_basictimer() == 1 ==> generates TRGO
   // -- additional flags ored to others. DISABLED state is default !  --
   basictimercfg_INTERRUPT = 16,       // expiration flag is used as interrupt trigger and must be cleared during interrupt serice routine
   basictimercfg_DMA       = 32,       // expiration flag is used to trigger new DMA transfer
   // -- do not use MASK values --
   basictimercfg_MASK_REPEATONCE = 1,  // Mask to differentiate between REPEAT and ONCE value.
   basictimercfg_MASK_TRIGOUT    = 12, // Mask to differentiate between TRIGOUT values
} basictimercfg_e;


// == exported functions

static inline int isstarted_basictimer(const volatile struct basictimer_t *timer);
static inline void start_basictimer(volatile struct basictimer_t *timer);
static inline void stop_basictimer(volatile struct basictimer_t *timer);
static inline void continue_basictimer(volatile struct basictimer_t *timer);
static inline void reset_basictimer(volatile struct basictimer_t *timer);
static inline void resetandexpire_basictimer(volatile struct basictimer_t *timer);
static inline int config_basictimer(volatile struct basictimer_t *timer, uint32_t period/*2..65536*/, uint32_t prescale/*1..65536*/, basictimercfg_e config);
static inline basictimercfg_e getconfig_basictimer(const volatile struct basictimer_t *timer);
static inline int update_basictimer(volatile struct basictimer_t *timer, uint32_t period/*2..65536*/, uint32_t prescale/*1..65536*/);
static inline int isexpired_basictimer(const volatile struct basictimer_t *timer);
static inline void clear_expired_basictimer(volatile struct basictimer_t *timer);
static inline uint16_t value_basictimer(const volatile struct basictimer_t *timer);
static inline uint32_t exvalue_basictimer(const volatile struct basictimer_t *timer);
static inline int isenabled_interrupt_basictimer(const volatile struct basictimer_t *timer);
static inline void enable_interrupt_basictimer(volatile struct basictimer_t *timer);
static inline void disable_interrupt_basictimer(volatile struct basictimer_t *timer);
static inline void enable_dma_basictimer(volatile struct basictimer_t *timer);
static inline void disable_dma_basictimer(volatile struct basictimer_t *timer);

// == definitions

/*
 *
 * Invariant:
 * Reserved bits must be kept at reset value (for all register).
 */
typedef volatile struct basictimer_t {

   /* control register 1; Address offset: 0x00; Reset value: 0x0000 */
   uint16_t    cr1;
   uint16_t    _dummy1;
   /* control register 2; Address offset: 0x04; Reset value: 0x0000 */
   uint16_t    cr2;
   uint16_t    _dummy2;
   uint32_t    _reserved1;
   /* DMA/Interrupt enable register; Address offset: 0x0C; Reset value: 0x0000 */
   uint16_t    dier;
   uint16_t    _dummy3;
   /* status register; Address offset: 0x10; Reset value: 0x0000 */
   uint16_t    sr;
   uint16_t    _dummy4;
   /* event generation register. Address offset: 0x14; Reset value: 0x0000 */
   uint16_t    egr;
   uint16_t    _dummy5;
   uint32_t    _reserved2[3];
   /* counter. Address offset: 0x24; Reset value: 0x0000
    * Bit 31 UIFCPY: UIF Copy if (UIFREMAP == 1) else read as 0 if (UIFREMAP == 0)
    * Bits 15:0 CNT[15:0]: Counter value */
   union {
      uint16_t    cnt;
      uint32_t    cnt2;
   };
   /* prescaler. Address offset: 0x28; Reset value: 0x0000
    * Bits 15:0 PSC[15:0]: Prescaler value
    * The counter clock frequency CK_CNT is equal to (CK_PSC / (PSC[15:0] + 1)). */
   uint16_t    psc;
   uint16_t    _dummy6;
   /* auto-reload register. Address offset: 0x2C; Reset value: 0xFFFF
    * Bits 15:0 ARR[15:0]: Reload value
    * ARR is the value to be loaded into the actual auto-reload register.
    * The counter is blocked while the auto-reload value is null. */
   uint16_t    arr;
   uint16_t    _dummy7;
} basictimer_t;

// == Bit Definitionen

// control register 1
// UIF status bit remapping. 1: Remapping enabled. UIF status bit is copied to CNT register bit 31.
#define HW_BIT_TIM6_CR1_UIFREMAP    (1u << 11)
// Auto-reload preload enable. 1: ARR register is buffered.
#define HW_BIT_TIM6_CR1_ARPE        (1u << 7)   // 0: ARR register is not buffered.
// One-pulse mode. 1: Counter stops counting at the next update event (clearing the CEN bit)
#define HW_BIT_TIM6_CR1_OPM         (1u << 3)   // 0: Counter is not stopped at update event
// Update request source. 1: Only counter overflow generates an update interrupt or DMA request if enabled.
#define HW_BIT_TIM6_CR1_URS         (1u << 2)   // 0: Any of the following events generates interrupt and/or DMA
                                                         // 1. Counter overflow 2. Setting the UG bit 3. Update generation through the slave mode controller
// Update disable. 1: UEV disabled. The Update event is not generated, shadow registers keep their value (ARR, PSC). However the counter and the prescaler are reset in case of UG bit set.
#define HW_BIT_TIM6_CR1_UDIS        (1u << 1)   // 0: The Update (UEV) event is generated by one of the following events:
                                                         // 1. Counter overflow 2. Setting the UG bit 3. Update generation through the slave mode controller
                                                         // Buffered registers are then loaded with their preload values.
// Counter enable. 1: Counter enabled
#define HW_BIT_TIM6_CR1_CEN         (1u << 0)   // 0: Counter disabled

// control register 2
// Master mode selection Bits 6:4. 000: Reset - the UG bit from EGR is used as a trigger output (TRGO).
// 001: Enable - the Counter enable signal, CNT_EN, is used as a trigger output (TRGO).
//               It is useful to start several timers at the same time or to control a time window.
// 010: Update - The update event is selected as a trigger output (TRGO).
//               For instance a master timer can then be used as a prescaler for a slave timer.
#define HW_BIT_TIM6_CR2_MMS_POS     4u
#define HW_BIT_TIM6_CR2_MMS_BITS    0x7
#define HW_BIT_TIM6_CR2_MMS_MASK    HW_REGISTER_MASK(TIM6, CR2, MMS)

// DMA/Interrupt enable register
// Update DMA request enable. 1: Update DMA request enabled.
#define HW_BIT_TIM6_DIER_UDE        (1u << 8)   // 0: Update DMA request disabled.
// Update interrupt enable. 1: Update interrupt enabled.
#define HW_BIT_TIM6_DIER_UIE        (1u << 0)   // 0: Update interrupt disabled.

// status register
// Update interrupt flag. (rclear_w0 sw, set hw) 1: Update interrupt pending. This bit is set by hardware when the registers are updated.
#define HW_BIT_TIM6_SR_UIF          (1u << 0)   // 0: No update occurred.

// event generation register
// Bit 0 UG: Update generation (set sw, clear hw) 1: Re-initializes the timer counter and generates an update of the registers.
#define HW_BIT_TIM6_EGR_UG          (1u << 0)


// section: inline implementation

static inline void assert_timerbits_basictimer(void)
{
   static_assert(TIMER6_BIT == HW_BIT_RCC_APB1ENR_TIM6EN);
   static_assert(TIMER7_BIT == HW_BIT_RCC_APB1ENR_TIM7EN);
}

/* Gibt 1 zurück, falls der Timer läuft, also gestartet wurde, sonst 0.
 * Nach Aufruf von stop_basictimer oder config_basictimer gibt diese Funktion immer 0 zurück.
 * Nach Aufruf von start_basictimer oder continue_basictimer gibt diese Funtkion immer 1 zurück,
 * sofern der Timer als basictimercfg_ONCE konfiguriert wurde und er abgelaufen ist.
 * Dann wird nämlich der Timer automatisch gestoppt.
 */
static inline int isstarted_basictimer(const basictimer_t *timer)
{
   return (timer->cr1 & HW_BIT_TIM6_CR1_CEN) / HW_BIT_TIM6_CR1_CEN;
}

/* Stoppt den Timer. Der zuletzt erreichte Zählerstand bleibt erhalten,
 * so dass mit continue_basictimervon diesem Stand aus weitergezählt werden kann.
 *
 * Zusätzlich wird noch das "Expired"-Flag gelöscht. Damit keine neuen Interruptrequests
 * mehr generiert werden, falls dieses Flag gesetzt war und zusätzlich die Interrupt-
 * unterstützung konfiguriert ist.
 */
static inline void stop_basictimer(basictimer_t *timer)
{
   timer->cr1 &= ~HW_BIT_TIM6_CR1_CEN; // disable
   timer->sr = 0; // clear expired flag (generate no more interrupt requests if interrupt is enabled and expired flag was set)
}

/* Startet den Timer mit dem aktuellen Zählerstand.
 * Es werden auch noch die alten Werte für Periode und Prescaler benutzt,
 * die in internen Schattenregistern gespeichert stehen.
 * Eventuell mittels config_basictimer oder update_basictimer neu gesetzte Werte für
 * Periode und Prescaler werden erst ab dem nächsten Durchlauf verwendet,
 * d.h. wenn der Timer sich mit Ablauf auf 0 zurückgesetzt hat.
 *
 * Wurde der Timer schon gestartet, so bewirkt diese Funktion gar nichts.
 */
static inline void continue_basictimer(basictimer_t *timer)
{
   timer->cr1 |= HW_BIT_TIM6_CR1_CEN;  // enable
}

/* Setzt den Zähler des Timers auf 0 zurück.
 * Das "Expired"-Flag wird nicht gesetzt, so dass auch keine Signale
 * für den DMA- und Interrupt-Controller generiert werden.
 *
 * Dies funktioniert auch im gestoppten Zustand des Timers.
 *
 * Nach Rückkehr sind eventuell neue gesetzte Werte für Periode und Presclaer aktiv. */
static inline void reset_basictimer(basictimer_t *timer)
{
   timer->egr = HW_BIT_TIM6_EGR_UG; // generate update event without interrupt/dma
                                             // but reset internal counter (set to 0)
                                             // and reload shadow register (HW_BIT_TIM6_CR1_UDIS not set)
}

/* Setzt den Zähler des Timers auf 0 zurück.
 * Gleichzeitig wird auch das "Expired"-Flag gesetzt, das den Ablauf des Timers
 * signalisiert. Wenn DMA und Interrupt eingeschaltet wurden, dann werden auch
 * entsprechende Signale an den Interrupt- DMA-Controller weitergeleitet.
 *
 * Dies funktioniert auch im gestoppten Zustand des Timers.
 *
 * Nach Rückkehr sind eventuell neue gesetzte Werte für Periode und Presclaer aktiv. */
static inline void resetandexpire_basictimer(basictimer_t *timer)
{
   timer->cr1 &= ~HW_BIT_TIM6_CR1_URS; // setting UG Bit updates shadow register AND generates interrupt/dma
   timer->egr = HW_BIT_TIM6_EGR_UG;    // generate update event
   timer->cr1 |= HW_BIT_TIM6_CR1_URS;  // setting UG Bit updates shadow register but generates no interrupt/dma
}

/* Startet den Timer von Zählerstand 0.
 * Läuft er schon, so wird er nur auf den Zählerstand 0 zurückgesetzt.
 */
static inline void start_basictimer(basictimer_t *timer)
{
   reset_basictimer(timer);    // ensures updated period values are in use (transfered into (buffered) shadow register)
   continue_basictimer(timer); // enable in case of not already started
}

/* Kofiguriert den Timer und setzt eine neue Timerperiode oder Timerablaufzeit.
 * Der Bustakt, oder mit mit 2 multiplizierte Bustakt, dient als Taktgeber (siehe Beschreibung ganz oben).
 * Der Takt wird durch prescale dividiert, so dass nur jeder x-te Takt gezählt wird (presscale == x).
 * Die Timerkonfiguration wird wie in config bechrieben gesetzt.
 * Siehe dazu die auch Beschreibung der einzelnen Werte zu basictimercfg_e.
 *
 * Vor der Neukonfiguration des Timers wird dieser gestoppt und bleibt es auch bis zur Rückkehr dieser Funktionen.
 * Der Aufrufer muss den Timer danach neu starten.

 * Die neue Timerperiode und Prescalerwert werden aber noch nicht in die internen Schattenregister geladen,
 * sondern werden zuerst in Preloadregistern zwischengespeichert.
 *
 * Mit einem Aufruf von start_basictimer werden diese jedoch in die Schattenregister geladen und der Timer
 * startet und läuft dann mit den neu konfigurierten Werten und startet vom Zählerstand 0.
 *
 * Wird dagegen continue_basictimer zum Starten des Timers verwendet, sind noch die alten Werte aus den Schattenregistern
 * für period und prescale gültig. Der Timer zählt auch nicht von 0 an aufwärts sondern vom zuletzt gestoppten
 * Stand an. Mit dem nächsten Timerablauf werden die Werte dann neu geladen.
 *
 */
static inline int config_basictimer(basictimer_t *timer, uint32_t period/*2..65536*/, uint32_t prescale/*1..65536*/, basictimercfg_e config)
{
   -- period;
   -- prescale;

   if (period == 0 || (period|prescale) >= 65536) {
      return EINVAL;
   }

   timer->cr1 = (0 & ~(
                  HW_BIT_TIM6_CR1_UDIS    // Update Event is generated (not disabled)
                | HW_BIT_TIM6_CR1_OPM     // timer reset at update event and not stopped
                | HW_BIT_TIM6_CR1_CEN))   // stop timer
              | ((config&basictimercfg_ONCE) * HW_BIT_TIM6_CR1_OPM) // counter stops at the next update event
              | HW_BIT_TIM6_CR1_ARPE      // timer->arr is buffered (new values from updateperiod are used only after update event)
              | HW_BIT_TIM6_CR1_URS       // setting UG Bit updates shadow register but generates no interrupt/dma
              | HW_BIT_TIM6_CR1_UIFREMAP  // Interrupt Flag mapped into cnt register (bit 31)
              ;
   static_assert((basictimercfg_MASK_TRIGOUT & 0x03) == 0);
   static_assert((basictimercfg_MASK_TRIGOUT >> 2 & 0x3) == 3);
   timer->cr2 = (config&basictimercfg_MASK_TRIGOUT) << (HW_BIT_TIM6_CR2_MMS_POS - 2);
   timer->sr = 0; // clear any pending interrupt
   timer->dier = ((config&basictimercfg_INTERRUPT) ? HW_BIT_TIM6_DIER_UIE : 0)
               | ((config&basictimercfg_DMA) ? HW_BIT_TIM6_DIER_UDE : 0) ;
   timer->psc = prescale;
   timer->arr = period;
   return 0;
}

/* Gibt die zuletzt mit config_basictimer gesetzte Konfiguration zurück.
 * Falls zwischenzeitlich die DMA bzw. Interruptunterstützung ein oder ausgeschalten
 * wurde ({disable,enable}_{dma,interrupt}_basictimer), wird das durch die Anwesenheit
 * oder Abwesenheit der Flags basictimercfg_DMA und basictimercfg_INTERRUPT widergespiegelt.
 */
static inline basictimercfg_e getconfig_basictimer(const volatile struct basictimer_t *timer)
{
   uint32_t config;
   uint32_t r;
   static_assert((basictimercfg_MASK_TRIGOUT & 0x03) == 0);
   static_assert((basictimercfg_MASK_TRIGOUT >> 2 & 0x3) == 3);
   static_assert(basictimercfg_ONCE == 1);
   config  = (timer->cr1 & HW_BIT_TIM6_CR1_OPM) / HW_BIT_TIM6_CR1_OPM;
   config |= (timer->cr2 & HW_BIT_TIM6_CR2_MMS_MASK) >> (HW_BIT_TIM6_CR2_MMS_POS - 2);
   r = timer->dier;
   config |=  (r&HW_BIT_TIM6_DIER_UIE ? basictimercfg_INTERRUPT : 0);
   config |=  (r&HW_BIT_TIM6_DIER_UDE ? basictimercfg_DMA : 0);
   return (basictimercfg_e) config;
}

/* Setzt eine neue Timerperiode oder Timerablaufzeit.
 * Der Bustakt, oder mit mit 2 multiplizierte Bustakt, dient als Taktgeber (siehe Beschreibung ganz oben).
 * Der Takt wird durch prescale dividiert, so dass nur jeder x-te Takt gezählt wird (presscale == x).
 * Nach period Takten wird das "Expired"-Flag gesetzt. Somit ist die Gesamtperiode des Timers
 * period*prescale. Die unterstützte Minimalwert von period ist 2, auch wenn prerscale einen Wert
 * grösser als 1 besitzt.
 *
 * Die neue gesetzten Werte sind nicht sofort aktiv. Die eigentlich aktiven Werte für period und prescale
 * werden in internen sogenannten Schattenregistern gehalten. Die Werte der Schattenregister werden mit den
 * neu gesetzten Werten aus den sogenannten Preloadregisteren beim nächsten Timerablauf überschrieben.
 *
 * Sollen die neuen Werte gleich übernommen werdenm muss nach einem update
 * reset_basictimer bzw. resetandexpire_basictimer aufgerufen werden. Diese setzen aber auch den Zählerstand
 * wieder auf 0 zurück.
 */
static inline int update_basictimer(basictimer_t *timer, uint32_t period/*2..65536*/, uint32_t prescale/*1..65536*/)
{
   -- period;
   -- prescale;

   if (period == 0 || (period|prescale) >= 65536) {
      return EINVAL;
   }

   timer->psc = prescale;
   timer->arr = period;

   return 0;
}

/* Gibt 1 zurück, wenn der Timer abgelaufen ist,
 * d.h. eine Timerperiode*Prescaler Clockticks vergangen sind, sonst 0.
 * Das Expirationflag wird gesetzt, nachdem der Timer seinen Höchststand
 * erreicht hat und mit dem nächsten Takt auf 0 zurückgesetzt wird.
 */
static inline int isexpired_basictimer(const basictimer_t *timer)
{
   return timer->sr;
}

/* Löscht das "Expiration"-Flag, das angibt, ob der Timer abgelaufen ist,
 * d.h. eine Timerperiode*Prescaler Clockticks vergangen sind.
 */
static inline void clear_expired_basictimer(basictimer_t *timer)
{
   timer->sr = 0; // clear pending interrupt
}

/* Lierert den 16-Bit Zählerstand des Timers.
 * Der Werteberiech ist von 0 bis Timerperiode-1.
 * Die Timerperiode kann mit config_basictimer initialisiert
 * und mit update_basictimer laufend angepasst werden.
 */
static inline uint16_t value_basictimer(const basictimer_t *timer)
{
   return timer->cnt;
}

/* Lierert den 16-Bit Zählerstand des Timers plus Expiration-Flag.
 * Bit 31 (0x80000000) ist zusätzlich gesetzt, wenn der Timer abgelaufen ist.
 */
static inline uint32_t exvalue_basictimer(const basictimer_t *timer)
{
   return timer->cnt2;
}

/* Liefert den Wert 1, wenn die Interruptgenerierung eingeschaltet ist, sonst 0.
 */
static inline int isenabled_interrupt_basictimer(const basictimer_t *timer)
{
   return (timer->dier & HW_BIT_TIM6_DIER_UIE) != 0;
}

/* Schaltet die Signalisierung des Interrupt-Controllers (NVIC) an.
 * Er wirdvon jetzt an bei gesetztem Expirationflag einen Interrupt auslösen.
 * Dieses Flag kann auch direkt über config_basictimer angeschaltet werden.
 */
static inline void enable_interrupt_basictimer(basictimer_t *timer)
{
   timer->dier |= HW_BIT_TIM6_DIER_UIE;
}

/* Schaltet die Signalisierung des Interrupt-Controllers (NVIC) ab.
 * Er würde sonst bei gesetztem Expirationflag einen neuen Interrupt auslösen.
 */
static inline void disable_interrupt_basictimer(basictimer_t *timer)
{
   timer->dier &= ~HW_BIT_TIM6_DIER_UIE;
}

/* Schaltet die Signalisierung des DMA-Controllers an.
 * Er wird bei Überlauf und Zurücksetzen des Timers einen neuen Datentransfer starten.
 * Vorher muss aber DMA-Controller entsprechend konfiguriert worden sein.
 * Dieses Flag kann auch direkt über config_basictimer angeschaltet werden.
 */
static inline void enable_dma_basictimer(basictimer_t *timer)
{
   timer->dier |= HW_BIT_TIM6_DIER_UDE;
}

/* Schaltet die Signalisierung des DMA-Controllers ab.
 * Er würde sonst bei Überlauf und Zurücksetzen des Timers einen neuen Datentransfer starten.
 */
static inline void disable_dma_basictimer(basictimer_t *timer)
{
   timer->dier &= ~HW_BIT_TIM6_DIER_UDE;
}

#endif
