/***********************************************************************
* pc_api.h
*
*  Structures and function signatures for point clouds
*
* Portions Copyright (c) 2012, OpenGeo
*
***********************************************************************/

#ifndef _PC_API_H
#define _PC_API_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "hashtable.h"

/**********************************************************************
* DATA STRUCTURES
*/

#define POINTCLOUD_VERSION "1.0"

/**
* Compression types for PCPOINTS in a PCPATCH
*/
enum COMPRESSIONS
{
	PC_NONE = 0,
	PC_GHT = 1
};

/**
* Flags of endianness for inter-architecture
* data transfers.
*/
enum ENDIANS
{
	PC_NDR, /* Little */
	PC_XDR  /* Big */
};



/**
* We need to hold a cached in-memory version of the format's
* XML structure for speed, and this is it.
*/
typedef struct
{
	char *name;
	char *description;
	uint32_t position;
	uint32_t size;
	uint32_t byteoffset;
	uint32_t interpretation;
	double scale;
	double offset;
	uint8_t active;
} PCDIMENSION;

typedef struct
{
	uint32_t pcid;        /* Unique ID for schema */
	uint32_t ndims;       /* How many dimensions does this schema have? */
	size_t size;          /* How wide (bytes) is a point with this schema? */
	PCDIMENSION **dims;   /* Array of dimension pointers */
	uint32_t srid;        /* Foreign key reference to SPATIAL_REF_SYS */
	int32_t x_position;  /* What entry is the x coordinate at? */
	int32_t y_position;  /* What entry is the y coordinate at? */
	uint32_t compression; /* Compression type applied to the data */
	hashtable *namehash;  /* Look-up from dimension name to pointer */
} PCSCHEMA;


/**
* Uncompressed structure for in-memory handling
* of points. A read-only PgSQL point can be wrapped in
* one of these by pointing the data element at the
* PgSQL memory and setting the capacity to 0
* to indicate it is read-only.
*/
typedef struct
{
	int8_t readonly;
	const PCSCHEMA *schema;
	uint8_t *data;
} PCPOINT;

/**
* Uncompressed Structure for in-memory handling
* of patches. A read-only PgSQL patch can be wrapped in
* one of these by pointing the data element at the
* PgSQL memory and setting the capacity to 0
* to indicate it is read-only.
*/
typedef struct
{
	int8_t readonly;
	size_t npoints; /* How many points we have */
	size_t maxpoints; /* How man points we can hold (or 0 for read-only) */
	const PCSCHEMA *schema;
	double xmin, xmax, ymin, ymax;
	uint8_t *data;
} PCPATCH;



/**
* Serialized point type for clouds. Variable length, because there can be
* an arbitrary number of dimensions. The pcid is a foreign key
* reference to the POINTCLOUD_SCHEMAS table, where
* the underlying structure of the data is described in XML,
* the spatial reference system is indicated, and the data
* packing scheme is indicated.
*/
typedef struct
{
	uint32_t size; /* PgSQL VARSIZE */
	uint32_t pcid;
	uint8_t data[1];
} SERPOINT;

/**
* PgSQL patch type (collection of points) for clouds.
* Variable length, because there can be
* an arbitrary number of points encoded within.
* The pcid is a foriegn key reference to the
* POINTCLOUD_SCHEMAS table, where
* the underlying structure of the data is described in XML,
* the spatial reference system is indicated, and the data
* packing scheme is indicated.
*/
typedef struct
{
	uint32_t size; /* PgSQL VARSIZE */
	float xmin, xmax, ymin, ymax;
	uint32_t pcid;
	uint32_t npoints;
	uint8_t data[1];
} SERPATCH;




/* Global function signatures for memory/logging handlers. */
typedef void* (*pc_allocator)(size_t size);
typedef void* (*pc_reallocator)(void *mem, size_t size);
typedef void  (*pc_deallocator)(void *mem);
typedef void  (*pc_message_handler)(const char *string, va_list ap);



/**********************************************************************
* MEMORY MANAGEMENT
*/

/** Allocate memory using the appropriate means (system/db) */
void* pcalloc(size_t size);
/** Reallocate memory using the appropriate means (system/db) */
void* pcrealloc(void* mem, size_t size);
/** Free memory using the appropriate means (system/db) */
void  pcfree(void* mem);
/** Emit an error message using the appropriate means (system/db) */
void  pcerror(const char *fmt, ...);
/** Emit an info message using the appropriate means (system/db) */
void  pcinfo(const char *fmt, ...);
/** Emit a warning message using the appropriate means (system/db) */
void  pcwarn(const char *fmt, ...);

/** Set custom memory allocators and messaging (used by PgSQL module) */
void pc_set_handlers(pc_allocator allocator, pc_reallocator reallocator,
                     pc_deallocator deallocator, pc_message_handler error_handler,
                     pc_message_handler info_handler, pc_message_handler warning_handler);

/** Set program to use system memory allocators and messaging */
void pc_install_default_handlers(void);


/**********************************************************************
* UTILITY
*/

/** Read a hex string into binary buffer */
uint8_t* bytes_from_hexbytes(const char *hexbuf, size_t hexsize);
/** Read the the PCID from WKB form of a POINT */
uint32_t wkb_get_pcid(uint8_t *bytes);
/** Get a pointer to the data buffer of the WKB form of a point */
uint8_t* wkb_point_get_data_ptr(uint8_t *bytes);

/** Force a byte array into the machine endianness */
uint8_t* bytes_flip_endian(const uint8_t *bytebuf, const PCSCHEMA *schema, uint32_t npoints);



/**********************************************************************
* SCHEMAS
*/

/** Release the memory in a schema structure */
void pc_schema_free(PCSCHEMA *pcs);
/** Build a schema structure from the XML serialisation */
int pc_schema_from_xml(const char *xmlstr, PCSCHEMA **schema);
/** Print out JSON readable format of schema */
char* pc_schema_to_json(const PCSCHEMA *pcs);
/** Extract dimension information by position */
PCDIMENSION* pc_schema_get_dimension(const PCSCHEMA *s, uint32_t dim);
/** Extract dimension information by name */
PCDIMENSION* pc_schema_get_dimension_by_name(const PCSCHEMA *s, const char *name);
/** Check if the schema has all the information we need to work with data */
uint32_t pc_schema_is_valid(const PCSCHEMA *s);



/**********************************************************************
* PCPOINT
*/

/** Create a new PCPOINT */
PCPOINT* pc_point_make(const PCSCHEMA *s);
/** Create a new readonly PCPOINT on top of a data buffer */
PCPOINT* pc_point_from_data(const PCSCHEMA *s, uint8_t *data);
/** Create a new read/write PCPOINT on top of a data buffer */
PCPOINT* pc_point_from_data_rw(const PCSCHEMA *s, uint8_t *data);
/** Create a new read/write PCPOINT from a double array */
PCPOINT* pc_point_from_double_array(const PCSCHEMA *s, double *array, uint32_t nelems);
/** Frees the PTPOINT and data (if not readonly) does not free referenced schema */
void pc_point_free(PCPOINT *pt);
/** Casts named dimension value to double and scale/offset appropriately before returning */
double pc_point_get_double_by_name(const PCPOINT *pt, const char *name);
/** Casts dimension value to double and scale/offset appropriately before returning */
double pc_point_get_double_by_index(const PCPOINT *pt, uint32_t idx);
/** Scales/offsets double, casts to appropriate dimension type, and writes into point */
int pc_point_set_double_by_index(PCPOINT *pt, uint32_t idx, double val);
/** Scales/offsets double, casts to appropriate dimension type, and writes into point */
int pc_point_set_double_by_name(PCPOINT *pt, const char *name, double val);
/** Returns X coordinate */
double pc_point_get_x(const PCPOINT *pt);
/** Returns Y coordinate */
double pc_point_get_y(const PCPOINT *pt);



/**********************************************************************
* PCPATCH
*/

/** Create new empty PCPATCH */
PCPATCH* pc_patch_make(const PCSCHEMA *s);
/** Create new PCPATCH from a PCPOINT set. Copies data, doesn't take ownership of points */
PCPATCH* pc_patch_make_from_points(const PCPOINT **pts, uint32_t numpts);
/** Add a point to read/write PCPATCH */
int pc_patch_add_point(PCPATCH *c, const PCPOINT *p);



#endif /* _PC_API_H */