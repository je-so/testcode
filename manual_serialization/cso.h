#ifndef CSO_HEADER
#define CSO_HEADER

#include <stddef.h>
#include <stdint.h>

typedef enum cso_e {
	CSO_NULL,
	CSO_ARRAY,
	CSO_DICT,
	CSO_BIN,
	CSO_STR,
	CSO_UINT8,
	CSO_UINT16,
	CSO_UINT32,
	CSO_UINT64,
	CSO_DOUBLE
} cso_e ;

typedef struct cso_obj_t cso_obj_t ;

typedef struct cso_elem_t {
	cso_e  type ;     /* type of next element */
	const char * key; /* key of element if container is of type CSO_DICT */
	size_t index ;    /* index of element, beginning with 0 and incrementing */
	union {
		struct {
			uint8_t * data ;
			size_t size ;
		} array ; /* valid if type == CSO_ARRAY */
		struct {
			uint8_t * data ;
			size_t size ;
		} dict ;  /* valid if type == CSO_DICT */
		struct {
			uint8_t * data ;
			size_t size ;
		} bin ;   /* valid if type == CSO_BIN */
		struct {
			char * data ;	/* data == 0 ==> null string ; data != 0 ==> strlen(data) == size-1 */
			size_t size ;	/* size == 0 ==> null string ; size > 0 ==> data[size-1] == 0 */
		} str ;   /* valid if type == CSO_STR */
		uint8_t  u8 ;  /* valid if type == CSO_UINT8 */
		uint16_t u16 ; /* valid if type == CSO_UINT16 */
		uint32_t u32 ; /* valid if type == CSO_UINT32 */
		uint64_t u64 ; /* valid if type == CSO_UINT64 */
		double   dbl ; /* valid if type == CSO_DOUBLE */
	} val ; /* data value only valid, if type != CSO_NULL. */
} cso_elem_t ;

typedef struct cso_iter_t {
	int     iskey ;  /* set to 1 if an object of type CSO_DICT is iterated else 0 */
	uint8_t * next ; /* points to start address of next element */
	uint8_t * end ;  /* points to end address of serialized memory block */
	size_t  index ;  /* index of next element to read */
} cso_iter_t ;


/* C Serialize Object
 *
 * The functions allows you to serialize data into a binary transportation format.
 * The data is serialized in the host byte order. The receiver makes it right, i.e. function
 * cso_load converts the received data to either big or little endian format.
 *
 * All functions return a value != 0 in case of an error else 0.
 * Only function cso_isnext returns 1 in case there are more elements to iterate over else 0.
 *
 * Returned error codes are:
 * EINVAL  - Wrong input parameter.
 * ENOMEM  - Out of memory.
 * E2BIG   - Serialized data is bigger than UINT32_MAX (maximum size of uint32_t).
 * ESRCH   - Function cso_skip_key has not found an element with the given key.
 *
 * Transactional:
 * In case of an error all functions guarantee that no changes are done.
 * Except function cso_del which tries to free all allocated memory and sets parameter cso to 0
 * even in case of an error.
 *
 * Lifetime of cso_obj_t:
 * o Function cso_new creates an empty object of type CSO_ARRAY or CSO_DICT.
 *   It preallocates size_prealloc data bytes for the serialized data (at least 16 bytes).
 * o Function cso_load creates a complex object from existing data of a serialized object.
 *   The data is copied so it could be freed after return of this function.
 * o Function cso_del frees all memory associated with an object of type cso_obj_t.
 *   After return (even in case of an error) cso is set to 0.
 *
 * Adding elements to cso_obj_t:
 * It is only possible to append data to the end of the serialized object.
 * Overwriting or removing data is not possible.
 * Every function takes a key argument which must point to a valid '\0' terminated string
 * in case the type of object is CSO_DICT. The value is ignored in case of type CSO_ARRAY.
 * It is not considered an error if the same key value is added more than once.
 * The key is appended to the internal buffer before the data is appended.
 *
 * o Function cso_add_bin copies size bytes of the memory block with the starting address data
 *   into the internal buffer.
 * o Function cso_add_str appends the \0 terminated C string val into the internal memor buffer.
 *   If val is 0 a null string is appended (size 0).
 * o Function cso_add_null appends a null element which has no data.
 * o Function cso_add_u8 appends a 8bit integer to the internal buffer.
 * o Function cso_add_u16 appends a 16bit integer to the internal buffer.
 * o Function cso_add_u32 appends a 32bit integer to the internal buffer.
 * o Function cso_add_u64 appends a 64bit integer to the internal buffer.
 * o Function cso_add_dbl appends a 64bit double to the internal buffer.
 * o Function cso_add_obj starts a new array or dictionary type within an existing array or dictionary.
 *   After return every add/start function adds to the this array.
 * o Function cso_end ends the last cso_add_obj.
 *   After return every add/start function adds to the array or dictionary
 *   before the last call to cso_add_obj.
 *   The value EINVAL is returned if you try to end the topmost array or dictionary created with cso_new.
 *
 * Query content of cso_obj_t:
 * o Function cso_lasterr returns the last error code occurred. This allows to call several add functions
 *   without checking the error code every time. After the last function call cso_lasterr to check if an
 *   error occurred.
 * o Function cso_get_type returns the value of the type argument used in cso_new or the last call to
 *   cso_add_obj.
 * o Function cso_get_depth returns a value greater or equal to 0. The value gives the number of calls to
 *   cso_add_obj which are not matched by a call to cso_end.
 * o Function cso_get_data returns the start address (lowest) of the internal memory block which
 *   contains the serialized data. Do not free or change the data. The pointer is valid as long as cso_obj_t
 *   is not changed.
 * o Function cso_get_size returns the size of the serialized data stored in the internal memory block.
 * o Function cso_get_string returns in str a \0 terminated C string which represents the content
 *   of cso_obj_t in human readable form. You have to call free(str) to free allocated memory
 *   if you do no longer need the string.
 *
 * Change cso_obj_t:
 * o Function cso_clear_lasterr sets the internal lasterr to 0. The next call to cso_lasterr will return 0.
 *
 * Iterating over elements stored in cso_obj_t:
 * During iteration it is not allowed to add new elements to cso_obj_t.
 * The reason is that adding an element could reallocate the internal memory block
 * wich invalidates all iterators.
 * o Function cso_init_iter initializes an iterator object of type cso_iter_t.
 *   The iterator points to the first element (index 0) contained in a complex object of type CSO_ARRAY or CSO_DICT.
 * o Function cso_init_iter2 initializes an iterator to iterate over a complex object of type CSO_ARRAY or
 *   CSO_DICT returned from a call to cso_next.
 * o Function cso_isnext returns 1 if there is at least one more element which can be read with cso_next
 *   else it returns 0.
 * o Function cso_next returns the next element decoded into cso_elem_t.
 *   If there are no more elements (cso_isnext returns 0) the type CSO_NULL with index set to (size_t)-1 is returned.
 *   To iterate over elements contained in complex elements of type CSO_ARRAY or CSO_DICT
 *   use function cso_init_iter2.
 * o Function cso_skip_key skips elements until one is found whose key matches the value of argument key.
 *   If the next element equals the searched key no element is skipped.
 *   Error EINVAL is returned if the current object is not of type CSO_DICT.
 *   Error ESRCH is returned if there is no element with the given key.
 *   In both error cases the iterator is not changed. If the function succeeds you can read
 *   the found element with a call to cso_next.
 *
 * Structure of the Binary Data Format:
 * The first byte gives the encoding which is either little or big endian.
 * The function cso_load converts to host endian type.
 *
 * Explanation of Notation:
 * *( x1 x2 )     - Repeat non terminal sequence x1 x2 0 or more times
 * value( x1 x2 ) - Repeat non terminal sequence x1 x2 exactly value times
 * (uintXX_t)name - encode XX bit integer with value name
 * ( a1 | a2 )    - Select either a1 or a2.
 * *( (uint8_t)v[x] ) - Means that value v is changing every iteration
 * "comment"      - Additional explanation to the human reader
 * Format:
 *
 * cso_data = ( (uint8_t)0 "big endian" | (uint8_t)1 "little endian" ) cso_obj
 * cso_obj  = cso_obj_nokey | cso_obj_withkey
 * cso_obj_nokey = (uint8_t)CSO_ARRAY (uint32_t)size_in_bytes   *( cso_element ) "all encoded cso_element must use exactly size_in_bytes bytes"
 * cso_obj_withkey = (uint8_t)CSO_DICT (uint32_t)size_in_bytes  *( cso_key cso_element ) "all encoded cso_key cso_element must use exactly size_in_bytes bytes"
 * cso_element = ( cso_null | cso_uint8 | cso_uint16 | cso_uint32 | cso_uint64 | cso_double | cso_binary | cso_string | cso_obj )
 * cso_null   = (uint8_t)CSO_NULL
 * cso_uint8  = (uint8_t)CSO_UINT8  (uint8_t)integer_value
 * cso_uint16 = (uint8_t)CSO_UINT16 (uint16_t)integer_value
 * cso_uint32 = (uint8_t)CSO_UINT32 (uint32_t)integer_value
 * cso_uint64 = (uint8_t)CSO_UINT64 (uint64_t)integer_value
 * cso_double = (uint8_t)CSO_DOUBLE (uint64_t)double_value "ieee 754 ?"
 * cso_binary = (uint8_t)CSO_BIN    (uint32_t)size_in_bytes_of_value  size_in_bytes_of_value( (uint8_t)value[x] )
 * cso_string = (uint8_t)CSO_STR    (uint32_t)size_in_bytes_of_value  size_in_bytes_of_value-1( (uint8_t)value[x] ) (uint8_t)0 "zero byte only allowed at end"
 * cso_key    = (uint32_t)keylen    keylen-1( (uint8_t)key[x] ) (uint8_t)0 "zero byte only allowed at end"
 *
 */

#ifdef CONFIG_UNITTEST
int unittest_cso(void) ;
#endif

// create and free objects
int cso_new(/*out*/cso_obj_t ** cso, cso_e csotype/*CSO_ARRAY or CSO_DICT*/, size_t size_prealloc) ;
int cso_load(/*out*/cso_obj_t ** cso, const void * data, size_t size) ;
int cso_del(cso_obj_t ** cso) ;

// add simple elements

int cso_add_null(cso_obj_t * cso, const char * key) ;
int cso_add_bin(cso_obj_t * cso, const char * key, const void * data, size_t size) ;
int cso_add_str(cso_obj_t * cso, const char * key, const char * val) ;
int cso_add_u8(cso_obj_t * cso, const char * key, uint8_t val) ;
int cso_add_u16(cso_obj_t * cso, const char * key, uint16_t val) ;
int cso_add_u32(cso_obj_t * cso, const char * key, uint32_t val) ;
int cso_add_u64(cso_obj_t * cso, const char * key, uint64_t val) ;
int cso_add_dbl(cso_obj_t * cso, const char * key, double val/*must be compatible to IEEE 754 + same endian as integer types*/) ;

// add complex elements

int cso_add_obj(cso_obj_t * cso, const char * key, cso_e csotype/*CSO_ARRAY or CSO_DICT*/) ;
int cso_end(cso_obj_t * cso) ;

// query

int cso_lasterr(const cso_obj_t * cso) ;
cso_e cso_get_type(const cso_obj_t * cso) ;
size_t cso_get_depth(const cso_obj_t * cso) ;
void * cso_get_data(const cso_obj_t * cso) ;
size_t cso_get_size(const cso_obj_t * cso) ;
int cso_get_string(const cso_obj_t * cso, /*out*/char ** str) ;

// change

void cso_clear_lasterr(cso_obj_t * cso) ;

// iteration

void cso_init_iter(cso_iter_t * iter, const cso_obj_t * cso) ;
int cso_init_iter2(cso_iter_t * iter, const cso_elem_t * elem) ;
int/*bool*/ cso_isnext(cso_iter_t * iter) ;
cso_elem_t cso_next(cso_iter_t * iter) ;
int cso_skip_key(cso_iter_t * iter, const char * key) ;

#endif
