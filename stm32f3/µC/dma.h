/* title: Direct-Memory-Access

   Gibt Zugriff auf

      o 7 uabhängige Kanäle gesteuert von Controller DMA1.
      o Zusätzlich 5 uabhängige Kanäle gesteuert von Controller DMA2.
      o DMA1 und DMA2 teilen sich den Bus untereinander und mit der CPU
        im round-robin Verfahren.
      o Bis zu 65535 Datenwörter können in einer Transaktion übertragen
        werden.
      o Datenwörter sind 8Bit, 16Bit oder 32Bit groß.
      o Datenübertragungen werden zwischen aktiven DMA unterstützenden
        Peripheriebausteinen und Speicher (Flash, (S)RAM) in beiden
        Richtungen unterstützt.
      o Es werden auch Speicher-Speicher Datenübertragungen unterstützt
        (Konfigurationsflag dmacfg_NOTRIGGER).
      o 4 durch Software programmierbare Prioritäten werden zwischen den Kanälen
        innerhalb eine Controllers unterstützt.

   Die DMA-Hardwareunit läuft mit demselben Takt wie die CPU (HCLK).

   Beide DMA-Controller können unabhängig auf interne Busse zugreifen.
   Greifen die CPU und ein Controller oder beide Controller gleichzeitig
   auf ein- und denselben Bus zu (der µC besitzt mehrere Datenbusse),
   so müssen sie sich den Bus teilen. Die interne Bus-Matrix implementiert
   ein round-robin Verfahren und sichert der CPU mindesten 50% des Datenverkehrs
   zu.

   Eine DMA-Datenübertragung (Transaktion) ist genau einem Kanal zugeordnet.
   Jeder Kanal unterstützt Speicher-zu-Speicher Datenübertragungen, aber
   jeder Kanal unterstützt nur eine fest zugeordnete Menge an Peripheriebausteinen,
   wobei aus dieser Menge immer nur genau einer aktiv sein kann.
   Die Zuordnung der Peripheriebausteine zu den Kanälen ist weiter unten in zwei
   Tabellen aufgelistet.

   Ablauf einer DMA-Transaktion:
   Nachdem ein zu einer Peripherie-Hardware-Einheit passender DMA-Kanal konfiguriert
   und aktiviert wurde, wartet der Controller auf einen Request (Trigger) der Peripherie.
   Wird dieser Erhalten startet der Controller die nächste Datenübertragung. Bei Zugriff
   auf die Peripherie signalisiert er eine Bestätigung an die Peripherie, die bis zu dem
   Zeitpunkt aktiv bleibt, bis diese den Request-Trigger deaktiviert.

   Ein Übertragung besteht aus grundsätzlichen 3 Schritten:
      1. Lesender Zugriff auf die Adresse des Speichers oder der Peripherie.
         Je nach Konfiguration werden 8, 16 oder 32 Bit gelesen.
         Ist es ein Periphiezugriff (HW-Unit), dann wird zusätzlich der DMA-Zugrif bestätigt.
      2. Schreibender Zugriff auf die Adresse des Speichers oder der Peripherie.
         Je nach Konfiguration werden 8, 16 oder 32 Bits geschrieben, wobei möglicherweise
         die höherwertigen Bits des gelesenen Wertes abgeschnitten werden (Einengung)
         oder die höherwertigen Bits des zu schreibenden Wertes auf 0 gesetzt werden (Erweiterung).
      3. Der Datenzähler (Wert gesetzt mit Parameter datacount) wird um eins verringert.
         Die internen Adresszähler werden, sofern konfiguriert
         (Konfigurationsbits dmacfg_MEM_INCRADDR und dmacfg_HW_INCRADDR), um die Anzahl der
         gelesenen oder geschriebenen Bytes (1,2 oder 4) erhöht.
         Erreicht der Zähler den Stand 0, werden weitere Requests ignoriert.
         Es sei denn das Bit dmacfg_LOOP wurde gesetzt, dann werden die internen
         Adressen und der Zähler wieder auf den Startwert zurückgesetzt und weitere
         Requests werden angenommen.

   Kanalprioritäten:
   Ist mehr als ein Kanal aktiv, werden diese anhand ihrer Priorität verwaltet.
   Softwareseitig kann einem Kanal eine von 4 Prioritäten zugeordnet werden (MAX,HIGH,LOW,MIN).
   Von Kanälen mit gleicher Priorität wird derjenige bevorzugt, dessen Kanalnummer kleiner ist,
   Kanal 2 wird etwa dem Kanal 4 vorgezogen, wenn beide die gleiche Priorität besitzen.

   Schreibzugriff auf eine Peripherie-HW-Unit mit 8 oder 16 Bits:
   Der Schreibzugriff erfolgt immer mit 32-Bit. Der 8-Bit Datenwert 0xAB
   wird in den Wert 0xABABABAB vor dem Zugriff gewandelt. Der 16-Bit Datenwert
   0xABCD in den Wert 0xABCDABCD. Unbenutzte Bytes oder Halbwörter werden
   erhalten dabei denselben Wert. Wertet die HW-Unit die Größe des Schreibzugriffs
   nicht aus oder erwartet einen 16- oder 32-Bit-Wert, es wird jedoch nur ein 8-Bit
   oder 16-Bit Wert geschrieben, so wird dieser nach obigem Muster gespiegelt.

   Beim Zugriff auf den DAC im Dualkanal-Modus ist dies hilfreich. Ein 8-Bit Schreibzugriff
   setzt dabei beide Ausgabekanäle auf denselben 8-Bit Wert, da das Register 16-Bit breit ist.

   Interrupts:
   Jeder Kanal bestitzt eine eigene Interruptserviceroutine mit folgenden Namen:
   dma1_channel1_interrupt, ..., dma1_channel7_interrupt, dma2_channel1_interrupt, ..., dma2_channel5_interrupt.

   Drei Interrupts werden unterstützt.

   Die Interrupts entsprechen den Zustandsflags dma_state_HALF, dma_state_COMPLETE und dma_state_ERROR.
   Der zustand kann mit state_dma erfragt werden. Wird ein Flag gesetzt und ist der entsprechende Interrupt
   angeschaltet, dann wird der implementierte Interrupt aufgerufen. Die Interruptserviceroutine muss
   das entsprechende dma_state_YYY Flag löschen, sonst würde der Interrupt immer wieder neu aufgerufen.

   Nach Übertragung der Hälfte der Datenwörter und mit gesetztem Flag dmacfg_INTERRUPT_HALF
   bzw. dmacfg_INTERRUPT wird das Bit dma_state_HALF gesetzt und ein Interrupt ausgelöst.

   Nach Übertragung aller Datenwörter und mit gesetztem Flag dmacfg_INTERRUPT_COMPLETE
   bzw. dmacfg_INTERRUPT wird das Bit dma_state_COMPLETE gesetzt und ein Interrupt ausgelöst.

   Nach einem Übertragungsfehler und mit gesetztem Flag dmacfg_INTERRUPT_ERROR
   bzw. dmacfg_INTERRUPT wird das Bit dma_state_ERROR gesetzt und ein Interrupt ausgelöst.
   Der Zugriff auf die Adressen 0..256K löst z.B. einen Zugriffsfehler aus,
   weshalb es die entsprechenden config_flah Funktionen gibt, welche die Adresse anpassen.
   Der zugehörige DMA-Kanal wird nach einem Fehler automatisch abgeschaltet.

   Trotz abgeschaltetem Kanal werden Interrupts bei gesetzten dma_state_YYY Flags
   und konfiguriertem dmacfg_INTERRUPT_YYY aufgerufen.

   Natürlich müssen Interrupts auch im NVIC-Modul eingeschaltet worden sein.

   µC-interne Kanal-Verbindungen DMA1:
   ┌────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
   │ HWUnit │ Kanal 1 │ Kanal 2 │ Kanal 3 │ Kanal 4 │ Kanal 5 │ Kanal 6 │ Kanal 7 │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  ADC   │  ADC1   │    -    │    -    │    -    │    -    │    -    │    -    │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  SPI   │    -    │ SPI1_RX │ SP1_TX  │ SPI2_RX │ SPI2_TX │    -    │    -    │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  USART │    -    │USART3_TX│USART3_RX│USART1_TX│USART1_RX│USART2_RX│USART2_TX│
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  I2C   │    -    │    -    │    -    │ I2C2_TX │ I2C2_RX │ I2C1_TX │ I2C1_RX │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │        │         │         │         │TIM1_CH4 │         │         │         │
   │  TIM1  │    -    │TIM1_CH1 │TIM1_CH2 │TIM1_TRIG│ TIM1_UP │TIM1_CH3 │    -    │
   │        │         │         │         │TIM1_COM │         │         │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM2  │TIM2_CH3 │TIM2_UP  │    -    │    -    │TIM2_CH1 │    -    │TIM2_CH2 │
   │        │         │         │         │         │         │         │TIM2_CH4 │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM3  │    -    │TIM3_CH3 │TIM3_CH4 │    -    │    -    │TIM3_CH1 │    -    │
   │        │         │         │ TIM3_UP │         │         │TIM3_TRIG│         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM4  │TIM4_CH1 │    -    │    -    │TIM4_CH2 │TIM4_CH3 │    -    │ TIM4_UP │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM6  │    -    │    -    │ TIM6_UP │    -    │    -    │    -    │    -    │
   │ / DAC  │         │         │DAC_CH1**│         │         │         │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM7  │    -    │    -    │    -    │ TIM7_UP │    -    │    -    │    -    │
   │ / DAC  │         │         │         │DAC_CH2**│         │         │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │        │         │         │         │         │TIM15_CH1│         │         │
   │ TIM15  │    -    │    -    │    -    │    -    │,UP,TRIG,│    -    │    -    │
   │        │         │         │         │         │,COM     │         │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │ TIM16  │    -    │    -    │TIM16_CH1│    -    │    -    │TIM16_CH1│    -    │
   │        │         │         │TIM16_UP │         │         │,UP**    │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │ TIM17  │TIM17_CH1│    -    │    -    │    -    │    -    │    -    │TIM17_CH1│
   │        │TIM17_UP │         │         │         │         │         │,UP**    │
   └────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
   ** Der Peripheriebaustein wird auf diesen DMA Kanal nur dann abgebildet, wenn
      das entsprechende "Remapping-"Bit im SYSCFG_CFGR1 Register gesetzt wurde.
      Siehe Datenblatt: Section 12.1.1: SYSCFG configuration register 1 (SYSCFG_CFGR1) on page 245.


   µC-interne Kanal-Verbindungen von DMA2:
   ┌────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
   │ HWUnit │ Kanal 1 │ Kanal 2 │ Kanal 3 │ Kanal 4 │ Kanal 5 │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  ADC   │  ADC2   │  ADC4   │  ADC2** │  ADC4** │  ADC3   │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  SPI   │ SPI3_RX │ SPI3_TX │    -    │    -    │    -    │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  UART4 │    -    │    -    │UART4_RX │    -    │UART4_TX │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM6  │    -    │    -    │ TIM6_UP │    -    │    -    │
   │ / DAC  │         │         │ DAC_CH1 │         │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │  TIM7  │    -    │    -    │    -    │ TIM7_UP │    -    │
   │ / DAC  │         │         │         │ DAC_CH2 │         │
   ├────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
   │        │TIM8_CH3 │TIM8_CH4 │         │         │         │
   │  TIM8  │ TIM8_UP │TIM8_TRIG│TIM8_CH1 │    -    │TIM8_CH2 │
   │        │         │TIM8_COM │         │         │         │
   └────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
   ** Der Peripheriebaustein wird auf diesen DMA Kanal nur dann abgebildet, wenn
      das entsprechende "Remapping-"Bit im SYSCFG_CFGR1 Register gesetzt wurde.
      Siehe Datenblatt: Section 12.1.1: SYSCFG configuration register 1 (SYSCFG_CFGR1) on page 245.

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/dma.h
    Header file <Direct-Memory-Access>.

   file: TODO: µC/dma.c
    Implementation file <Direct-Memory-Access impl>.
*/
#ifndef STM32F303xC_MC_DMA_HEADER
#define STM32F303xC_MC_DMA_HEADER

// == exported Peripherals/HW-Units
#define DMA1   ((dma_t*)HW_BASEADDR_DMA1)
#define DMA2   ((dma_t*)HW_BASEADDR_DMA2)

// == exported types
struct dma_t;

typedef enum dma_bit_e {
   DMA1_BIT = 1,
   DMA2_BIT = 2,
} dma_bit_e;

typedef enum dma_channel_e {
   dma_channel_1 = 0,
   dma_channel_2 = 1,
   dma_channel_3 = 2,
   dma_channel_4 = 3,
   dma_channel_5 = 4,
   dma_channel_6 = 5,
   dma_channel_7 = 6,
} dma_channel_e;

/* Mapping of DMA channels to peripherals */

#define DMA2_CHANNEL_DAC1_CH1  dma_channel_3
#define DMA2_CHANNEL_DAC1_CH2  dma_channel_4

typedef enum dma_state_e {
   dma_state_ERROR    = 1 << 3,  // error occurred ==> channel disabled
   dma_state_HALF     = 1 << 2,  // half of the data items transfered
   dma_state_COMPLETE = 1 << 1,  // all data items transfered
} dma_state_e;

/* enums: dmacfg_e
 * Konfgiurationsflags für alle config_dma und config_YYY_dma Funktionen.
 * Aus den 4 definierten Abschnitten ist jeweils ein Flag auszuwählen.
 * Wird kein Flag ausgewählt ist das mit (default) gekennzeichnete Flag aktiv.
 * Die Flags des 5ten Abschnitt können beliebig dazuaddiert werden, wenn deren
 * Funktionalität gewünscht wird.
 *
 * Die Flags gültig für den Peripheriebaustein beginnen immer mit dmacfg_HW,
 * die für den Speicher gültigen Flags beginnen immer mit dmacfg_MEM.
 *
 * dmacfg_HW_8BITDATA   - Die Daten werden 8-Bit-Weise an die Peripherie-Hardware-Unit weitergereicht oder von ihr
 *                        gelesen. Der Schreibzugriff erfolgt intern immer mit 32 Bit und die nicht benutzten Bytes
 *                        werden alle auf denselben Wert gesetzt. Beim Schreiben wird der Wert des niederwertigsten
 *                        Bytes verwendet, falls vom Speicher 16- oder 32-bit-weise gelesen wird.
 * dmacfg_HW_16BITDATA  - Die Daten werden 16-Bit-Weise an die Peripherie-Hardware-Unit weitergereicht oder von ihr
 *                        gelesen. Der Schreibzugriff erfolgt intern immer mit 32 Bit und das nicht benutzte Halfword
 *                        wird auf denselben Wert gesetzt. Beim Schreiben werden nur die niederwertigen 16-Bits
 *                        verwendet, falls vom Speicher 32-bit-weise gelesen wird. Wird vom Speicher 8-bit-weise
 *                        gelesen, dann sind die Bits 15:8 auf 0 gesetzt.
 * dmacfg_HW_32BITDATA  - Die Daten werden 32-Bit-Weise an die Peripherie-Hardware-Unit weitergereicht oder von ihr
 *                        gelesen. Wird vom Speicher 8-bit-weise gelesen, dann sind beim HWUnitschreibzugriff die
 *                        Bits 31:8 auf 0 gesetzt. Wird 16-bit-weise gelesen, dann sind beim HWUnitschreibzugriff
 *                        die Bits 31:16 auf 0 gesetzt.
 * dmacfg_MEM_8BITDATA  - Die Daten werden 8-Bit-Weise an den Speicher (Ram oder Flash) weitergereicht oder von ihm
 *                        gelesen. Beim Schreiben wird nur das niederwertigste Byte verwendet, falls von der
 *                        HW-Unit 16- oder 32-bit-weise gelesen wird.
 * dmacfg_MEM_16BITDATA - Die Daten werden 16-bit-weise an den Speicher (Ram oder Flash) weitergereicht oder von ihm
 *                        gelesen. Beim Schreiben werden nur die niederwertigen 16-Bits verwendet, falls von der
 *                        HW-Unit 32-bit-weise gelesen wird. Wird von der HW-Unit 8-bit-weise gelesen, dann sind die
 *                        Bits 15:8 auf 0 gesetzt.
 * dmacfg_MEM_32BITDATA - Die Daten werden 32-Bit-Weise an den Speicher (Ram oder Flash) weitergereicht oder von ihm
 *                        gelesen. Wird von der HW-Unit 8-bit-weise gelesen, dann sind beim Speicherschreibzugriff die
 *                        Bits 31:8 auf 0 gesetzt. Wird 16-bit-weise gelesen, dann sind beim Speicherschreibzugriff die
 *                        Bits 31:16 auf 0 gesetzt.
 * dmacfg_PRIORITY_MAX  - Gibt einem Kanal die maximale Prioriät bezüglich der anderen Kanäle innerhalb eines Controllers.
 *                        Besitzen mehrere Kanäle maximale Priorät, wird der mit der kleineren Kanalnummer bevorzugt.
 * dmacfg_PRIORITY_HIGH - Gibt einem Kanal eine hohe Prioriät bezüglich der anderen Kanäle innerhalb eines Controllers.
 *                        Besitzen mehrere Kanäle hohe Priorät, wird der mit der kleineren Kanalnummer bevorzugt.
 *                        Diese Priorität ist geringer als dmacfg_PRIORITY_MAX.
 * dmacfg_PRIORITY_LOW  - Gibt einem Kanal eine niedrige Prioriät bezüglich der anderen Kanäle innerhalb eines Controllers.
 *                        Besitzen mehrere Kanäle niedrige Priorät, wird der mit der kleineren Kanalnummer bevorzugt.
 *                        Diese Priorität ist geringer als dmacfg_PRIORITY_HIGH.
 * dmacfg_PRIORITY_MIN  - Gibt einem Kanal die minimale Prioriät bezüglich der anderen Kanäle innerhalb eines Controllers.
 *                        Besitzen mehrere Kanäle minimale Priorät, wird der mit der kleineren Kanalnummer bevorzugt.
 *                        Diese Priorität ist geringer als dmacfg_PRIORITY_LOW.
 * dmacfg_MEM_READ      - Greife lesend auf den Speicher zu und schreibend auf die HW-Peripherie-Unit.
 *                        Dieses Flag wird bei Aufruf der Funktionen config_flash_dma, config_copy_dma und config_copyflash_dma
 *                        automatisch gesetzt.
 * dmacfg_MEM_WRITE     - Greife schreibend auf den Speicher zu und lesend auf die HW-Peripherie-Unit.
 * dmacfg_NOTRIGGER     - Ein weiterer Datentransfer zwischen Speicher und HW-Peripherie-Baustein geschieht nur dann,
 *                        wenn dieser von der Peripherie angefordert wird. Deshalb muss beim Peripheriebaustein
 *                        auch die DMA-Unterstützung zusätzlich eingeschaltet sein. Soll der DMA-Controller ohne diese
 *                        Synchronisation arbeiten, dann muss diese Flag gesetzt werden. Die Datentransfers erfolgen
 *                        so schnell nacheinander, wie es die Taktrate und die Belastung des Datenbusses erlauben.
 *                        Falls der Adressparameter "void *hwunit" auf Speicher zeigt und auf keine HW-Unit mit
 *                        DMA-Unterstützung, dann muss dieses Flag zwingend gesetzt werden, da der DMA-Controller sonst
 *                        vergeblich auf ein externes Signal (Trigger) warten würde.
 *                        Dieses Flag wird bei Aufruf der Funktionen config_copy_dma und config_copyflash_dma
 *                        automatisch gesetzt.
 * dmacfg_LOOP          - Nachdem alle Daten kopiert wurden, der Zählerstand also 0 erreicht hat (counter_dma()==0),
 *                        wird dieser wieder auf seinen Initialisierungswert gesetzt und die möglicherweise inkrementerten
 *                        Hardwareaddressen auch wieder auf ihren Startwert zurückgesetzt. Der Kopiervorgang startet von
 *                        Neuem. disable_dma muss aufgerufen werden, um die Transaktion zu beenden. Ohne dieses Flag
 *                        verbleibt am Ende einer Transkation der Zählerstand bei 0 und – obwohl inaktiv -
 *                        verbleibt der DMA-Kanal weiterhin angeschaltet.
 * dmacfg_MEM_INCRADDR  - Nach jeder erfolgreichen Datentransaktion wird die Speicheradresse (Parameter memory bzw. flashmem)
 *                        um ein, zwei oder 4 Byte erhöht korrespondierend zu
 *                        dmacfg_MEM_8BITDATA, dmacfg_MEM_16BITDATA bzw. dmacfg_MEM_32BITDATA.
 *                        Dieses Flag wird bei Aufruf der Funktionen config_copy_dma und config_copyflash_dma
 *                        automatisch gesetzt.
 * dmacfg_HW_INCRADDR   - Nach jeder erfolgreichen Datentransaktion wird die Hardwarunitadresse (Parameter hwunit)
 *                        um ein, zwei oder 4 Byte erhöht korrespondierend zu
 *                        dmacfg_HW_8BITDATA, dmacfg_HW_16BITDATA bzw. dmacfg_HW_32BITDATA.
 *                        Dieses Flag wird bei Aufruf der Funktionen config_copy_dma und config_copyflash_dma
 *                        automatisch gesetzt.
 * dmacfg_ENABLE        - Schaltet bei Aufruf einer der Funktionen config_dma, config_flash_dma, config_copy_dma
 *                        und config_copyflash_dma den DMA-Kanal auch aktiv. Ansonsten muss nach der Konfiguration
 *                        enable_dma für das Starten der Transaktion aufgerufen werden.
 * dmacfg_INTERRUPT_ERROR    - Ein Interrupt wird ausgelöst, wenn während der Übertragung ein Fehler auftrat.
 *                             Der DMA-Kanal schaltet sich bei enem Fehler autoamtisch ab.
 * dmacfg_INTERRUPT_HALF     - Ein Interrupt wird ausgelöst, wenn die Hälfte der Datenwörter übertragen wurde.
 * dmacfg_INTERRUPT_COMPLETE - Ein Interrupt wird ausgelöst, wenn alle Datenwörter einer Transaktion übertragen wurden.
 * dmacfg_INTERRUPT          - Schaltet alle drei vorher definierten Interrupts zusammen an.
 */
typedef enum dmacfg_e {
   // -- 1. choose one of --
   dmacfg_HW_8BITDATA  = 0u << 8,   // (default)
   dmacfg_HW_16BITDATA = 1u << 8,
   dmacfg_HW_32BITDATA = 2u << 8,   // size of transfered data item from/to peripheral hwunit is 32Bit
   // -- 2. choose one of --
   dmacfg_MEM_8BITDATA  = 0u << 10, // (default)
   dmacfg_MEM_16BITDATA = 1u << 10,
   dmacfg_MEM_32BITDATA = 2u << 10, // size of transfered data item from/to memory is 32Bit
   // -- 3. choose one of --
   dmacfg_PRIORITY_MAX = 3u << 12,
   dmacfg_PRIORITY_HIGH = 2u << 12,
   dmacfg_PRIORITY_LOW = 1u << 12,
   dmacfg_PRIORITY_MIN = 0u << 12,  // (default)
   // -- 4. choose one of --
   dmacfg_MEM_READ  = 1u << 4,
   dmacfg_MEM_WRITE = 0u << 4,      // (default)
   // -- 5. additonal flags ored to others. Default is disabled --
   dmacfg_NOTRIGGER    = 1u << 14,  // no trigger from peripheral hwunit needed, copy data items as fast as possible after channel is enabled
                                    // useful for memory to memory copy where there is no peripheral trigger
   dmacfg_LOOP         = 1u << 5,   // at end of transfer restart from beginning (makes no sense for mem-to-mem copy but is supported)
   dmacfg_MEM_INCRADDR = 1u << 7,
   dmacfg_HW_INCRADDR  = 1u << 6,
   dmacfg_ENABLE       = 1 << 0,
   dmacfg_INTERRUPT_ERROR    = 1 << 3,
   dmacfg_INTERRUPT_HALF     = 1 << 2,
   dmacfg_INTERRUPT_COMPLETE = 1 << 1,
   dmacfg_INTERRUPT          = dmacfg_INTERRUPT_ERROR | dmacfg_INTERRUPT_HALF | dmacfg_INTERRUPT_COMPLETE,
} dmacfg_e;


// == exported functions

static inline int config_dma(volatile struct dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, volatile void *memory/*_MEM_*/, uint16_t datacount, dmacfg_e config);
static inline int config_flash_dma(volatile struct dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, const void *flashmem/*_MEM_*/, uint16_t datacount, dmacfg_e config);
static inline int config_copy_dma(volatile struct dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, volatile const void *memory/*_MEM_*/, uint16_t datacount, dmacfg_e config);
static inline int config_copyflash_dma(volatile struct dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, const void *flashmem/*_MEM_*/, uint16_t datacount, dmacfg_e config);
static inline int isenabled_dma(const volatile struct dma_t *dma, dma_channel_e channel/*DMA1:0..6/DMA2:0..4*/);
static inline int enable_dma(volatile struct dma_t *dma, dma_channel_e channel/*DMA1:0..6/DMA2:0..4*/);
static inline int disable_dma(volatile struct dma_t *dma, dma_channel_e channel/*DMA1:0..6/DMA2:0..4*/);
static inline int enable_interrupt_dma(volatile struct dma_t *dma, dma_channel_e channel, dmacfg_e config/*masked with dmacfg_INTERRUPT*/);
static inline int disable_interrupt_dma(volatile struct dma_t *dma, dma_channel_e channel, dmacfg_e config/*masked with dmacfg_INTERRUPT*/);
static inline uint32_t counter_dma(const volatile struct dma_t *dma, dma_channel_e channel);
static inline dma_state_e state_dma(const volatile struct dma_t *dma, dma_channel_e channel);
static inline void clearstate_dma(volatile struct dma_t *dma, dma_channel_e channel, dma_state_e state);


// == definitions

/* struct: dma_t
 * Definiert die Register der integrierten DMA-Hardwareunit.
 *
 * Invariant:
 * Reserved bits must be kept at reset value (for all register). */
typedef volatile struct dma_t {
   /* DMA interrupt status register (DMA_ISR); Address offset: 0x00; Reset value: 0x00000000 */
   uint32_t    isr;
   /* DMA interrupt flag clear register (DMA_IFCR); Address offset: 0x04; Reset value: 0x00000000
    * Write 1 to bit position to clear flag in isr.
    * GIFx: Channel x global interrupt clear write 1: Clears the GIF, TEIF, HTIF and TCIF flags in the DMA_ISR register */
   uint32_t    ifcr;
   struct {
      /* DMA channel x configuration register (DMA_CCRx) (x = 1..7 , where x = channel number)
       * Address offset: 0x08 + 20 × (channel number – 1); Reset value: 0x00000000
       * All bits are set by software. */
      uint32_t    ccr;
      /* DMA channel x number of data register (DMA_CNDTRx) (x = 1..7, where x = channel number)
       * Address offset: 0x0C + 20 × (channel number – 1); Reset value: 0x00000000
       * Bits 15:0 NDT[15:0]: Number of data to be transferred (0 up to 65535).
       * This register can only be written when the channel is disabled.
       * This register is decremented by one every time a data item is transfered. */
      uint32_t    cndtr;
      /* DMA channel x peripheral address register (DMA_CPARx) (x = 1..7, where x = channel number)
       * Address offset: 0x10 + 20 × (channel number – 1); Reset value: 0x00000000
       * This register must not be written when the channel is enabled.
       * Bits 31:0 PA[31:0]: Peripheral address
       * Base address of the peripheral data register from/to which the data will be read/written.
       * Access is automatically aligned to a half-word or word address (PA[0] or PA[1:0] ignored).
       * This register keeps its configured initial value during a transaction. */
      uint32_t    cpar;
      /* DMA channel x memory address register (DMA_CMARx) (x = 1..7, where x = channel number)
       * Address offset: 0x14 + 20 × (channel number – 1); Reset value: 0x00000000
       * Bits 31:0 MA[31:0]: Memory address
       * Base address of the memory area from/to which the data will be read/written.
       * Access is automatically aligned to a half-word or word address (MA[0] or MA[1:0] ignored).
       * This register keeps its configured initial value during a transaction. */
      uint32_t    cmar;
      uint32_t    _reserved1;
   } channel[7];
} dma_t;


// == Bit Definitionen

// DMA interrupt status register (DMA_ISR)
// Bits per channel. 3:0 first channel, 7:4 2nd channel, ...
#define HW_BIT_DMA_ISR_CHANNEL_BITS       0x4
// Number of channels in the ISR register 1..7
#define HW_BIT_DMA_ISR_CHANNEL_NROF       0x7
// All bits in one channel
#define HW_BIT_DMA_ISR_CHANNEL_MASK       0xf
// Channel 1 transfer error flag (set hw, cleared by sw with DMA_IFCR), 1: A transfer error (TE) occurred on channel 1
#define HW_BIT_DMA_ISR_TEIF1              (1u << 3)
// Channel 1 half transfer flag (set hw, cleared by sw with DMA_IFCR), 1: A half transfer (HT) event occurred on channel 1
#define HW_BIT_DMA_ISR_HTIF1              (1u << 2)
// Channel 1 transfer complete flag (set hw, cleared by sw with DMA_IFCR), 1: A transfer complete (TC) event occurred on channel 1
#define HW_BIT_DMA_ISR_TCIF1              (1u << 1)
// Channel 1 global interrupt flag (set hw, cleared by sw with DMA_IFCR), 1: A TE, HT or TC event occurred on channel 1
#define HW_BIT_DMA_ISR_GIF1               (1u << 0)

// DMA channel configuration register (DMA_CCR)
// Bit 14 MEM2MEM: Memory to memory mode
#define HW_BIT_DMA_CCR_MEM2MEM            (1u << 14)
// Bits 13:12 PL[1:0]: Channel priority level, 00: Low, 01: Medium, 10: High, 11: Very high
#define HW_BIT_DMA_CCR_PL_POS             12u
#define HW_BIT_DMA_CCR_PL_BITS            0x3
#define HW_BIT_DMA_CCR_PL_MASK            HW_REGISTER_MASK(DMA, CCR, PL)
// Bits 11:10 MSIZE[1:0]: Memory size, 00: 8-bits, 01: 16-bits, 10: 32-bits, 11: Reserved
#define HW_BIT_DMA_CCR_MSIZE_POS          10u
#define HW_BIT_DMA_CCR_MSIZE_BITS         0x3
#define HW_BIT_DMA_CCR_MSIZE_MASK         HW_REGISTER_MASK(DMA, CCR, MSIZE)
// Bits 9:8 PSIZE[1:0]: Peripheral size, 00: 8-bits, 01: 16-bits, 10: 32-bits, 11: Reserved
#define HW_BIT_DMA_CCR_PSIZE_POS          8u
#define HW_BIT_DMA_CCR_PSIZE_BITS         0x3
#define HW_BIT_DMA_CCR_PSIZE_MASK         HW_REGISTER_MASK(DMA, CCR, PSIZE)
// Bit 7 MINC: Memory increment mode, 1: Memory increment mode enabled
#define HW_BIT_DMA_CCR_MINC               (1u << 7)
// Bit 6 PINC: Peripheral increment mode, 1: Peripheral increment mode enabled
#define HW_BIT_DMA_CCR_PINC               (1u << 6)
// Bit 5 CIRC: Circular mode, 1: Circular mode enabled
#define HW_BIT_DMA_CCR_CIRC               (1u << 5)
// Bit 4 DIR: Data transfer direction, 0: Read from peripheral, 1: Read from memory
#define HW_BIT_DMA_CCR_DIR                (1u << 4)
// Bit 3 TEIE: Transfer error interrupt enable, 1: TE interrupt enabled
#define HW_BIT_DMA_CCR_TEIE               (1u << 3)
// Bit 2 HTIE: Half transfer interrupt enable, 1: HT interrupt enabled
#define HW_BIT_DMA_CCR_HTIE               (1u << 2)
// Bit 1 TCIE: Transfer complete interrupt enable, 1: TC interrupt enabled
#define HW_BIT_DMA_CCR_TCIE               (1u << 1)
// Bit 0 EN: Channel enable, 1: Channel enabled
#define HW_BIT_DMA_CCR_EN                 (1u << 0)


// section: inline implementation

static inline void assert_dmabits_dma(void)
{
   // DMAX_BIT can be used for enable_dma_clockcntrl
   static_assert(HW_BIT_RCC_AHBENR_DMA1EN == DMA1_BIT);
   static_assert(HW_BIT_RCC_AHBENR_DMA2EN == DMA2_BIT);

   static_assert(dma_state_ERROR    == HW_BIT_DMA_ISR_TEIF1);
   static_assert(dma_state_HALF     == HW_BIT_DMA_ISR_HTIF1);
   static_assert(dma_state_COMPLETE == HW_BIT_DMA_ISR_TCIF1);
}

static inline void assert_offsets_dma(void)
{
      static_assert(offsetof(dma_t, isr)   == 0x00);
      static_assert(offsetof(dma_t, ifcr)  == 0x04);
      static_assert(offsetof(dma_t, channel[0].ccr)   == 0x08);
      static_assert(offsetof(dma_t, channel[0].cndtr) == 0x0c);
      static_assert(offsetof(dma_t, channel[0].cpar)  == 0x10);
      static_assert(offsetof(dma_t, channel[0].cmar)  == 0x14);
      static_assert(offsetof(dma_t, channel[1].ccr)   == 0x08+1*20);
      static_assert(offsetof(dma_t, channel[1].cndtr) == 0x0c+1*20);
      static_assert(offsetof(dma_t, channel[1].cpar)  == 0x10+1*20);
      static_assert(offsetof(dma_t, channel[1].cmar)  == 0x14+1*20);
      static_assert(offsetof(dma_t, channel[6].ccr)   == 0x08+6*20);
      static_assert(offsetof(dma_t, channel[6].cndtr) == 0x0c+6*20);
      static_assert(offsetof(dma_t, channel[6].cpar)  == 0x10+6*20);
      static_assert(offsetof(dma_t, channel[6].cmar)  == 0x14+6*20);
}

static inline void assert_dmcfg_dma(void)
{
   static_assert(dmacfg_PRIORITY_MAX == 3 << HW_BIT_DMA_CCR_PL_POS);
   static_assert(dmacfg_PRIORITY_HIGH == 2 << HW_BIT_DMA_CCR_PL_POS);
   static_assert(dmacfg_PRIORITY_LOW == 1 << HW_BIT_DMA_CCR_PL_POS);
   static_assert(dmacfg_PRIORITY_MIN == 0 << HW_BIT_DMA_CCR_PL_POS);
   static_assert(dmacfg_MEM_8BITDATA  == 0 << HW_BIT_DMA_CCR_MSIZE_POS);
   static_assert(dmacfg_MEM_16BITDATA == 1 << HW_BIT_DMA_CCR_MSIZE_POS);
   static_assert(dmacfg_MEM_32BITDATA == 2 << HW_BIT_DMA_CCR_MSIZE_POS);
   static_assert(dmacfg_HW_8BITDATA  == 0 << HW_BIT_DMA_CCR_PSIZE_POS);
   static_assert(dmacfg_HW_16BITDATA == 1 << HW_BIT_DMA_CCR_PSIZE_POS);
   static_assert(dmacfg_HW_32BITDATA == 2 << HW_BIT_DMA_CCR_PSIZE_POS);
   static_assert(dmacfg_NOTRIGGER          == HW_BIT_DMA_CCR_MEM2MEM);
   static_assert(dmacfg_MEM_INCRADDR == HW_BIT_DMA_CCR_MINC);
   static_assert(dmacfg_HW_INCRADDR  == HW_BIT_DMA_CCR_PINC);
   static_assert(dmacfg_LOOP      == HW_BIT_DMA_CCR_CIRC);
   static_assert(dmacfg_MEM_READ  == HW_BIT_DMA_CCR_DIR);
   static_assert(dmacfg_MEM_WRITE == 0);
   static_assert(dmacfg_ENABLE == HW_BIT_DMA_CCR_EN);
   static_assert(dmacfg_INTERRUPT_ERROR    == HW_BIT_DMA_CCR_TEIE);
   static_assert(dmacfg_INTERRUPT_HALF     == HW_BIT_DMA_CCR_HTIE);
   static_assert(dmacfg_INTERRUPT_COMPLETE == HW_BIT_DMA_CCR_TCIE);
}

#define VALIDATE_CHANNEL_DMA(dma, channel, return_code) \
         if (channel > dma_channel_5) {                     \
            if (channel > dma_channel_7 || dma == DMA2) {   \
               return return_code;                          \
            }                                               \
         }

/* Schaltet den Kanal ab und konfiguriert ihn neu.
 * Alle state-Flags (dma_state_YYY) werden auf 0 zurückgesetzt.
 *
 * Der Parameter hwunit mit der ersten Speicheradresse muss auf das Peripheriemodul (HW-Unit) zeigen,
 * das den DMA-Trigger setzt. Die Kanalnummer muss zu diesem Modul passend gewählt worden sein.
 * Die Konfigurationsflags, welche für diese Speicheradresse spezifisch sind, beginnen alle mit dmacfg_HW_.
 *
 * Der Parameter memory mit der zweiten Speicheradresse kann Speicher oder ein anderes Peripheriemodul zeigen,
 * das aber keinen DMA-Trigger setzt. Die Kanalnummer muss sich nicht nach dieser Adresse richten.
 * Die Konfigurationsflags, welche für diese Speicheradresse spezifisch sind, beginnen alle mit dmacfg_MEM_.
 *
 * Falls das Flag dmacfg_ENABLE angegeben wurde, wird der Kanal vor Bendigung dieser Funktion angeschaltet.
 * Wurde diese Flag nicht angegeben, muss zu einem späteren Zeitpunkt enable_dma aufgerufen werden.
 *
 * Der Kanal sollte vor der HW-Unit konfiguriert und eingeschaltet werden.
 * Die HW-Unit sollte solange deaktiviert sein, damit kein unabsichtlicher Trigger gesendet wird.
 *
 * Mit dem Parametr datacount werden die Anazhl an Datenwörtern, die kopiert werden sollen, festgelegt.
 * Die Quelle und das Ziel können unterschiedliche Wortgrössen definieren, von 8 über 16 bis zu 32 Bits.
 * Die übertragenen Datenwörter werden entweder abgeschnitten wie bei einem C-Cast ((uint8_t)0xabcd==0xcd)
 * oder einfach mit 0en erweitert wie bei der Integer-Promotion in C ((uint32_t)0xab == 0x000000ab). */
static inline int config_dma(dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, volatile void *memory/*_MEM_*/, uint16_t datacount, dmacfg_e config)
{
   VALIDATE_CHANNEL_DMA(dma, channel, EINVAL);

   // disable first ==> allows to change configuration
   dma->channel[channel].ccr &= ~HW_BIT_DMA_CCR_EN;

   // clear all interrupt flags for this channel
   dma->ifcr = HW_BIT_DMA_ISR_CHANNEL_MASK << (channel * HW_BIT_DMA_ISR_CHANNEL_BITS);

   dma->channel[channel].cndtr = datacount;
   dma->channel[channel].cpar = (uintptr_t) hwunit;
   dma->channel[channel].cmar = (uintptr_t) memory;
   dma->channel[channel].ccr  = config;
   return 0;
}

/* Hat dieselben Parameter wie config_dma und verhält sich auch so.
 * Mit dem Unterschied, dass diese Funktion noch einige Konfigurationsflags dazuaddiert.
 * Nämlich: dmacfg_MEM_READ, dmacfg_NOTRIGGER, dmacfg_HW_INCRADDR, dmacfg_MEM_INCRADDR.
 * Das bedeutet, dass diese Funktion auf memory lesend zugreift, weshalb auch ein const
 * davorsteht, und sie schreibt nach hwunit.
 *
 * Die Adresse hwunit sollte auch auf eine Speicheradresse ohne DMA-Trigger Unterstützung
 * zeigen, da zusätzlich das Flag dmacfg_NOTRIGGER gesetzt wurde. Nach dem Anschalten des
 * Kanals kopiert der DMA-Controller datacount Wörter von memory nach hwunit ohne auf einen
 * externen Trigger zur Synchronisation zu warten.
 *
 * Die Flags dmacfg_HW_INCRADDR und dmacfg_MEM_INCRADDR sorgen dafür,
 * dass nach jedem Kopiervorgang beide Adressen, sowohl hwunit als auch memory um die
 * jeweilige Wortgrösse (kann für jede Adresse unterschiedlich sein), inkrementiert wird. */
static inline int config_copy_dma(dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, volatile const void *memory/*_MEM_*/, uint16_t datacount, dmacfg_e config)
{
   return config_dma(dma, channel, hwunit, (void*)(uintptr_t)memory, datacount, config|(dmacfg_MEM_READ|dmacfg_NOTRIGGER|dmacfg_HW_INCRADDR|dmacfg_MEM_INCRADDR));
}

/* Hat dieselben Parameter wie config_dma und verhält sich auch so.
 * Mit dem Unterschied, dass diese Funktion noch das Flag dmacfg_MEM_READ zu config dazuaddiert und
 * die Adresse flashmem anpasst.
 *
 * Das bedeutet, dass diese Funktion auf flashmem lesend zugreift, weshalb auch ein const
 * davorsteht, und sie schreibt nach hwunit.
 *
 * Es wird angenommen, dass flashmem auf die ersten 256K des Speuichers zeigt (0..256K).
 * Intern wird auf diese Adresse der Wert 0x08000000 (HW_MEMORYREGION_MAINFLASH_START) addiert.
 * Dies dient dazu, dass der DMA-Controller nur auf die Flash-Speicherbank über ihre normale
 * Adresse zugreifen kann und nicht auf die ab Adresse 0 eingeblendete.
 *
 * Es ist auch möglich, ab Adresse 0 ein Bootrom einzublenden.
 */
static inline int config_flash_dma(dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, const void *flashmem/*_MEM_*/, uint16_t datacount, dmacfg_e config)
{
   // base memory of internal flash rom 0x08000000
   // flash memory is mapped into addresses space beginning from 0x00
   // but dma could only access flash rom from 0x08000000
   return config_dma(dma, channel, hwunit, (uint8_t*)HW_MEMORYREGION_MAINFLASH_START + (uintptr_t)flashmem, datacount, config|dmacfg_MEM_READ);
}

/* Diese Funktion verhält sich wie config_copy_dma mit dem Unterschied,
 * dass die Speicheradresse flashmem wie in der Funktion config_flash_dma
 * angepasst wird. Siehe dazu die Beschreibungen der beiden anderen Funktionen. */
static inline int config_copyflash_dma(dma_t *dma, dma_channel_e channel, volatile void *hwunit/*_HW_*/, const void *flashmem/*_MEM_*/, uint16_t datacount, dmacfg_e config)
{
   return config_dma(dma, channel, hwunit, (uint8_t*)HW_MEMORYREGION_MAINFLASH_START + (uintptr_t)flashmem, datacount, config|(dmacfg_MEM_READ|dmacfg_NOTRIGGER|dmacfg_HW_INCRADDR|dmacfg_MEM_INCRADDR));
}

/* Gibt 1 zurück, wenn der Kanal eingeschaltet ist.
 * Der Kanal ist aber nur noch dann aktiv, wenn counter_dma einen Wert > 0 zurückliefert. */
static inline int isenabled_dma(const dma_t *dma, dma_channel_e channel)
{
   VALIDATE_CHANNEL_DMA(dma, channel, 0);
   return (dma->channel[channel].ccr & HW_BIT_DMA_CCR_EN) / HW_BIT_DMA_CCR_EN;
}

/* Schaltet einen DMA-Kanal ein.
 * Wurde der Kanal vorher konfiguriert, wird diese Transaktion ausgeführt.
 * Wurde der Kanal vorher mit disable_dma ausgeschaltet, wird nach dem Anschalten
 * an der Unterbrochenen Stelle weitergemacht. */
static inline int enable_dma(dma_t *dma, dma_channel_e channel)
{
   VALIDATE_CHANNEL_DMA(dma, channel, EINVAL);

   dma->channel[channel].ccr |= HW_BIT_DMA_CCR_EN;

   return 0;
}

/* Deaktiviert den Kanal.
 * Nach dem Deaktivieren des Kanals bleiben die konfigurierten Werte und
 * der aktuelle Zählerstand weiterhin erhalten.
 * Ein erneuter aktivieren mit enable_dma setzt die Transaktion an der
 * unterbrochenen Stelle fort. */
static inline int disable_dma(dma_t *dma, dma_channel_e channel)
{
   VALIDATE_CHANNEL_DMA(dma, channel, EINVAL);

   dma->channel[channel].ccr &= ~HW_BIT_DMA_CCR_EN;

   return 0;
}

/* Schaltet die in config aufgelisteten Interrupts an.
 * Ist config == dmacfg_INTERRUPT werden alle Interrupts angeschaltet.
 * Wenn config andere Konfigurationsflags als die Interrupts betreffend enthält,
 * werden diese ignoriert. */
static inline int enable_interrupt_dma(dma_t *dma, dma_channel_e channel, dmacfg_e config/*masked with dmacfg_INTERRUPT*/)
{
   VALIDATE_CHANNEL_DMA(dma, channel, EINVAL);

   dma->channel[channel].ccr |= (config & dmacfg_INTERRUPT);

   return 0;
}

/* Schaltet die in config aufgelisteten Interrupts ab.
 * Ist config == dmacfg_INTERRUPT werden alle Interrupts ausgeschaltet.
 * Wenn config andere Konfigurationsflags als die Interrupts betreffend enthält,
 * werden diese ignoriert. */
static inline int disable_interrupt_dma(dma_t *dma, dma_channel_e channel, dmacfg_e config/*masked with dmacfg_INTERRUPT*/)
{
   VALIDATE_CHANNEL_DMA(dma, channel, EINVAL);

   dma->channel[channel].ccr &= ~(config & dmacfg_INTERRUPT);

   return 0;
}

/* Gibt die noch zu kopierende Anzahl an Datenwerten zurück.
 * Ein Datenwort kann 8,16 oder 32-Bits umfassen.
 * Ist der zurückgegebene Wert 0, dann ist der Kanal nicht mehr aktiv.
 * Ist das Konfigurationsflag dmacfg_LOOP gesetzt, dann wird dieser
 * Wert wieder auf seinen ursprünglichen zurückgesetzt und die Transaktion
 * startet wieder von vorne. */
static inline uint32_t counter_dma(const dma_t *dma, dma_channel_e channel)
{
   VALIDATE_CHANNEL_DMA(dma, channel, 0);

   return dma->channel[channel].cndtr; // contains nr of data items waiting for transfer
}

/* Liefert den Kanalzustand zurück. Mehr als ein Flag kann aktiv sein.
 * Ein gesetztes dma_state_ERROR Flag zeigt an, dass der Kanal abgeschaltet wurde. */
static inline dma_state_e state_dma(const dma_t *dma, dma_channel_e channel)
{
   return (dma->isr >> (channel * HW_BIT_DMA_ISR_CHANNEL_BITS)) & (HW_BIT_DMA_ISR_CHANNEL_MASK-HW_BIT_DMA_ISR_GIF1);
}

/* Setzt die in state aufgeführten Zustandflags zurück auf 0.
 * Interruptimplementierungen müssen diese Funktion aufrufen,
 * damit sie nicht in einer Endlossschleife immer und immer wieder aufgerufen werden.
 * Es sollten immer nur die Flags zurückgesetzt werden, die behandelt wurden.
 * Sonst könnte während des Zurücksetzens aller Flags ein weiteres Flag gesetzt werden,
 * das aber gleich wieder zurückgesetzt wird und so vom Interrupt ignoriert würde. */
static inline void clearstate_dma(dma_t *dma, dma_channel_e channel, dma_state_e state)
{
   dma->ifcr = (uint32_t)state << (channel * HW_BIT_DMA_ISR_CHANNEL_BITS);
}

#undef VALIDATE_CHANNEL_DMA


#endif
