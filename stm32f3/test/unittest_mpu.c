
static void fault_interrupt5(void)
{
   if (isinit_cpustate(&cpustate)) {
      ret2threadmode_cpustate(&cpustate);
   }
   assert(0);
}

static uint32_t encode_rasr(uint32_t tex, uint32_t size, uint32_t ap, uint32_t scb)
{
   return     (HW_BIT(MPU, RASR, TEX) & (tex << HW_BIT(MPU, RASR, TEX_POS)))
            | (HW_BIT(MPU, RASR, SIZE) & (size << HW_BIT(MPU, RASR, SIZE_POS)))
            | (HW_BIT(MPU, RASR, AP)   & (ap << HW_BIT(MPU, RASR, AP_POS)))
            |  HW_BIT(MPU, RASR, ENABLE) | scb;
}

int unittest_mpu(void)
{
   int err;
   uint32_t * const CCMRAM = (uint32_t*) HW_MEMORYREGION_CCMRAM_START;
   uint32_t   const CCMRAM_SIZE = HW_MEMORYREGION_CCMRAM_SIZE;

   // prepare
   assert( CCMRAM_SIZE/sizeof(uint32_t) > len_interruptTable());
   assert( 0 == relocate_interruptTable(CCMRAM));
   CCMRAM[coreinterrupt_FAULT] = (uintptr_t) &fault_interrupt5;

   // TEST nrbytes2size_mpu
   assert(mpu_size_4GB == nrbytes2size_mpu(0xffffffff));
   assert(mpu_size_4GB == nrbytes2size_mpu((1u << 31) + 1));
   assert(mpu_size_2GB == nrbytes2size_mpu(1u << 31));
   assert(mpu_size_2GB == nrbytes2size_mpu(1024*1024*1024 + 1));
   assert(mpu_size_1GB == nrbytes2size_mpu(1024*1024*1024));
   assert(mpu_size_1GB == nrbytes2size_mpu(512 *1024*1024 + 1));
   assert(mpu_size_512MB == nrbytes2size_mpu(512*1024*1024));
   assert(mpu_size_512MB == nrbytes2size_mpu(256*1024*1024 + 1));
   assert(mpu_size_256MB == nrbytes2size_mpu(256*1024*1024));
   assert(mpu_size_256MB == nrbytes2size_mpu(128*1024*1024 + 1));
   assert(mpu_size_128MB == nrbytes2size_mpu(128*1024*1024));
   assert(mpu_size_128MB == nrbytes2size_mpu(64*1024*1024 + 1));
   assert(mpu_size_64MB == nrbytes2size_mpu(64*1024*1024));
   assert(mpu_size_64MB == nrbytes2size_mpu(32*1024*1024 + 1));
   assert(mpu_size_32MB == nrbytes2size_mpu(32*1024*1024));
   assert(mpu_size_32MB == nrbytes2size_mpu(16*1024*1024 + 1));
   assert(mpu_size_16MB == nrbytes2size_mpu(16*1024*1024));
   assert(mpu_size_16MB == nrbytes2size_mpu(8*1024*1024 + 1));
   assert(mpu_size_8MB == nrbytes2size_mpu(8*1024*1024));
   assert(mpu_size_8MB == nrbytes2size_mpu(4*1024*1024 + 1));
   assert(mpu_size_4MB == nrbytes2size_mpu(4*1024*1024));
   assert(mpu_size_4MB == nrbytes2size_mpu(2*1024*1024 + 1));
   assert(mpu_size_2MB == nrbytes2size_mpu(2*1024*1024));
   assert(mpu_size_2MB == nrbytes2size_mpu(1*1024*1024 + 1));
   assert(mpu_size_1MB == nrbytes2size_mpu(1*1024*1024));
   assert(mpu_size_1MB == nrbytes2size_mpu(512*1024 + 1));
   assert(mpu_size_512K == nrbytes2size_mpu(512*1024));
   assert(mpu_size_512K == nrbytes2size_mpu(256*1024 + 1));
   assert(mpu_size_256K == nrbytes2size_mpu(256*1024));
   assert(mpu_size_256K == nrbytes2size_mpu(128*1024 + 1));
   assert(mpu_size_128K == nrbytes2size_mpu(128*1024));
   assert(mpu_size_128K == nrbytes2size_mpu(64*1024 + 1));
   assert(mpu_size_64K == nrbytes2size_mpu(64*1024));
   assert(mpu_size_64K == nrbytes2size_mpu(32*1024 + 1));
   assert(mpu_size_32K == nrbytes2size_mpu(32*1024));
   assert(mpu_size_32K == nrbytes2size_mpu(16*1024 + 1));
   assert(mpu_size_16K == nrbytes2size_mpu(16*1024));
   assert(mpu_size_16K == nrbytes2size_mpu(8*1024 + 1));
   assert(mpu_size_8K == nrbytes2size_mpu(8*1024));
   assert(mpu_size_8K == nrbytes2size_mpu(4*1024 + 1));
   assert(mpu_size_4K == nrbytes2size_mpu(4*1024));
   assert(mpu_size_4K == nrbytes2size_mpu(2*1024 + 1));
   assert(mpu_size_2K == nrbytes2size_mpu(2*1024));
   assert(mpu_size_2K == nrbytes2size_mpu(1*1024 + 1));
   assert(mpu_size_1K == nrbytes2size_mpu(1*1024));
   assert(mpu_size_1K == nrbytes2size_mpu(512 + 1));
   assert(mpu_size_512 == nrbytes2size_mpu(512));
   assert(mpu_size_512 == nrbytes2size_mpu(256 + 1));
   assert(mpu_size_256 == nrbytes2size_mpu(256));
   assert(mpu_size_256 == nrbytes2size_mpu(128 + 1));
   assert(mpu_size_128 == nrbytes2size_mpu(128));
   assert(mpu_size_128 == nrbytes2size_mpu(64 + 1));
   assert(mpu_size_64 == nrbytes2size_mpu(64));
   assert(mpu_size_64 == nrbytes2size_mpu(32 + 1));
   assert(mpu_size_32 == nrbytes2size_mpu(32));
   assert(mpu_size_32 == nrbytes2size_mpu(0));

   // TEST mpu_region_ENCODE_ACCESS_PRIVILEGE
   assert(0 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_NONE,mpu_access_NONE));
   assert(1 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_RW,mpu_access_NONE));
   assert(2 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_RW,mpu_access_READ));
   assert(3 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_RW,mpu_access_RW));
   assert(5 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_READ,mpu_access_NONE));
   assert(6 == mpu_region_ENCODE_ACCESS_PRIVILEGE(mpu_access_READ,mpu_access_READ));

   // TEST mpu_region_VALIDATE: wrong access privilege
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_RW+1, mpu_access_NONE));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, -1, mpu_access_NONE));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, -1, -1));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_RW, -1));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_RW, mpu_access_RW+1));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_NONE, mpu_access_NONE+1));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_READ, mpu_access_READ+1));
   assert( 0 == mpu_region_VALIDATE(0, mpu_size_256, 0, mpu_access_RW, mpu_access_RW+1));

   // TEST mpu_region_VALIDATE: wrong size
   for (mpu_size_e size = 0; size < mpu_size_32; ++size) {
      assert( 0 == mpu_region_VALIDATE(0, size, 0, mpu_access_NONE, mpu_access_NONE));
   }
   for (mpu_size_e size = mpu_size_32; size < mpu_size_256; ++size) {
      assert( 0 == mpu_region_VALIDATE(0, size, 0xFF, mpu_access_NONE, mpu_access_NONE));
   }
   for (mpu_size_e size = mpu_size_4GB+1; size < mpu_size_4GB+5; ++size) {
      assert( 0 == mpu_region_VALIDATE(0, size, 0, mpu_access_NONE, mpu_access_NONE));
   }

   // TEST mpu_region_VALIDATE: address not aligned by size
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      uint32_t addr = size2nrbytes_mpu(size);
      assert( 1 == mpu_region_VALIDATE(0, size, 0, mpu_access_NONE, mpu_access_NONE));
      assert( 1 == mpu_region_VALIDATE(addr, size, 0, mpu_access_NONE, mpu_access_NONE));
      assert( 0 == mpu_region_VALIDATE(addr+1, size, 0, mpu_access_NONE, mpu_access_NONE));
      assert( 0 == mpu_region_VALIDATE(addr-1, size, 0, mpu_access_NONE, mpu_access_NONE));
      assert( 0 == mpu_region_VALIDATE(addr|(addr?addr/2:0x80000000), size, 0, mpu_access_NONE, mpu_access_NONE));
   }

   // TEST mpu_config_INIT: wrong access rights ==> invalid region
   {
      mpu_region_t reg = mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_RW+1, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, -1, mpu_access_NONE);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, -1, -1);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_RW, -1);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW+1);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_NONE, mpu_access_NONE+1);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_READ, mpu_access_READ+1);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(0, mpu_size_256, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW+1);
      assert( ! isvalid_mpuregion(&reg));
   }

   // TEST mpu_config_INIT: wrong size ==> invalid region
   for (mpu_size_e size = 0; size < mpu_size_32; ++size) {
      mpu_region_t reg = mpu_region_INIT(0, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
   }
   for (mpu_size_e size = 0; size < mpu_size_256; ++size) {
      mpu_region_t reg = mpu_region_INIT(0, size, 0xff, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
   }
   for (uint8_t disablesubreg = 1; disablesubreg; ++disablesubreg) {
      mpu_region_t reg = mpu_region_INIT(0, mpu_size_128, disablesubreg, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
   }
   for (mpu_size_e size = mpu_size_4GB+1; size < mpu_size_4GB+5; ++size) {
      mpu_region_t reg = mpu_region_INIT(0, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
   }

   // TEST mpu_config_INIT: unaligned base address ==> invalid region
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      uint32_t addr = size2nrbytes_mpu(size);
      mpu_region_t reg = mpu_region_INIT(0, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(addr, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(addr+1, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(addr-1, size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
      reg = (mpu_region_t) mpu_region_INIT(addr|(addr?addr/2:0x80000000), size, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( ! isvalid_mpuregion(&reg));
   }

   // TEST mpu_config_INIT: mpu_mem_ORDERED
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      mpu_region_t reg = mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_ORDERED, mpu_access_READ, mpu_access_READ);
      assert( reg.addr == size2nrbytes_mpu(size));
      assert( reg.conf == encode_rasr(0/*TEX*/, size, 6/*AP*/, HW_BIT(MPU, RASR, XN)|HW_BIT(MPU, RASR, S)/*S,C,B*/));
   }

   // TEST mpu_config_INIT: mpu_mem_DEVICE
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      mpu_region_t reg = mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_DEVICE, mpu_access_RW, mpu_access_RW);
      assert( reg.addr == size2nrbytes_mpu(size));
      assert( reg.conf == encode_rasr(0/*TEX*/, size, 3/*AP*/, HW_BIT(MPU, RASR, XN)|HW_BIT(MPU, RASR, S)|HW_BIT(MPU, RASR, B)/*S,C,B*/));
   }

   // TEST mpu_config_INIT: mpu_mem_DEVICE_NOTSHARED
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      mpu_region_t reg = mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_DEVICE_NOTSHARED, mpu_access_READ, mpu_access_NONE);
      assert( reg.addr == size2nrbytes_mpu(size));
      assert( reg.conf == encode_rasr(2/*TEX*/, size, 5/*AP*/, HW_BIT(MPU, RASR, XN)/*S,C,B*/));
   }

   // TEST mpu_config_INIT: mpu_mem_NORMAL
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      for (uint32_t S = 0; S <= mpu_mem_SHARED; S = (S >> 1) | mpu_mem_SHARED) {
         for (uint32_t XN = 0; XN <= mpu_mem_NOEXEC; XN = (XN >> 1) | mpu_mem_NOEXEC) {
            mpu_region_t reg = mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_NORMAL(mpu_cache_WT)|XN|S, mpu_access_RW, mpu_access_READ);
            assert( reg.addr == size2nrbytes_mpu(size));
            assert( reg.conf == encode_rasr(0/*TEX*/, size, 2/*AP*/, XN|S|HW_BIT(MPU, RASR, C)/*S,C,B*/));
            reg = (mpu_region_t) mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_NORMAL(mpu_cache_WB)|XN|S, mpu_access_RW, mpu_access_READ);
            assert( reg.addr == size2nrbytes_mpu(size));
            assert( reg.conf == encode_rasr(0/*TEX*/, size, 2/*AP*/, XN|S|HW_BIT(MPU, RASR, C)|HW_BIT(MPU, RASR, B)/*S,C,B*/));
            reg = (mpu_region_t) mpu_region_INIT(size2nrbytes_mpu(size), size, 0, mpu_mem_NORMAL(mpu_cache_NONE)|XN|S, mpu_access_RW, mpu_access_READ);
            assert( reg.addr == size2nrbytes_mpu(size));
            assert( reg.conf == encode_rasr(1/*TEX*/, size, 2/*AP*/, XN|S/*S,C,B*/));
            reg = (mpu_region_t) mpu_region_INIT(0, size, 0, mpu_mem_NORMAL(mpu_cache_WBALLOCATE)|XN|S, mpu_access_RW, mpu_access_READ);
            assert( reg.addr == 0x0);
            assert( reg.conf == encode_rasr(1/*TEX*/, size, 2/*AP*/, XN|S|HW_BIT(MPU, RASR, C)|HW_BIT(MPU, RASR, B)/*S,C,B*/));
         }
      }
   }

   // TEST mpu_config_INIT: mpu_mem_NORMAL2
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_4GB; ++size) {
      for (uint32_t S = 0; S <= mpu_mem_SHARED; S = (S >> 1) | mpu_mem_SHARED) {
         for (uint32_t XN = 0; XN <= mpu_mem_NOEXEC; XN = (XN >> 1) | mpu_mem_NOEXEC) {
            for (uint32_t dsub = 0; dsub <= 255; dsub = (dsub << 1) + 1) {
               if (dsub && size < mpu_size_256) break;
               for (mpu_cache_e CP1 = 0; CP1 < mpu_cache__NROF; ++ CP1) {
                  for (mpu_cache_e CP2 = 0; CP2 < mpu_cache__NROF; ++ CP2) {
                     mpu_region_t reg = mpu_region_INIT(size2nrbytes_mpu(size), size, dsub, mpu_mem_NORMAL2(CP1,CP2)|XN|S, mpu_access_RW, mpu_access_NONE);
                     assert( reg.addr == size2nrbytes_mpu(size));
                     assert( reg.conf == encode_rasr((4+CP1)/*TEX*/, size, 1/*AP*/, XN|S|(CP2<<HW_BIT(MPU, RASR, B_POS))/*S,C,B*/|(dsub<<HW_BIT(MPU,RASR,SRD_POS))));
                  }
               }
            }
         }
      }
   }

   // TEST nrregions_mpu
   assert(8 == nrregions_mpu());

   // TEST isavailable_mpu
   assert(1 == isavailable_mpu());

   // TEST config_mpu: privileged access rights / unprivileged access rights
   for (mpu_access_e priv = mpu_access_NONE; priv <= mpu_access_RW; ++priv) {
      uint32_t addr = ((uintptr_t)&CCMRAM[len_interruptTable()] + 255) & ~0xff;
      assert(addr % 256 == 0 && addr + 256 <= (uintptr_t)CCMRAM + 8192);
      for (mpu_access_e unpriv = mpu_access_NONE; unpriv <= priv; ++unpriv) {
         uint32_t val = 0;
         mpu_region_t reg = mpu_region_INITRAM(addr, mpu_size_256, 0, priv, unpriv);
         assert( 0 == config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
         err = init_cpustate(&cpustate);
         if (0 == err) {
            val = *(volatile uint32_t*)addr;
         }
         assert(err == (priv == mpu_access_NONE ? EINTR : 0));
         err = init_cpustate(&cpustate);
         if (0 == err) {
            *(volatile uint32_t*)(addr + 252) = ~ val;
         }
         assert(err == (priv != mpu_access_RW ? EINTR : 0));
         err = init_cpustate(&cpustate);
         if (0 == err) {
            __asm volatile(
                  "ldrt %0, [%1]\n"
                  : "=r" (val) : "r" (addr + 252)
            );
         }
         assert(err == (unpriv == mpu_access_NONE ? EINTR : 0));
         err = init_cpustate(&cpustate);
         if (0 == err) {
            __asm volatile(
                  "strt %0, [%1]\n"
                  : : "r" (val & 0xf0f0), "r" (addr)
            );
         }
         assert(err == (unpriv != mpu_access_RW ? EINTR : 0));
         free_cpustate(&cpustate);
      }
   }
   disable_mpu();


   // TEST config_mpu: size parameter mpu_size_32..mpu_size_256K
   for (mpu_size_e size = mpu_size_32; size <= mpu_size_256K; ++size) {
      mpu_region_t reg = mpu_region_INITROM(0, size, 0, mpu_access_READ);
      assert( 0 == config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
      uint32_t ptr = size2nrbytes_mpu(size);
      __asm volatile(
            "ldrt %0, [%1]\n"       // ptr in range => allowed
            : "=r" (err) : "r" (ptr-4)
      );
      err = init_cpustate(&cpustate);
      if (0 == err) {
         __asm volatile(
               "ldrt %0, [%1]\n"    // ptr out of range => not allowed
               : "=r" (err) : "r" (ptr)
         );
         assert(0);                 // never reached
      }
      assert(EINTR == err);
      free_cpustate(&cpustate);
      disable_mpu();
   }

   // TEST config_mpu: partition into subregions
   static uint8_t disablesub = 0;
   ++ disablesub;
   for (mpu_size_e size = mpu_size_256; size <= mpu_size_256K; ++size) {
      mpu_region_t reg = mpu_region_INITROM(0, size, disablesub, mpu_access_READ);
      assert( 0 == config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
      uint32_t ptr = size2nrbytes_mpu(size);
      for (uint32_t si = 0; si < 8; ++si) {
         for (uint32_t off = 0; off < ptr/8; off += ptr/8 -4) {
            if (0 == (disablesub & (1u << si))) {
               __asm volatile(
                     "ldrt %0, [%1]\n"       // ptr in enabled subregion => allowed
                     : "=r" (err) : "r" (si*(ptr/8)+off)
               );
            } else {
               err = init_cpustate(&cpustate);
               if (0 == err) {
                  __asm volatile(
                        "ldrt %0, [%1]\n"    // ptr in disabled subregion => not allowed
                        : "=r" (err) : "r" (si*(ptr/8)+off)
                  );
                  assert(0);                 // never reached
               }
               assert(EINTR == err);
               free_cpustate(&cpustate);
            }
         }
      }
      disable_mpu();
   }

   // TEST config_mpu: Can not add additional privileges (unprivileged access to PPB for example)
   {
      mpu_region_t reg = mpu_region_INIT(0xE0000000/*SCS*/, mpu_size_1MB, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
      assert( 0 == config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
      assert( 1 <= hSCS->ictr);     // privileged access allowed to SCS
      err = init_cpustate(&cpustate);
      if (0 == err) {
         __asm volatile(
               "ldrt %0, [%1]\n"    // unprivileged access to PPB not allowed
               : "=r" (err) : "r" (&hSCS->ictr)
         );
         assert(0);                 // never reached
      }
      assert(EINTR == err);
      free_cpustate(&cpustate);
      disable_mpu();
   }

   // TEST config_mpu: MPU is ignored if priority <= -1 (and used if mpucfg_USEWITHFAULTPRIORITY is set)
   {
      static_assert(CCMRAM_SIZE == 8192);
      mpu_region_t reg = mpu_region_INITRAM(CCMRAM, mpu_size_8K, 0, mpu_access_NONE, mpu_access_NONE);
#if (0)
      // adding mpucfg_USEWITHFAULTPRIORITY would result into locked state of CPU
      assert( 0 == config_mpu(1, &reg, mpucfg_USEWITHFAULTPRIORITY|mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
#else
      assert( 0 == config_mpu(1, &reg, mpucfg_ALLOWPRIVACCESS|mpucfg_ENABLE));
#endif
      enable_coreinterrupt(coreinterrupt_MPUFAULT);
      setfaultmask_interrupt();
      volatile uint32_t sum = CCMRAM[0];  // privileged access to CCMRAM not allowed cause prio <= -1 (set faultmask)
      __asm volatile(
            "ldrt %0, [%1]\n"             // unprivileged access to CCMRAM not allowed but ignored cause prio <= -1
            : "=r" (sum) : "r" (&CCMRAM[0])
      );
      disable_mpu();
      clearfaultmask_interrupt();
      assert(0 == is_coreinterrupt(coreinterrupt_MPUFAULT));
      disable_coreinterrupt(coreinterrupt_MPUFAULT);
   }

   // reset
   reset_interruptTable();

   return 0;
}

