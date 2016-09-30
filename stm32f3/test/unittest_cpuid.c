
int unittest_cpuid(void)
{
   // TEST CPUID Scheme

   assert( 0x41 == ((hSCB->cpuid >> HW_BIT(SCB, CPUID, IMPLEMENTER_POS)) & HW_BIT(SCB, CPUID, IMPLEMENTER_MAX)));
   assert( 0x00 == ((hSCB->cpuid >> HW_BIT(SCB, CPUID, VARIANT_POS)) & HW_BIT(SCB, CPUID, VARIANT_MAX)));
   // (ARCHITECTURE == 0x0f) ==> CPUID Scheme supported
   assert( 0x0F == ((hSCB->cpuid >> HW_BIT(SCB, CPUID, ARCHITECTURE_POS)) & HW_BIT(SCB, CPUID, ARCHITECTURE_MAX)));
   assert( 0xC24 == ((hSCB->cpuid >> HW_BIT(SCB, CPUID, PARTNO_POS)) & HW_BIT(SCB, CPUID, PARTNO_MAX)));
   assert( 0x01 == ((hSCB->cpuid >> HW_BIT(SCB, CPUID, REVISION_POS)) & HW_BIT(SCB, CPUID, REVISION_MAX)));
   assert( 3 == ((hSCB->pfr[0] >> HW_BIT(SCB, PFR0, THUMBINST_POS)) & HW_BIT(SCB, PFR0, THUMBINST_MAX)));
   assert( 0 == ((hSCB->pfr[0] >> HW_BIT(SCB, PFR0, ARMINST_POS)) & HW_BIT(SCB, PFR0, ARMINST_MAX)));
   assert( 2 == ((hSCB->pfr[1] >> HW_BIT(SCB, PFR1, MODEL_POS)) & HW_BIT(SCB, PFR1, MODEL_MAX)));
   assert( 1 == ((hSCB->dfr >> HW_BIT(SCB, DFR, DEBUGMODEL_POS)) & HW_BIT(SCB, DFR, DEBUGMODEL_MAX)));
   assert( 1 == ((hSCB->mmfr[0] >> HW_BIT(SCB, MMFR0, AUXREG_POS)) & HW_BIT(SCB, MMFR0, AUXREG_MAX)));
   assert( 0 == ((hSCB->mmfr[0] >> HW_BIT(SCB, MMFR0, SHARE_POS)) & HW_BIT(SCB, MMFR0, SHARE_MAX)));
   assert( 0 == ((hSCB->mmfr[0] >> HW_BIT(SCB, MMFR0, OUTSHARE_POS)) & HW_BIT(SCB, MMFR0, OUTSHARE_MAX)));
   assert( 3 == ((hSCB->mmfr[0] >> HW_BIT(SCB, MMFR0, PMSA_POS)) & HW_BIT(SCB, MMFR0, PMSA_MAX)));
   assert( 1 == ((hSCB->mmfr[2] >> HW_BIT(SCB, MMFR2, WFISTALL_POS)) & HW_BIT(SCB, MMFR2, WFISTALL_MAX)));
   assert( 1 == ((hSCB->isar[0] >> HW_BIT(SCB, ISAR0, DIV_POS)) & HW_BIT(SCB, ISAR0, DIV_MAX)));
   assert( 1 == ((hSCB->isar[0] >> HW_BIT(SCB, ISAR0, DEBUG_POS)) & HW_BIT(SCB, ISAR0, DEBUG_MAX)));
   assert( 0 == ((hSCB->isar[0] >> HW_BIT(SCB, ISAR0, COPROC_POS)) & HW_BIT(SCB, ISAR0, COPROC_MAX)));
   assert( 1 == ((hSCB->isar[0] >> HW_BIT(SCB, ISAR0, CMPBRA_POS)) & HW_BIT(SCB, ISAR0, CMPBRA_MAX)));
   assert( 1 == ((hSCB->isar[0] >> HW_BIT(SCB, ISAR0, BITFIELD_POS)) & HW_BIT(SCB, ISAR0, BITFIELD_MAX)));
   assert( 2 == ((hSCB->isar[1] >> HW_BIT(SCB, ISAR1, INTERWORK_POS)) & HW_BIT(SCB, ISAR1, INTERWORK_MAX)));
   assert( 1 == ((hSCB->isar[1] >> HW_BIT(SCB, ISAR1, IMMEDIATE_POS)) & HW_BIT(SCB, ISAR1, IMMEDIATE_MAX)));
   assert( 1 == ((hSCB->isar[1] >> HW_BIT(SCB, ISAR1, IFTHEN_POS)) & HW_BIT(SCB, ISAR1, IFTHEN_MAX)));
   assert( 2 == ((hSCB->isar[1] >> HW_BIT(SCB, ISAR1, EXTEND_POS)) & HW_BIT(SCB, ISAR1, EXTEND_MAX)));
   assert( 2 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, REVERSAL_POS)) & HW_BIT(SCB, ISAR2, REVERSAL_MAX)));
   assert( 2 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, MULTU_POS)) & HW_BIT(SCB, ISAR2, MULTU_MAX)));
   assert( 3 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, MULTS_POS)) & HW_BIT(SCB, ISAR2, MULTS_MAX)));
   assert( 2 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, MULT_POS)) & HW_BIT(SCB, ISAR2, MULT_MAX)));
   assert( 2 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, LDMSTMINT_POS)) & HW_BIT(SCB, ISAR2, LDMSTMINT_MAX)));
   assert( 3 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, MEMHINT_POS)) & HW_BIT(SCB, ISAR2, MEMHINT_MAX)));
   assert( 1 == ((hSCB->isar[2] >> HW_BIT(SCB, ISAR2, LDRSTR_POS)) & HW_BIT(SCB, ISAR2, LDRSTR_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, TRUENOP_POS)) & HW_BIT(SCB, ISAR3, TRUENOP_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, THMBCOPY_POS)) & HW_BIT(SCB, ISAR3, THMBCOPY_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, TABBRANCH_POS)) & HW_BIT(SCB, ISAR3, TABBRANCH_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, SYNCHPRIM_POS)) & HW_BIT(SCB, ISAR3, SYNCHPRIM_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, SVC_POS)) & HW_BIT(SCB, ISAR3, SVC_MAX)));
   assert( 3 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, SIMD_POS)) & HW_BIT(SCB, ISAR3, SIMD_MAX)));
   assert( 1 == ((hSCB->isar[3] >> HW_BIT(SCB, ISAR3, SATURATE_POS)) & HW_BIT(SCB, ISAR3, SATURATE_MAX)));
   assert( 1 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, PSR_M_POS)) & HW_BIT(SCB, ISAR4, PSR_M_MAX)));
   assert( 3 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, SYNFRAC_POS)) & HW_BIT(SCB, ISAR4, SYNFRAC_MAX)));
   assert( 1 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, BARRIER_POS)) & HW_BIT(SCB, ISAR4, BARRIER_MAX)));
   assert( 1 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, WRITEBACK_POS)) & HW_BIT(SCB, ISAR4, WRITEBACK_MAX)));
   assert( 3 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, WITHSHIFT_POS)) & HW_BIT(SCB, ISAR4, WITHSHIFT_MAX)));
   assert( 2 == ((hSCB->isar[4] >> HW_BIT(SCB, ISAR4, UNPRIV_POS)) & HW_BIT(SCB, ISAR4, UNPRIV_MAX)));

   return 0;
}

