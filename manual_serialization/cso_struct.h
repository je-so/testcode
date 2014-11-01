#ifndef CSO_STRUCT_HEADER
#define CSO_STRUCT_HEADER

#include <stddef.h>
#include <stdint.h>

struct cso_buffer_t ;

typedef struct cso_struct_t cso_struct_t ;

typedef struct cso_structmember_t cso_structmember_t ;

#ifdef CONFIG_UNITTEST
int unittest_cso_struct(void) ;
#endif


/* struct: cso_structmember_t
 * Describes type and byte offsets of data member in struct.
 * */
struct cso_structmember_t {
	uint8_t  type ;
	uint16_t offset ;
} ;


/* struct: cso_struct_t
 * Describes C struct as set of data members.
 * */
struct cso_struct_t {
	uint8_t id[3] ;
	size_t  nrmember ;
	// describes types of all members
	cso_structmember_t * member ; // member[nrmember]
	// contains additional infos for every string or binary parameter
	struct cso_structext_t {
		// addr is the start address of an external memory block
		// (addr == 0) ==> cso_structmember_t->offset is used to determine memory address
		uint8_t * addr ;
		// maximum size in bytes of allocated memory for string or binary
		// length of strings is determined by terminating \0 byte (but strlen < size !)
		size_t    size ;
		// (isvar == 1) ==> lenindex is valid ; (isvar == 0) ==> size is used to determine size of member
		// only used for binary parameter
		uint8_t	  isvar ;
		// If valid then use cso_struct_t->member[lenindex] (must be of integer type) to determine
		// size of member; cso_struct_t->member[lenindex] must be smaller or equal to size
		uint16_t  lenindex ;
	}       * extmember ;	// extmember[]
} ;

// group: encode & decode

int cso_struct_encode(const cso_struct_t * type, struct cso_buffer_t * dest/*msg-buffer*/,  const void * src/*struct-data*/) ;

int cso_struct_decode(const cso_struct_t * type, void * dest/*struct-data*/, size_t size, const uint8_t src[size]/*msg-buffer*/) ;

// group: query

int cso_struct_get_typeid(size_t size, const uint8_t data[size], /*out*/uint8_t id[3]) ;

#endif
