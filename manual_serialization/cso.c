#define _GNU_SOURCE
#include "cso.h"
#include "cso_buffer.h"
#include <endian.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#ifdef CONFIG_UNITTEST
#include "test.h"
#endif


struct cso_obj_t {
	cso_buffer_t buf ;       /* contains serialized data */
	struct {
		uint8_t  type ;  /* CSO_DICT or CSO_ARRAY */
		uint32_t size ;  /* size of contained elements */
	} objheader ;
	size_t    nropen ;       /* number of open CSO_ARRAY or CSO_DICT (value always >= 1) */
	size_t    nrallocated ;  /* number of allocated entries in array objoffset */
	uint32_t  * objoffset ;  /* uint32_t objoffset[nrallocated] ; objoffset[0..nropen-1] are valid
	                          * (start+objoffset[nropen-1]) points to start (object header) of last added object. */
	int       lasterr ;      /* contains the error value of the last occurred error. */
} ;

// private helper

#define SIZEMEMORY()		(cso_buffer_sizeallocated(&cso->buf))

#define SIZEMEMORY_USED()	(cso_buffer_sizeused(&cso->buf))

#define ISFREEMEMORY(size)	(cso_buffer_sizefree(&cso->buf) >= (size))

#if    __BYTE_ORDER == __LITTLE_ENDIAN
#define IS_HOST_LITTLE_ENDIAN	1
#elif  __BYTE_ORDER == __BIG_ENDIAN
#define IS_HOST_LITTLE_ENDIAN	0
#else
#error define __BYTE_ORDER as either __LITTLE_ENDIAN or __BIG_ENDIAN
#endif

static int cso_expand_objoffset(cso_obj_t * cso)
{
	if (cso->nropen == cso->nrallocated) {
		/* nrallocated does not overflow cause
		 * cso->nropen * (1+sizeof(uint32_t)) bytes stored in memory overflow first */
		uint32_t * newobjoffset = (uint32_t*) realloc(cso->objoffset, cso->nrallocated*(2*sizeof(uint32_t))) ;
		if (!newobjoffset) return ENOMEM ;
		cso->objoffset    = newobjoffset ;
		cso->nrallocated *= 2 ;
		memset(cso->objoffset + cso->nropen, 0, cso->nropen * sizeof(cso->objoffset[0])) ;
	}
	return 0 ;
}

// implementation

int cso_new(/*out*/cso_obj_t ** cso, cso_e csotype/*CSO_ARRAY or CSO_DICT*/, size_t size_prealloc)
{
	int err ;

	// check in param
	if (CSO_ARRAY != csotype && CSO_DICT != csotype) {
		return EINVAL ;
	}

	if (size_prealloc >= UINT32_MAX) {
		return E2BIG ;
	}

	if (size_prealloc < 16) {
		size_prealloc = 16 ;
	}

	// allocate memory
	cso_buffer_t buf ;
	cso_buffer_init(&buf) ;
	err = cso_buffer_expand(&buf, size_prealloc) ;
	if (err) return err ;

	uint32_t  *  objoffset = (uint32_t*) malloc(16*sizeof(uint32_t)) ;
	cso_obj_t *  newcso    = (cso_obj_t*) malloc(sizeof(cso_obj_t)) ;

	if (!newcso || !objoffset) {
		free(objoffset) ;
		free(newcso) ;
		cso_buffer_free(&buf) ;
		return ENOMEM ;
	}

	/* init newcso */
	newcso->buf = buf ;
	newcso->objheader.type = (uint8_t) csotype ;
	newcso->objheader.size = 0 ;
	newcso->nropen = 1 ;
	newcso->nrallocated = 16 ;
        memset(objoffset, 0, 16*sizeof(objoffset[0])) ;
	newcso->objoffset    = objoffset ;
        newcso->lasterr      = 0 ;
	newcso->objoffset[0] = 1 ;
	newcso->buf.start[0] = IS_HOST_LITTLE_ENDIAN ;
	newcso->buf.start[1] = (uint8_t) csotype ;
	memset(newcso->buf.start+2, 0, sizeof(uint32_t)) ;
	newcso->buf.next    += 1 + (1+sizeof(uint32_t)) ;

	/* out */
	*cso = newcso ;

	return 0 ;
}

int cso_del(cso_obj_t ** cso)
{
	if (*cso) {
		cso_buffer_free(&(*cso)->buf) ;
		free((*cso)->objoffset) ;
		(*cso)->objoffset = 0 ;
		free(*cso) ;
		*cso = 0 ;
	}

	return 0 ;
}

int cso_lasterr(const cso_obj_t * cso)
{
	return cso->lasterr ;
}

cso_e cso_get_type(const cso_obj_t * cso)
{
	return cso->objheader.type ;
}

size_t cso_get_depth(const cso_obj_t * cso)
{
	return cso->nropen-1 ;
}

void * cso_get_data(const cso_obj_t * cso)
{
	memcpy(cso->buf.start+cso->objoffset[cso->nropen-1]+1, &cso->objheader.size, sizeof(cso->objheader.size)) ;
	return cso->buf.start ;
}

size_t cso_get_size(const cso_obj_t * cso)
{
	return SIZEMEMORY_USED() ;
}

typedef struct stringbuffer_t {
	size_t next ;
	size_t allocated ;
	char   * buffer ;
} stringbuffer_t ;


static int cso_print_obj(cso_iter_t * iter, stringbuffer_t * strbuf, /*out*/size_t * nrelem, unsigned depth)
{
#define BUFSIZE (strbuf->allocated-strbuf->next)
#define BUFFER 	strbuf->buffer+strbuf->next, BUFSIZE
	enum state_e { STATE_BEGIN, STATE_KEY, STATE_ELEMENT, STATE_SEPARATOR, STATE_END } ;
	enum state_e state = STATE_BEGIN ;
	cso_elem_t   elem  = { .type = CSO_NULL } ;
	const char   * spaces = "                " ;
	size_t       binsize ;
	int          nrchars = 0 ;

	*nrelem = 0 ;

	for (;;) {
		for (;;) {
			switch (state) {
			case STATE_BEGIN:
				if (cso_isnext(iter))
					nrchars = snprintf(BUFFER, "%c\n", iter->iskey ? '{' : '[') ;
				else
					nrchars = snprintf(BUFFER, "%s", iter->iskey ? "{ }" : "[ ]") ;
				break ;
			case STATE_KEY:
				nrchars = 0 ;
				if (elem.key) {
					nrchars = snprintf(BUFFER, "%.*s\"%.256s\": ", depth, spaces, elem.key) ;
				} else {
					nrchars = snprintf(BUFFER, "%.*s", depth, spaces) ;
				}
				break ;
			case STATE_ELEMENT:
				++ *nrelem ;
				switch (elem.type) {
				case CSO_NULL:
					nrchars = snprintf(BUFFER, "null") ;
					break ;
				case CSO_ARRAY:
				case CSO_DICT:
					{
						size_t     subnrelem ;
						size_t     oldnext  = strbuf->next ;
						unsigned   subdepth = depth < 16 ? depth+1 : 16u ;
						cso_iter_t iter2 ;
						cso_init_iter2(&iter2, &elem) ;

						if (cso_print_obj(&iter2, strbuf, &subnrelem, subdepth)) goto ONABORT ;
						if (subnrelem < 32 && strbuf->next <= oldnext + 32 + subdepth + subdepth*subnrelem) {
							// remove all '\n'
							size_t endnext = strbuf->next ;
							for (strbuf->next = ++oldnext; oldnext < endnext; ++oldnext) {
								if (strbuf->buffer[oldnext] == '\n') {
									strbuf->buffer[strbuf->next ++] = ' ' ;
									while (' ' == strbuf->buffer[oldnext+1])
										++ oldnext ;
									continue ;
								}
								strbuf->buffer[strbuf->next ++] = strbuf->buffer[oldnext] ;
							}
							// strbuf->buffer[strbuf->next] != 0 but STATE_END adds \0 byte
						}
					}
					nrchars = 0 ;
					break ;
				case CSO_BIN:
					binsize  = elem.val.bin.size > 16 ? 16 : elem.val.bin.size ;
					nrchars  = (int) binsize * 5 ;
					if (nrchars) -- nrchars ;
					if ((unsigned)nrchars < BUFSIZE) {
						char * buffer = strbuf->buffer + strbuf->next ;
						for (unsigned i = 0; i < binsize; ++i) {
							int hex ;
							if (i) buffer[5*i-1] = ',' ;
							buffer[5*i] = '0' ;
							buffer[5*i+1] = 'x' ;
							hex = (((uint8_t*)elem.val.bin.data)[i] / 16) ;
							buffer[5*i+2] = (char) (hex + (hex <= 9 ? '0' : 'A'-10)) ;
							hex = (((uint8_t*)elem.val.bin.data)[i] & 15) ;
							buffer[5*i+3] = (char) (hex + (hex <= 9 ? '0' : 'A'-10)) ;
						}
					}
					break ;
				case CSO_STR:
					if (0 == elem.val.str.size) {
						nrchars = snprintf(BUFFER, "null") ;
					} else {
						unsigned maxi = (elem.val.str.size-1 > 256 ? 256 : elem.val.str.size-1) ;
						nrchars = 2 ;
						for (unsigned i = 0; i < maxi; ++i) {
							char chr = elem.val.str.data[i] ;
							if (chr == '\n' || chr == '\r'
							    || chr == '\t' || chr == '"')
								nrchars += 2 ;
							else if ((unsigned char)chr < ' ')
								nrchars += 4 ;
							else
								nrchars += 1 ;
						}
						if ((unsigned)nrchars < BUFSIZE) {
							char * buffer = strbuf->buffer + strbuf->next ;
							buffer[0] = '"' ;
							++ buffer ;
							for (unsigned i = 0; i < maxi; ++i) {
								char chr = elem.val.str.data[i] ;
								if (chr == '\n') {
									memcpy(buffer, "\\n", 2) ;
									buffer += 2 ;
								} else if (chr == '\r') {
									memcpy(buffer, "\\r", 2) ;
									buffer += 2 ;
								} else if (chr == '\t') {
									memcpy(buffer, "\\t", 2) ;
									buffer += 2 ;
								} else if (chr == '"') {
									memcpy(buffer, "\\\"", 2) ;
									buffer += 2 ;
								} else if ((unsigned char)chr < ' ') {
									int hex = ((unsigned char)chr / 16) ;
									memcpy(buffer, "\\x", 2) ;
									buffer[2] = (char) (hex + (hex <= 9 ? '0' : 'A'-10)) ;
									hex = ((unsigned char)chr & 15) ;
									buffer[3] = (char) (hex + (hex <= 9 ? '0' : 'A'-10)) ;
									buffer += 4 ;
								} else {
										buffer[0] = chr ;
										buffer += 1 ;
								}
							}
							buffer[0] = '"' ;
							buffer[1] = 0 ;
						}
					}
					break;
				case CSO_UINT8:
					nrchars = snprintf(BUFFER, "%"PRIu8, elem.val.u8) ;
					break ;
				case CSO_UINT16:
					nrchars = snprintf(BUFFER, "%"PRIu16, elem.val.u16) ;
					break ;
				case CSO_UINT32:
					nrchars = snprintf(BUFFER, "%"PRIu32, elem.val.u32) ;
					break ;
				case CSO_UINT64:
					nrchars = snprintf(BUFFER, "%"PRIu64, elem.val.u64) ;
					break ;
				case CSO_DOUBLE:
					nrchars = snprintf(BUFFER, "%g", elem.val.dbl) ;
					break ;
				}
				break ;
			case STATE_SEPARATOR:
				nrchars = snprintf(BUFFER, ",\n") ;
				break ;
			case STATE_END:
				nrchars = snprintf(BUFFER, "\n%.*s%c", depth-1, spaces, iter->iskey ? '}' : ']') ;
				break ;
			}
			// sprintf error ?
			if (nrchars < 0) goto ONABORT ;

			if ((unsigned)nrchars < BUFSIZE) {
				// buffer big enough
				strbuf->next += (unsigned)nrchars ;
				break ;

			} else {
				// grow buffer and retry
				if (strbuf->allocated >= ((size_t)-1)/2) goto ONABORT ;
				strbuf->allocated *= 2 ;
				void * newbuffer = realloc(strbuf->buffer, strbuf->allocated) ;
				if (!newbuffer) goto ONABORT ;
				strbuf->buffer = newbuffer ;
			}
		}

		switch (state) {
		case STATE_BEGIN:
			if (cso_isnext(iter)) {
				elem  = cso_next(iter) ;
				state = STATE_KEY ;
			} else {
				return 0 ;
			}
			break ;
		case STATE_KEY:
			state = STATE_ELEMENT ;
			break ;
		case STATE_ELEMENT:
			if (cso_isnext(iter))
				state = STATE_SEPARATOR ;
			else
				state = STATE_END ;
			break ;
		case STATE_SEPARATOR:
			elem  = cso_next(iter) ;
			state = STATE_KEY ;
			break ;
		case STATE_END:
			return 0 ;
		}
	}
#undef BUFSIZE
#undef BUFFER

ONABORT:
	return ENOMEM ;
}

int cso_get_string(const cso_obj_t * cso, /*out*/char ** str)
{
	stringbuffer_t strbuf = { 0, SIZEMEMORY_USED(), malloc(SIZEMEMORY_USED()) } ;
	void         * smallestbuffer ;
	cso_iter_t     iter ;
	size_t         nrelem ;

	if (!strbuf.buffer) goto ONABORT ;

	cso_init_iter(&iter, cso) ;

	if (cso_print_obj(&iter, &strbuf, &nrelem, 1)) goto ONABORT ;

	smallestbuffer = realloc(strbuf.buffer, strbuf.next + 1) ;
	if (smallestbuffer) {
		strbuf.buffer = smallestbuffer ;
	}

	*str = strbuf.buffer ;

	return 0 ;
ONABORT:
	free(strbuf.buffer) ;
	return ENOMEM ;
}

void cso_clear_lasterr(cso_obj_t * cso)
{
	cso->lasterr = 0 ;
}

#define IMPLEMENT_ADD(elemtype, ptrval, sizeval, issize) \
	size_t    keylen    = 0 ; \
	size_t    elemsize  = (issize ? 1+sizeof(uint32_t) : 1) + sizeval ; \
	uint32_t  objsize ; \
	if (sizeval >= UINT32_MAX-2*sizeof(uint32_t)) { err = E2BIG ; goto ONABORT ; } \
	if (cso->objheader.type == CSO_DICT) { \
		if (0 == key || 0 == (keylen = strlen(key))) { \
			err = EINVAL ; \
			goto ONABORT ; \
		} \
		++ keylen ; \
		elemsize += sizeof(uint32_t) + keylen ; \
		if (elemsize < keylen) { err = E2BIG ; goto ONABORT ; } \
	} \
	if (! ISFREEMEMORY(elemsize)) { \
		err = cso_buffer_expand(&cso->buf, elemsize) ; \
		if (err) goto ONABORT ; \
	} \
	cso->objheader.size += elemsize ; \
	if (keylen) { \
		objsize = (uint32_t) keylen ; \
		memcpy(cso->buf.next, &objsize, sizeof(objsize)) ; \
		cso->buf.next += sizeof(objsize) ; \
		memcpy(cso->buf.next, key, keylen) ; \
		cso->buf.next += keylen ; \
	} \
	cso->buf.next[0] = elemtype ; \
	++ cso->buf.next ; \
	if (issize) { \
		objsize = (uint32_t) sizeval ; \
		memcpy(cso->buf.next, &objsize, sizeof(objsize)) ; \
		cso->buf.next += sizeof(objsize) ; \
	} \
	memcpy(cso->buf.next, ptrval, sizeval) ; \
	cso->buf.next += sizeval

int cso_add_null(cso_obj_t * cso, const char * key)
{
	int err ;
	IMPLEMENT_ADD(CSO_NULL, &err/*dummy*/, 0, 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_bin(cso_obj_t * cso, const char * key, const void * data, size_t size)
{
	int err ;
	IMPLEMENT_ADD(CSO_BIN, data, size, 1) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_str(cso_obj_t * cso, const char * key, const char * val)
{
	int err ;
	size_t len = val ? strlen(val) + 1u : 0 ;
	IMPLEMENT_ADD(CSO_STR, val, len, 1) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_u8(cso_obj_t * cso, const char * key, uint8_t val)
{
	int err ;
	IMPLEMENT_ADD(CSO_UINT8, &val, sizeof(val), 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_u16(cso_obj_t * cso, const char * key, uint16_t val)
{
	int err ;
	IMPLEMENT_ADD(CSO_UINT16, &val, sizeof(val), 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_u32(cso_obj_t * cso, const char * key, uint32_t val)
{
	int err ;
	IMPLEMENT_ADD(CSO_UINT32, &val, sizeof(val), 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_u64(cso_obj_t * cso, const char * key, uint64_t val)
{
	int err ;
	IMPLEMENT_ADD(CSO_UINT64, &val, sizeof(val), 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_dbl(cso_obj_t * cso, const char * key, double val)
{
	int err ;
	/* double must be compatible to IEEE 754 + same endian as integer types */
	IMPLEMENT_ADD(CSO_DOUBLE, &val, sizeof(val), 0) ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_add_obj(cso_obj_t * cso, const char * key, cso_e csotype)
{
	int err ;
	uint32_t val = 0 ;
	if (csotype != CSO_ARRAY && csotype != CSO_DICT) {
		err = EINVAL ;
		goto ONABORT ;
	}
	err = cso_expand_objoffset(cso) ;
	if (err) goto ONABORT ;
	IMPLEMENT_ADD(csotype, &val, sizeof(val), 0) ;
	memcpy(cso->buf.start+cso->objoffset[cso->nropen-1]+1, &cso->objheader.size, sizeof(cso->objheader.size)) ;
	cso->objoffset[cso->nropen] = (uint32_t) ((size_t)(cso->buf.next - cso->buf.start) - (1+sizeof(uint32_t))) ;
	++ cso->nropen ;
	cso->objheader.type = csotype ;
	cso->objheader.size = 0 ;
	return 0 ;
ONABORT:
	cso->lasterr = err ;
	return err ;
}

int cso_end(cso_obj_t * cso)
{
	uint32_t parentsize ;
	if (cso->nropen <= 1) goto ONABORT ;
	-- cso->nropen ;
	// safe cached object size
	memcpy(cso->buf.start+cso->objoffset[cso->nropen]+1, &cso->objheader.size, sizeof(cso->objheader.size)) ;
	cso->objoffset[cso->nropen] = 0 ;
	memcpy(&parentsize, cso->buf.start+cso->objoffset[cso->nropen-1]+1, sizeof(parentsize)) ;
	cso->objheader.type  = *(cso->buf.start+cso->objoffset[cso->nropen-1]) ;
	cso->objheader.size += parentsize ;
	return 0 ;
ONABORT:
	cso->lasterr = EINVAL ;
	return EINVAL ;
}

void cso_init_iter(cso_iter_t * iter, const cso_obj_t * cso)
{
	iter->iskey = (cso->buf.start[1] == CSO_DICT) ;
	iter->next  = cso->buf.start + 1 /*skip endian marker*/ + (1+sizeof(uint32_t)) /*skip object header*/ ;
	iter->end   = cso->buf.next ;
	iter->index = 0 ;
}

int cso_init_iter2(cso_iter_t * iter, const cso_elem_t * elem)
{
	if (elem->type == CSO_ARRAY) {
		iter->iskey = 0 ;
		iter->next  = elem->val.array.data ;
		iter->end   = elem->val.array.data + elem->val.array.size ;

	} else if (elem->type == CSO_DICT) {
		iter->iskey = 1 ;
		iter->next  = elem->val.dict.data ;
		iter->end   = elem->val.dict.data + elem->val.dict.size ;

	} else {
		return EINVAL ;
	}

	iter->index = 0 ;

	return 0 ;
}

int/*bool*/ cso_isnext(cso_iter_t * iter)
{
	return iter->next < iter->end ;
}

cso_elem_t cso_next(cso_iter_t * iter)
{
	cso_e    type = CSO_NULL ;
	char     * key  = 0 ;
	uint8_t  * data ;
	uint16_t u16 ;
	uint32_t u32 ;
	uint32_t size ;
	uint64_t u64 ;
	double   dbl ;

	if (iter->iskey) {
		uint32_t keylen ;
		if ((size_t)(iter->end - iter->next) < sizeof(uint32_t)) goto ONABORT ;
		memcpy(&keylen, iter->next, sizeof(keylen)) ;
		iter->next += sizeof(uint32_t) ;
		if (keylen && (size_t)(iter->end - iter->next) >= keylen && iter->next[keylen-1] == 0) {
			key = (char*) iter->next ;
			iter->next += keylen ;
		} else {
			goto ONABORT ;
		}
	}

	if (iter->next >= iter->end) goto ONABORT ;

	type = *iter->next++ ;

	switch (type) {
	case CSO_NULL:
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { { 0, 0 } } } ;
	case CSO_ARRAY:
	case CSO_DICT:
	case CSO_BIN:
		if ((size_t)(iter->end - iter->next) < sizeof(size)) break ;
		memcpy(&size, iter->next, sizeof(size)) ;
		iter->next += sizeof(size) ;
		if ((size_t)(iter->end - iter->next) < size) break ;
		data = iter->next ;
		iter->next += size ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { { data, size } } } ;
	case CSO_STR:
		if ((size_t)(iter->end - iter->next) < sizeof(size)) break ;
		memcpy(&size, iter->next, sizeof(size)) ;
		iter->next += sizeof(size) ;
		if ((size_t)(iter->end - iter->next) < size) break ;
		data = size ? iter->next : 0 ;
		iter->next += size ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { { data, size } } } ;
	case CSO_UINT8:
		if (iter->end <= iter->next) break ;
		data = iter->next ;
		++ iter->next ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { .u8 = *data } } ;
	case CSO_UINT16:
		if ((size_t)(iter->end - iter->next) < sizeof(uint16_t)) break ;
		memcpy(&u16, iter->next, sizeof(uint16_t)) ;
		iter->next += sizeof(uint16_t) ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { .u16 = u16 } } ;
	case CSO_UINT32:
		if ((size_t)(iter->end - iter->next) < sizeof(uint32_t)) break ;
		memcpy(&u32, iter->next, sizeof(uint32_t)) ;
		iter->next += sizeof(uint32_t) ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { .u32 = u32 } } ;
	case CSO_UINT64:
		if ((size_t)(iter->end - iter->next) < sizeof(uint64_t)) break ;
		memcpy(&u64, iter->next, sizeof(uint64_t)) ;
		iter->next += sizeof(uint64_t) ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { .u64 = u64 } } ;
	case CSO_DOUBLE:
		if ((size_t)(iter->end - iter->next) < sizeof(double)) break ;
		memcpy(&dbl, iter->next, sizeof(double)) ;
		iter->next += sizeof(double) ;
		return (cso_elem_t) { .type = type, .key = key, .index = (iter->index ++), .val = { .dbl = dbl } } ;
	}

ONABORT:
	iter->next = iter->end ;
	return (cso_elem_t) { .type = CSO_NULL, .key = 0, .index = (size_t)-1, .val = { { 0, 0 } } } ;
}

int cso_skip_key(cso_iter_t * iter, const char * key)
{
	if (! iter->iskey) return EINVAL ;

	cso_iter_t iter2 = *iter ;

	for (;;) {
		void * oldnext = iter2.next ;
		cso_elem_t elem = cso_next(&iter2) ;
		if (!elem.key) break ;
		if (0 == strcmp(elem.key, key)) {
			iter->next  = oldnext ;
			iter->index = iter2.index - 1 ;
			return 0 ;
		}
	}

	return ESRCH ;
}

#define convert16(value, memaddr) \
	do { \
		memcpy(&value, (memaddr), sizeof(value)) ; \
		if (isconvert) { \
			if (IS_HOST_LITTLE_ENDIAN) value = be16toh(value) ; else value = le16toh(value) ; \
			memcpy((memaddr), &value, sizeof(value)) ; \
		} \
	} while (0)

#define convert32(value, memaddr) \
	do { \
		memcpy(&value, (memaddr), sizeof(value)) ; \
		if (isconvert) { \
			if (IS_HOST_LITTLE_ENDIAN) value = be32toh(value) ; else value = le32toh(value) ; \
			memcpy((memaddr), &value, sizeof(value)) ; \
		} \
	} while (0)

#define convert64(value, memaddr) \
	do { \
		memcpy(&value, (memaddr), sizeof(value)) ; \
		if (isconvert) { \
			if (IS_HOST_LITTLE_ENDIAN) value = be64toh(value) ; else value = le64toh(value) ; \
			memcpy((memaddr), &value, sizeof(value)) ; \
		} \
	} while (0)

static size_t cso_validate_key(int isconvert, uint8_t * start, size_t size)
{
	uint32_t  objsize ;

	if (size < sizeof(objsize)) return 0 ;

	convert32(objsize, start) ;

	if (objsize < 2 || objsize > size-sizeof(objsize) || start[sizeof(objsize)-1+objsize] != 0) return 0 ;

	if (memchr(start+sizeof(objsize), 0, objsize) != start+sizeof(objsize)-1+objsize) return 0 ;

	return sizeof(objsize) + objsize ;
}

static size_t cso_validate_bin(int isconvert, uint8_t * start, size_t size)
{
	uint32_t  objsize ;

	if (size < 1+sizeof(objsize)) return 0 ;

	convert32(objsize, start+1) ;

	if (objsize > size-sizeof(objsize)-1) return 0 ;

	return 1 + sizeof(objsize) + objsize ;
}

static size_t cso_validate_str(int isconvert, uint8_t * start, size_t size)
{
	uint32_t  objsize ;

	if (size < 1+sizeof(objsize)) return 0 ;

	convert32(objsize, start+1) ;

	if (objsize > size-sizeof(objsize)-1) return 0 ;

	if (0 != objsize && memchr(start+1+sizeof(objsize), 0, objsize) != start+sizeof(objsize)+objsize) return 0 ;

	return 1 + sizeof(objsize) + objsize ;
}

static size_t cso_validate_obj(int isconvert, uint8_t * start, size_t size)
{
	int       iskey ;
	size_t    offset = 1 + sizeof(uint32_t) ;
	size_t    validsize ;
	uint32_t  objsize ;

	if (size < 1+sizeof(uint32_t)) return 0 ;

	iskey = (start[0] == CSO_DICT) ;

	convert32(objsize, start+1) ;
	if (objsize > size-offset) return 0 ;

	objsize += offset ;

	while (offset < objsize) {
		if (iskey) {
			validsize = cso_validate_key(isconvert, start + offset, objsize - offset) ;
			if (!validsize) return 0 ;
			offset += validsize ;
			if (offset >= objsize) return 0 ;
		}

		switch ((cso_e)start[offset]) {
		case CSO_NULL:
			validsize = 1 ;
			break ;
		case CSO_ARRAY:
		case CSO_DICT:
			validsize = cso_validate_obj(isconvert, start + offset, objsize - offset) ;
			break ;
		case CSO_BIN:
			validsize = cso_validate_bin(isconvert, start + offset, objsize - offset) ;
			break ;
		case CSO_STR:
			validsize = cso_validate_str(isconvert, start + offset, objsize - offset) ;
			break ;
		case CSO_UINT8:
			if (objsize - offset < 2) return 0 ;
			validsize = 2 ;
			break ;
		case CSO_UINT16:
			if (objsize - offset < 1+sizeof(uint16_t)) return 0 ;
			if (isconvert) {
				uint16_t value ;
				convert16(value, start+offset+1) ;
			}
			validsize = 1+sizeof(uint16_t) ;
			break ;
		case CSO_UINT32:
			if (objsize - offset < 1+sizeof(uint32_t)) return 0 ;
			if (isconvert) {
				uint32_t value ;
				convert32(value, start+offset+1) ;
			}
			validsize = 1+sizeof(uint32_t) ;
			break ;
		case CSO_UINT64:
			if (objsize - offset < 1+sizeof(uint64_t)) return 0 ;
			if (isconvert) {
				uint64_t value ;
				convert64(value, start+offset+1) ;
			}
			validsize = 1+sizeof(uint64_t) ;
			break ;
		case CSO_DOUBLE:
			if (objsize - offset < 1+sizeof(double)) return 0 ;
			if (isconvert) {
				uint64_t value ;
				convert64(value, start+offset+1) ;
			}
			validsize = 1+sizeof(double) ;
			break ;
		default: /* unknown type */
			return 0 ;
		}
		offset += validsize ;
		if (!validsize) return 0 ;
	}

	return offset ;
}

int cso_load(/*out*/cso_obj_t ** cso, const void * data, size_t size)
{
	int err ;
	cso_obj_t     * newcso = 0 ;
	const uint8_t * start  = data ;
	uint32_t        validsize ;

	if (size < 1+1+sizeof(uint32_t) || start[0] > 1) {
		return EINVAL ;
	}

	// copy memory
	err = cso_new(&newcso, start[1], size) ;
	if (err) goto ONABORT ;
	memcpy(newcso->buf.start+1, start+1, size-1) ;
	newcso->buf.next = newcso->buf.start + size ;

	// validate content and correct endian
	validsize = cso_validate_obj(start[0] != IS_HOST_LITTLE_ENDIAN, newcso->buf.start+1, size-1) ;
	if (validsize != size-1) {
		err = EINVAL ;
		goto ONABORT ;
	}
	memcpy(&newcso->objheader.size, newcso->buf.start+newcso->objoffset[0]+1, sizeof(newcso->objheader.size)) ;

	// out param
	*cso = newcso ;

	return 0 ;
ONABORT:
	cso_del(&newcso) ;
	return err ;
}



#ifdef CONFIG_UNITTEST

static int test_initfree(void)
{
	union {
		uint32_t u32 ;
		uint8_t  u8 ;
	}           endian = { (uint32_t)0x01 } ;
	cso_obj_t * cso = 0 ;

	// TEST cso_new, cso_free_cso
	for (unsigned prealloc = 0; prealloc <= 1024; prealloc += (prealloc <= 16 ? 1 : 1024-17)) {
	for (unsigned ti = 0; ti < 2; ++ti) {
		cso_e type = (ti ? CSO_DICT : CSO_ARRAY) ;
		// cso_new: allocates memory and sets members
		TEST(0 == cso_new(&cso, type, prealloc)) ;
		TEST(0 != cso) ;
		TEST(0 != cso->buf.next) ;
		TEST(0 != cso->buf.end) ;
		TEST(0 != cso->buf.start) ;
		TEST(cso->buf.end  == cso->buf.start + (prealloc <= 16 ? 16 : prealloc)) ;
		TEST(cso->buf.next == cso->buf.start + 2*sizeof(uint8_t)+sizeof(uint32_t)) ;
		TEST(type == cso->objheader.type) ;
		TEST(0  == cso->objheader.size) ;
		TEST(1  == cso->nropen) ;
		TEST(16 == cso->nrallocated) ;
		TEST(0  != cso->objoffset) ;
		TEST(0  == cso->lasterr) ;
		TEST(endian.u8 == cso->buf.start[0]) ;
		TEST(1  == cso->objoffset[0]) ;
		for (unsigned i = 1; i < cso->nrallocated; ++i) {
			TEST(0 == cso->objoffset[i]) ;
		}
		TEST(type == cso->buf.start[cso->objoffset[0]]) ;
		for (unsigned i = 0; i < sizeof(uint32_t); ++i) {
			TEST(0 == cso->buf.start[cso->objoffset[0]+1+i]) ;
		}
		// cso_del: frees all memory
		TEST(0 == cso_del(&cso)) ;
		TEST(0 == cso) ;
		// cso_del: calling twice does no harm
		TEST(0 == cso_del(&cso)) ;
		TEST(0 == cso) ;
	}}

	// TEST cso_new: EINVAL
	for (unsigned type = 0; type < 255; ++type) {
		if (type == CSO_ARRAY || type == CSO_DICT) continue ;
		TEST(EINVAL == cso_new(&cso, (cso_e)type, 1024)) ;
		TEST(0 == cso) ;
	}

	// TEST cso_new: E2BIG
	TEST(E2BIG == cso_new(&cso, CSO_ARRAY, UINT32_MAX)) ;
	TEST(0 == cso) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	return EINVAL ;
}

static int test_query(void)
{
	cso_obj_t * cso = 0 ;
	uint8_t   * oldstart = 0 ;
	size_t      oldsize = 0 ;

	// prepare
	TEST(0 == cso_new(&cso, CSO_ARRAY, 256)) ;
	oldstart = cso->buf.start ;
	oldsize  = (size_t) (cso->buf.end - cso->buf.start) ;

	// TEST cso_lasterr
	for (int err = 256; err >= 0; --err) {
		cso->lasterr = err ;
		TEST(err == cso_lasterr(cso)) ;
	}

	// TEST cso_get_type
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		cso->objheader.type = type ;
		TEST(type == cso_get_type(cso)) ;
	}
	cso->objheader.type = CSO_ARRAY ;

	// TEST cso_get_depth
	TEST(0 == cso_get_depth(cso)) ;
	for (size_t t = 1; t; t *= 2) {
		cso->nropen = t ;
		TEST(t-1 == cso_get_depth(cso)) ;
	}
	cso->nropen = 1 ;

	// TEST cso_get_data
	for (int i = 128; i >= 0; --i) {
		cso->buf.start = oldstart + i ;
		memset(oldstart + 1, 255, oldsize-1) ;
		TEST(oldstart+i == cso_get_data(cso)) ;
		for (unsigned bi = 1; bi < oldsize; ++bi) {
			uint8_t v = (bi >= (unsigned)i+2 && bi < (unsigned)i+2+sizeof(uint32_t)) ? 0 : 255 ;
			TEST(v == oldstart[bi]) ;
		}
	}

	// TEST cso_get_size
	for (unsigned i = 256; i <= 256; --i) {
		cso->buf.start = oldstart + i ;
		cso->buf.next  = oldstart + oldsize ;
		TEST(oldsize-i == cso_get_size(cso)) ;
		cso->buf.start = oldstart ;
		cso->buf.next  = oldstart + oldsize + i ;
		TEST(oldsize+i == cso_get_size(cso)) ;
	}

	// unprepare
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	if (cso) {
		cso->buf.start = oldstart ;
		cso->buf.end   = oldstart + oldsize ;
	}
	cso_del(&cso) ;
	return EINVAL ;
}

static int test_change(void)
{
	cso_obj_t * cso = 0 ;

	// prepare
	TEST(0 == cso_new(&cso, CSO_ARRAY, 256)) ;

	// TEST cso_clear_lasterr
	cso->lasterr = 100 ;
	cso_clear_lasterr(cso) ;
	TEST(0 == cso->lasterr) ;

	// unprepare
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	return EINVAL ;
}

static int test_helper(void)
{
	cso_obj_t * cso = 0 ;
	uint8_t   * oldstart = 0 ;
	uint8_t   * oldnext ;
	size_t      oldsize = 0 ;

	// prepare
	TEST(0 == cso_new(&cso, CSO_ARRAY, 256)) ;
	oldstart = cso->buf.start ;
	oldnext  = cso->buf.next ;
	oldsize  = (size_t) (cso->buf.end - cso->buf.start) ;

	// TEST SIZEMEMORY
	for (unsigned i = 256; i <= 256; --i) {
		cso->buf.start = oldstart + i ;
		cso->buf.end   = oldstart + oldsize ;
		TEST(oldsize-i == SIZEMEMORY()) ;
		cso->buf.start = oldstart ;
		cso->buf.end   = oldstart + oldsize + i ;
		TEST(oldsize+i == SIZEMEMORY()) ;
	}

	// TEST SIZEMEMORY_USED
	for (unsigned i = 256; i <= 256; --i) {
		cso->buf.start = oldstart + i ;
		cso->buf.next  = oldstart + oldsize ;
		TEST(oldsize-i == SIZEMEMORY_USED()) ;
		cso->buf.start = oldstart ;
		cso->buf.next  = oldstart + i ;
		TEST(i == SIZEMEMORY_USED()) ;
	}

	// TEST ISFREEMEMORY
	for (unsigned i = 256; i <= 256; --i) {
		cso->buf.next = oldstart ;
		cso->buf.end  = oldstart + i ;
		TEST(1 == ISFREEMEMORY(i)) ;
		TEST(0 == ISFREEMEMORY(i+1)) ;
	}
	cso->buf.next = oldnext ;
	cso->buf.end  = oldstart + oldsize ;

	// TEST cso_expand_objoffset
	for (size_t i = cso->nrallocated; i <= 1024; i *= 2) {
		cso->nropen = i ;
		TEST(0 == cso_expand_objoffset(cso)) ;
		TEST(cso->nropen == i) ;
		TEST(cso->nrallocated == 2*i) ;
		TEST(cso->buf.next   == oldnext) ;
		TEST(cso->buf.end    == oldstart + oldsize) ;
		TEST(cso->buf.start  == oldstart) ;
		for (size_t hi = i; hi < 2*i; ++hi) {
			TEST(0 == cso->objoffset[hi]) ;
		}
	}

	// unprepare
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	if (cso) {
		cso->buf.start = oldstart ;
		cso->buf.end   = oldstart + oldsize ;
	}
	cso_del(&cso) ;
	return EINVAL ;
}

static uint8_t read_u8(uint8_t ** next)
{
	++ (*next) ;
	return (*next)[-1] ;
}

static uint16_t read_u16(uint8_t ** next)
{
	uint16_t val ;
	memcpy(&val, *next, sizeof(val)) ;
	(*next) += sizeof(val) ;
	return val ;
}

static uint32_t read_u32(uint8_t ** next)
{
	uint32_t val ;
	memcpy(&val, *next, sizeof(val)) ;
	(*next) += sizeof(val) ;
	return val ;
}

static uint64_t read_u64(uint8_t ** next)
{
	uint64_t val ;
	memcpy(&val, *next, sizeof(val)) ;
	(*next) += sizeof(val) ;
	return val ;
}

static double read_dbl(uint8_t ** next)
{
	double val ;
	memcpy(&val, *next, sizeof(val)) ;
	(*next) += sizeof(val) ;
	return val ;
}

static int test_add(void)
{
	cso_obj_t * cso = 0 ;
	cso_obj_t   old ;
	uint8_t   * oldnext ;
	uint8_t   * oldend ;
	uint8_t   * oldstart ;
	char        key[256] ;

	// TEST cso_add_u8, cso_add_u16, cso_add_u32, cso_add_u64, cso_add_dbl,
	//      cso_add_bin, cso_add_str, cso_add_null
	for (unsigned t = 0; t < 2; ++t) {
	for (unsigned vt = 0; vt < 9; ++vt) {
	for (unsigned isalloc = 0; isalloc < 2; ++isalloc) {
		cso_e type = (t ? CSO_DICT: CSO_ARRAY) ;
		size_t elemsize = t ? strlen(key)+1+sizeof(uint32_t) /*key*/ : 0 ;
		elemsize += 1 ; /* type id */
		switch (vt) {
		case 0: elemsize += 1 ; break ;
		case 1: elemsize += 2 ; break ;
		case 2: elemsize += 4 ; break ;
		case 3: elemsize += 8 ; break ;
		case 4: elemsize += 8 ; break ;
		case 5: elemsize += sizeof(uint32_t) + 10 ; break ;
		case 6: elemsize += sizeof(uint32_t) + 7 ; break ;
		case 7: elemsize += 0 ; break ;
		case 8: elemsize += sizeof(uint32_t) ; break ;
		}
		TEST(0 == cso_new(&cso, type, 256)) ;
		oldnext  = cso->buf.next ;
		oldend   = cso->buf.end ;
		oldstart = cso->buf.start ;
		sprintf(key, "--key--%d", vt) ;
		memset(oldnext, 0xff, (size_t) (cso->buf.end-cso->buf.next)) ;
		if (isalloc) {
			cso->buf.next = cso->buf.end - elemsize + 1 ;
			oldnext       = cso->buf.next ;
		}
		switch (vt) {
		case 0: TEST(0 == cso_add_u8(cso, key, 0x12)) ; break ;
		case 1: TEST(0 == cso_add_u16(cso, key, 0x1234)) ; break ;
		case 2: TEST(0 == cso_add_u32(cso, key, 0x12345678)) ; break ;
		case 3: TEST(0 == cso_add_u64(cso, key, 0x12345678aabbccdd)) ; break ;
		case 4: TEST(0 == cso_add_dbl(cso, key, 100.25)) ; break ;
		case 5: TEST(0 == cso_add_bin(cso, key, "1234567890", 10)) ; break ;
		case 6: TEST(0 == cso_add_str(cso, key, "abcde\n")) ; break ;
		case 7: TEST(0 == cso_add_null(cso, key)) ; break ;
		case 8: TEST(0 == cso_add_str(cso, key, 0/*null str*/)) ; break ;
		}
		if (isalloc) {
			oldnext  = cso->buf.start + (oldnext - oldstart) ;
			oldend   = cso->buf.end ;
			oldstart = cso->buf.start ;
		}
		TEST((isalloc ? 512 : 256) == SIZEMEMORY()) ;
		if (t) {
			TEST(strlen(key)+1 == read_u32(&oldnext)) ;
			TEST(0 == memcmp(key, oldnext, strlen(key)+1)) ;
			oldnext += strlen(key)+1 ;
		}
		switch (vt) {
		case 0:
			TEST(CSO_UINT8  == oldnext[0]) ; ++oldnext ; TEST(0x12 == read_u8(&oldnext)) ;
			break ;
		case 1:
			TEST(CSO_UINT16 == oldnext[0]) ; ++oldnext ; TEST(0x1234 == read_u16(&oldnext)) ;
			break ;
		case 2:
			TEST(CSO_UINT32 == oldnext[0]) ; ++oldnext ; TEST(0x12345678 == read_u32(&oldnext)) ;
			break ;
		case 3:
			TEST(CSO_UINT64 == oldnext[0]) ; ++oldnext ;
			TEST(0x12345678aabbccdd == read_u64(&oldnext)) ;
			break ;
		case 4:
			TEST(CSO_DOUBLE == oldnext[0]) ; ++oldnext ;
			TEST(100.25 == read_dbl(&oldnext)) ;
			break ;
		case 5:
			TEST(CSO_BIN    == oldnext[0]) ; ++oldnext ;
			TEST(10 == read_u32(&oldnext)) ;
			TEST(0 == memcmp(oldnext, "1234567890", 10)) ;
			oldnext += 10 ;
			break ;
		case 6:
			TEST(CSO_STR    == oldnext[0]) ; ++oldnext ;
			TEST(7 == read_u32(&oldnext)) ;
			TEST(0 == memcmp(oldnext, "abcde\n", 7)) ;
			oldnext += 7 ;
			break ;
		case 7:
			TEST(CSO_NULL   == oldnext[0]) ; ++oldnext ;
			break ;
		case 8:
			TEST(CSO_STR    == oldnext[0]) ; ++oldnext ;
			TEST(0 == read_u32(&oldnext)) ;
			break ;
		}
		TEST(cso->buf.next  == oldnext) ;
		TEST(cso->buf.end   == oldend) ;
		TEST(cso->buf.start == oldstart) ;
		// cached object header incremented
		TEST(cso->objheader.size == elemsize) ;
		// size in object header not incremented
		uint32_t u32 = 0 ;
		TEST(0 == memcmp(&u32, cso->buf.start+cso->objoffset[0]+1, sizeof(u32))) ;
		// check size again
		TEST(0 == cso_add_u8(cso, key, 0x12)) ;
		TEST(0 == memcmp(&u32, cso->buf.start+cso->objoffset[0]+1, sizeof(u32))) ;
		TEST(cso->objheader.size == elemsize+2+(t?sizeof(uint32_t)+strlen(key)+1:0)) ;
		TEST(0 == cso_del(&cso)) ;
	}}}

	// TEST cso_add_null, cso_add_u8, cso_add_u16, cso_add_u32, cso_add_u64,
	//      cso_add_dbl, cso_add_bin, cso_add_str: EINVAL
	TEST(0 == cso_new(&cso, CSO_DICT, 256)) ;
	memcpy(&old, cso, sizeof(old)) ;
	old.lasterr = E2BIG ;
	TEST(EINVAL == cso_add_null(cso, "")) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_u8(cso, 0, 1)) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_u16(cso, "", 1)) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_u32(cso, 0, 1)) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_u64(cso, "", 1)) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_bin(cso, 0, "data", 4)) ;
	memcpy(&old, cso, sizeof(old)) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_str(cso, "", "str")) ;
	memcpy(&old, cso, sizeof(old)) ;
	TEST(0 == cso_add_u64(cso, "key", 1)) ;	// lasterr is not cleared
	memcpy(&old, cso, sizeof(old)) ;
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_add_bin: E2BIG
	TEST(0 == cso_new(&cso, CSO_ARRAY, 256)) ;
	memcpy(&old, cso, sizeof(old)) ;
	old.lasterr = E2BIG ;
	TEST(E2BIG == cso_add_bin(cso, 0, "data", UINT32_MAX-2*sizeof(uint32_t))) ;
	TEST(0 == memcmp(&old, cso, sizeof(old))) ;
	TEST(0 == cso_del(&cso)) ;
	TEST(0 == cso_new(&cso, CSO_DICT, 256)) ;
	memcpy(&old, cso, sizeof(old)) ;
	old.lasterr = E2BIG ;
	TEST(E2BIG == cso_add_bin(cso, "K", "data", UINT32_MAX-2*sizeof(uint32_t)-2)) ;
	TEST(0 == memcmp(&old, cso, sizeof(old))) ;
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	return EINVAL ;
}

static int test_addobj(void)
{
	cso_obj_t * cso = 0 ;
	cso_obj_t   old ;
	char        key[256] ;
	uint32_t    u32 ;

	// TEST cso_add_obj, cso_end
	for (unsigned t = 0; t < 2; ++t) {
		cso_e  type    = t  ? CSO_DICT : CSO_ARRAY ;
		size_t keysize = t ? sizeof(uint32_t)+5 : 0 ;
		size_t topsize = 0 ;
		sprintf(key, "%s", "1234") ;
		TEST(0 == cso_new(&cso, type, 128)) ;
		for (unsigned depth = 0; depth < 128; ++depth) {
			uint32_t parentsize = cso->objheader.size ;
			topsize    += 1 + sizeof(uint32_t) + keysize ;
			parentsize += 1 + sizeof(uint32_t) + keysize ; // size of obj-header is added
			old = *cso ;
			// test cso_add_obj
			TEST(0 == cso_add_obj(cso, key, type)) ;
			// compare change
			TEST(cso->buf.next == cso->buf.start + (old.buf.next - old.buf.start) + 1 + sizeof(uint32_t) + keysize) ;
			TEST(type == cso->objheader.type) ;
			TEST(0 == cso->objheader.size) ;
			TEST(cso->nropen == old.nropen+1) ;
			TEST(0 != cso->objoffset[cso->nropen-1]) ;
			u32 = 0 ;
			TEST(type == cso->buf.start[cso->objoffset[cso->nropen-1]]) ;
			TEST(0 == memcmp(&u32, cso->buf.start+cso->objoffset[cso->nropen-1]+1, sizeof(u32))) ;
			// adding data changes only cached header
			for (unsigned i = 0; i < 10; ++i) {
				uint32_t s = (i+1) * (t ? sizeof(uint32_t)+5+2 : 2) ;
				TEST(0 == cso_add_u8(cso, key, 1)) ;
				TEST(0 == memcmp(&u32, cso->buf.start+cso->objoffset[cso->nropen-1]+1, sizeof(u32))) ;
				TEST(s == cso->objheader.size) ;
			}
			topsize += 10 * (t ? sizeof(uint32_t)+5+2 : 2) ;
			// size stored in parent header did not change
			memcpy(&u32, cso->buf.start+cso->objoffset[cso->nropen-2]+1, sizeof(u32)) ;
			TEST(u32 == parentsize) ;
		}
		for (unsigned depth = 0; depth < 128; ++depth) {
			// test cso_end
			uint32_t parentsize ;
			uint32_t childsize ;
			uint8_t  * parentheader = cso->buf.start+cso->objoffset[cso->nropen-2] ;
			uint8_t  * childheader  = cso->buf.start+cso->objoffset[cso->nropen-1] ;
			old = *cso ;
			memcpy(&childsize, childheader+1, sizeof(childsize)) ;
			memcpy(&parentsize, parentheader+1, sizeof(parentsize)) ;
			TEST(childsize == (depth ? t ? 124 : 25 : 0)) ;
			childsize = cso->objheader.size ;
			TEST(0 == cso_end(cso)) ;
			// compare
			TEST(cso->nropen      == old.nropen-1) ;
			TEST(cso->nrallocated == old.nrallocated) ;
			TEST(cso->objoffset[cso->nropen]   == 0) ;
			TEST(cso->objoffset[cso->nropen-1] == (size_t) (parentheader-cso->buf.start)) ;
			memcpy(&u32, childheader+1, sizeof(u32)) ;
			TEST(childsize == u32) ;
			TEST(0 == memcmp(&parentsize, parentheader+1, sizeof(parentsize))) ; // not changed
			parentsize += childsize ; // end has added obj size to cached header
			TEST(parentsize == cso->objheader.size) ;
		}
		TEST(topsize == cso->objheader.size) ;
		memcpy(&u32, cso->buf.start+cso->objoffset[0]+1, sizeof(u32)) ;
		TEST((t ? 9+5 : 5) == u32) ;
		// cso_get_data copies cached size into header
		TEST(cso->buf.start == cso_get_data(cso)) ;
		memcpy(&u32, cso->buf.start+cso->objoffset[0]+1, sizeof(u32)) ;
		TEST(u32 == topsize) ;
		TEST(topsize == cso->objheader.size) ;
		TEST(1 == cso->nropen) ;
		TEST(0 == cso_del(&cso)) ;
	}

	// TEST cso_add_obj: EINVAL
	TEST(0 == cso_new(&cso, CSO_DICT, 16)) ;
	memcpy(&old, cso, sizeof(old)) ;
	old.lasterr = EINVAL ; // will be set
	for (unsigned type = 0; type < 256; ++type) {
		if (type == CSO_ARRAY || type == CSO_DICT) continue ;
		cso->lasterr = 0 ;
		TEST(EINVAL == cso_add_obj(cso, key, type)) ;
		TEST(0 == memcmp(&old, cso, sizeof(old))) ;
	}
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_obj(cso, 0, CSO_DICT)) ;
	TEST(0 == memcmp(&old, cso, sizeof(old))) ;
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_add_obj(cso, "", CSO_DICT)) ;
	TEST(0 == memcmp(&old, cso, sizeof(old))) ;

	// TEST cso_end: EINVAL
	cso->lasterr = 0 ;
	TEST(EINVAL == cso_end(cso)) ;
	TEST(0 == memcmp(&old, cso, sizeof(old))) ;
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	return EINVAL ;
}

static int add_elements(cso_obj_t * cso, const char * key, unsigned depth)
{
	TEST(0 == cso_add_null(cso, key)) ;
	TEST(0 == cso_add_u8(cso, key, 0x01)) ;
	TEST(0 == cso_add_u16(cso, key, 0x0102)) ;
	TEST(0 == cso_add_u32(cso, key, 0x01020304)) ;
	TEST(0 == cso_add_u64(cso, key, 0x0102030405060708)) ;
	TEST(0 == cso_add_dbl(cso, key, 12345.0625)) ;
	if (depth) {
		TEST(0 == cso_add_obj(cso, key, CSO_ARRAY)) ;
		TEST(0 == add_elements(cso, key, depth-1)) ;
		TEST(0 == cso_end(cso)) ;
		TEST(0 == cso_add_obj(cso, key, CSO_DICT)) ;
		TEST(0 == add_elements(cso, key, depth-1)) ;
		TEST(0 == cso_end(cso)) ;
	}
	TEST(0 == cso_add_bin(cso, key, key, strlen(key)+1)) ;
	TEST(0 == cso_add_str(cso, key, "abcdef")) ;
	TEST(0 == cso_add_str(cso, key, 0/*null str*/)) ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int compare_elem(cso_iter_t * iter, /*out*/cso_elem_t * elem, cso_e type, unsigned idx, const char * key)
{
	memset(elem, 255, sizeof(cso_elem_t)) ;
	TEST(1 == cso_isnext(iter)) ;
	*elem = cso_next(iter) ;
	TEST(elem->type == type) ;
	if (key) {
		TEST(elem->key != 0) ;
		TEST(0 == strcmp(elem->key, key)) ;
	} else {
		TEST(elem->key == 0) ;
	}
	TEST(elem->index == idx) ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int cmp_elements(cso_iter_t * iter, const char * key, unsigned depth)
{
	cso_elem_t   elem ;
	const char * key2 = (iter->iskey ? key : 0) ;
	// null
	TEST(0 == compare_elem(iter, &elem, CSO_NULL, 0, key2)) ;
	// uint8
	TEST(0 == compare_elem(iter, &elem, CSO_UINT8, 1, key2)) ;
	TEST(0x1 == elem.val.u8) ;
	// uint16
	TEST(0 == compare_elem(iter, &elem, CSO_UINT16, 2, key2)) ;
	TEST(0x102 == elem.val.u16) ;
	// uint32
	TEST(0 == compare_elem(iter, &elem, CSO_UINT32, 3, key2)) ;
	TEST(0x1020304 == elem.val.u32) ;
	// uint64
	TEST(0 == compare_elem(iter, &elem, CSO_UINT64, 4, key2)) ;
	TEST(0x102030405060708 == elem.val.u64) ;
	// double
	TEST(0 == compare_elem(iter, &elem, CSO_DOUBLE, 5, key2)) ;
	TEST(12345.0625 == elem.val.dbl) ;
	if (depth) {
		TEST(0 == compare_elem(iter, &elem, CSO_ARRAY, 6, key2)) ;
		cso_iter_t iter2 ;
		TEST(0 == cso_init_iter2(&iter2, &elem)) ;
		TEST(0 == cmp_elements(&iter2, key, depth-1)) ;
		TEST(0 == compare_elem(iter, &elem, CSO_DICT, 7, key2)) ;
		TEST(0 == cso_init_iter2(&iter2, &elem)) ;
		TEST(0 == cmp_elements(&iter2, key, depth-1)) ;
	}
	// binary
	TEST(0 == compare_elem(iter, &elem, CSO_BIN, 6+(depth!=0)*2, key2)) ;
	TEST(strlen(key)+1 == elem.val.bin.size) ;
	TEST(0 == memcmp(key, elem.val.bin.data, elem.val.bin.size)) ;
	// string
	TEST(0 == compare_elem(iter, &elem, CSO_STR, 7+(depth!=0)*2, key2)) ;
	TEST(7 == elem.val.bin.size) ;
	TEST(0 == memcmp("abcdef", elem.val.bin.data, elem.val.bin.size)) ;
	// null string
	TEST(0 == compare_elem(iter, &elem, CSO_STR, 8+(depth!=0)*2, key2)) ;
	TEST(0 == elem.val.bin.size) ;
	TEST(0 == elem.val.bin.data) ;
	// end
	TEST(0 == cso_isnext(iter)) ;
	TEST(iter->end == iter->next) ;
	elem = cso_next(iter) ;
	TEST(iter->end == iter->next) ;
	TEST(elem.type  == CSO_NULL) ;
	TEST(elem.key   == 0) ;
	TEST(elem.index == (size_t)-1) ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int test_iterator(void)
{
	cso_obj_t * cso = 0 ;
	cso_iter_t  iter ;
	cso_elem_t  elem ;

	// TEST cso_init_iter
	for (int t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		TEST(0 == cso_new(&cso, type, 1024)) ;
		memset(&iter, 255, sizeof(iter)) ;
		cso_init_iter(&iter, cso) ;
		TEST(iter.iskey == t) ;
		TEST(iter.next  == cso->buf.start+2+sizeof(uint32_t)) ;
		TEST(iter.end   == cso->buf.next) ;
		TEST(iter.index == 0) ;
		for (unsigned t2 = 0; t2 < 2; ++t2) {
			cso_e type2 = t2 ? CSO_DICT : CSO_ARRAY ;
			TEST(0 == cso_add_obj(cso, "key", type2)) ;
			memset(&iter, 255, sizeof(iter)) ;
			cso_init_iter(&iter, cso) ;
			TEST(iter.iskey == t) ;
			TEST(iter.next  == cso->buf.start+2+sizeof(uint32_t)) ;
			TEST(iter.end   == cso->buf.next) ;
			TEST(iter.index == 0) ;
		}
		TEST(0 == cso_del(&cso)) ;
	}

	// TEST cso_init_iter2
	for (int t = 0; t < 2; ++t) {
		uint8_t data[128] ;
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		for (unsigned i = 0; i < 128; ++i) {
			elem = (cso_elem_t) { .type = type, .key = "key", .index = 1, { { data + i, 100+i } } } ;
			TEST(0 == cso_init_iter2(&iter, &elem)) ;
			TEST(iter.iskey == t) ;
			TEST(iter.next  == data + i) ;
			TEST(iter.end   == data + i + 100 + i) ;
			TEST(iter.index == 0) ;
		}
	}

	// TEST cso_init_iter2: EINVAL
	for (unsigned t = 0; t < 256; ++t) {
		uint8_t data[128] ;
		if (t == CSO_ARRAY || t == CSO_DICT) continue ;
		elem = (cso_elem_t) { .type = (cso_e) t, .key = "key", .index = 1, { { data, 128 } } } ;
		TEST(EINVAL == cso_init_iter2(&iter, &elem)) ;
	}


	// TEST cso_isnext, cso_next: empty object
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		TEST(0 == cso_new(&cso, type, 16)) ;
		cso_init_iter(&iter, cso) ;
		TEST(0 == cso_isnext(&iter)) ;
		elem = cso_next(&iter) ;
		TEST(CSO_NULL == elem.type) ;
		TEST(0 == elem.key) ;
		TEST((size_t)-1 == elem.index) ;
		TEST(0 == cso_del(&cso)) ;
	}

	// TEST cso_isnext, cso_next
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		const char * key = "12345" ;
		TEST(0 == cso_new(&cso, type, 1024)) ;
		TEST(0 == add_elements(cso, key, 3)) ;
		cso_init_iter(&iter, cso) ;
		TEST(0 == cmp_elements(&iter, key, 3)) ;
		TEST(0 == cso_del(&cso)) ;
	}

	// TEST cso_skip_key
	TEST(0 == cso_new(&cso, CSO_DICT, 1024)) ;
	TEST(0 == cso_add_u32(cso, "key1", 1)) ;
	TEST(0 == cso_add_u32(cso, "key2", 2)) ;
	TEST(0 == cso_add_u32(cso, "key1", 3)) ;
	TEST(0 == cso_add_obj(cso, "key2", CSO_DICT)) ;
	TEST(0 == cso_add_u32(cso, "key1", 1)) ;
	TEST(0 == cso_add_u32(cso, "key2", 2)) ;
	TEST(0 == cso_add_u32(cso, "key1", 3)) ;
	TEST(0 == cso_end(cso)) ;
	cso_init_iter(&iter, cso) ;
	TEST(0 == cso_skip_key(&iter, "key1")) ;
	elem = cso_next(&iter) ;
	TEST(0 == elem.index) ;
	TEST(1 == elem.val.u32) ;
	TEST(0 == cso_skip_key(&iter, "key1")) ;
	elem = cso_next(&iter) ;
	TEST(2 == elem.index) ;
	TEST(3 == elem.val.u32) ;
	TEST(ESRCH == cso_skip_key(&iter, "key1")) ;
	TEST(0 == cso_skip_key(&iter, "key2")) ;
	elem = cso_next(&iter) ;
	TEST(3 == elem.index) ;
	TEST(CSO_DICT == elem.type) ;
	TEST(0 == cso_isnext(&iter)) ;
	TEST(ESRCH == cso_skip_key(&iter, "key1")) ;
	cso_init_iter(&iter, cso) ;
	TEST(0 == cso_skip_key(&iter, "key2")) ;
	elem = cso_next(&iter) ;
	TEST(1 == elem.index) ;
	TEST(2 == elem.val.u32) ;
	TEST(0 == cso_skip_key(&iter, "key2")) ;
	elem = cso_next(&iter) ;
	TEST(3 == elem.index) ;
	TEST(CSO_DICT == elem.type) ;
	TEST(0 == cso_isnext(&iter)) ;
	TEST(ESRCH == cso_skip_key(&iter, "key2")) ;
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_skip_key: EINVAL
	TEST(0 == cso_new(&cso, CSO_ARRAY, 16)) ;
	cso_init_iter(&iter, cso) ;
	TEST(EINVAL == cso_skip_key(&iter, "key")) ;
	TEST(0 == cso_del(&cso)) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	return EINVAL ;
}

static int test_load(void)
{
	cso_obj_t * cso  = 0 ;
	cso_obj_t * cso2 = 0 ;
	cso_iter_t  iter ;
	uint8_t ledata[] = {
		0x01/*little endian*/, CSO_DICT, 0x89,0,0,0/*size*/,
		0x4,0,0,0/*keylen*/, '-', '1', '-', 0, CSO_UINT16, 0x02, 0x01,
		0x4,0,0,0/*keylen*/, '-', '2', '-', 0, CSO_UINT32, 0x04, 0x03, 0x02, 0x01,
		0x4,0,0,0/*keylen*/, '-', '3', '-', 0, CSO_BIN, 0x7,0,0,0/*size*/, 'b', 'i', 'n', 'a', 'r', 'y', 0,
		0x4,0,0,0/*keylen*/, '-', '4', '-', 0, CSO_ARRAY, 0x9,0,0,0/*size*/,
		CSO_UINT64, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x4,0,0,0/*keylen*/, '-', '5', '-', 0, CSO_ARRAY, 0x9,0,0,0/*size*/,
		CSO_DOUBLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x70, 0x40,
		0x4,0,0,0/*keylen*/, '-', '6', '-', 0, CSO_NULL,
		0x4,0,0,0/*keylen*/, '-', '7', '-', 0, CSO_STR,  0x4,0,0,0/*size*/, 's', 't', 'r', 0,
		0x4,0,0,0/*keylen*/, '-', '8', '-', 0, CSO_STR,  0,0,0,0/*size*/, /* null str */
		0x4,0,0,0/*keylen*/, '-', '9', '-', 0, CSO_UINT8, 1
	} ;

	uint8_t bedata[] = {
		0x00/*  big  endian*/, CSO_DICT, 0,0,0,0x89/*size*/,
		0,0,0,0x4/*keylen*/, '-', '1', '-', 0, CSO_UINT16, 0x01, 0x02,
		0,0,0,0x4/*keylen*/, '-', '2', '-', 0, CSO_UINT32, 0x01, 0x02, 0x03, 0x04,
		0,0,0,0x4/*keylen*/, '-', '3', '-', 0, CSO_BIN, 0,0,0,0x7/*size*/, 'b', 'i', 'n', 'a', 'r', 'y', 0,
		0,0,0,0x4/*keylen*/, '-', '4', '-', 0, CSO_ARRAY, 0,0,0,0x9/*size*/,
		CSO_UINT64, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0,0,0,0x4/*keylen*/, '-', '5', '-', 0, CSO_ARRAY, 0,0,0,0x9/*size*/,
		CSO_DOUBLE, 0x40, 0x70, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0,0,0,0x4/*keylen*/, '-', '6', '-', 0, CSO_NULL,
		0,0,0,0x4/*keylen*/, '-', '7', '-', 0, CSO_STR, 0,0,0,0x4/*size*/, 's', 't', 'r', 0,
		0,0,0,0x4/*keylen*/, '-', '8', '-', 0, CSO_STR, 0,0,0,0/*size*/, /* null str */
		0,0,0,0x4/*keylen*/, '-', '9', '-', 0, CSO_UINT8, 1
	} ;

	uint8_t errdata[sizeof(ledata)] ;
	uint8_t * errpos ;

	static_assert(sizeof(ledata) == sizeof(bedata), "must have same size") ;

	// TEST cso_load: empty document
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		TEST(0 == cso_new(&cso, type, 4096)) ;
		TEST(6 == cso_buffer_sizeused(&cso->buf)) ;
		TEST(0 == cso_load(&cso2, cso_get_data(cso), cso_get_size(cso))) ;
		TEST(6 == cso_buffer_sizeused(&cso2->buf)) ;
		TEST(16== cso_buffer_sizeallocated(&cso2->buf)) ;
		TEST(0 == memcmp(cso->buf.start, cso2->buf.start, cso_buffer_sizeused(&cso->buf))) ;
		TEST(cso->objheader.type == cso2->objheader.type) ;
		TEST(cso->objheader.size == cso2->objheader.size) ;
		cso_init_iter(&iter, cso2) ;
		TEST(0 == cso_isnext(&iter)) ;
		TEST(0 == cso_del(&cso)) ;
		TEST(0 == cso_del(&cso2)) ;
	}

	// TEST cso_load: no conversion
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		TEST(0 == cso_new(&cso, type, 4096)) ;
		TEST(0 == add_elements(cso, "xyzabc", 3)) ;
		TEST(0 == cso_load(&cso2, cso_get_data(cso), cso_get_size(cso))) ;
		TEST(0 != cso2->buf.start) ;
		TEST(0 != cso2->buf.next) ;
		TEST(0 != cso2->buf.end) ;
		TEST(cso_buffer_sizeused(&cso->buf) == cso_buffer_sizeused(&cso2->buf)) ;
		TEST(cso_buffer_sizeused(&cso->buf) == cso_buffer_sizeallocated(&cso2->buf)) ;
		TEST(cso->objheader.type == cso2->objheader.type) ;
		TEST(cso->objheader.size == cso2->objheader.size) ;
		TEST(1 == cso2->nropen) ;
		TEST(16 == cso2->nrallocated) ;
		TEST(0 != cso2->objoffset) ;
		TEST(0 == cso2->lasterr) ;
		TEST(0 == memcmp(cso->objoffset, cso2->objoffset, 16*sizeof(cso->objoffset[0]))) ;
		TEST(0 == memcmp(cso->buf.start, cso2->buf.start, cso_buffer_sizeused(&cso->buf))) ;
		cso_init_iter(&iter, cso) ;
		TEST(0 == cmp_elements(&iter, "xyzabc", 3)) ;
		cso_init_iter(&iter, cso2) ;
		TEST(0 == cmp_elements(&iter, "xyzabc", 3)) ;
		TEST(0 == cso_del(&cso)) ;
		TEST(0 == cso_del(&cso2)) ;
	}

	// prepare
	TEST(0 == cso_new(&cso, CSO_DICT, 1024)) ;
	TEST(0 == cso_add_u16(cso, "-1-", 0x0102)) ;
	TEST(0 == cso_add_u32(cso, "-2-", 0x01020304)) ;
	TEST(0 == cso_add_bin(cso, "-3-", "binary", 7)) ;
	TEST(0 == cso_add_obj(cso, "-4-", CSO_ARRAY)) ;
	TEST(0 == cso_add_u64(cso, 0, 0x0102030405060708)) ;
	TEST(0 == cso_end(cso)) ;
	TEST(0 == cso_add_obj(cso, "-5-", CSO_ARRAY)) ;
	TEST(0 == cso_add_dbl(cso, 0, 256.5)) ;
	TEST(0 == cso_end(cso)) ;
	TEST(0 == cso_add_null(cso, "-6-")) ;
	TEST(0 == cso_add_str(cso, "-7-", "str")) ;
	TEST(0 == cso_add_str(cso, "-8-", 0/*null str*/)) ;
	TEST(0 == cso_add_u8(cso, "-9-", 1)) ;

	// TEST cso_load: little endian
	TEST(0 == cso_load(&cso2, ledata, sizeof(ledata))) ;
	TEST(cso->objheader.type == cso2->objheader.type) ;
	TEST(cso->objheader.size == cso2->objheader.size) ;
	TEST(cso_get_size(cso) == cso_get_size(cso2)) ;
	TEST(0 == memcmp(cso_get_data(cso), cso_get_data(cso2), cso_get_size(cso2))) ;
	TEST(0 == cso_del(&cso2)) ;

	// TEST cso_load: big endian
	TEST(0 == cso_load(&cso2, bedata, sizeof(bedata))) ;
	TEST(cso->objheader.type == cso2->objheader.type) ;
	TEST(cso->objheader.size == cso2->objheader.size) ;
	TEST(cso_get_size(cso) == cso_get_size(cso2)) ;
	TEST(0 == memcmp(cso_get_data(cso), cso_get_data(cso2), cso_get_size(cso2))) ;
	TEST(0 == cso_del(&cso2)) ;

	// unprepare
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_load: EINVAL
	// size too small
	memcpy(errdata, ledata, sizeof(errdata)) ;
	TEST(EINVAL == cso_load(&cso2, errdata, 1+sizeof(uint32_t))) ;
	// wrong endian code
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[0] = 2 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// size of CSO_DICT too small
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[2] = (uint8_t) (errdata[2]-10) ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// wrong obj type
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[1] = CSO_BIN ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// size of embedded CSO_ARRAY too small
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '4', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-4-", 4)) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint32_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// size of CSO_DICT too big
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[2] = (uint8_t) (errdata[2]+1) ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// not enough bytes for keylen value
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[2] = 3 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// keylen too low
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[6]   = 1 ;
	errdata[6+4] = 0 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// keylen too big
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[6] = 200 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// no \0 byte at end of key
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[6] = 3 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// key contains two \0 bytes
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[6] = 5 ;
	errdata[6+4+5-1] = 0 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// no more bytes after key
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[2] = 8 ;
	TEST(EINVAL == cso_load(&cso2, errdata, 6+8)) ;
	// not enough bytes for BIN
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '3', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-3-", 4)) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint32_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// encoded size of bin too large
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '3', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-3-", 4)) ;
	errpos[4] = 200 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// not enough bytes for STR
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '7', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-7-", 4)) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint32_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// encoded size of STR too large
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '7', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-7-", 4)) ;
	errpos[4] = 200 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// no \0 byte ate end of STR
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '7', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-7-", 4)) ;
	TEST(0 == errpos[3+1+sizeof(uint32_t)+4-1]) ;
	errpos[3+1+sizeof(uint32_t)+4-1] = ' ' ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// STR contains two \0
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '7', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-7-", 4)) ;
	TEST(0 == errpos[3+1+sizeof(uint32_t)+4-1]) ;
	errpos[3+1+sizeof(uint32_t)+4-1-2] = 0 ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	// not enough bytes for UINT8
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errdata[2] = (uint8_t) (errdata[2]-1) ;
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata)-1)) ;
	// not enough bytes for UINT16
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '1', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-1-", 4)) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint16_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// not enough bytes for UINT32
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '2', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-2-", 4)) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint32_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// not enough bytes for UINT64
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '4', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-4-", 4)) ;
	TEST(1+sizeof(uint64_t) == errpos[3+1]) ;
	errpos[3+1] = sizeof(uint64_t) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3+1+sizeof(uint32_t)+1+sizeof(uint64_t)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// not enough bytes for DOUBLE
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '5', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-5-", 4)) ;
	TEST(1+sizeof(double) == errpos[3+1]) ;
	errpos[3+1] = sizeof(double) ;
	errdata[2] = (uint8_t) ((size_t)(errpos-errdata)+3u+1+sizeof(uint32_t)+1+sizeof(double)-1-6) ;
	TEST(EINVAL == cso_load(&cso2, errdata, errdata[2]+6u)) ;
	// unknown type
	memcpy(errdata, ledata, sizeof(errdata)) ;
	errpos = memchr(errdata, '1', sizeof(errdata)) ;
	TEST(0 == memcmp(errpos-1, "-1-", 4)) ;
	TEST(CSO_UINT16 == errpos[3]) ;
	errpos[3] = 20 ; /* invalid type id */
	TEST(EINVAL == cso_load(&cso2, errdata, sizeof(errdata))) ;
	TEST(0 == cso2) ;

	return 0 ;
ONABORT:
	cso_del(&cso) ;
	cso_del(&cso2) ;
	return EINVAL ;
}

static int test_querystr(void)
{
	char      * str  = 0 ;
	cso_obj_t * cso  = 0 ;
	cso_obj_t * cso2 = 0 ;

	// TEST cso_get_string: empty
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		TEST(0 == cso_new(&cso, type, 16)) ;
		TEST(0 == cso_get_string(cso, &str)) ;
		TEST(0 != str) ;
		if (t) { TEST(0 == strcmp(str, "{ }")) ; }
		else   { TEST(0 == strcmp(str, "[ ]")) ; }
		free(str) ;
		str = 0 ;
		TEST(0 == cso_del(&cso)) ;
	}

	// TEST cso_get_string: nested CSO_ARRAY
	TEST(0 == cso_new(&cso, CSO_DICT, 16)) ;
	TEST(0 == cso_add_obj(cso, "a", CSO_ARRAY)) ;
	for (unsigned i = 0; i < 10; ++i) {
		TEST(0 == cso_add_u32(cso, 0, i)) ;
	}
	TEST(0 == cso_end(cso)) ;
	TEST(0 == cso_get_string(cso, &str)) ;
	TEST(0 != str) ;
	TEST(0 == strcmp(str, "{\n \"a\": [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]\n}")) ;
	free(str) ;
	str = 0 ;
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_get_string: 5 times nested CSO_ARRAY
	TEST(0 == cso_new(&cso, CSO_DICT, 16)) ;
	for (unsigned i = 0; i < 5; ++i) {
		TEST(0 == cso_add_obj(cso, "a", CSO_ARRAY)) ;
	}
	for (unsigned i = 0; i < 10; ++i) {
		TEST(0 == cso_add_u32(cso, 0, i)) ;
	}
	for (unsigned i = 0; i < 5; ++i) {
		TEST(0 == cso_end(cso)) ;
	}
	TEST(0 == cso_get_string(cso, &str)) ;
	TEST(0 != str) ;
	TEST(0 == strcmp(str, "{\n \"a\": [\n  [\n   [\n    [\n     [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]\n    ]\n   ]\n  ]\n ]\n}")) ;
	free(str) ;
	str = 0 ;
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_get_string: nested CSO_DICT
	TEST(0 == cso_new(&cso, CSO_DICT, 16)) ;
	for (unsigned i = 0; i < 3; ++i) {
		TEST(0 == cso_add_u8(cso, "k1", 1)) ;
		TEST(0 == cso_add_obj(cso, "k2", CSO_DICT)) ;
	}
	for (unsigned i = 0; i < 3; ++i) {
		TEST(0 == cso_end(cso)) ;
	}
	TEST(0 == cso_get_string(cso, &str)) ;
	TEST(0 != str) ;
	TEST(0 == strcmp(str, "{\n \"k1\": 1,\n \"k2\": {\n  \"k1\": 1,\n  \"k2\": { \"k1\": 1, \"k2\": { } }\n }\n}")) ;
	free(str) ;
	str = 0 ;
	TEST(0 == cso_del(&cso)) ;

	// TEST cso_get_string: all elements
	for (unsigned t = 0; t < 2; ++t) {
		cso_e type = t ? CSO_DICT : CSO_ARRAY ;
		char buffer[1024] ;
		sprintf(buffer, "%s\n", t ? "{" : "[") ;
		sprintf(buffer + strlen(buffer), " %snull,\n", t ? "\"k0\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s1,\n", t ? "\"k1\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s2,\n", t ? "\"k2\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s3,\n", t ? "\"k3\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s4,\n", t ? "\"k4\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s5.5e+100,\n", t ? "\"k5\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s0x01,0x0A,0x10,0xFF,0x99,0x0F,0xA0,0xCD,\n", t ? "\"k6\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s\"\\n\\t\\r\\x01\\x1F\\\"ABC0123 \",\n", t ? "\"k7\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s255,\n", t ? "\"k8\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s65535,\n", t ? "\"k9\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s4294967295,\n", t ? "\"k10\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s18446744073709551615,\n", t ? "\"k11\": " : "") ;
		sprintf(buffer + strlen(buffer), " %snull,\n", t ? "\"k12\": " : "") ;
		sprintf(buffer + strlen(buffer), " %s\"\"\n", t ? "\"k13\": " : "") ;
		sprintf(buffer + strlen(buffer), "%s", t ? "}" : "]") ;
		TEST(0 == cso_new(&cso, type, 16)) ;
		TEST(0 == cso_add_null(cso, "k0")) ;
		TEST(0 == cso_add_u8(cso, "k1", 1)) ;
		TEST(0 == cso_add_u16(cso, "k2", 2)) ;
		TEST(0 == cso_add_u32(cso, "k3", 3)) ;
		TEST(0 == cso_add_u64(cso, "k4", 4)) ;
		TEST(0 == cso_add_dbl(cso, "k5", 5.5e100)) ;
		TEST(0 == cso_add_bin(cso, "k6", "\x01\x0a\x10\xff\x99\x0f\xa0\xcd", 8)) ;
		TEST(0 == cso_add_str(cso, "k7", "\n\t\r\x01\x1F\"" "ABC0123 ")) ;
		TEST(0 == cso_add_u8(cso, "k8", 255)) ;
		TEST(0 == cso_add_u16(cso, "k9", 65535)) ;
		TEST(0 == cso_add_u32(cso, "k10", (uint32_t)-1)) ;
		TEST(0 == cso_add_u64(cso, "k11", (uint64_t)-1)) ;
		TEST(0 == cso_add_str(cso, "k12", 0/* null str */)) ;
		TEST(0 == cso_add_str(cso, "k13", "")) ;
		TEST(0 == cso_get_string(cso, &str)) ;
		TEST(0 != str) ;
		TEST(0 == strcmp(buffer, str)) ;
		free(str) ;
		str = 0 ;
		TEST(0 == cso_del(&cso)) ;
	}

	return 0 ;
ONABORT:
	free(str) ;
	cso_del(&cso) ;
	cso_del(&cso2) ;
	return EINVAL ;
}

int unittest_cso(void)
{
	static_assert(8 == CHAR_BIT, "assume char holds exactly 8 bits") ;
	static_assert(8 == sizeof(double), "same size as uint64_t") ;

	if (test_initfree()) goto ONABORT ;
	if (test_query())    goto ONABORT ;
	if (test_change())   goto ONABORT ;
	if (test_helper())   goto ONABORT ;
	if (test_add())      goto ONABORT ;
	if (test_addobj())   goto ONABORT ;
	if (test_iterator()) goto ONABORT ;
	if (test_load())     goto ONABORT ;
	if (test_querystr()) goto ONABORT ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

#endif

