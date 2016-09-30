
volatile uint32_t s_interruptTable_counter1;
volatile uint32_t s_interruptTable_counter2;

void dma2_channel5_interrupt(void)
{
   ++s_interruptTable_counter1;
}

static void overwritten1_interruptTable(void)
{
   ++s_interruptTable_counter2;
}

int unittest_interruptTable(void)
{
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable());

   // TEST sizealign_interruptTable
   assert( 512 == sizealign_interruptTable());

   // TEST len_interruptTable
   assert( 98  == len_interruptTable());

   // TEST relocate_interruptTable: EINVAL
   for (uint32_t i = 1; i < 512; ++i) {
      assert( EINVAL == relocate_interruptTable((uint32_t*)((uintptr_t)CCMRAM + i)));
   }

   // TEST relocate_interruptTable
   for (uint32_t i = 0; i < len_interruptTable(); ++i) {
      CCMRAM[i] = 0;
   }
   CCMRAM[len_interruptTable()] = 0x12345678;
   assert( 0 == relocate_interruptTable(CCMRAM));
   for (uint32_t i = 0; i < len_interruptTable(); ++i) {
      assert( ((uint32_t*)0)[i] == CCMRAM[i]);
   }
   assert( 0x12345678 == CCMRAM[len_interruptTable()]);  // copied no more than len_interruptTable entries

   // TEST reset_interruptTable
   assert( 0 == relocate_interruptTable((uint32_t*)((uintptr_t)CCMRAM + 512)));
   assert( (uintptr_t)CCMRAM + 512 == hSCB->vtor);
   reset_interruptTable();
   assert( 0 == hSCB->vtor);

   // TEST relocate_interruptTable: overwrite entry
   relocate_interruptTable(CCMRAM);
   enable_interrupt(interrupt_DMA2_CHANNEL5);
   s_interruptTable_counter1 = 0;
   s_interruptTable_counter2 = 0;
   generate_interrupt(interrupt_DMA2_CHANNEL5);
   assert( 0 == s_interruptTable_counter2);
   assert( 1 == s_interruptTable_counter1);
   CCMRAM[interrupt_DMA2_CHANNEL5] = (uintptr_t) &overwritten1_interruptTable;
   generate_interrupt(interrupt_DMA2_CHANNEL5);
   assert( 1 == s_interruptTable_counter1);
   assert( 1 == s_interruptTable_counter2);

   // TEST reset_interruptTable: use ROM entry
   reset_interruptTable();
   generate_interrupt(interrupt_DMA2_CHANNEL5);
   assert( 1 == s_interruptTable_counter2);
   assert( 2 == s_interruptTable_counter1);
   disable_interrupt(interrupt_DMA2_CHANNEL5);

   return 0;
}

