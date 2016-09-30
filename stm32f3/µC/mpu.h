/* title: CortexM4-Memory-Protection-unit

   Gibt Zugriff auf

      o MPU: Memory Protection Unit

   Die MPU teilt den gesamten Speicherbereich in (bis zu 8) einzelne Regionen
   und definiert Ort, Grösse und Zugriffsrechte sowie weitere Attribute.

   Unterstützt werden
     • Unabhängige Attributeinstellungen pro Region
     • Sich überlappende Regionen, wobei die Attribute der
       Region zum Zuge kommen, welche die höchste Nummer besitzt.
     • Die Attribute werden über den Bus an das System weitergereicht.
     • Die Cortex-M4 MPU unterstützt acht unabhängige Regionen von 0 bis Nummer 7.
     • Eine Hintergrundregion, welche nur privilegierte Zugriffe für alle nicht
       von den 8 Regionen erfasste Speicherbereiche erlaubt. Es werden dieselben
       Defaultattribute angewandt, wie wenn die MPU ausgeschalten wäre.
     • Daten- und Insruktionszugriffe werden einheitlich abgebildet und sind nicht
       separat ausgeführt.
     • Die Größe einer Region geht von 32, 64, 128, ... bis 4GB immer in 2er Potenzen.
       Die Startadresse einer Region muss ein Vielfaches ihrer Größe sein (teilbar durch Größe ohne Rest).
     • Für den privilegierten Modus und den unprivilegierten Modus der ARM-CPU können
       unterschiedliche Zugriffsrechte (Read-only,ReadWrite,No Access) pro Speicherregion vergeben werden.
     • Exception Vektoren werden von der Vektortabelle immer mit den Defaultattributen gelesen.

   Zugriffsregelung:
   Ist die MPU ausgeschaltet oder nicht präsent, werden der Defaultzugriffsrechte aktiv.
   Ist sie eingeschaltet, kann die MPU nur Rechte wegnehmen, keine hinzufügen.

   Weitere Bedingungen sind:
     - Zugriffe auf den Private Peripheral Bus (PPB) verwenden immer die Defaultzugriffsrechte,
       genauso Zugriffe auf die Interruptvektortabelle.
     - Systemspace (ab 0xE0000000) und die Peripheriebausteine sind immer XN (execute never) markiert.
     - Mittels MPU_CTRL.HFNMIENA können auch NMI und FAULT Interrupts unter der MPU laufen.
     - Ein Zugriff auf eine undefinierte Region bzw. der nicht alle Bedingungen der MPU erfüllt
       oder die Defaultzugriffsrechte verletzt, generiert einen MPU- oder BUSFAULT.
       Das Register SCB_MMFSR (Memory management fault address) beschreibt den Grund näher.
     - Für Execution-Priority von < 0 (NMI, FAULT, setfaultmask_interrupt) werden privilegierte/unprivilegierte
       Speicherzugriffe ohne MPU-Unterstützung ausgeführt.
       Wird das Bit MPU_CTRL.HFNMIENA gesetzt, dann verwenden die Speicherzugriffe auch bei Priority < 0
       die MPU, was aber bei einem Fehler zu einem Lockup der CPU führt. Dieses Bit wird bei Konfiguration
       der MPU immer gelöscht. So dass beachtet werden muss, dass Programme die mit setfaultmask_interrupt() laufen
       (oder NMI- bzw. FAULT-Interrupts) die MPU vollständig ignorieren und niemels einen MPUFAULT generieren.
       Und deshalb unprivilegierte Zugriffe mittels LDRT/STRT wie LDR/STR funktionieren, jedoch einen BUSFAULT generieren,
       wenn auf privilegierte Speicherbereiche (siehe »Unprivilegierter Default Zugriff«) zugegriffen wird.


   Um unterschiedliche Prozesse durch ein Betriebssystem zu unterstützen, können
   die Regionen dynamisch angepasst werden.

   Befehle wie DMB, DSB, ISB zum Setzen von Speicher Barrieren werden verwendet

    • Vor Neukonfiguration der MPU, damit keine Speichertransfers mehr ausstehen,
      welche von anderen Zugriffsrechten betroffen sein könnten.
    • Nach einer Neukonfiguration, damit nachfolgende Zugriffe auch die neuen Rechte verwenden.
      Mittels einer DSB und einer ISB Instruction tritt unmittelbar danach die Neukonf. in Kraft.

   Wird die MPU innerhalb eines Exceptionhandlers (Interrupts) umkonfiguriert, sind explizite
   Speicherbarrieren nicht erforderlich, da Interrupteintritt und -rückkehr dasselbe Verhalten
   wie Speicherbarrieren aufzeigen.

   DMA Transfer wird von den MPU Attributen nicht beeinflusst. Um also Speicherbereiche
   vor versehentlichem DMA zugriff zu schützen, muss der Zugriff auf die DMA Register
   eingeschränkt werden.

   Die Speicher-Typen
   ┌────────┬────────────────────────────────────────────────────────────────────────┐
   │ Normal │ Die CPU darf Zugriffe umordnen und den Cache vorauslesend befüllen.    │
   ├────────┼────────────────────────────────────────────────────────────────────────┤
   │ Device │ Not-Shared: Die CPU ordnet Zugriffe nicht um bezüglich Strongly-Ordered│
   │        │ und Not-Shared-Device Speichertypen. Shared: Die CPU ordnet Zugriffe   │
   │        │ nicht um bezüglich Strongly-Ordered und Shared-Device Speichertypen.   │
   ├────────┼────────────────────────────────────────────────────────────────────────┤
   │Strongly│ Die CPU ordnet Zugriffe nicht um bezüglich Strongly-Ordered und Shared │
   │-Ordered│ sowie Not-Shared-Device Speichertypen.                                 │
   └────────┴────────────────────────────────────────────────────────────────────────┘

   Speicherattribute der MPU

   B: Bufferable: Der Schreibzugriff wird in einem Puffer zwischengespeichert,
     (Write Back) so dass der Prozessor mit der Ausführung der nächsten Operation
                  weitermacht.
   C: Cacheable:  Der vom Speicher gelesene Wert kann in einem Cache gepsichert werden,
                  so dass der nächste Lesezugriff aus dem Cache erfolgen kann.
   XN: Execute Never: Der Prozessor darf keine Befehler aus diesem Speicherbereich lesen
                      und ausführen.
   S: Shareable:  Mehrere Bus-Master (Multi-Core, Externer Cache, DMA, ...) haben Zugriff
                  auf diesen Speicherbereich. So dass z.B. der Cache etwa
                  Multicore-Kohärenz-Protokolle implementieren muss.


   ┌─────────────────────────────────────────────────────────────────────────────────────────────┐
   │ Default System Memory Map. Code/SRAM/RAM are type mpu_mem_NORMAL; Peripheral, Device and    │
   │                            Vendor are type mpu_mem_DEVICE, PPB is type mpu_mem_ORDERED.     │
   ├──────────┬───────────────────────┬──┬─────────┬─────────────────────────────────────────────┤
   │ Name     │ Address Range         │XN│ Unpriv. │ Description                                 │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ Code     │ 0x00000000-0x1FFFFFFF │- │ Allowed │ ROM or flash memory. Boot from 0x00000000.  │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ SRAM     │ 0x20000000-0x3FFFFFFF │- │ Allowed │ On-chip RAM with mpu_cache_WBALLOCATE attr. │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │Peripheral│ 0x40000000-0x5FFFFFFF │XN│ Allowed │ On-chip peripheral address space.           │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ RAM      │ 0x60000000-0x7FFFFFFF │- │ Allowed │ Memory with mpu_cache_WBALLOCATE attribute. │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ RAM      │ 0x80000000-0x9FFFFFFF │- │ Allowed │ Memory with mpu_cache_WT attribute.         │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ Device   │ 0xA0000000-0xBFFFFFFF │XN│ Allowed │ Shared (off-chip) device space.             │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ Device   │ 0xC0000000-0xDFFFFFFF │XN│ Allowed │ Non shared (off-chip) device space.         │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ PPB      │ 0xE0000000-0xE00FFFFF │XN│Bus-Fault│ Strongly-ordered system control space.**    │
   ├──────────┼───────────────────────┼──┼─────────┼─────────────────────────────────────────────┤
   │ Vendor   │ 0xE0100000-0xEFFFFFFF │XN│ Allowed │ ARM recommends this as reserved region.     │
   │          │ 0xF0000000-0xFFFFFFFF │XN│ Allowed │ ARM recommends this for vendor resources.   │
   └──────────┴───────────────────────┴──┴─────────┴─────────────────────────────────────────────┘
    ** SCS_STIR could be accessed with unprivileged rights if configurated so.


   Begriffserklärung

   Read-Allocate:
      Falls ein Wert nicht im Cache liegt, wird neuer Platz allokiert
      und eine ganze Speicherzeile (der Wert selbst in der Regel zuerst) in den Cache geladen.
   Write-through (WT):
      Geschrieben wird simultan in den Cache und den dahinterliegenden Speicher. Sollte der
      Wert nicht im Cache liegen, wird nur in den Speicher geschrieben.
      Normalerweise werden die Werte nicht direkt in den Speicher geschriben, sondern in einen
      1 oder wenige Wort grossen Write-buffer, so dass der Prozessor den nächsten Befehl abarbeiten
      kann, ohne auf das Ende warten zu müssen.
   Write-back (WB):
      Falls der Wert im Cache liegt, wird er nur dort geändert. Das "Dirty-Flag" der Cache-Zeile wird
      gesetzt. "Dirty" Cachezeilen werden nur dann in den Speicher zurückgeschrieben, wenn neuer Platz
      geschaffen werden muss. Liegt der Wert nicht im Cache, wird er direkt in den dahinterliegenden
      Speicher geschrieben unter Umgehung des Caches (No-write allocate).
   Write-allocate (WA):
      Liegt der zu schreibende Wert nicht im Cache, wird zuerst der Wert in den Cache gelesen (Read-Allocate)
      und dann ein WB oder WT durchgeführt.

   Precondition:
    - Include "µC/core.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Common header file <CortexM-Core-Peripherals>.

   file: µC/mpu.h
    Header file <CortexM4-Memory-Protection-unit>.

   file: TODO: mpu.c
    Implementation file <CortexM4-Memory-Protection-unit impl>.
*/
#ifndef CORTEXM4_MC_MPU_HEADER
#define CORTEXM4_MC_MPU_HEADER

/* == HW-Units == */
#define hMPU      ((core_mpu_t*) HW_BASEADDR_MPU)

/* == Typen == */
typedef struct mpu_region_t   mpu_region_t;

typedef enum mpucfg_e {
   mpucfg_NONE = 0,
               // (default) 1. No default background region for privileged access
               //           2. MPU not used in NMI or FAULT interrupts or if FAULTMASK is set (execution priority of thread <= -1)
               //           3. MPU disabled after return of config_mpu

   mpucfg_ALLOWPRIVACCESS = HW_BIT(MPU,CTRL,PRIVDEFENA),
               // Enables the default memory map as a background region for privileged access.
               // The background region acts as region number -1 (lowest priority, will be overwritten with any other region definition).
               // Every privileged access which is not covered by another region uses the default background region.

   mpucfg_USEWITHFAULTPRIORITY = HW_BIT(MPU,CTRL,HFNMIENA),
               // Enables MPU for memory access even for execution priority less than 0
               // (coreinterrupts NMI(-2), FAULT(-1), or FAULTMASK set to 1(-1) with call to setfaultmask_interrupt).
               // With this config any MPUFAULT would cause a lockup of the cpu.

   mpucfg_ENABLE = HW_BIT(MPU,CTRL,ENABLE),
               // Enables MPU at the end of config_mpu.

} mpucfg_e;

typedef enum mpu_access_e {
   mpu_access_NONE = 0,    // No access
   mpu_access_READ = 1,    // Read access
   mpu_access_RW   = 2     // Read and write access
} mpu_access_e;

typedef enum mpu_cache_e {
   mpu_cache_NONE,         // No caching allowed
   mpu_cache_WBALLOCATE,   // Caching with write-back, read and write allocate
   mpu_cache_WT,           // Caching with write-through, no write allocate
   mpu_cache_WB,           // Caching with write-back, no write allocate
} mpu_cache_e;

#define mpu_cache__NROF (mpu_cache_WB+1)


typedef enum mpu_mem_e {
   // ===
   // === following values could be ored to mpu_mem_NORMAL, mpu_mem_NORMAL2
   // ===
   mpu_mem_SHARED = HW_BIT(MPU, RASR, S),
               // Indicates if memory is shared between multiple bus masters or CPUs.
               // Shared requires caches to be entirely transparent to data accesses - but instruction caches always need explicit software management for coherency.
               // If multiple CPU could access unshared memory software must maintain cache coherency between different CPUs.
               // Shared also supports a global monitor to check exclusive access (ldrex/strex) between different CPUs.
               // Unshared only rely on a local monitor to check exclusive access (ldrex/strex) between processes on a single CPU.

   mpu_mem_NOEXEC = HW_BIT(MPU, RASR, XN),
               // RAM contains only data, no program code, CPU not allowed to load instructions from this memory region
   // ===
   // === choose one of the following
   // ===
   mpu_mem_ORDERED = (0 << HW_BIT(MPU, RASR, TEX_POS))|mpu_mem_NOEXEC|mpu_mem_SHARED,
               // Strongly-ordered memory, not cacheable, memory is always shared and not executable
               // Flag mpu_mem_SHARED ignored but included in definition to indicate shared state
               // Access is globally observed in program order with respect to ORDERED, DEVICE and DEVICE_NOTSHARED memory

   mpu_mem_DEVICE  = (0 << HW_BIT(MPU, RASR, TEX_POS))|mpu_mem_NOEXEC|mpu_mem_SHARED|HW_BIT(MPU, RASR, B),
               // Device memory, not cacheable but with possible write-buffer, memory is always shared and not executable
               // Flag mpu_mem_SHARED ignored but included in definition to indicate shared state
               // Access is globally observed in program order with respect to DEVICE and ORDERED memory

   mpu_mem_DEVICE_NOTSHARED = (2 << HW_BIT(MPU, RASR, TEX_POS))|mpu_mem_NOEXEC,
               // Device memory, not cacheable but with possible write-buffer, memory is not shared and not executable
               // Flag mpu_mem_SHARED ignored and not included to indicate absence of shared state
               // Access is globally observed in program order with respect to DEVICE_NOTSHARED and ORDERED memory

#define mpu_mem_NORMAL(/*mpu_cache_e*/ cache_policy) \
         ((mpu_mem_e) ( cache_policy == mpu_cache_NONE       ? (1 << HW_BIT(MPU, RASR, TEX_POS)) : \
                        cache_policy == mpu_cache_WBALLOCATE ? (1 << HW_BIT(MPU, RASR, TEX_POS))|HW_BIT(MPU, RASR, C)|HW_BIT(MPU, RASR, B) : \
                        cache_policy == mpu_cache_WT         ? (0 << HW_BIT(MPU, RASR, TEX_POS))|HW_BIT(MPU, RASR, C) : \
                        /* default case mpu_cache_WB */        (0 << HW_BIT(MPU, RASR, TEX_POS))|HW_BIT(MPU, RASR, C)|HW_BIT(MPU, RASR, B) ))
               // Normal memory ((S/D)RAM,FLASH), cacheability, shared-state and executability is configurable
               // The cache configuration is the same for outer (off-chip) and inner (CPU) cache implementations
               // Access is not observed in program order, synchronisation barriers are needed

#define mpu_mem_NORMAL2(/*mpu_cache_e*/ outer_cache_policy, /*mpu_cache_e*/ inner_cache_policy) \
         ((mpu_mem_e) ( (4 << HW_BIT(MPU, RASR, TEX_POS)) \
                        | ((outer_cache_policy) << HW_BIT(MPU, RASR, TEX_POS)) \
                        | ((inner_cache_policy) << HW_BIT(MPU, RASR, B_POS))  ))
               // Normal memory ((S/D)RAM,FLASH), cacheability, shared-state and executability is configurable
               // The cache configuration is different for outer (other) and inner (CPU) cache implementations
               // Access is not observed in program order, synchronisation barriers are needed
} mpu_mem_e;

typedef enum mpu_size_e {
   mpu_size_32  = 4,
   mpu_size_64,
   mpu_size_128,
   mpu_size_256,
   mpu_size_512,
   mpu_size_1K,
   mpu_size_2K,
   mpu_size_4K,
   mpu_size_8K,
   mpu_size_16K,
   mpu_size_32K,
   mpu_size_64K,
   mpu_size_128K,
   mpu_size_256K,
   mpu_size_512K,
   mpu_size_1MB,
   mpu_size_2MB,
   mpu_size_4MB,
   mpu_size_8MB,
   mpu_size_16MB,
   mpu_size_32MB,
   mpu_size_64MB,
   mpu_size_128MB,
   mpu_size_256MB,
   mpu_size_512MB,
   mpu_size_1GB,
   mpu_size_2GB,
   mpu_size_4GB,
} mpu_size_e;

/* == Funktion == */

static inline mpu_size_e nrbytes2size_mpu(uint32_t size_in_bytes);
static inline uint32_t size2nrbytes_mpu(mpu_size_e size);   // returns 0: size==mpu_size_4GB. >0: size<mpu_size_4GB
static inline int  isavailable_mpu(void);    // returns 1: MPU available 0: MPU not implemented
static inline int  isenabled_mpu(void);      // returns 1: MPU enabled   0: MPU disabled
static inline uint32_t nrregions_mpu(void);  // returns 0: MPU not available >0: Number of supported configurable memory regions
static inline void enable_mpu(mpucfg_e cfg); // Enables MPU or to set new mpucfg_e.
static inline void disable_mpu(void);        // Disables MPU, default memory map is used.
static inline int  config_mpu(uint32_t nrregions/*<=nrregions_mpu*/, const mpu_region_t *regions/*[nrregions]*/, mpucfg_e cfg); // Configures MPU to use nrregions, other regions are disabled.
static inline uint32_t nextfreeregion_mpu(uint32_t firstnr);
static inline void update_mpu(uint32_t firstnr, uint32_t nrregions, const mpu_region_t *regions/*nrregions*/);
static inline void clear_mpu(uint32_t firstnr, uint32_t nrregions);

/* == Definition == */

struct mpu_region_t {
   uint32_t    addr;
   uint32_t    conf;
};

static inline int isvalid_mpuregion(const mpu_region_t *reg)
{
         return (reg->conf & HW_BIT(MPU, RASR, ENABLE)) >> HW_BIT(MPU, RASR, ENABLE_POS);
}

// -- Encoding Scheme --
// VALUE Privileged Unprivleged
// 0b000 No access  No access
// 0b001 RW         No access
// 0b010 RW         Read-Only
// 0b011 RW         RW
// 0b101 Read-Only  No access
// 0b110 Read-Only  Read-Only
#define mpu_region_ENCODE_ACCESS_PRIVILEGE( \
               /*mpu_access_e*/ priv, \
               /*mpu_access_e*/ unpriv) \
         ((((priv)!=0)|(((priv)&mpu_access_READ)<<2)) + (unpriv))
            // !!! Both arguments are evaluated more than once

#define mpu_region_VALIDATE(/*void* */ base_addr, /*mpu_size_e*/ size, /*uint8_t*/ disablesubregions, /*mpu_access_e*/ priv, /*mpu_access_e*/ unpriv) \
         (  (((size) >= mpu_size_32) && ((size) <= mpu_size_4GB)) \
            && ((disablesubregions) == 0 || (size) >= mpu_size_256) \
            && 0 == ((uintptr_t)(base_addr) & ((1u << (size))|((1u << (size))-1))) \
            && mpu_access_RW >= (uint32_t)(priv) && (priv) >= (uint32_t)(unpriv))
            // !!! All arguments are evaluated more than once (except base_addr)

#define mpu_region_INIT( \
               /*void* */ base_addr,   \
               /*mpu_size_e*/ size, \
               /*uint8_t*/ disablesubregions, \
               /*mpu_mem_e*/  type,    \
               /*mpu_access_e*/ privileged, \
               /*mpu_access_e*/ unprivileged) \
         {  .addr = (uintptr_t) (base_addr), \
            .conf = ((type) & (HW_BIT(MPU,RASR,TEX)|HW_BIT(MPU,RASR,S)|HW_BIT(MPU,RASR,C)|HW_BIT(MPU,RASR,B)|HW_BIT(MPU, RASR, XN))) \
                  | (((size) & HW_BIT(MPU,RASR,SIZE_MAX)) << HW_BIT(MPU,RASR,SIZE_POS)) \
                  | (((disablesubregions) & HW_BIT(MPU,RASR,SRD_MAX)) << HW_BIT(MPU,RASR,SRD_POS)) \
                  | (mpu_region_ENCODE_ACCESS_PRIVILEGE(privileged, unprivileged) << HW_BIT(MPU, RASR, AP_POS)) \
                  | (mpu_region_VALIDATE(base_addr, size, disablesubregions, privileged, unprivileged) ? HW_BIT(MPU, RASR, ENABLE) : 0) \
         }
            // !!! Arguments base_addr, size, disablesubregions, privileged and unprivileged are evaluated more than once
            // Initializes a memory region ranging from base_addr to base_addr + size -1. Parameter size
            // is an enumeration type and writing size in this text really does mean the value size2nrbytes_mpu(size).
            // Parameter type describes one of the 3 main memory types (see mpu_mem_e). Parameters privileged and
            // unprivileged describes the access rights of privileged and unprivileged threads. Where unprivileged rights
            // are always less than or equal to the privileged ones.
            // For regions of 256 bytes or larger, the region can be divided up into eight sub-regions of size/8 bytes.
            // The only operation available to a sub-region is to disable it on an individual basis.
            // Every sub-region has an associated disable bit in the 8-bit parameter disablesubregions.
            // When a sub-region is disabled, an access match is required from another region, or privileged background region if enabled.
            // If an access match does not occur a fault is generated.
            // If bit X (0..7) of disablesubregions is set, sub-region X with address range [base_addr+X*size/8..base_addr+(X+1)*size/8-1] is disabled.

#define mpu_region_INITROM(/*void* */ addr, /*mpu_size_e*/ size, /*uint8_t*/ disablesubregions, /*mpu_access_e*/ unprivileged) \
         mpu_region_INIT(addr, size, disablesubregions, mpu_mem_NORMAL(mpu_cache_WT), mpu_access_READ, unprivileged)
            // !!! Arguments are evaluated more than once

#define mpu_region_INITRAM(/*void* */ addr, /*mpu_size_e*/ size, /*uint8_t*/ disablesubregions, /*mpu_access_e*/ privileged, /*mpu_access_e*/ unprivileged) \
         mpu_region_INIT(addr, size, disablesubregions, mpu_mem_NORMAL(mpu_cache_WB)|mpu_mem_SHARED, privileged, unprivileged)
            // !!! Arguments are evaluated more than once

#define mpu_region_INITSRAM(/*mpu_size_e*/ size, /*mpu_access_e*/ unprivileged) \
         mpu_region_INITRAM(0x20000000, size, 0, mpu_access_RW, unprivileged)
            // !!! Arguments are evaluated more than once

#define mpu_region_INITPERIPHERAL(/*mpu_access_e*/ unprivileged) \
         mpu_region_INIT(0x40000000, mpu_size_512MB, 0, mpu_mem_DEVICE, mpu_access_RW, unprivileged)

#define mpu_region_INITSYS(/*mpu_access_e*/ unprivileged) \
         mpu_region_INIT(0xE0000000, mpu_size_512MB, 0, mpu_mem_ORDERED, mpu_access_RW, unprivileged)
            // Address range 0xE0000000-0xFFFFFFFF (includes PPB + vendor specific region). PPB allows only privileged access.
            // Must be assigned a lower region nr (lower array index) than mpu_region_INITPPB cause of overlapping

#define mpu_region_INITPPB() \
         mpu_region_INIT(0xE0000000, mpu_size_1MB, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW/*RW only for STIR*/)
            // Address range 0xE0000000-0xE00FFFFF. No unprivileged access allowed except SCS_STIR if configured.
            // Must be assigned a higher region nr (higher array index) than mpu_region_INITSYS cause of overlapping

#define mpu_region_INITVENDOR(/*mpu_access_e*/ unprivileged) \
         mpu_region_INIT(0xF0000000, mpu_size_256MB, 0, mpu_mem_DEVICE, mpu_access_RW, unprivileged)
            // Address range 0xF0000000-0xFFFFFFFF



/* == Inline == */

static inline void assert_enums_mpu(void)
{
   static_assert( mpu_size_32  == 4);
   static_assert( mpu_size_64  == mpu_size_32+1  && mpu_size_128 == mpu_size_64+1  && mpu_size_256  == mpu_size_128+1);
   static_assert( mpu_size_512 == mpu_size_256+1 && mpu_size_1K  == mpu_size_512+1 && mpu_size_2K   == mpu_size_1K+1);
   static_assert( mpu_size_4K  == mpu_size_2K+1  && mpu_size_8K  == mpu_size_4K+1  && mpu_size_16K  == mpu_size_8K+1);
   static_assert( mpu_size_32K == mpu_size_16K+1 && mpu_size_64K == mpu_size_32K+1 && mpu_size_128K == mpu_size_64K+1);
   static_assert( mpu_size_256K == mpu_size_128K+1 && mpu_size_512K == mpu_size_256K+1 && mpu_size_1MB  == mpu_size_512K+1);
   static_assert( mpu_size_2MB  == mpu_size_1MB+1  && mpu_size_4MB  == mpu_size_2MB+1  && mpu_size_8MB  == mpu_size_4MB+1);
   static_assert( mpu_size_16MB == mpu_size_8MB+1  && mpu_size_32MB == mpu_size_16MB+1 && mpu_size_64MB == mpu_size_32MB+1);
   static_assert( mpu_size_128MB== mpu_size_64MB+1 && mpu_size_256MB== mpu_size_128MB+1&& mpu_size_512MB== mpu_size_256MB+1);
   static_assert( mpu_size_1GB == mpu_size_512MB+1 && mpu_size_2GB  == mpu_size_1GB+1);
   static_assert( mpu_size_4GB == (uint32_t) HW_BIT(MPU, RASR, SIZE_MAX));
   static_assert( mpu_cache_NONE       == 0);
   static_assert( mpu_cache_WBALLOCATE == 1);
   static_assert( mpu_cache_WT         == 2);
   static_assert( mpu_cache_WB         == 3);
}

static inline mpu_size_e nrbytes2size_mpu(uint32_t size_in_bytes)
{
   return size_in_bytes > (1u << 31) ? mpu_size_4GB :
          size_in_bytes > (1u << 30) ? mpu_size_2GB :
          size_in_bytes > (1u << 29) ? mpu_size_1GB :
          size_in_bytes > (1u << 28) ? mpu_size_512MB :
          size_in_bytes > (1u << 27) ? mpu_size_256MB :
          size_in_bytes > (1u << 26) ? mpu_size_128MB :
          size_in_bytes > (1u << 25) ? mpu_size_64MB :
          size_in_bytes > (1u << 24) ? mpu_size_32MB :
          size_in_bytes > (1u << 23) ? mpu_size_16MB :
          size_in_bytes > (1u << 22) ? mpu_size_8MB :
          size_in_bytes > (1u << 21) ? mpu_size_4MB :
          size_in_bytes > (1u << 20) ? mpu_size_2MB :
          size_in_bytes > (1u << 19) ? mpu_size_1MB :
          size_in_bytes > (1u << 18) ? mpu_size_512K :
          size_in_bytes > (1u << 17) ? mpu_size_256K :
          size_in_bytes > (1u << 16) ? mpu_size_128K :
          size_in_bytes > (1u << 15) ? mpu_size_64K :
          size_in_bytes > (1u << 14) ? mpu_size_32K :
          size_in_bytes > (1u << 13) ? mpu_size_16K :
          size_in_bytes > (1u << 12) ? mpu_size_8K :
          size_in_bytes > (1u << 11) ? mpu_size_4K :
          size_in_bytes > (1u << 10) ? mpu_size_2K :
          size_in_bytes > (1u << 9)  ? mpu_size_1K :
          size_in_bytes > (1u << 8)  ? mpu_size_512 :
          size_in_bytes > (1u << 7)  ? mpu_size_256 :
          size_in_bytes > (1u << 6)  ? mpu_size_128 :
          size_in_bytes > (1u << 5)  ? mpu_size_64 :
          mpu_size_32;
}

static inline uint32_t size2nrbytes_mpu(mpu_size_e size)
{
   return 1u << (size+1);
}

/* function: nrregions_mpu
 * Gibt die Anzahl an unterstützten Memory-Regionen wieder.
 * Jede Region beschreibt die Zugriffsrechte, die ein privilegierter bzw. unprivilegierter Prozess auf diese Region hat.
 * Diese Rechte können unterschiedlich sein. */
static inline uint32_t nrregions_mpu(void)
{
   return (hMPU->type & HW_BIT(MPU, TYPE, DREGION)) >> HW_BIT(MPU, TYPE, DREGION_POS);
}

static inline int isavailable_mpu(void)
{
   return nrregions_mpu() > 0;
}

/* function: enable_mpu
 * Schalte MPU ein und erlaube privilegierten Zugriff auf alle nicht definierten Regionen. */
static inline void enable_mpu(mpucfg_e cfg)
{
   hMPU->ctrl = (cfg|mpucfg_ENABLE);
}

static inline void disable_mpu(void)
{
   hMPU->ctrl = 0;
}

static inline int isenabled_mpu(void)
{
   return (hMPU->ctrl >> HW_BIT(MPU, CTRL, ENABLE_POS)) & 1;
}

static inline int config_mpu(uint32_t nrregions, const mpu_region_t *regions, mpucfg_e cfg)
{
   const uint32_t maxnr = nrregions_mpu();
   if (nrregions > maxnr) return EINVAL;
   disable_mpu();
   for (uint32_t i = 0; i < nrregions; ++i) {      // config and enable first nrregions regions (assumes maxnr <= 16)
      static_assert(HW_BIT(MPU,RBAR,REGION) == 0x0f);
      hMPU->rbar = regions[i].addr | HW_BIT(MPU,RBAR,VALID) | i;
      hMPU->rasr = regions[i].conf;
   }
   for (uint32_t i = nrregions; i < maxnr; ++i) {  // disable other regions
      hMPU->rnr  = i;
      hMPU->rasr = 0;
   }
   if ((cfg&mpucfg_ENABLE) != 0) {
      hMPU->ctrl = cfg; // enable configs without ENABLE would result into undefined behaviour
   }
   return 0;
}

/* function: nextfreeregion_mpu
 * Gibt die niedrigste Nummer einer nicht aktivierten Region zurück, die größer oder gleich firstnr ist.
 * Wenn keine Region in [firstnr, nrregions_mpu()-1] frei ist, wird nrregions_mpu() zurückgegeben. */
static inline uint32_t nextfreeregion_mpu(uint32_t firstnr)
{
   const uint32_t maxnr = nrregions_mpu();
   for (uint32_t i = firstnr; i < maxnr; ++i) {
      hMPU->rnr = i;
      if ((hMPU->rasr & HW_BIT(MPU,RASR,ENABLE)) == 0) return i;
   }
   return maxnr;
}

/* function: update_mpu
 * Konfiguriert die Regionen der Nummern [firstnr, firstnr+nrregions-1] neu.
 * Aktivierte Regionen werden mit einer neuen Konfigurationen überschrieben
 * und nicht aktivierte Regionen werden konfiguriert und aktiviert. */
static inline void update_mpu(uint32_t firstnr, uint32_t nrregions, const mpu_region_t *regions)
{
   for (uint32_t i = 0; i < nrregions; ++i) {
      hMPU->rnr  = (firstnr + i);
      hMPU->rasr = 0; // disable
      hMPU->rbar = regions[i].addr;
      hMPU->rasr = regions[i].conf;
   }
}

/* function: clear_mpu
 * Schaltet die Regionen der Nummern [firstnr, firstnr+nrregions-1] aus. */
static inline void clear_mpu(uint32_t firstnr, uint32_t nrregions)
{
   // disable regions
   for (uint32_t i = nrregions; (i--) > 0; ) {
      hMPU->rnr  = (firstnr+i);
      hMPU->rasr = 0;
   }
}

#endif
