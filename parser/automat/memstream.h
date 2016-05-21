/* title: MemoryStream

   Wraps a memory block which points to start and end address.
   Use this structure to stream memory, i.e. read bytes and increment
   the start address to point to the next unread byte.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/memstream.h
    Header file <MemoryStream>.

   file: C-kern/memory/memstream.c
    Implementation file <MemoryStream impl>.
*/
#ifndef CKERN_MEMORY_MEMSTREAM_HEADER
#define CKERN_MEMORY_MEMSTREAM_HEADER

// forward
struct string_t;

// == exported types
struct memstream_t;
struct memstream_ro_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_memstream
 * Test <memstream_t> functionality. */
int unittest_memory_memstream(void);
#endif


/* struct: memstream_ro_t
 * Wie <memstream_t>, erlaubt aber nur lesenden Zugriff auf den Speicher.
 * Zur Ansteuerung können dieselben Funktion, etwa <init_memstream>, Verwendung finden. */
typedef struct memstream_ro_t {
   /* variable: next
    * Points to next unread memory byte. next is always lower or equal to <end>. */
   const uint8_t *next;
   /* variable: end
    * Points one after the last memory byte. The number of unread bytes can be determined
    * by "end - next". If next equals end all bytes are read. */
   const uint8_t *end;
} memstream_ro_t;

// group: lifetime

/* function: initPstr_memstreamro
 * Initialisiert memstr aus Werten von str vom Typ <string_t>. */
void initPstr_memstreamro(/*out*/memstream_ro_t * memstr, const struct string_t * str);

// group: generic

/* function: cast_memstreamro
 * Adaptiere obj Zeiger in Zeiger auf Typ <memstream_ro_t>.
 * Parameter obj muss auf ein generisches Objekt mit zwei Feldern namens
 * nameprefix##next and nameprefix##end zeigen die mit <memstream_ro_t>
 * strukturkompatibel sind. */
memstream_ro_t * cast_memstreamro(void * obj, IDNAME nameprefix);


/* struct: memstream_t
 * Wraps a memory block which points to start and end address.
 * The start address is the lowest address of an allocated memory block.
 * The end address points one after the highest address of the allocated memory block. */
typedef struct memstream_t {
   /* variable: next
    * Points to next unread memory byte. next is always lower or equal to <end>. */
   uint8_t *next;
   /* variable: end
    * Points one after the last memory byte. The number of unread bytes can be determined
    * by "end - next". If next equals end all bytes are read. */
   uint8_t *end;
} memstream_t;

// group: lifetime

/* define: memstream_FREE
 * Static initializer. */
#define memstream_FREE \
         { 0, 0 }

/* define: memstream_INIT
 * Static initializer.
 *
 * Parameter:
 * start - Points to first unread byte.
 * end   - Points one after the last unread byte.
 *         The difference (end - start) is the positive number of the memory block size.
 * */
#define memstream_INIT(start, end) \
         { start, end }

/* function: init_memstream
 * Initialisiert memstr mit start und end. Die Parameter zeigen auf die Startadresse
 * des Speichers und die Speicheradresse eins nach dem letzen Datenbyte.
 * Die Größe des Speicherbereichs in Bytes ergibt sich somit aus (end-start). */
void init_memstream(/*out*/memstream_t * memstr, uint8_t * start, uint8_t * end);

/* function: free_memstream
 * Resets memstr with null pointer. */
void free_memstream(memstream_t * memstr);

// group: query

/* function: isnext_memstream
 * Returns true if there are is at least one more unread bytes. */
bool isnext_memstream(const memstream_t * memstr);

/* function: size_memstream
 * Returns the number of unread bytes. */
size_t size_memstream(const memstream_t * memstr);

/* function: offset_memstream
 * Returns the offset of the next pointer relative to start.
 * Parameter start must be the value given in <init_memstream> or <memstream_INIT>. */
size_t offset_memstream(const memstream_t * memstr, uint8_t * start);

/* function: next_memstream
 * Returns start address of memstr. The start address points to a memory bock
 * whose size is at least <size_memstream> bytes. */
/*const*/ uint8_t * next_memstream(const memstream_t * memstr);

/* function: findbyte_memstream
 * Finds byte in memstr.
 * The returned value points to the position of the found byte.
 * The value 0 is returned if *memstr* does not contain the byte. */
/*const*/ uint8_t * findbyte_memstream(const memstream_t * memstr, uint8_t byte);

// group: update

/* function: skip_memstream
 * Increments memstr->next with len.
 * Use <tryskip_memstream>
 *
 * Unchecked Precondition:
 * len <= size_memstream(memstr) */
void skip_memstream(memstream_t * memstr, size_t len);

/* function: tryskip_memstream
 * Erhöht memstr->next (Speicherstartadresse) um len (Bytes).
 *
 * Return:
 * 0      - memstr->next wurde um len Bytes erhöht.
 * EINVAL - memstr wurde nicht verändert, da <size_memstream> < len. */
int tryskip_memstream(memstream_t * memstr, size_t len);

/* function: nextbyte_memstream
 * Gibt nächstes Byte von memstr zurück.
 * memstr->next wird um eins inkrementiert.
 *
 * Unchecked Precondition:
 * size_memstream(memstr) >= 1 */
uint8_t nextbyte_memstream(memstream_t * memstr);

// group: write

/* function: printf_memstream
 * Writes formatted output with printf style arguments (format, ...) to memstr.
 * At max <size_memstream> bytes (including a terminating \0 byte) are written
 * truncating the formatted output if necessary.
 *
 * If <size_memstream> > 0 the formatted string is written to memstr including a
 * terminating \0 byte even in case of error.
 * Returns:
 * 0       - The formatted string (incl. \0 byte) fits into <size_memstream> bytes and
 *           memstr->next is incremented with the number of written bytes excluding the terminating \0 byte.
 * ENOBUFS - The formatted string was truncated and memstr->next is not incremented. */
int printf_memstream(memstream_t * memstr, const char * format, ...);

/* function: write_memstream
 * Copies len bytes from src to memstr->next and increments next with len.
 *
 * Unchecked Precondition:
 * len <= size_memstream(memstr) */
void write_memstream(memstream_t * memstr, size_t len, const uint8_t src[len]);

/* function: writebyte_memstream
 * Appends byte to memstr, memstr->next is incremented by one.
 *
 * Unchecked Precondition:
 * 1 <= size_memstream(memstr) */
void writebyte_memstream(memstream_t * memstr, uint8_t byte);

// group: generic

/* function: cast_memstream
 * Casts pointer obj into pointer to <memstream_t>.
 * obj must point a generic object with two members nameprefix##next and nameprefix##end
 * of the same type as the members of <memstream_t> and in the same order. */
memstream_t * cast_memstream(void * obj, IDNAME nameprefix);



// section: inline implementation

// group: memstream_ro_t

/* define: cast_memstreamro
 * Implements <memstream_ro_t.cast_memstreamro>. */
#define cast_memstreamro(obj, nameprefix) \
         ( __extension__ ({                              \
            typeof(obj) _obj = (obj);                    \
            /* TODO: use _Generic to check for */        \
            /* uint8_t* == const uint8_t* !!   */        \
            (void) sizeof( (const uint8_t *)0            \
                     == _obj->nameprefix##next);         \
            (void) sizeof( (const uint8_t *)0            \
                     == _obj->nameprefix##end);          \
            static_assert(                               \
                  sizeof(((memstream_ro_t*)0)->next)     \
                  == sizeof(_obj->nameprefix##next)      \
               && sizeof(((memstream_ro_t*)0)->end)      \
                  == sizeof(_obj->nameprefix##end)       \
               && offsetof(memstream_ro_t, end)          \
                  - offsetof(memstream_ro_t, next)       \
                  == (uintptr_t) &_obj->nameprefix##end  \
                  - (uintptr_t) &_obj->nameprefix##next, \
               "offsets and size are compatible");       \
            (memstream_ro_t *)(&_obj->nameprefix##next); \
         }))

/* define: initPstr_memstreamro
 * Implements <memstream_t.initPstr_memstreamro>. */
#define initPstr_memstreamro(memstr, str) \
         do {                                \
            typeof(memstr) _m = (memstr);    \
            string_t *     _s = (str);       \
            _m->next = _s->addr;             \
            _m->end  = _s->addr + _s->size;  \
         } while(0)

// group: memstream_t

/* define: cast_memstream
 * Implements <memstream_t.cast_memstream>. */
#define cast_memstream(obj, nameprefix) \
         ( __extension__ ({                           \
            typeof(obj) _obj = (obj);                 \
            static_assert(                            \
               &((memstream_t*)((uintptr_t)           \
               &_obj->nameprefix##next))->next        \
               == &_obj->nameprefix##next             \
               && &((memstream_t*)((uintptr_t)        \
                  &_obj->nameprefix##next))->end      \
                  == &_obj->nameprefix##end,          \
               "obj is compatible");                  \
            (memstream_t *)(&_obj->nameprefix##next); \
         }))

/* define: findbyte_memstream
 * Implements <memstream_t.findbyte_memstream>. */
#define findbyte_memstream(memstr, byte) \
         ( __extension__ ({                  \
            typeof(memstr) _m = (memstr);    \
            (typeof(_m->next))               \
            memchr(  _m->next,               \
                     (uint8_t)(byte),        \
                     size_memstream(_m));    \
         }))

/* define: free_memstream
 * Implements <memstream_t.free_memstream>. */
#define free_memstream(memstr) \
         do {                             \
            typeof(memstr) _m = (memstr); \
            _m->next = 0;                 \
            _m->end  = 0;                 \
         } while(0)

/* define: init_memstream
 * Implements <memstream_t.init_memstream>. */
#define init_memstream(memstr, _start, _end) \
         do {                                \
            typeof(memstr) _m = (memstr);    \
            _m->next = (_start);             \
            _m->end  = (_end);               \
         } while(0)

/* define: isnext_memstream
 * Implements <memstream_t.isnext_memstream>. */
#define isnext_memstream(memstr) \
         ( __extension__ ({            \
            const typeof(*memstr)* _m; \
            _m = (memstr);             \
            (_m->next != _m->end);     \
         }))

/* define: next_memstream
 * Implements <memstream_t.next_memstream>. */
#define next_memstream(memstr) \
         ((memstr)->next)

/* define: nextbyte_memstream
 * Implements <memstream_t.nextbyte_memstream>. */
#define nextbyte_memstream(memstr) \
         (*((memstr)->next ++))

/* define: offset_memstream
 * Implements <memstream_t.offset_memstream>. */
#define offset_memstream(memstr, start) \
         ((size_t)((memstr)->next - (start)))

/* define: printf_memstream
 * Implements <memstream_t.printf_memstream>. */
#define printf_memstream(memstr, format, ...) \
         ( __extension__ ({                     \
            memstream_t * _m;                   \
            size_t _s;                          \
            int _l;                             \
            _m = (memstr);                      \
            _s = (size_t) (_m->end - _m->next); \
            _l = snprintf( (char*)_m->next, _s, \
                          format, __VA_ARGS__); \
            int _e = ENOBUFS;                   \
            if (0 <= _l && (size_t) _l < _s) {  \
               _m->next += _l;                  \
               _e = 0;                          \
            }                                   \
            _e;                                 \
         }))

/* define: size_memstream
 * Implements <memstream_t.size_memstream>. */
#define size_memstream(memstr)            \
         ( __extension__ ({               \
            const typeof(*memstr)* _m2;   \
            _m2 = (memstr);               \
            (size_t) (_m2->end            \
                      - _m2->next);       \
         }))

/* define: skip_memstream
 * Implements <memstream_t.skip_memstream>. */
#define skip_memstream(memstr, len) \
         do {                             \
            typeof(*memstr)* _m;          \
            const size_t _l = (len);      \
            _m = (memstr);                \
            _m->next += _l;               \
         } while (0)

/* define: tryskip_memstream
 * Implements <memstream_t.tryskip_memstream>. */
#define tryskip_memstream(memstr, len) \
         ( __extension__ ({               \
            typeof(*memstr)* _m;          \
             size_t _l;                   \
            _m = (memstr);                \
            _l = (len);                   \
            int _err = (size_t) (         \
                         _m->end          \
                         - _m->next       \
                       ) < _l;            \
            if (!_err) {                  \
               (_m)->next += _l;          \
            }                             \
            (_err ? EINVAL : 0);          \
         }))


/* define: write_memstream
 * Implements <memstream_t.write_memstream>. */
#define write_memstream(memstr, len, src) \
         do {                             \
            memstream_t * _m;             \
            const size_t _l = (len);      \
            _m = (memstr);                \
            memcpy(_m->next, (src), _l);  \
            _m->next += _l;               \
         } while (0)

/* define: writebyte_memstream
 * Implements <memstream_t.writebyte_memstream>. */
#define writebyte_memstream(memstr, byte) \
         do {                       \
            memstream_t * _m;       \
            _m = (memstr);          \
            _m->next[0] = (byte);   \
            ++ _m->next;            \
         } while (0)

#endif
