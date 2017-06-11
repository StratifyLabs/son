/*! \file */ //Copyright 2011-2017 Tyler Gilbert; All Rights Reserved

#ifndef SON_H_
#define SON_H_

/*! \mainpage
 *
 * #Introduction
 *
 * Stratify Object Notation (SON) is an object storage file format similar to JSON but
 * optimized for use on systems with limited resources (e.g. microcontrollers).
 *
 *
 */

#define SON_STR_VERSION "0.0.2"
#define SON_VERSION 0x00000002
#include "son_phy.h"

typedef u32 son_size_t;
typedef u16 son_count_t;
typedef int son_bool_t;

#define TRUE 1
#define FALSE 0

#define SON_ACCESS_NAME_CAPACITY 64
#define SON_ACCESS_NAME_SIZE (SON_ACCESS_NAME_CAPACITY-1)
#define SON_ACCESS_MAX_USER_SIZE (SON_ACCESS_NAME_SIZE - 2)
#define SON_KEY_NAME_CAPACITY 24
#define SON_KEY_NAME_SIZE (SON_KEY_NAME_CAPACITY-1)

typedef struct {
	son_size_t pos;
} son_stack_t;

enum {
	SON_ERR_NONE,
	SON_ERR_NO_ROOT,
	SON_ERR_OPEN_IO,
	SON_ERR_READ_IO,
	SON_ERR_WRITE_IO,
	SON_ERR_CLOSE_IO,
	SON_ERR_SEEK_IO,
	SON_ERR_READ_CHECKSUM,
	SON_ERR_CANNOT_APPEND,
	SON_ERR_CANNOT_WRITE,
	SON_ERR_INVALID_ROOT,
	SON_ERR_INVALID_ARRAY,
	SON_ERR_ARRAY_INDEX_NOT_FOUND,
	SON_ERR_ACCESS_TOO_LONG,
	SON_ERR_KEY_NOT_FOUND,
	SON_ERR_STACK_OVERFLOW,
	SON_ERR_INVALID_KEY,
};

typedef struct {
	son_phy_t phy;
	son_stack_t * stack;
	size_t stack_size;
	size_t stack_loc;
	u32 err;
} son_t;

typedef union {
	float * f;
	int * n;
	u32 * n_u32;
	s32 * n_s32;
	char * cdata;
	void * data;
} son_type_t;

typedef struct MCU_PACK {
	u8 name[SON_KEY_NAME_CAPACITY];
} son_key_t;

typedef struct MCU_PACK {
	u8 page;
	u16 page_offset;
} son_pos_t;

//an object has members that are accessed by using a unique identifier (key)
typedef struct MCU_PACK {
	u8 o_flags;
	son_pos_t pos;
	son_key_t key;
	u32 checksum;
} son_store_t;


/*! \brief Value types */
typedef enum {
	SON_STRING /*! String */,
	SON_FLOAT /*! Float */,
	SON_NUMBER_U32 /*! Unsigned 32-bit value */,
	SON_NUMBER_S32 /*! Signed 32-bit value */,
	SON_DATA /*! Data (user-defined size) */,
	SON_OBJ /*! Object group */,
	SON_ARRAY /*! Array */, //can be an array of distinct objects
	SON_TRUE /*! True value */,
	SON_FALSE /*! False value */,
	SON_NULL,
	SON_TOTAL
} son_marker_t;

#ifdef __cplusplus
extern "C" {
#endif

static inline void son_reset(son_t * h){ son_phy_lseek(&(h->phy), 0, SEEK_SET); }

#ifdef __link
#include <iface/link.h>
#endif

void son_set_driver(son_t * h, void * driver);

/*! \details This function creates a new SON file.
 *
 * When you create an SON file, you specify the name where the file
 * will be created. If the file already exists, it is overwritten.
 *
 * You also need to provide a stack. This is so the library can work
 * without using dynamic memory allocation. The \a stack_size needs to
 * be as large as the deepest depth of the data. For example, the
 * following JSON data has a depth of 3.
 *
 * \code
 * {
 *   "make": { //depth level 1 (all files have a depth of at least 1)
 *   	"model" : { //depth level 2
 *   		"color" : "red" //depth level 3
 *   	}
 *   }
 * }
 * \endcode
 *
 * @param h A pointer to the SON handle
 * @name The path to the file to create
 * @param stack A pointer to the SON stack structure
 * @param stack_size The number of stack entries available in the stack
 */
int son_create(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
int son_append(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
int son_open(son_t * h, const char * name);
int son_close(son_t * h, int close_all);
int son_to_json(son_t * h, const char * path);
int son_open_obj(son_t * h, const char * key);
int son_close_obj(son_t * h);
int son_open_array(son_t * h, const char * key);
int son_close_array(son_t * h);
int son_open_data(son_t * h, const char * key);
int son_close_data(son_t * h);
int son_write_str(son_t * h, const char * key, const char * v);
int son_write_num(son_t * h, const char * key, s32 v);
int son_write_unum(son_t * h, const char * key, u32 v);
int son_write_float(son_t * h, const char * key, float v);
int son_write_true(son_t * h, const char * key);
int son_write_false(son_t * h, const char * key);
int son_write_null(son_t * h, const char * key);
int son_write_data(son_t * h, const char * key, const void * v, son_size_t size);
int son_write_open_data(son_t * h, const void * v, son_size_t size);
int son_read_str(son_t * h, const char * access, char * str, son_size_t capacity);
s32 son_read_num(son_t * h, const char * access);
u32 son_read_unum(son_t * h, const char * access);
float son_read_float(son_t * h, const char * access);
int son_read_data(son_t * h, const char * access, void * data, son_size_t size);
son_bool_t son_read_bool(son_t *h, const char * key);

int son_seek(son_t * h, const char * access, son_size_t * data_size);

int son_edit(son_t * h, const char * name);
int son_edit_float(son_t * h, const char * key, float v);
int son_edit_data(son_t * h, const char * key,  void * data, son_size_t size);
int son_edit_str(son_t * h, const char * key, const char *v);
int son_edit_num(son_t *h, const char * key, s32 v);
int son_edit_unum(son_t * h, const char * key, u32 v);
int son_edit_bool(son_t * h, const char * key, son_bool_t v);

typedef struct MCU_PACK {
	u32 version;
	int (*create)(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
	int (*append)(son_t * h, const char * name, son_stack_t * stack, size_t stack_size);
	int (*open)(son_t * h, const char * name);
	int (*close)(son_t * h, int close_all);
	int (*to_json)(son_t * h, const char * path);
	int (*open_obj)(son_t * h, const char * key);
	int (*close_obj)(son_t * h);
	int (*open_array)(son_t * h, const char * key);
	int (*close_array)(son_t * h);
	int (*open_data)(son_t * h, const char * key);
	int (*close_data)(son_t * h);
	int (*write_str)(son_t * h, const char * key, const char * v);
	int (*write_num)(son_t * h, const char * key, s32 v);
	int (*write_unum)(son_t * h, const char * key, u32 v);
	int (*write_float)(son_t * h, const char * key, float v);
	int (*write_true)(son_t * h, const char * key);
	int (*write_false)(son_t * h, const char * key);
	int (*write_null)(son_t * h, const char * key);
	int (*write_data)(son_t * h, const char * key, const void * v, son_size_t size);
	int (*write_open_data)(son_t * h, const void * v, son_size_t size);
	int (*read_str)(son_t * h, const char * access, char * str, son_size_t capacity);
	s32 (*read_num)(son_t * h, const char * access);
	u32 (*read_unum)(son_t * h, const char * access);
	float (*read_float)(son_t * h, const char * access);
	int (*read_data)(son_t * h, const char * access, void * data, son_size_t size);
	son_bool_t (*read_bool)(son_t *h, const char * key);
	int (*seek)(son_t * h, const char * access, son_size_t * data_size);
	int (*edit)(son_t * h, const char * name);
	int (*edit_float)(son_t * h, const char * key, float v);
	int (*edit_data)(son_t * h, const char * key,  void * data, son_size_t size);
	int (*edit_str)(son_t * h, const char * key, const char *v);
	int (*edit_num)(son_t *h, const char * key, s32 v);
	int (*edit_unum)(son_t * h, const char * key, u32 v);
	int (*edit_bool)(son_t * h, const char * key, son_bool_t v);
} son_api_t;

const son_api_t * son_api();


#ifdef __cplusplus
}
#endif


#endif /* SON_H_ */
