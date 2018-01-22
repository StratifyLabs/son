//Copyright 2011-2016 Tyler Gilbert; All Rights Reserved

#ifndef SON_LOCAL_H_
#define SON_LOCAL_H_

#if !defined __StratifyOS__
#if !defined MCU_USED
#define MCU_UNUSED __attribute__((unused))
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

#include "son.h"
#include "son_phy.h"

#define SON_MARKER_MASK (0x0F)

typedef struct MCU_PACK {
	u16 version;
	u16 resd;
} son_hdr_t;

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

#define TRUE 1
#define FALSE 0

#define SON_BUFFER_SIZE 32

void son_local_assign_checksum(son_t * h);
int son_local_verify_checksum(son_t * h);

static u8 son_local_store_type(const son_store_t * son) MCU_UNUSED;
u8 son_local_store_type(const son_store_t * son){
	return son->o_flags & SON_MARKER_MASK;
}

static void son_local_store_set_type(son_store_t * son, u8 type) MCU_UNUSED;
void son_local_store_set_type(son_store_t * son, u8 type){
	son->o_flags = (type & SON_MARKER_MASK);
}

static u32 son_local_store_next(const son_store_t * son) MCU_UNUSED;
u32 son_local_store_next(const son_store_t * son){
	return son->pos.page*65536 + son->pos.page_offset;
}

static void son_local_store_set_next(son_store_t * son, u32 offset) MCU_UNUSED;
void son_local_store_set_next(son_store_t * son, u32 offset){
	son->pos.page = offset >> 16;
	son->pos.page_offset = offset & 0xFFFF;
}

void son_local_store_insert_key(son_store_t * store, const char * key);
u32 son_local_store_calc_checksum(son_store_t * store);

int son_local_store_read(son_t * h, son_store_t * store);
int son_local_store_write(son_t * h, son_store_t * store);
int son_local_store_seek(son_t * h, const char * access, son_store_t * store, son_size_t * data_size);

int son_local_phy_lseek_current(son_t * h, s32 offset);
int son_local_phy_lseek_set(son_t * h, s32 offset);

int son_local_read_raw_data(son_t * h, const char * access, void * data, son_size_t size, son_store_t * son);


#if !defined __StratifyOS__

#define SON_PRINTF_INT "%d"

#else

#define SON_PRINTF_INT "%ld"

#define SON_SEEK_SET SEEK_SET
#define SON_SEEK_CUR SEEK_CUR
#define SON_SEEK_END SEEK_END
#define SON_O_RDONLY O_RDONLY
#define SON_O_RDWR O_RDWR
#define SON_O_CREAT O_CREAT
#define SON_O_TRUNC O_TRUNC
#endif



#endif /* SON_LOCAL_H_ */
