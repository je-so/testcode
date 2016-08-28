#include "konfig.h"

static int test_config(void);

static void set_led(uint32_t sampled_value)
{
   for (unsigned i = 0; i < 8; ++i) {
      if (sampled_value >= 4096-((i+1)*512)) {  // 12-Bit
      // if (sampled_value >= 1024-((i+1)*128)) {  // 10-Bit
      // if (sampled_value >= 256-((i+1)*32)) {    // 8-Bit
      // if (sampled_value >= 64-((i+1)*8)) {      // 6-Bit
         write_gpio(GPIOE, GPIO_PIN15>>i, GPIO_PINS(15,8));
         break;
      }
   }
}

/*
   Teste ADC1 Kanal 2 an Pin PA1 mit Potentiometer (50KΩ).

   Pinout:
   Pin PA0: User Switch
   Pin PA1: ADC1 Channel 2
   Pin PE[15:8]: User LEDs

   Zum Testen ist ein Potentiometer an 0V und 3.3V mit den beiden äußeren Pins anzuschließen.
   Der mittlere Pin des Potentiometers wird an PA1 zur Spannungsmessung angeschlossen.

   Der gelesene Wert wird grob auf eine Position der 8 User LEDs übertragen.

   Zum besseren Testen kann der Debugger eingesetzt werden.

   Dazu make clean; make flash; make debug.
   Dann "break read_adc" eingeben.
   Die Tasten (CTRL-X und 2) zwimal drücken zur Aktvieriung des Text-User-Interfaces.
   Der Befehl "cont" startet den Prozess und stoppt ihn dann in read_adc.
   Der Wert des Registers r0 zeigt den vom ADC gelesenen Wert (0x0 - 0xfff (4095)).

*/
int main(void)
{
   uint32_t start_time;
   uint32_t stop_time;
   uint32_t diff_time;

   enable_gpio_clockcntrl(GPIOA_BIT/*switch+adc*/|GPIOE_BIT/*led*/);
   enable_clock_adc(ADC1and2);

   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF); // switch
   // Wichtig: Zuerst IOPIN auf analog umschalten, damit keine parasitären Ströme fliessen
   config_analog_gpio(GPIOA, GPIO_PIN1);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));

   config_systick(8000000, systickcfg_CORECLOCK|systickcfg_START);

   // Initialisierungsequenz ADC

   enable_vreg_adc(ADC1);
   // Warte 10µs
   start_time = value_systick();
   for (;;) {
      stop_time = value_systick();
      diff_time = (start_time - stop_time) & 0xffffff;
      if (diff_time > 8*10/*10µs(8MHZ)*/) break;
   }
   if (calibrate_adc(ADC1)) goto ONERR;
   if (isenabled_adc(ADC1)) goto ONERR;
   if (getchannelmode_adc(ADC1, adc_chan_2) != adc_channelmode_SINGLEMODE) goto ONERR; // default Modus nach Reset
#if 0
   // Teste Differenz-Modus
   if (setchannelmode_adc(ADC1, adc_chan_2, adc_channelmode_DIFFMODE) != 0) goto ONERR;
   if (getchannelmode_adc(ADC1, adc_chan_2) != adc_channelmode_DIFFMODE) goto ONERR;
#endif

   // Vor der eigentlichen Konfiguration (bislang wurde nur die SPannungsversorgung eingeschaltet und kalibriert)
   // werden noch die config-Funktionen dahingehend getestet, ob die Registerm welche die Kanalnummern der Sequenzen
   // beschreiben, korrekt gesetzt wurden
   if (test_config()) goto ONERR;

   // Konfiguriert den Sequenzer auf einen Software-getriggerten Kanal und schaltet den ADC an
   config_single_adc(ADC1, adc_chan_2, adc_config_SAMPLETIME_601_5);
   if (! isenabled_adc(ADC1)) goto ONERR; // config schaltet ADC1 an

   // Teste zusätzlich die J-Sequenz
   if (config_jseq_adc(ADC1, 2, 2, (adc_chan_e[]){ adc_chan_2, adc_chan_2 }, adc_config_SAMPLETIME_601_5|adc_config_RESOLUTION_12BIT)) goto ONERR;
   if (lenjseq_adc(ADC1) != 2) goto ONERR;

   while (1) {

      // ===
      // === reguläre Sequenz

      // Starte Konvertierung (Software-Trigger)
      if (isstarted_adc(ADC1))  goto ONERR;  // Sequenz wurde noch nicht gestartet
      start_adc(ADC1);                       // Das ist das Startsignal für den SW-Trigger
      if (!isstarted_adc(ADC1)) goto ONERR;  // Sequenz ist gestartet
      // Busy-Wait, bis Daten konvertiert wurden
      while (!isdata_adc(ADC1)) ;
      if (! isdata_adc(ADC1))  goto ONERR;   // Flag ist gesetzt, dann kann Wert gelesen werden
      if (! iseos_adc(ADC1))   goto ONERR;   // End-of-Sequenz erreicht, da nur ein Kanal lang
      if (isstarted_adc(ADC1)) goto ONERR;   // Start-flag wurde am Ende der Wandlung auch zurückgesetzt
      clear_eos_adc(ADC1);                   // lösche Flag
      if (iseos_adc(ADC1))  goto ONERR;      // End-of-Sequenz Flag gelöscht
      if (! isdata_adc(ADC1))  goto ONERR;   // data-Flag ist immer noch gesetzt
      uint32_t data = read_adc(ADC1);        // Lese den Spannungs-Wert.
      if (isdata_adc(ADC1)) goto ONERR;      // Lesen des Wertes hat das Flag gelöscht
      if (isoverflow_adc(ADC1)) goto ONERR;  // Es trat kein Overflow auf

      // Bilde Wert auf eine der 8 LEDs ab
      set_led(data);

      // Teste Overflow/Overrun
      ADC1->cfgr &= ~HW_REGISTER_BIT_ADC_CFGR_AUTDLY;
      start_adc(ADC1);
      while (isstarted_adc(ADC1)) ;
      if (!isdata_adc(ADC1))    goto ONERR;  // Datenflag gesetzt !
      if (isoverflow_adc(ADC1)) goto ONERR;  // Es trat kein Overflow auf
      if (!iseos_adc(ADC1))     goto ONERR;  // End-of-Sequenz Flag gesetzt
      clear_eos_adc(ADC1);                   // Lösche EOS-Flag
      if (iseos_adc(ADC1))       goto ONERR; // End-of-Sequenz Flag gelöscht
      if (!isdata_adc(ADC1))    goto ONERR;  // Datenflag immer noch gesetzt !
      start_adc(ADC1);
      while (isstarted_adc(ADC1)) ;
      if (!isoverflow_adc(ADC1)) goto ONERR; // Es trat ein Overflow auf !
      clear_overflow_adc(ADC1);              // Lösche Overflow Flags
      if (isoverflow_adc(ADC1))  goto ONERR; // Overflow wurde gelöscht
      if (! iseos_adc(ADC1))     goto ONERR; // End-of-Sequenz noch gesetzt
      if (! isdata_adc(ADC1))    goto ONERR; // Datenflag noch gesetzt
      clear_flags_adc(ADC1);                 // Lösche alle Flags
      if (iseos_adc(ADC1))       goto ONERR; // End-of-Sequenz Flag gelöscht
      if (isdata_adc(ADC1))      goto ONERR; // Datenflag gelöscht

      // ===
      // === J-Sequenz

      uint32_t len = lenjseq_adc(ADC1);
      if (isjstarted_adc(ADC1))   goto ONERR;   // J-Sequenz wurde noch nicht gestartet
      startj_adc(ADC1);                         // Start J-Seq.
      if (! isjstarted_adc(ADC1)) goto ONERR;   // Sie wurde gestartet
      for (uint32_t i = 0; i < len; ++i) {
         while (! isjdata_adc(ADC1)) ;          // Warte bis Daten vorhanden sind
         if (i == 0) {
            if (isjeos_adc(ADC1) != 0) goto ONERR;  // Noch kein Ende der J-Sequenz erreicht
         } else if (i == len-1) {
            if (isjeos_adc(ADC1) != 1) goto ONERR;  // End of J-Sequenz Flag gesetzt
            clear_jeos_adc(ADC1);                   // lösche Flag
            if (isjeos_adc(ADC1) != 0) goto ONERR;  // End of J-Sequenz Flag gelöscht
         }
         if (! isjdata_adc(ADC1)) goto ONERR;   // Datenflag gesetzt
         data = readj_adc(ADC1, i);             // Lese Daten
         if (isjdata_adc(ADC1))   goto ONERR;   // Datenflag wurde gelöscht

         // Bilde Wert auf eine der 8 LEDs ab
         set_led(data);
      }
      // Die Differenz zweier direkt aufeinanderfolgender Samplings sollte nicht allzu groß sein
      uint32_t diff = readj_adc(ADC1, 0) > readj_adc(ADC1, 1) ? readj_adc(ADC1, 0) - readj_adc(ADC1, 1) : readj_adc(ADC1, 1) - readj_adc(ADC1, 0);
      if (diff > 200) goto ONERR;
   }

ONERR:
   write1_gpio(GPIOE, GPIO_PINS(15,8));
   while (1) ;
}

static int test_config_jseq(void)
{
   adc_chan_e jseq[4][4] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
      { 1, 2, 3, 4 },
      { 15, 14, 13, 12 },
      { 18, 1, 10, 8 },
   };

   // TEST config_jseq_adc

   for (uint32_t l = 1; l <= lengthof(jseq[0]); ++l) {
      for (uint32_t js = 0; js < lengthof(jseq); ++js) {
         adc_config_e config = (adc_config_e) (((l+1))|((js&adc_config_BITS_RESOLUTION)<<adc_config_POS_RESOLUTION));
         if (config_jseq_adc(ADC1, l, l, jseq[js], config)) goto ONERR;
         // check adc_config_SAMPLETIME_
         for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
            adc_config_e time = getsampletime_adc(ADC1, c);
            int ismatch = 0;
            for (uint32_t i = 0; i < l; ++i) {
               if (jseq[js][i] == c) {
                  ismatch = 1;
                  break;
               }
            }
            if (ismatch  && time != (l+1)) goto ONERR;
            if (!ismatch && time != 0) goto ONERR;
         }
         // reset adc_config_SAMPLETIME_
         ADC1->smpr1 = 0;
         ADC1->smpr2 = 0;
         // check ADC1->jsqr == jseq[js][0..l-1]
         uint32_t jsqr = (l-1);
         for (uint32_t i = 0; i < l; ++i) {
            jsqr += (jseq[js][i] << (8+6*i));
         }
         if (jsqr != ADC1->jsqr) goto ONERR;
         // check ADC1->cr
         if (isenabled_adc(ADC1) != 1) goto ONERR;
         if (isjstarted_adc(ADC1) != 0) goto ONERR;
         // check ADC1->cfgr
         uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_OVRMOD
                        |(((config&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
         uint32_t off = HW_REGISTER_BIT_ADC_CFGR_JAUTO|HW_REGISTER_BIT_ADC_CFGR_JQM|HW_REGISTER_BIT_ADC_CFGR_JDISCEN
                        |HW_REGISTER_BIT_ADC_CFGR_AUTDLY|HW_REGISTER_BIT_ADC_CFGR_ALIGN|HW_REGISTER_BIT_ADC_CFGR_DMAEN;
         if ((ADC1->cfgr != on) != 0) goto ONERR;
         // reset cfgr
         ADC1->cfgr = off;
      }
   }

   // TEST startj_adc
   if (config_jseq_adc(ADC1, 1, 1, jseq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   startj_adc(ADC1);
   if (isjstarted_adc(ADC1) != 1) goto ONERR;

   // TEST stopj_adc
   stopj_adc(ADC1);
   if ((ADC1->cr & HW_REGISTER_BIT_ADC_CR_JADSTP) != 0) goto ONERR;
   if (isjstarted_adc(ADC1) != 0) goto ONERR;
   if (isjdata_adc(ADC1) != 0) goto ONERR;
   if (isjeos_adc(ADC1) != 0) goto ONERR;

   // reset
   ADC1->cfgr = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_config_jpart(void)
{
   adc_chan_e jseq[4][4] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
      { 1, 2, 3, 4 },
      { 15, 14, 13, 12 },
      { 18, 1, 10, 8 },
   };

   // TEST config_jseq_adc: partioniert

   for (uint32_t l = 2; l <= lengthof(jseq[0]); ++l) {
      for (uint32_t js = 0; js < lengthof(jseq); ++js) {
         adc_config_e config = (adc_config_e) ((l+1) | ((js&adc_config_BITS_RESOLUTION)<<adc_config_POS_RESOLUTION));
         if (config_jseq_adc(ADC1, 1, l, jseq[js], config)) goto ONERR;
         // check adc_config_SAMPLETIME_
         for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
            adc_config_e time = getsampletime_adc(ADC1, c);
            int ismatch = 0;
            for (uint32_t i = 0; i < l; ++i) {
               if (jseq[js][i] == c) {
                  ismatch = 1;
                  break;
               }
            }
            if (ismatch  && time != (l+1)) goto ONERR;
            if (!ismatch && time != 0) goto ONERR;
         }
         // reset adc_config_SAMPLETIME_
         ADC1->smpr1 = 0;
         ADC1->smpr2 = 0;
         // check ADC1->jsqr == jseq[js][0..l-1]
         uint32_t jsqr = (l-1);
         for (uint32_t i = 0; i < l; ++i) {
            jsqr += (jseq[js][i] << (8+6*i));
         }
         if (jsqr != ADC1->jsqr) goto ONERR;
         // check ADC1->cr
         if (isenabled_adc(ADC1) != 1) goto ONERR;
         if (isjstarted_adc(ADC1) != 0) goto ONERR;
         // check ADC1->cfgr
         uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_OVRMOD|HW_REGISTER_BIT_ADC_CFGR_JDISCEN
                        |(((config&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
         uint32_t off = HW_REGISTER_BIT_ADC_CFGR_JAUTO|HW_REGISTER_BIT_ADC_CFGR_JQM|HW_REGISTER_BIT_ADC_CFGR_AUTDLY
                        |HW_REGISTER_BIT_ADC_CFGR_ALIGN|HW_REGISTER_BIT_ADC_CFGR_DMAEN;
         if ((ADC1->cfgr != on) != 0) goto ONERR;
         // reset cfgr
         ADC1->cfgr = off;
      }
   }

   // TEST startj_adc
   if (config_jseq_adc(ADC1, 1, 2, jseq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   startj_adc(ADC1);
   if (isjstarted_adc(ADC1) != 1) goto ONERR;

   // TEST stopj_adc
   stopj_adc(ADC1);
   if ((ADC1->cr & HW_REGISTER_BIT_ADC_CR_JADSTP) != 0) goto ONERR;
   if (isjstarted_adc(ADC1) != 0) goto ONERR;
   if (isjdata_adc(ADC1) != 0) goto ONERR;
   if (isjeos_adc(ADC1) != 0) goto ONERR;

   // TEST start_adc: Konvertiere 4 Mal den Kanal 2 zu Partionen von 1
   if (config_jseq_adc(ADC1, 1, 4, jseq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   startj_adc(ADC1);
   for (uint32_t i = 0, old = 0; i < 4; ++i) {
      if (i) {
         // Ende einer Partition ==> neuer Trigger erforderlich
         if (isjstarted_adc(ADC1)) goto ONERR;
         startj_adc(ADC1);
      }
      if (!isjstarted_adc(ADC1)) goto ONERR;
      if (isjeos_adc(ADC1)) goto ONERR;
      while (!isjdata_adc(ADC1)) ;
      uint32_t data = readj_adc(ADC1, i);
      uint32_t diff = !i ? 0 : old < data ? data - old : old - data;
      if (diff > 100) goto ONERR;
      old = data;
   }
   if (! isjeos_adc(ADC1))   goto ONERR;
   if (isoverflow_adc(ADC1)) goto ONERR;
   if (isjstarted_adc(ADC1)) goto ONERR;
   clear_jeos_adc(ADC1);

   // reset
   ADC1->cfgr = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_config_seq(void)
{
   const uint32_t MASK = (adc_config_MASK_SAMPLETIME|adc_config_MASK_RESOLUTION);
   adc_chan_e seq[4][16] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
      { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
      { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 },
      { 18, 1, 17, 2, 16, 3, 15, 4, 14, 5, 13, 6, 12, 7, 11, 8 },
   };

   // TEST config_seq_adc

   for (uint32_t l = 1; l <= lengthof(seq[0]); ++l) {
      for (uint32_t rs = 0; rs < lengthof(seq); ++rs) {
         if (config_seq_adc(ADC1, l, l, seq[rs], (adc_config_e)(l&MASK))) goto ONERR;
         // check adc_config_SAMPLETIME_
         for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
            adc_config_e time = getsampletime_adc(ADC1, c);
            int ismatch = 0;
            for (uint32_t i = 0; i < l; ++i) {
               if (seq[rs][i] == c) {
                  ismatch = 1;
                  break;
               }
            }
            if (ismatch  && time != (l&adc_config_MASK_SAMPLETIME)) goto ONERR;
            if (!ismatch && time != 0) goto ONERR;
         }
         // reset adc_config_SAMPLETIME_
         ADC1->smpr1 = 0;
         ADC1->smpr2 = 0;
         // check ADC1->sqr1..4 == seq[rs][0..l-1]
         uint32_t sqr = (l-1);
         volatile uint32_t *sqr1 = &ADC1->sqr1;
         for (uint32_t i = 0, pos = 1; i < 16; ++i) {
            sqr += i < l ? (seq[rs][i] << (6*pos)) : 0;
            if ((++pos) == 5 || i == 15) {
               if (sqr != *sqr1++) goto ONERR;
               pos = 0;
               sqr = 0;
            }
         }
         // reset ADC1->sqr1..4
         ADC1->sqr1 = 0;
         ADC1->sqr2 = 0;
         ADC1->sqr3 = 0;
         ADC1->sqr4 = 0;
         // check ADC1->cr
         if (isenabled_adc(ADC1) != 1) goto ONERR;
         if (isstarted_adc(ADC1) != 0) goto ONERR;
         // check ADC1->cfgr
         uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_OVRMOD
                        |(((l&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
         uint32_t off = HW_REGISTER_BIT_ADC_CFGR_DISCNUM_MASK|HW_REGISTER_BIT_ADC_CFGR_DISCEN|HW_REGISTER_BIT_ADC_CFGR_AUTDLY
                        |HW_REGISTER_BIT_ADC_CFGR_ALIGN|HW_REGISTER_BIT_ADC_CFGR_CONT|HW_REGISTER_BIT_ADC_CFGR_EXTEN_MASK
                        |HW_REGISTER_BIT_ADC_CFGR_EXTSEL_MASK|HW_REGISTER_BIT_ADC_CFGR_DMAEN;
         if ((ADC1->cfgr != on) != 0) goto ONERR;
         // reset cfgr
         ADC1->cfgr = off;
      }
   }

   // TEST start_adc
   if (config_seq_adc(ADC1, 1, 1, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   if (isstarted_adc(ADC1) != 1) goto ONERR;

   // TEST stop_adc
   stop_adc(ADC1);
   if ((ADC1->cr & HW_REGISTER_BIT_ADC_CR_ADSTP) != 0) goto ONERR;
   if (isstarted_adc(ADC1) != 0) goto ONERR;
   if (isdata_adc(ADC1) != 0) goto ONERR;
   if (iseos_adc(ADC1) != 0) goto ONERR;

   // TEST start_adc: Konvertiere 16 Mal den Kanal 2
   if (config_seq_adc(ADC1, 16, 16, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   for (uint32_t i = 0, old = 0; i < 16; ++i) {
      if (iseos_adc(ADC1)) goto ONERR;
      while (!isdata_adc(ADC1)) ;
      uint32_t data = read_adc(ADC1);
      uint32_t diff = !i ? 0 : old < data ? data - old : old - data;
      if (diff > 100) goto ONERR;
      old = data;
   }
   if (! iseos_adc(ADC1))    goto ONERR;
   if (isoverflow_adc(ADC1)) goto ONERR;
   if (isstarted_adc(ADC1))  goto ONERR;
   clear_eos_adc(ADC1);

   // reset
   ADC1->cfgr = 0;
   ADC1->sqr1 = 0;
   ADC1->sqr2 = 0;
   ADC1->sqr3 = 0;
   ADC1->sqr4 = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_config_cont(void)
{
   const uint32_t MASK = (adc_config_MASK_SAMPLETIME|adc_config_MASK_RESOLUTION);
   adc_chan_e seq[4][16] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
      { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
      { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 },
      { 18, 1, 17, 2, 16, 3, 15, 4, 14, 5, 13, 6, 12, 7, 11, 8 },
   };

   // TEST config_contseq_adc

   for (uint32_t l = 1; l <= lengthof(seq[0]); ++l) {
      for (uint32_t rs = 0; rs < lengthof(seq); ++rs) {
         if (config_contseq_adc(ADC1, l, seq[rs], (adc_config_e)(l&MASK))) goto ONERR;
         // check adc_config_SAMPLETIME_
         for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
            adc_config_e time = getsampletime_adc(ADC1, c);
            int ismatch = 0;
            for (uint32_t i = 0; i < l; ++i) {
               if (seq[rs][i] == c) {
                  ismatch = 1;
                  break;
               }
            }
            if (ismatch  && time != (l&adc_config_MASK_SAMPLETIME)) goto ONERR;
            if (!ismatch && time != 0) goto ONERR;
         }
         // reset adc_config_SAMPLETIME_
         ADC1->smpr1 = 0;
         ADC1->smpr2 = 0;
         // check ADC1->sqr1..4 == seq[rs][0..l-1]
         uint32_t sqr = (l-1);
         volatile uint32_t *sqr1 = &ADC1->sqr1;
         for (uint32_t i = 0, pos = 1; i < 16; ++i) {
            sqr += i < l ? (seq[rs][i] << (6*pos)) : 0;
            if ((++pos) == 5 || i == 15) {
               if (sqr != *sqr1++) goto ONERR;
               pos = 0;
               sqr = 0;
            }
         }
         // reset ADC1->sqr1..4
         ADC1->sqr1 = 0;
         ADC1->sqr2 = 0;
         ADC1->sqr3 = 0;
         ADC1->sqr4 = 0;
         // check ADC1->cr
         if (isenabled_adc(ADC1) != 1) goto ONERR;
         if (isstarted_adc(ADC1) != 0) goto ONERR;
         // check ADC1->cfgr
         uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_OVRMOD|HW_REGISTER_BIT_ADC_CFGR_CONT
                        |(((l&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
         uint32_t off = HW_REGISTER_BIT_ADC_CFGR_DISCNUM_MASK|HW_REGISTER_BIT_ADC_CFGR_DISCEN|HW_REGISTER_BIT_ADC_CFGR_AUTDLY
                        |HW_REGISTER_BIT_ADC_CFGR_ALIGN|HW_REGISTER_BIT_ADC_CFGR_EXTEN_MASK
                        |HW_REGISTER_BIT_ADC_CFGR_EXTSEL_MASK|HW_REGISTER_BIT_ADC_CFGR_DMAEN;
         if ((ADC1->cfgr != on) != 0) goto ONERR;
         // reset cfgr
         ADC1->cfgr = off;
      }
   }

   // TEST start_adc
   if (config_contseq_adc(ADC1, 1, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   if (isstarted_adc(ADC1) != 1) goto ONERR;

   // TEST stop_adc
   stop_adc(ADC1);
   if ((ADC1->cr & HW_REGISTER_BIT_ADC_CR_ADSTP) != 0) goto ONERR;
   if (isstarted_adc(ADC1) != 0) goto ONERR;
   if (isdata_adc(ADC1) != 0) goto ONERR;
   if (iseos_adc(ADC1) != 0) goto ONERR;

   // TEST start_adc: Konvertiere 2*16 Mal den Kanal 2
   if (config_contseq_adc(ADC1, 16, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   for (uint32_t i = 0, old = 0; i < 32; ++i) {
      if (i == 16) {
         // Wiederhole Sequenz
         if (! iseos_adc(ADC1))     goto ONERR;
         if (! isstarted_adc(ADC1)) goto ONERR;
         clear_eos_adc(ADC1);
      }
      if (iseos_adc(ADC1)) goto ONERR;
      while (!isdata_adc(ADC1)) ;
      uint32_t data = read_adc(ADC1);
      uint32_t diff = !i ? 0 : old < data ? data - old : old - data;
      if (diff > 100) goto ONERR;
      old = data;
   }
   if (! iseos_adc(ADC1))    goto ONERR;
   if (isoverflow_adc(ADC1)) goto ONERR;
   if (!isstarted_adc(ADC1)) goto ONERR;
   stop_adc(ADC1);
   if (isstarted_adc(ADC1))  goto ONERR;
   if (! iseos_adc(ADC1))    goto ONERR;
   clear_eos_adc(ADC1);

   // reset
   ADC1->cfgr = 0;
   ADC1->sqr1 = 0;
   ADC1->sqr2 = 0;
   ADC1->sqr3 = 0;
   ADC1->sqr4 = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_config_partioned(void)
{
   const uint32_t MASK = (adc_config_MASK_SAMPLETIME|adc_config_MASK_RESOLUTION);
   adc_chan_e seq[4][16] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
      { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
      { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 },
      { 18, 1, 17, 2, 16, 3, 15, 4, 14, 5, 13, 6, 12, 7, 11, 8 },
   };

   // TEST config_seq_adc: size_part != size_seq

   for (uint32_t l = 1; l <= lengthof(seq[0]); ++l) {
      for (uint32_t p = 1; p <= 8 && p < l; ++p) {
         for (uint32_t rs = 0; rs < lengthof(seq); ++rs) {
            if (config_seq_adc(ADC1, p, l, seq[rs], (adc_config_e) (l&MASK))) goto ONERR;
            // check adc_config_SAMPLETIME_
            for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
               adc_config_e time = getsampletime_adc(ADC1, c);
               int ismatch = 0;
               for (uint32_t i = 0; i < l; ++i) {
                  if (seq[rs][i] == c) {
                     ismatch = 1;
                     break;
                  }
               }
               if (ismatch  && time != (l&adc_config_MASK_SAMPLETIME)) goto ONERR;
               if (!ismatch && time != 0) goto ONERR;
            }
            // reset adc_config_SAMPLETIME_
            ADC1->smpr1 = 0;
            ADC1->smpr2 = 0;
            // check ADC1->sqr1..4 == seq[rs][0..l-1]
            uint32_t sqr = (l-1);
            volatile uint32_t *sqr1 = &ADC1->sqr1;
            for (uint32_t i = 0, pos = 1; i < 16; ++i) {
               sqr += i < l ? (seq[rs][i] << (6*pos)) : 0;
               if ((++pos) == 5 || i == 15) {
                  if (sqr != *sqr1++) goto ONERR;
                  pos = 0;
                  sqr = 0;
               }
            }
            // reset ADC1->sqr1..4
            ADC1->sqr1 = 0;
            ADC1->sqr2 = 0;
            ADC1->sqr3 = 0;
            ADC1->sqr4 = 0;
            // check ADC1->cr
            if (isenabled_adc(ADC1) != 1) goto ONERR;
            if (isstarted_adc(ADC1) != 0) goto ONERR;
            // check ADC1->cfgr
            uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_DISCEN | HW_REGISTER_BIT_ADC_CFGR_OVRMOD | ((p-1) << 17)
                           |(((l&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
            uint32_t off = HW_REGISTER_BIT_ADC_CFGR_AUTDLY|HW_REGISTER_BIT_ADC_CFGR_ALIGN|HW_REGISTER_BIT_ADC_CFGR_JAUTO
                           |HW_REGISTER_BIT_ADC_CFGR_CONT|HW_REGISTER_BIT_ADC_CFGR_EXTEN_MASK
                           |HW_REGISTER_BIT_ADC_CFGR_EXTSEL_MASK|HW_REGISTER_BIT_ADC_CFGR_DMAEN;
            if ((ADC1->cfgr != on) != 0) goto ONERR;
            // reset cfgr
            ADC1->cfgr = off;
         }
      }
   }

   // TEST start_adc
   if (config_seq_adc(ADC1, 1, 2, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   if (isstarted_adc(ADC1) != 1) goto ONERR;

   // TEST stop_adc
   stop_adc(ADC1);
   if ((ADC1->cr & HW_REGISTER_BIT_ADC_CR_ADSTP) != 0) goto ONERR;
   if (isstarted_adc(ADC1) != 0) goto ONERR;
   if (isdata_adc(ADC1) != 0) goto ONERR;
   if (iseos_adc(ADC1) != 0) goto ONERR;

   // TEST start_adc: Konvertiere 16 Mal den Kanal 2 in Partionen zu 3
   if (config_seq_adc(ADC1, 3, 16, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   for (uint32_t i = 0, old = 0; i < 16; ++i) {
      if (i && (i % 3) == 0) {
         // Ende einer Partition ==> neuer Trigger erforderlich
         if (isstarted_adc(ADC1)) goto ONERR;
         start_adc(ADC1);
      }
      if (!isstarted_adc(ADC1)) goto ONERR;
      if (iseos_adc(ADC1)) goto ONERR;
      while (!isdata_adc(ADC1)) ;
      uint32_t data = read_adc(ADC1);
      uint32_t diff = !i ? 0 : old < data ? data - old : old - data;
      if (diff > 100) goto ONERR;
      old = data;
   }
   if (! iseos_adc(ADC1))    goto ONERR;
   if (isoverflow_adc(ADC1)) goto ONERR;
   if (isstarted_adc(ADC1))  goto ONERR;
   clear_eos_adc(ADC1);

   // reset
   ADC1->cfgr = 0;
   ADC1->sqr1 = 0;
   ADC1->sqr2 = 0;
   ADC1->sqr3 = 0;
   ADC1->sqr4 = 0;
   ADC1->smpr1 = 0;
   ADC1->smpr2 = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;

}

static int test_config_auto(void)
{
   adc_chan_e seq[1][16] = {
      { adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2, adc_chan_2 },
   };
   adc_chan_e jseq[3][4] = {
      { 1, 2, 3, 4 },
      { 15, 14, 13, 12 },
      { 18, 1, 10, 8 },
   };

   // TEST config_autojseq_adc

   for (uint32_t l = 1; l <= lengthof(jseq[0]); ++l) {
      for (uint32_t js = 0; js < lengthof(jseq); ++js) {
         adc_config_e config = (adc_config_e) (((l+1) << adc_config_POS_SAMPLETIME) | ((js&adc_config_BITS_RESOLUTION) << adc_config_POS_RESOLUTION));
         if (config_autojseq_adc(ADC1, l, jseq[js], config)) goto ONERR;
         // check adc_config_SAMPLETIME_
         for (adc_chan_e c = adc_chan_1; c <= adc_chan_18; ++c) {
            adc_config_e time = getsampletime_adc(ADC1, c);
            int ismatch = 0;
            for (uint32_t i = 0; i < l; ++i) {
               if (jseq[js][i] == c) {
                  ismatch = 1;
                  break;
               }
            }
            if (ismatch  && time != (l+1)) goto ONERR;
            if (!ismatch && time != 0) goto ONERR;
         }
         // reset adc_config_SAMPLETIME_
         ADC1->smpr1 = 0;
         ADC1->smpr2 = 0;
         // check ADC1->jsqr == jseq[js][0..l-1]
         uint32_t jsqr = (l-1);
         for (uint32_t i = 0; i < l; ++i) {
            jsqr += (jseq[js][i] << (8+6*i));
         }
         if (jsqr != ADC1->jsqr) goto ONERR;
         // check ADC1->cr
         if (isenabled_adc(ADC1) != 1) goto ONERR;
         if (isjstarted_adc(ADC1) != 0) goto ONERR;
         // check ADC1->cfgr
         uint32_t on  = HW_REGISTER_BIT_ADC_CFGR_OVRMOD|HW_REGISTER_BIT_ADC_CFGR_JAUTO
                        |(((config&adc_config_MASK_RESOLUTION)>>adc_config_POS_RESOLUTION)<<HW_REGISTER_BIT_ADC_CFGR_RES_POS);
         uint32_t off = HW_REGISTER_BIT_ADC_CFGR_JQM|HW_REGISTER_BIT_ADC_CFGR_JDISCEN
                        |HW_REGISTER_BIT_ADC_CFGR_AUTDLY|HW_REGISTER_BIT_ADC_CFGR_ALIGN
                        |HW_REGISTER_BIT_ADC_CFGR_DMAEN;
         if ((ADC1->cfgr != on) != 0) goto ONERR;
         // reset cfgr
         ADC1->cfgr = off;
      }
   }

   // TEST startj_adc: nicht erlaubt
   // TEST stopj_adc: nicht erlaubt

   // TEST start_adc: Konvertiere 2*20 Mal den Kanal 2
   if (config_contseq_adc(ADC1, 16, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   if (config_autojseq_adc(ADC1, 4, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   start_adc(ADC1);
   for (uint32_t i = 0, old = 0; i < 40; ++i) {
      if (i == 16 || i == 36) {
         // Ende der regulären Sequenz
         if (!iseos_adc(ADC1)) goto ONERR;
         clear_eos_adc(ADC1);
      } else if (i == 20) {
         if (!isjeos_adc(ADC1)) goto ONERR;
         clear_jeos_adc(ADC1);
      }
      if (!isstarted_adc(ADC1)) goto ONERR;
      if (iseos_adc(ADC1)) goto ONERR;
      uint32_t data;
      if (16 <= i && i <= 19) {
         while (!isjdata_adc(ADC1)) ;
         data = readj_adc(ADC1, i-16);
      } else if (36 <= i && i <= 39) {
         while (!isjdata_adc(ADC1)) ;
         data = readj_adc(ADC1, i-36);
      } else {
         while (!isdata_adc(ADC1)) ;
         data = read_adc(ADC1);
      }
      uint32_t diff = !i ? 0 : old < data ? data - old : old - data;
      if (diff > 100) goto ONERR;
      old = data;
   }
   if (iseos_adc(ADC1))      goto ONERR;
   if (!isjeos_adc(ADC1))    goto ONERR;
   if (isoverflow_adc(ADC1)) goto ONERR;
   if (!isstarted_adc(ADC1)) goto ONERR;
   stop_adc(ADC1);
   if (isstarted_adc(ADC1))  goto ONERR;
   if (! isjeos_adc(ADC1))   goto ONERR;
   clear_jeos_adc(ADC1);

   // TEST config_autojseq_adc: partionierte Sequenz ==> EAGAIN
   if (config_seq_adc(ADC1, 1, 2, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   if (config_autojseq_adc(ADC1, 4, seq[0], adc_config_SAMPLETIME_601_5) != EAGAIN) goto ONERR;
   // OK
   if (config_seq_adc(ADC1, 1, 1, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   if (config_autojseq_adc(ADC1, 4, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;
   // config_seq_adc clears JAUTO flag
   if (config_seq_adc(ADC1, 1, 2, seq[0], adc_config_SAMPLETIME_601_5)) goto ONERR;

   // reset
   ADC1->cfgr = 0;
   ADC1->smpr1 = 0;
   ADC1->smpr2 = 0;
   ADC1->sqr1 = 0;
   ADC1->sqr2 = 0;
   ADC1->sqr3 = 0;
   ADC1->sqr4 = 0;
   disable_adc(ADC1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_config(void)
{
   if (test_config_jseq())  goto ONERR;
   if (test_config_jpart()) goto ONERR;
   if (test_config_seq())   goto ONERR;
   if (test_config_cont())  goto ONERR;
   if (test_config_partioned()) goto ONERR;
   if (test_config_auto())  goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}
