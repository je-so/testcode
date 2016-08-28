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

   Ein Zugriff auf eine undefinierte Region oder ein unerlaubter Zugriff lösen
   eine (mpu)Fault Exception aus. Das Register MMFSR (Memory management fault address)
   beschreibt den Grund näher. Dieses Register ist im System control block (SCB) zu finden.

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
   NX: Not Executable: Der Prozessor darf keine Befehler aus diesem Speicherbereich lesen
                       und ausführen.
   S: Shareable:  Mehrere Bus-Master (Multi-Core, Externer Cache, DMA, ...) haben Zugriff
                  auf diesen Speicherbereich. So dass z.B. der Cache etwa
                  Multicore-Kohärenz-Protokolle implementieren muss.

   Unprivilegierter Default Zugriff ist erlaubt für:
   ┌──────────┬───────────────────────┬─────────┐
   │ Code     │ 0x00000000-0x1fffffff │ Erlaubt │
   ├──────────┼───────────────────────┼─────────┤
   │ SRAM     │ 0x20000000-0x3fffffff │ Erlaubt │
   ├──────────┼───────────────────────┼─────────┤
   │Peripherie│ 0x40000000-0x5fffffff │ Erlaubt │
   ├──────────┼───────────────────────┼─────────┤
   │ Ext. RAM │ 0x60000000-0x9fffffff │ Erlaubt │
   ├──────────┼───────────────────────┼─────────┤
   │Ext.Device│ 0xA0000000-0xDfffffff │ Erlaubt │
   ├──────────┼───────────────────────┼─────────┤
   │ PPB      │ 0xE0000000-0xE00fffff │Bus-Fault│ Bestimmte Ausnahmen durch Runtime-Konfiguration möglich.
   ├──────────┼───────────────────────┼─────────┤
   │Hersteller│ 0xE0100000-0xFfffffff │ Erlaubt │ Herstellerspezifische Information
   └──────────┴───────────────────────┴─────────┘

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

   === MPU configuration for a microcontroller

   Usually, a microcontroller system has only a single processor and no caches.
   In such a system, program the MPU as follows:

   Memory region  TEX   C  B  S  Memory type and attributes
   Flash memory   0b000 1  0  0  Normal memory, Non-shareable, write-through
   Internal SRAM  0b000 1  0  1  Normal memory, Shareable, write-through
   External SRAM  0b000 1  1  1  Normal memory, Shareable, write-back, write-allocate
   Peripherals    0b000 0  1  1  Device memory, Shareable

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

// == exported Peripherals/HW-Units

// == exported types
struct mpu_region_t;

typedef enum mpu_rawbit_e {
   mpu_rawbit_XN = 1u << 28, // Not Executable, Maschinenbefehle werden nicht aus Region geladen
   mpu_rawbit_AP = 7u << 24, // Data Access Permissions, Zugriffsrechte (siehe mpu_access_e)
   mpu_rawbit_TEX_100 = 1u << 21, // Type Extension Field (outer policy != inner policy)
   mpu_rawbit_TEX_010 = 1u << 20, // Type Extension Field (outer policy == inner policy, Not shared Device, )
   mpu_rawbit_TEX_001 = 1u << 19, // Type Extension Field (outer policy == inner policy, Normal, uncached / write-allocate)
   mpu_rawbit_TEX_000 = 0u << 19, // Type Extension Field (outer policy == inner policy)
   mpu_rawbit_S  = 1u << 18, // Shared(1), Not-Shared(0)
   mpu_rawbit_C  = 1u << 17, // With Cache(1), No Cache(0)
   mpu_rawbit_B  = 1u << 16, // Write Buffer(1), No Write Buffer(0)
   mpu_rawbit_SRD = 255u << 8, // 8 Sub-Regions ; Setting a bit 1 disables the corresponding sub-region (bit0 == sub-region with lowest addr)
   mpu_rawbit_SIZE = 31u << 1, // Region Size in Bytes: 32, 64, 128, ..., 4GB
   mpu_rawbit_ENABLE     = 1u, // Region enabled
} mpu_rawbit_e;

typedef enum mpu_access_e {
   mpu_access_NONE = 0,
   mpu_access_READ = 1,
   mpu_access_RW   = 3
} mpu_access_e;

typedef enum mpu_memtype_e {
   // system config, strongly-ordered, no caching
   mpu_memtype_ORDERED_SHARED     = mpu_rawbit_TEX_000|mpu_rawbit_S,
   // memory-mapped device, ordered, write buffer
   mpu_memtype_DEVICE_SHARED      = mpu_rawbit_TEX_000|mpu_rawbit_S|mpu_rawbit_B,
   // ram with read cache, write-through, not shared
   mpu_memtype_NORMAL_WT          = mpu_rawbit_TEX_000|mpu_rawbit_C,
   // ram with read cache, write-through, shared
   mpu_memtype_NORMAL_WT_SHARED   = mpu_memtype_NORMAL_WT|mpu_rawbit_S,
   // ram with read cache, write-back, not shared
   mpu_memtype_NORMAL_WB          = mpu_memtype_NORMAL_WT|mpu_rawbit_B,
   // ram with read cache, write-back, shared
   mpu_memtype_NORMAL_WB_SHARED   = mpu_memtype_NORMAL_WB|mpu_rawbit_S,
   // ram with no caching, not shared
   mpu_memtype_NORMAL_NOCACHE     = mpu_rawbit_TEX_001,
   // ram with no caching, shared
   mpu_memtype_NORMAL_NOCACHE_SHARED = mpu_memtype_NORMAL_NOCACHE|mpu_rawbit_S,
   // ram with read cache, write-back, write-allocate, not shared
   mpu_memtype_NORMAL_WBWA        = mpu_rawbit_TEX_001|mpu_rawbit_C|mpu_rawbit_B,
   // ram with read cache, write-back, write-allocate, shared
   mpu_memtype_NORMAL_WBWA_SHARED = mpu_memtype_NORMAL_WBWA|mpu_rawbit_S,
   // memory-mapped device, ordered, write buffer, not shared
   mpu_memtype_DEVICE             = mpu_rawbit_TEX_010,
   // ram with different outer-policy (external-cache) different from inner-policy (CPU), not shared
   mpu_memtype_NORMAL_OUTIN_POLICY = mpu_rawbit_TEX_100,
   // ram with different outer-policy (external-cache) different from inner-policy (CPU), shared
   mpu_memtype_NORMAL_OUTIN_POLICY_SHARED = mpu_memtype_NORMAL_OUTIN_POLICY|mpu_rawbit_S,
   // ===
   // === the following values are only valid if combined with mpu_memtype_NORMAL_OUTIN_POLICY(_SHARED)
   // ===
   // outer-policy: ram with no caching
   mpu_memtype_OUTER_POLICY_NOCACHE = 0,
   // outer-policy: ram with read cache, write-back, write-allocate
   mpu_memtype_OUTER_POLICY_WBWA    = mpu_rawbit_TEX_001,
   // outer-policy: ram with read cache, write-through
   mpu_memtype_OUTER_POLICY_WT      = mpu_rawbit_TEX_010,
   // outer-policy: ram with read cache, write-back
   mpu_memtype_OUTER_POLICY_WB      = mpu_rawbit_TEX_010|mpu_rawbit_TEX_001,
   // inner-policy: ram with no caching
   mpu_memtype_INNER_POLICY_NOCACHE = 0,
   // inner-policy: ram with read cache, write-back, write-allocate
   mpu_memtype_INNER_POLICY_WBWA    = mpu_rawbit_B,
   // inner-policy: ram with read cache, write-through
   mpu_memtype_INNER_POLICY_WT      = mpu_rawbit_C,
   // inner-policy: ram with read cache, write-back
   mpu_memtype_INNER_POLICY_WB      = mpu_rawbit_C|mpu_rawbit_B,
   // ===
   // === the following values are only valid if combined with any other value
   // ===
   // RAM contains only data, no program code, not executable region
   mpu_memtype_DATAONLY = mpu_rawbit_XN
} mpu_memtype_e;

typedef enum mpu_memsize_e {
   mpu_memsize_32  = 4,
   mpu_memsize_64,
   mpu_memsize_128,
   mpu_memsize_256,
   mpu_memsize_512,
   mpu_memsize_1K,
   mpu_memsize_2K,
   mpu_memsize_4K,
   mpu_memsize_8K,
   mpu_memsize_16K,
   mpu_memsize_32K,
   mpu_memsize_64K,
   mpu_memsize_128K,
   mpu_memsize_256K,
   mpu_memsize_512K,
   mpu_memsize_1MB,
   mpu_memsize_2MB,
   mpu_memsize_4MB,
   mpu_memsize_8MB,
   mpu_memsize_16MB,
   mpu_memsize_32MB,
   mpu_memsize_64MB,
   mpu_memsize_128MB,
   mpu_memsize_256MB,
   mpu_memsize_512MB,
   mpu_memsize_1GB,
   mpu_memsize_2GB,
   mpu_memsize_4GB,
} mpu_memsize_e;

/////////
// Definitionen der Standard-Regionen, wie sie in einem Cortex-M4 Single-Prozessor ohne Cache angelegt sind.
/////////
#define MPU_REGION_ROM(size, unpriv_access)  mpu_region_INIT(0, size, mpu_memtype_NORMAL_WT, mpu_access_READ, unpriv_access)
#define MPU_REGION_SRAM(size, unpriv_access) mpu_region_INIT(0x20000000, size, mpu_memtype_NORMAL_WT, mpu_access_RW, unpriv_access)
#define MPU_REGION_DEVICE(unpriv_access)     mpu_region_INIT(0x40000000, mpu_memsize_512MB, mpu_memtype_DEVICE_SHARED|mpu_memtype_DATAONLY, mpu_access_RW, unpriv_access)
#define MPU_REGION_EXTRAM(unpriv_access)     mpu_region_INIT(0x60000000, mpu_memsize_1GB, mpu_memtype_NORMAL_WB, mpu_access_RW, unpriv_access)
#define MPU_REGION_EXTDEVICE(unpriv_access)  mpu_region_INIT(0xA0000000, mpu_memsize_1GB, mpu_memtype_DEVICE_SHARED|mpu_memtype_DATAONLY, mpu_access_RW, unpriv_access)
// MPU_REGION_VENDOR überschneidet sich mit MPU_REGION_PPB ==> MPU_REGION_PPB sollte grössere Regionsnummer erhalten, so dass höhere Priorität
#define MPU_REGION_VENDOR(unpriv_access)     mpu_region_INIT(0xE0000000, mpu_memsize_512MB, mpu_memtype_ORDERED_SHARED|mpu_memtype_DATAONLY, mpu_access_RW, unpriv_access)
// MPU_REGION_PPB überschneidet sich mit MPU_REGION_VENDOR ==> MPU_REGION_PPB sollte grössere Regionsnummer erhalten, so dass höhere Priorität
#define MPU_REGION_PPB(unpriv_access)        mpu_region_INIT(0xE0000000, mpu_memsize_1MB, mpu_memtype_ORDERED_SHARED|mpu_memtype_DATAONLY, mpu_access_RW, unpriv_access)

// == exported functions

static inline int isavailable_mpu(void);
static inline int isenabled_mpu(void);
static inline uint32_t nrregions_mpu(void);
static inline void enable_mpu(void);
static inline void disable_mpu(void);
static inline int config_mpu(uint32_t nrregions, const struct mpu_region_t *config/*[nrregions]*/);
static inline uint32_t nextfreeregion_mpu(uint32_t firstnr);
static inline void update_mpu(uint32_t firstnr, uint32_t nrregions, const struct mpu_region_t *config/*nrregions*/);
static inline void clear_mpu(uint32_t firstnr, uint32_t nrregions);

// == definitions

typedef struct mpu_region_t {
   uint32_t    addr;
   uint32_t    type_access_size;
} mpu_region_t;

#define mpu_region_INIT(base_addr, size, type, privileged_access, unprivileged_access) \
         {  .addr = (base_addr) & ~(HW_REGISTER_BIT_MPU_RBAR_RNR|HW_REGISTER_BIT_MPU_RBAR_VALID), \
            .type_access_size = (type) \
                           | ((size) << HW_REGISTER_BIT_MPU_RASR_SIZE_POS) \
                           | (((0x3321650 >> (4*(privileged_access + (privileged_access&unprivileged_access)))) & HW_REGISTER_BIT_MPU_RASR_AP_BITS) \
                              << HW_REGISTER_BIT_MPU_RASR_AP_POS) \
         }

// Register Offsets

/* MPU_TYPE: MPU type register; Reset value: 0x00000800; Privileged */
#define HW_REGISTER_OFFSET_MPU_TYPE             0x00
/* MPU_CTRL: MPU control register; Reset value: 0x00000000; Privileged */
#define HW_REGISTER_OFFSET_MPU_CTRL             0x04
/* MPU_RNR: MPU region number register; Reset value: 0x00000000; Privileged */
#define HW_REGISTER_OFFSET_MPU_RNR              0x08
/* MPU_RBAR: MPU region base address register; Reset value: 0x00000000; Privileged */
#define HW_REGISTER_OFFSET_MPU_RBAR             0x0C
/* MPU_RASR: MPU region attribute and size register; Reset value: 0x00000000; Privileged */
#define HW_REGISTER_OFFSET_MPU_RASR             0x10

// Register Bits

/* TYPE_DREGION: Bits 15:8: Number of MPU data regions */
#define HW_REGISTER_BIT_MPU_TYPE_DREGION_POS       8u
#define HW_REGISTER_BIT_MPU_TYPE_DREGION_BITS      255u
#define HW_REGISTER_BIT_MPU_TYPE_DREGION_MASK      HW_REGISTER_MASK(MPU,TYPE,DREGION)
/* CTRL_PRIVDEFENA: Enable priviliged software access to default memory map */
#define HW_REGISTER_BIT_MPU_CTRL_PRIVDEFENA        (1u << 2)
/* CTRL_HFNMIENA: Enables the operation of MPU during hard fault, NMI, and FAULTMASK handlers. */
#define HW_REGISTER_BIT_MPU_CTRL_HFNMIENA          (1u << 1)
/* CTRL_ENABLE: Enables the MPU */
#define HW_REGISTER_BIT_MPU_CTRL_ENABLE            (1u << 0)
/* RBAR_RNR: MPU region number valid */
#define HW_REGISTER_BIT_MPU_RBAR_VALID             (1u << 4)
/* RBAR_RNR: Bits 3:0: MPU region number field */
#define HW_REGISTER_BIT_MPU_RBAR_RNR               (15u << 0)
/* RASR_XN: Instruction access disable bit */
#define HW_REGISTER_BIT_MPU_RASR_XN                (1u << 28)
/* RASR_AP: Bits 26:24: Access permission */
#define HW_REGISTER_BIT_MPU_RASR_AP_POS            24u
#define HW_REGISTER_BIT_MPU_RASR_AP_BITS           7u
#define HW_REGISTER_BIT_MPU_RASR_AP_MASK           HW_REGISTER_MASK(MPU,RASR,AP)
/* RASR_TEX: Bits 21:19: extended memory attribute */
#define HW_REGISTER_BIT_MPU_RASR_TEX_POS           19u
#define HW_REGISTER_BIT_MPU_RASR_TEX_BITS          7u
#define HW_REGISTER_BIT_MPU_RASR_TEX_MASK          HW_REGISTER_MASK(MPU,RASR,TEX)
/* RASR_S: Shareable memory attribute */
#define HW_REGISTER_BIT_MPU_RASR_S                 (1u << 18)
/* RASR_C: Cacheable memory attribute */
#define HW_REGISTER_BIT_MPU_RASR_C                 (1u << 17)
/* RASR_B: Write-buffer memory attribute */
#define HW_REGISTER_BIT_MPU_RASR_B                 (1u << 16)
/* RASR_SRD: Bits 15:8: Subregion disable bits. Only supported if region size >= 256. */
#define HW_REGISTER_BIT_MPU_RASR_SRD_POS           8u
#define HW_REGISTER_BIT_MPU_RASR_SRD_BITS          255u
#define HW_REGISTER_BIT_MPU_RASR_SRD_MASK          HW_REGISTER_MASK(MPU,RASR,SRD)
/* RASR_SIZE: Size of the MPU protection region. */
#define HW_REGISTER_BIT_MPU_RASR_SIZE_POS          1u
#define HW_REGISTER_BIT_MPU_RASR_SIZE_BITS         0x1F
#define HW_REGISTER_BIT_MPU_RASR_SIZE_MASK         HW_REGISTER_MASK(MPU,RASR,SIZE)
#define HW_REGISTER_BIT_MPU_RASR_SIZE_MINVAL       4u
#define HW_REGISTER_BIT_MPU_RASR_SIZE_MINSIZE      32u
#define HW_REGISTER_BIT_MPU_RASR_ENABLE            1u
static inline uint32_t HW_REGISTER_BIT_MPU_RASR_SIZE_VALUE(uint32_t size/*in bytes; >= 32*/)
{
   uint32_t value = HW_REGISTER_BIT_MPU_RASR_SIZE_MINVAL;
   /*if (size not power of 2 and size > 32) then select next higher region size*/
   value += (((size-1) & size) != 0) && (size > HW_REGISTER_BIT_MPU_RASR_SIZE_MINSIZE);
   for (size /= HW_REGISTER_BIT_MPU_RASR_SIZE_MINSIZE; size > 1; size /= 2) {
      ++ value;
   }
   return value << HW_REGISTER_BIT_MPU_RASR_SIZE_POS;
}
static inline uint32_t HW_REGISTER_BIT_MPU_RASR_AP_VALUE(mpu_access_e privileged, mpu_access_e unprivileged)
{  // VALUE Privileged Unprivleged
   // 000   No access  No access
   // 001   RW         No access
   // 010   RW         Read-Only
   // 011   RW         RW
   // 101   Read-Only  No access
   // 110   Read-Only  Read-Only
   return ((0x3321650 >> (4*(privileged + (privileged&unprivileged)))) & HW_REGISTER_BIT_MPU_RASR_AP_BITS)
          << HW_REGISTER_BIT_MPU_RASR_AP_POS;
}


// section: inline implementation

/* function: nrregions_mpu
 * Gibt die Anzahl an unterstützten Memory-Regionen wieder.
 * Jede Region beschreibt die Zugriffsrechte, die ein privilegierter bzw. unprivilegierter Prozess auf diese Region hat.
 * Diese Rechte können unterschiedlich sein. */
static inline uint32_t nrregions_mpu(void)
{  // returns 8 !
   return (HW_REGISTER(MPU, TYPE) >> HW_REGISTER_BIT_MPU_TYPE_DREGION_POS) & HW_REGISTER_BIT_MPU_TYPE_DREGION_BITS;
}

static inline int isavailable_mpu(void)
{
   return nrregions_mpu() > 0;
}

/* function: enable_mpu
 * Schalte MPU ein und erlaube privilegierten Zugriff auf alle nicht definierten Regionen. */
static inline void enable_mpu(void)
{
   HW_REGISTER(MPU, CTRL) = HW_REGISTER_BIT_MPU_CTRL_PRIVDEFENA|HW_REGISTER_BIT_MPU_CTRL_ENABLE;
}

static inline void disable_mpu(void)
{
   HW_REGISTER(MPU, CTRL) = 0;
}

static inline int isenabled_mpu(void)
{
   return (HW_REGISTER(MPU, CTRL) & HW_REGISTER_BIT_MPU_CTRL_ENABLE) / HW_REGISTER_BIT_MPU_CTRL_ENABLE;
}

static inline int config_mpu(uint32_t nrregions, const mpu_region_t *config)
{
   const uint32_t maxnr = nrregions_mpu();
   if (nrregions > maxnr) return EINVAL;
   disable_mpu();
   static_assert(HW_REGISTER_BIT_MPU_RBAR_RNR == 0x0f);
   // config and enable first nrregions regions
   for (uint32_t i = 0; i < nrregions; ++i) {
      HW_SREGISTER(MPU, RBAR) = config[i].addr|HW_REGISTER_BIT_MPU_RBAR_VALID|i;
      HW_SREGISTER(MPU, RASR) = config[i].type_access_size|HW_REGISTER_BIT_MPU_RASR_ENABLE;
   }
   // disable other regions
   for (uint32_t i = nrregions; i < maxnr; ++i) {
      HW_SREGISTER(MPU, RNR)  = i;
      HW_SREGISTER(MPU, RASR) = 0;
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
      HW_SREGISTER(MPU, RNR) = i;
      if ((HW_SREGISTER(MPU, RASR) & HW_REGISTER_BIT_MPU_RASR_ENABLE) == 0) return i;
   }
   return maxnr;
}

/* function: update_mpu
 * Konfiguriert die Regionen der Nummern [firstnr, firstnr+nrregions-1] neu.
 * Aktivierte Regionen werden mit einer neuen Konfigurationen überschrieben
 * und nicht aktivierte Regionen werden konfiguriert und aktiviert. */
static inline void update_mpu(uint32_t firstnr, uint32_t nrregions, const mpu_region_t *config)
{
   for (uint32_t i = 0; i < nrregions; ++i) {
      HW_SREGISTER(MPU, RNR)  = (firstnr + i);
      HW_SREGISTER(MPU, RASR) = 0; // disable
      HW_SREGISTER(MPU, RBAR) = config[i].addr;
      HW_SREGISTER(MPU, RASR) = config[i].type_access_size|HW_REGISTER_BIT_MPU_RASR_ENABLE;
   }
}

/* function: clear_mpu
 * Schaltet die Regionen der Nummern [firstnr, firstnr+nrregions-1] aus. */
static inline void clear_mpu(uint32_t firstnr, uint32_t nrregions)
{
   // disable regions
   for (uint32_t i = nrregions; (i--) > 0; ) {
      HW_SREGISTER(MPU, RNR)  = (firstnr+i);
      HW_SREGISTER(MPU, RASR) = 0;
   }
}

#endif
