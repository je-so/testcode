
typedef struct unittest_cpustate_t {
   void       *arg;        // must be 1st
   uint32_t    stack[128]; // must be 2nd
   cpustate_t  restore;
   cpustate_t  state;
} unittest_cpustate_t;

unittest_cpustate_t  *s_cs;

__attribute__ ((naked))
static void test_task(void *arg)
{
   __asm volatile(
      "push { r1 }\n"
      "movw r1, #:lower16:s_cs\n"
      "movt r1, #:upper16:s_cs\n"
      "ldr  r1, [r1]\n" // r1 = s_cs
      "ldr  r1, [r1]\n" // r1 = s_cs->arg;
      "cmp  r0, r1\n"
      "bne  1f\n"
      "pop { r1 }\n"
      "cmp  r1, #0\n"
      "bne  1f\n"
      "cmp  r2, #0\n"
      "bne  1f\n"
      "cmp  r3, #0\n"
      "bne  1f\n"
      "cmp  r4, #0\n"
      "bne  1f\n"
      "cmp  r5, #0\n"
      "bne  1f\n"
      "cmp  r6, #0\n"
      "bne  1f\n"
      "cmp  r7, #0\n"
      "bne  1f\n"
      "cmp  r8, #0\n"
      "bne  1f\n"
      "cmp  r9, #0\n"
      "bne  1f\n"
      "cmp  r10, #0\n"
      "bne  1f\n"
      "cmp  r11, #0\n"
      "bne  1f\n"
      "cmp  r12, #0\n"
      "bne  1f\n"
      "push { r1 }\n"
      "movw r1, #:lower16:s_cs\n"
      "movt r1, #:upper16:s_cs\n"
      "ldr  r1, [r1]\n"       // r1 = &s_cs->arg;
      "add  r1, r1, #128*4\n" // r1 = &s_cs->stack[127];
      "cmp  r13, r1\n"
      "bne  1f\n"
      "pop { r1 }\n"
      "cmp  LR, #0xFFFFFFFF\n"
      "bne  1f\n"
      "b 2f\n"
      "1: mvn r0, r0\n" // error
      "2: nop\n"        // ok
      );
      assert(arg == s_cs->arg);
      jump_cpustate(&s_cs->restore);
}

__attribute__ ((naked))
static void test_savereg(void *arg)
{
   __asm volatile(
      "stm  r0, {r0-r12}\n"      // args[0..12] = r0..r12
      "str  sp, [r0, #13*4]\n"   // args[13] = sp
      "str  lr, [r0, #14*4]\n"   // args[14] = lr
      "movw r1, #:lower16:test_savereg\n"
      "movt r1, #:upper16:test_savereg\n"
      "str  r1, [r0, #15*4]\n"   // args[15] = &test_savereg
      "mrs  r1, xpsr\n"          // r1 = xpsr
      "str  r1, [r0, #16*4]\n"   // args[16] = xpsr
      "mrs  r1, control\n"       // r1 = control
      "str  r1, [r0, #17*4]\n"   // args[17] = control
      "mov  r2, sp\n"            // r2 = (MSP or PSP)
      "msr  msp, r2\n"           // MSP = r2
      "bic  r1, #2\n"            // r1 = set MSP as current SP
      "msr  control, r1\n"       // control = r1
      ::: "r1", "memory"
   );
   jump_cpustate(&s_cs->restore);
}

static void pendsv_interrupt4(void)
{
   if (isinit_cpustate(&s_cs->state)) {
      ret2threadmode_cpustate(&s_cs->state);
   }
   assert(0);
}

static void pendsv_interrupt5(void)
{
   if (isinit_cpustate(&s_cs->state)) {
      uint32_t dummy;
      ret2threadmodepsp_cpustate(&s_cs->state, &dummy/*keep msp at current value, reset in test_savereg*/);
   }
   assert(0);
}

int unittest_cpustate(void)
{
   int err;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;
   uint32_t   regs[18/*r0-r15,psr,control*/];
   unittest_cpustate_t global_state;

   // prepare
   s_cs = &global_state;
   free_cpustate(&global_state.restore);
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable())
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt4;
   __asm volatile ("dsb");

   // TEST init_cpustate: write different values to registers and psr
   for (uint32_t psr = 1 << 16, rv = 1; psr; psr <<= 1, ++rv) {
      regs[16] = (psr&0xf80f0000) | (1<<24)/*Thumb state bit*/;
      for (uint32_t i = 0; i < lengthof(regs)-4/*sp,lr,pc,psr*/; ++i) {
         regs[i] = (rv + i);
      }
      __asm volatile(
         "push {r0-r12,lr}\n" // save content of register
         "push {%0}\n"        // store &regs[0]
         "push {%1}\n"        // store &s_cs->state
         "mov  r0, %0\n"
         "str  sp, [r0, #13*4]\n"   // regs[13] = sp;
         "ldr  r1, =1f\n"           // r1 = &"label 1:"
         "orr  r1, #1\n"            // set thumb state (bit 0 of address)
         "str  r1, [r0, #14*4]\n"   // regs[14/*LR*/] = r1;
         "str  r1, [r0, #15*4]\n"   // regs[15/*PC*/] = r1;
         "ldr  r1, [r0, #16*4]\n"   // r1 = regs[16/*PSR*/];
         "msr  apsr_nzcvqg, r1\n"   // APSR = r1;
         "ldm  r0, {r0-r12}\n"      // load r0-r12 from &regs[0]
         "pop  {r0}\n"              // r0 = &s_cs->state;
         "bl init_cpustate\n"       // init_cpustate(&s_cs->state);
         "1: pop {r1}\n"      // "label 1:" next instruction after call to init_cpustate !!
         "str r0, [r1]\n"     // regs[0] = r0;
         "pop {r0-r12,lr}\n"  // restore content of register
         :: "r" (regs), "r" (&s_cs->state) : "memory"
      );
      assert(regs[0]  == 0);   // return value of init_cpustate
      assert(regs[13] == s_cs->state.sp-4);   // r13 + 4 == sp
      assert(EINTR == s_cs->state.iframe[0]); // return value used for jump_cpustate or ret2threadmode_cpustate
      assert(regs[1] == s_cs->state.iframe[1/*r1*/]);
      assert(regs[2] == s_cs->state.iframe[2/*r2*/]);
      assert(regs[3] == s_cs->state.iframe[3/*r3*/]);
      assert(regs[12] == s_cs->state.iframe[4]/*r12*/);
      assert(regs[14] == s_cs->state.iframe[5]/*lr*/);
      assert(regs[15] == s_cs->state.iframe[6]/*pc*/);
      assert(regs[16] == s_cs->state.iframe[7]/*psr*/);
      for (uint32_t i = 0; i < 8; ++i) {
         assert(regs[4+i] == s_cs->state.regs[i/*r4..r11*/]);
      }
      free_cpustate(&s_cs->state);
   }

   // TEST inittask_cpustate
   for (size_t i = 0; i < sizeof(s_cs->state); ++i) {
      *((uint8_t*)&s_cs->state) = (uint8_t) i;
   }
   inittask_cpustate(&s_cs->state, &test_task, (void*)(uintptr_t)0x12345678, lengthof(s_cs->stack), s_cs->stack);
   assert( s_cs->state.sp        == (uintptr_t) &s_cs->stack[lengthof(s_cs->stack)]);
   assert( s_cs->state.iframe[0] == 0x12345678);
   assert( s_cs->state.iframe[1] == 0);
   assert( s_cs->state.iframe[2] == 0);
   assert( s_cs->state.iframe[3] == 0);
   assert( s_cs->state.iframe[4] == 0);
   assert( s_cs->state.iframe[5] == 0xffffffff);
   assert( s_cs->state.iframe[6] == (uintptr_t) &test_task);
   assert( s_cs->state.iframe[7] == (1 << 24)/* Thumb state bit*/);
   for (size_t i = 0; i < lengthof(s_cs->state.regs); ++i) {
      assert( 0 == s_cs->state.regs[i]);
   }
   free_cpustate(&s_cs->state);

   // TEST jump_cpustate: call test_task and jump back
   for (uintptr_t i = 1; i; i <<= 1) {
      err = init_cpustate(&s_cs->restore);
      if (err == 0) {
         inittask_cpustate(&s_cs->state, &test_task, (void*)i, lengthof(s_cs->stack), s_cs->stack);
         s_cs->arg = (void*)i;
         jump_cpustate(&s_cs->state);
      }
      assert(err == EINTR);
      free_cpustate(&s_cs->state);
   }

   // TEST ret2threadmode_cpustate: call test_task and jump back
   for (uintptr_t i = 1; i; i <<= 1) {
      err = init_cpustate(&s_cs->restore);
      if (err == 0) {
         inittask_cpustate(&s_cs->state, &test_task, (void*)i, lengthof(s_cs->stack), s_cs->stack);
         s_cs->arg = (void*)i;
         generate_coreinterrupt(coreinterrupt_PENDSV);
      }
      assert(err == EINTR);
      free_cpustate(&s_cs->state);
   }

   // TEST jump_cpustate: write different values to registers and psr
   for (uint32_t psr = 1 << 16, rv = 1; psr; psr <<= 1, ++rv) {
      err = init_cpustate(&s_cs->restore);
      if (err == 0) {
         inittask_cpustate(&s_cs->state, &test_savereg, regs, lengthof(s_cs->stack), s_cs->stack);
         s_cs->state.iframe[7/*PSR*/] = psr;  // thumb bit not used restored in exit interrupt
         s_cs->state.iframe[7/*PSR*/] = (psr&0xf80f0000)|(1<<24/*thumb bit restored in exit interrupt*/);
         s_cs->state.iframe[5/*LR*/]  = rv + 14;
         s_cs->state.iframe[4/*R12*/] = rv + 12;
         for (uint32_t i = 1; i <= 11; ++i) {
            if (i <= 3) s_cs->state.iframe[i] = (rv + i);
            else        s_cs->state.regs[i-4] = (rv + i);
         }
         for (uint32_t i = 0; i < lengthof(regs); ++i) {
            regs[i] = 0;
         }
         jump_cpustate(&s_cs->state);
      }
      assert(err == EINTR);
      assert(regs[0]  == (uintptr_t) regs);
      assert(regs[13] == (uintptr_t) &s_cs->stack[lengthof(s_cs->stack)]);
      assert(regs[14] == (rv + 14));
      assert(regs[15] == (uintptr_t) &test_savereg);
      assert(regs[16] == (psr&0xf80f0000));  // mrs r0, xpsr reads only APSR_NZCVQG
      for (uint32_t i = 1; i <= 12; ++i) {
         assert(regs[i] == (rv + i));
      }
      free_cpustate(&s_cs->state);
   }

   // TEST ret2threadmode_cpustate: write different values to registers and psr
   for (uint32_t psr = 1 << 16, rv = 1; psr; psr <<= 1, ++rv) {
      err = init_cpustate(&s_cs->restore);
      if (err == 0) {
         inittask_cpustate(&s_cs->state, &test_savereg, regs, lengthof(s_cs->stack), s_cs->stack);
         s_cs->state.iframe[7/*PSR*/] = (psr&0xf80f0000)|(1<<24/*thumb bit restored in exit interrupt*/);
         s_cs->state.iframe[5/*LR*/]  = rv + 14;
         s_cs->state.iframe[4/*R12*/] = rv + 12;
         for (uint32_t i = 1; i <= 11; ++i) {
            if (i <= 3) s_cs->state.iframe[i] = (rv + i);
            else        s_cs->state.regs[i-4] = (rv + i);
         }
         for (uint32_t i = 0; i < lengthof(regs); ++i) {
            regs[i] = 0;
         }
         generate_coreinterrupt(coreinterrupt_PENDSV);
      }
      assert(err == EINTR);
      assert(regs[0]  == (uintptr_t) regs);
      assert(regs[13] == (uintptr_t) &s_cs->stack[lengthof(s_cs->stack)]);
      assert(regs[14] == (rv + 14));
      assert(regs[15] == (uintptr_t) &test_savereg);
      assert(regs[16] == (psr&0xf80f0000));  // mrs r0, xpsr reads only APSR_NZCVQG
      assert(regs[17] == 0);                 // CONTROL privileged(bit 0=0), MSP(bit 1=0), no FPU(bit 2=0)
      for (uint32_t i = 1; i <= 12; ++i) {
         assert(regs[i] == (rv + i));
      }
      free_cpustate(&s_cs->state);
   }

   // TEST ret2threadmodepsp_cpustate: write different values to registers and psr
   CCMRAM[coreinterrupt_PENDSV] = (uintptr_t) &pendsv_interrupt5;
   __asm volatile ("dsb");
   for (uint32_t psr = 1 << 16, rv = 1; psr; psr <<= 1, ++rv) {
      err = init_cpustate(&s_cs->restore);
      if (err == 0) {
         inittask_cpustate(&s_cs->state, &test_savereg, regs, lengthof(s_cs->stack), s_cs->stack);
         s_cs->state.iframe[7/*PSR*/] = (psr&0xf80f0000)|(1<<24/*thumb bit restored in exit interrupt*/);
         s_cs->state.iframe[5/*LR*/]  = rv + 14;
         s_cs->state.iframe[4/*R12*/] = rv + 12;
         for (uint32_t i = 1; i <= 11; ++i) {
            if (i <= 3) s_cs->state.iframe[i] = (rv + i);
            else        s_cs->state.regs[i-4] = (rv + i);
         }
         for (uint32_t i = 0; i < lengthof(regs); ++i) {
            regs[i] = 0;
         }
         generate_coreinterrupt(coreinterrupt_PENDSV);
      }
      assert(err == EINTR);
      assert(regs[0]  == (uintptr_t) regs);
      assert(regs[13] == (uintptr_t) &s_cs->stack[lengthof(s_cs->stack)]);
      assert(regs[14] == (rv + 14));
      assert(regs[15] == (uintptr_t) &test_savereg);
      assert(regs[16] == (psr&0xf80f0000));  // mrs r0, xpsr reads only APSR_NZCVQG
      assert(regs[17] == 2);                 // CONTROL = privileged(bit 0=0), MSP(bit 1=1), no FPU(bit 2=0)
      for (uint32_t i = 1; i <= 12; ++i) {
         assert(regs[i] == (rv + i));
      }
      free_cpustate(&s_cs->state);
   }

   // reset
   s_cs = 0;
   reset_interruptTable();

   return 0;
}
