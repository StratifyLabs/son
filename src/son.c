/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

#include "son.h"
#include "son_phy.h"

#define SON_BUFFER_SIZE 32

static u8 store_type(const son_store_t * store);
static void store_set_type(son_store_t * store, u8 type);
static u32 store_next(const son_store_t * store);
static void store_set_next(son_store_t * store, u32 offset);
static void store_insert_key(son_store_t * store, const char * key);
static void store_set_checksum(son_store_t * store);
static u32 store_calc_checksum(son_store_t * store);

static int store_read(son_t * h, son_store_t * store);
static int store_write(son_t * h, son_store_t * store);
static int store_seek(son_t * h, const char * access, son_store_t * store, son_size_t * data_size);

static void to_json_recursive(son_t * h, son_size_t last_pos, int indent, int is_array, son_phy_t * phy);
static int write_raw_data(son_t * h, const char * key, son_marker_t type, const void * v, son_size_t size);
static int edit_raw_data(son_t * h, const char * key, const void * data, son_size_t size, son_marker_t new_data_marker);
static int write_open_marker(son_t * h, const char * key, u8 type);
static int write_close_marker(son_t * h);

static char * is_token_array(char * tok, son_size_t * index);
static int seek_array_key(son_t * h, son_size_t ind, son_store_t * store, son_size_t * size);
static int seek_key(son_t * h, const char * name, son_store_t * ob, son_size_t * size);

static void phy_fprintf(son_phy_t * phy, const char * format, ...);
static int phy_lseek_current(son_t * h, s32 offset);
static int phy_lseek_set(son_t * h, s32 offset);
static int phy_lseek_end(son_t * h);

#define SON_MARKER_MASK (0x0F)

#if !defined __StratifyOS__
void son_set_driver(son_t * h, void * handle){
	son_phy_set_driver(&(h->phy), handle);
}

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



u8 store_type(const son_store_t * son){
	return son->o_flags & SON_MARKER_MASK;
}

void store_set_type(son_store_t * son, u8 type){
	son->o_flags = (type & SON_MARKER_MASK);
}


u32 store_next(const son_store_t * son){
	return son->pos.page*65536 + son->pos.page_offset;
}

int store_read(son_t * h, son_store_t * store){
	int ret;

	ret = son_phy_read(&(h->phy), store, sizeof(son_store_t));

	if( ret < 0 ){
		h->err = SON_ERR_READ_IO;
		return -1;
	}

	if( store_calc_checksum(store) != 0 ){
		h->err = SON_ERR_READ_CHECKSUM;
		return -1;
	}

	return (ret == sizeof(son_store_t));
}

int store_write(son_t * h, son_store_t * store){
	store_set_checksum(store);

	if( son_phy_write(&(h->phy), store, sizeof(son_store_t)) != sizeof(son_store_t) ){
		h->err = SON_ERR_WRITE_IO;
		return -1;
	}
	return 0;
}

void store_set_next(son_store_t * son, u32 offset){
	son->pos.page = offset >> 16;
	son->pos.page_offset = offset & 0xFFFF;
}

void store_set_checksum(son_store_t * store){
	u32 * p = (u32*)store;
	u32 other_sum = 0;
	store->checksum = 0;
	u32 i;
	for(i=0; i < (sizeof(son_store_t)/sizeof(u32)); i++){
		other_sum += p[i];
	}
	store->checksum = -1*other_sum;
}

u32 store_calc_checksum(son_store_t * store){
	u32 * p = (u32*)store;
	u32 checksum = 0;
	u32 i;
	for(i=0; i < (sizeof(son_store_t)/sizeof(u32)); i++){
		checksum += p[i];
	}
	return checksum;
}

int write_open_marker(son_t * h, const char * key, u8 type){
	son_store_t store;
	size_t pos;

	if( h->stack_loc < h->stack_size ){

		if( h->stack_loc == 0 ){
			//need to write the root object first
			if( key[0] == 0 ){
				//empty key -- make root
				memset(store.key.name, 0, SON_KEY_NAME_CAPACITY);
				strncpy((char*)store.key.name, "$", SON_KEY_NAME_SIZE);
			} else {
				h->err = SON_ERR_NO_ROOT;
				return -1;
			}
		} else {
			store_insert_key(&store, key);
		}

		pos = phy_lseek_current(h, 0);
		h->stack[h->stack_loc].pos = pos;
		h->stack_loc++;

		store_set_type(&store, type);
		store_set_next(&store, 0);

		return store_write(h, &store);
	}

	h->err = SON_ERR_STACK_OVERFLOW;
	return -1;
}

int write_close_marker(son_t * h){
	son_size_t pos;
	son_size_t current;
	son_store_t store;
	//write the size of the object
	if( h->stack_loc > 0 ){
		h->stack_loc--;
		pos = h->stack[h->stack_loc].pos;
		current = phy_lseek_current(h, 0);

		//seek to the store of the open marker
		if( phy_lseek_set(h, pos) < 0 ){ return -1; }

		//read the current store
		if( store_read(h, &store) < 0 ){
			return -1;
		}

		//seek back to the store position
		if( phy_lseek_set(h, pos) < 0 ){ return -1; }

		//update the store position
		store_set_next(&store, current);

		//save the store
		if( store_write(h, &store) < 0 ){ return -1; }

		if( phy_lseek_set(h, current) < 0 ){
			return -1;
		}

		return 0;
	}
	h->err = SON_ERR_STACK_OVERFLOW;
	return -1;
}

int son_open(son_t * h, const char * name){

	h->stack_loc = 0;
	h->stack = 0;
	h->stack_size = 0;

	if( son_phy_open(&(h->phy), name, SON_O_RDONLY, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return 0;
}

int son_edit(son_t * h, const char * name){
	son_phy_t * phy;
	phy = &(h->phy);

	h->stack_loc = 0;
	h->stack = 0;
	h->stack_size = 0;

	if( son_phy_open(phy, name, SON_O_RDWR, 0) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return 0;
}

int son_append(son_t * h, const char * name, son_stack_t * stack, size_t stack_size){
	son_store_t store;

	if( son_phy_open(&(h->phy), name, SON_O_RDWR, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	//check to see if parent object size is 0
	if( store_read(h, &store) > 0 ){

		//check to see if file can be appended
		if( store_next(&store) == 0 ){
			//now check to see if the final obj is still open for writing

			h->stack = stack;
			h->stack_size = stack_size;
			h->stack_loc = 0;

			phy_lseek_end(h);
			return 0;
		} else {
			h->err = SON_ERR_CANNOT_APPEND;
		}
	}

	son_phy_close(&(h->phy));
	return -1;
}


int son_create(son_t * h, const char * name, son_stack_t * stack, size_t stack_size){

	if( son_phy_open(&(h->phy), name, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	h->stack = stack;
	h->stack_size = stack_size;
	h->stack_loc = 0;
	return 0;
}

int son_close(son_t * h, int close_all){
	int ret;
	if( h->phy.fd < 0 ){
		return 0;
	}

	if( h->stack != 0 ){
		if( close_all ){
			while( h->stack_loc > 0 ){
				write_close_marker(h);
			}
		}
	}


	ret = son_phy_close(&(h->phy));
	h->phy.fd = -1;
	if( ret < 0 ){
		h->err = SON_ERR_CLOSE_IO;
		return -1;
	}
	return 0;
}


int son_open_obj(son_t * h, const char * key){
	return write_open_marker(h, key, SON_OBJ);
}

int son_close_obj(son_t * h){
	return write_close_marker(h);
}

int son_open_data(son_t * h, const char * key){
	return write_open_marker(h, key, SON_DATA);
}

int son_close_data(son_t * h){
	return write_close_marker(h);
}

void store_insert_key(son_store_t * son, const char * key){
	memset(son->key.name, 0, SON_KEY_NAME_CAPACITY);
	strncpy((char*)son->key.name, key, SON_KEY_NAME_SIZE);
}

int son_open_array(son_t * h, const char * key){
	return write_open_marker(h, key, SON_ARRAY);
}

int son_close_array(son_t * h){
	return write_close_marker(h);
}

int son_write_str(son_t * h, const char * key, const char * v){
	return write_raw_data(h, key, SON_STRING, v, strlen(v));
}

int son_write_num(son_t * h, const char * key, s32 v){
	return write_raw_data(h, key, SON_NUMBER_S32, &v, sizeof(v));
}


int son_write_unum(son_t * h, const char * key, u32 v){
	return write_raw_data(h, key, SON_NUMBER_U32, &v, sizeof(v));
}

int son_write_float(son_t * h, const char * key, float v){
	return write_raw_data(h, key, SON_FLOAT, &v, sizeof(float));
}

int son_write_true(son_t * h, const char * key){
	return write_raw_data(h, key, SON_TRUE, 0, 0);
}

int son_write_false(son_t * h, const char * key){
	return write_raw_data(h, key, SON_FALSE, 0, 0);
}

int son_write_null(son_t * h, const char * key){
	return write_raw_data(h, key, SON_NULL, 0, 0);
}

int son_write_data(son_t * h, const char * key, const void * v, son_size_t size){
	return write_raw_data(h, key, SON_DATA, v, size);
}

int son_write_open_data(son_t * h, const void * v, son_size_t size){
	int ret;
	ret = son_phy_write(&(h->phy), v, size);
	if( ret < 0 ){
		h->err = SON_ERR_WRITE_IO;
	}
	return ret;
}

int write_raw_data(son_t * h, const char * key, son_marker_t type, const void * v, son_size_t size){
	size_t pos;
	son_store_t store;
	int ret;

	if( h->stack_size == 0 ){
		//stack size is set to zero when the file is opened for read only
		h->err = SON_ERR_CANNOT_WRITE;
		return -1;
	}

	if( h->stack_loc == 0 ){
		h->err = SON_ERR_NO_ROOT;
		return -1;
	}

	store_insert_key(&store, key);
	store_set_type(&store, type);


	pos = phy_lseek_current(h, 0);
	store_set_next(&store, pos + sizeof(son_store_t) + size);

	if( store_write(h, &store) != 0 ){ return -1; }

	ret = son_phy_write(&(h->phy), v, size);
	if( ret < 0 ){
		h->err = SON_ERR_WRITE_IO;
	}
	return ret;
}

char * is_token_array(char * tok, son_size_t * index){
	char * arr;
	arr = strchr(tok, '[');
	if( arr == 0 ){
		return 0;
	}
#if !defined __StratifyOS__
	sscanf(arr, "[%d]", index);
#else
	sscanf(arr, "[%ld]", index);
#endif


	arr[0] = 0;
	return arr + 1;
}

int seek_array_key(son_t * h, son_size_t ind, son_store_t * store, son_size_t * size){
	son_size_t pos;
	son_size_t i;
	son_size_t next;

	pos = phy_lseek_current(h, 0);

	for(i=0; i <= ind; i++){

		if( store_read(h, store) <= 0 ){
			//this is an error which is set by store_read()
			return 0;
		}

		pos = phy_lseek_current(h, 0);

		next = store_next(store);

		if( next == 0 ){
			h->err = SON_ERR_ARRAY_INDEX_NOT_FOUND;
			return 0;
		}

		if( i != ind ){
			phy_lseek_set(h, next);
		}
	}

	*size = store_next(store) - pos;
	return 1;


	//never arrives here
	return 0;
}

int seek_key(son_t * h, const char * name, son_store_t * ob, son_size_t * size){
	son_store_t store;
	size_t pos;
	u32 next;
	int ret;

	*size = 0;

	while( (ret = store_read(h, &store)) > 0 ){

		next = store_next(&store);

		//check to see if son is a valid object
		if( store.key.name[0] == 0 ){
			//this is not a valid son object or is the end of the file
			h->err = SON_ERR_INVALID_KEY;
			return 0;
		}

		pos = phy_lseek_current(h, 0);

		//if next is 0, then the object hasn't been closed yet (and must be an array or object marker)
		if( next > 0 ){
			*size = next - pos;
		}

		if( strncmp(name, (char*)store.key.name, SON_KEY_NAME_SIZE) == 0 ){
			*ob = store;
			return 1;
		}

		//seek to the next object
		if( next > 0 ){
			phy_lseek_set(h, next);
		}

	}

	if( ret == 0 ){
		h->err = SON_ERR_KEY_NOT_FOUND;
	}
	return 0;
}

int store_seek(son_t * h, const char * access, son_store_t * son, son_size_t * data_size){
	char acc[SON_ACCESS_NAME_CAPACITY];
	char * str;
	char * tok;
	char * nested_array_tok;
	char acc_item[SON_ACCESS_NAME_CAPACITY];
	son_size_t ind;

	phy_lseek_set(h, 0);

	if( strnlen(access, SON_ACCESS_NAME_SIZE) > (SON_ACCESS_MAX_USER_SIZE) ){
		h->err = SON_ERR_ACCESS_TOO_LONG;
		return -1;
	}

	strncpy(acc, "$.", SON_ACCESS_NAME_SIZE);
	strncat(acc, access, SON_ACCESS_NAME_SIZE - 2);

	//! \todo Need to check and see if the access string is too large

	str = acc;

	//peel off object names or index values
	while( (tok = strtok_r(str, ".", &str)) != 0 ){

		strncpy(acc_item, tok, SON_ACCESS_NAME_SIZE);

		if( (nested_array_tok = is_token_array(acc_item, &ind)) != 0 ){

			//search for token then array index
			if( seek_key(h, acc_item, son, data_size) ){

				do {
					//now find the array object
					if( seek_array_key(h, ind, son, data_size) == 0){
						return -1;
					}

					nested_array_tok = is_token_array(nested_array_tok, &ind);

				} while( nested_array_tok != 0 );

			} else {
				return -1;
			}

		} else {
			//search for token
			if( seek_key(h, tok, son, data_size) == 0 ){
				return -1;
			}
		}
	}

	return 0;
}

int son_seek(son_t * h, const char * access, son_size_t * data_size){
	son_store_t son;
	if( store_seek(h, access, &son, data_size) < 0 ){
		return -1;
	}

	return son_phy_lseek(&(h->phy), 0, SEEK_CUR);
}

int son_read_raw_data(son_t * h, const char * access, void * data, son_size_t size, son_store_t * son){
	son_size_t data_size;

	if( store_seek(h, access, son, &data_size) < 0 ){
		return -1;
	}

	memset(data, 0, size);
	if( data_size > size ){
		data_size = size;
	}


	if( son_phy_read(&(h->phy), data, data_size) != data_size ){
		h->err = SON_ERR_READ_IO;
		return -1;
	}

	return data_size;
}

int son_read_str(son_t * h, const char * access, char * str, son_size_t capacity){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h, access, str, capacity, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = str;
	memset(buffer, 0, SON_BUFFER_SIZE);

	switch(store_type(&son)){
	case SON_FLOAT: snprintf(buffer, 32, "%f", *(ptype.f)); break;
	case SON_NUMBER_U32: snprintf(buffer, 32, SON_PRINTF_INT, *(ptype.n_u32)); break;
	case SON_NUMBER_S32: snprintf(buffer, 32, SON_PRINTF_INT, *(ptype.n_s32)); break;
	case SON_TRUE: strcpy(buffer, "true"); break;
	case SON_FALSE: strcpy(buffer, "false"); break;
	case SON_NULL: strcpy(buffer, "null"); break;
	case SON_DATA:
	case SON_STRING:
		//convert to an encoded string
		return data_size;

	}

	if( buffer[0] != 0 ){
		strncpy(str, buffer, capacity-1);
	}

	return data_size;
}

s32 son_read_num(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(store_type(&son)){
	case SON_FLOAT: return (int)*(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1;
	case SON_FALSE: return 0;
	case SON_NULL: return 0;
	case SON_STRING: return atoi(buffer);
	case SON_DATA: return -1;
	}

	return 0;
}

u32 son_read_unum(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(store_type(&son)){
	case SON_FLOAT: return (int)*(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1;
	case SON_FALSE: return 0;
	case SON_NULL: return 0;
	case SON_STRING: return atoi(buffer);
	case SON_DATA: return -1;
	}

	return 0;
}

float son_read_float(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(store_type(&son)){
	case SON_FLOAT: return *(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1.0;
	case SON_FALSE: return 0.0;
	case SON_NULL: return 0.0;
	case SON_STRING: return atof(buffer);
	case SON_DATA: return 0.0;
	}

	return 0.0;
}

int son_read_data(son_t * h, const char * access, void * data, son_size_t size){
	son_store_t son;
	return son_read_raw_data(h, access, data, size, &son);
}

son_bool_t son_read_bool(son_t *h, const char * key){
	int data_size;
	son_store_t son;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE,&son);

	if(data_size < 0){
		return -1;
	}

	//ptype.cdata = buffer;

	switch(store_type(&son)){
	case SON_TRUE: return TRUE;
	case SON_FALSE: return FALSE;
	}

	return 0;
}
static void print_indent(int indent, son_phy_t * phy){
	int i;
	for(i=0; i < indent; i++){ phy_fprintf(phy, "\t"); }
}

int son_to_json(son_t * h, const char * path){
	int is_array;

	son_store_t store;
	son_phy_t phy;

	memset(&phy, 0, sizeof(phy));

	u8 type;

	if( store_read(h, &store) <= 0 ){
		return -1;
	}

	type = store_type(&store);

	if( type == SON_OBJ ){
		is_array = 0;
	} else if( type == SON_ARRAY ){
		is_array = 1;
	} else {
		h->err = SON_ERR_INVALID_ROOT;
		return -1;
	}

	//create a new file
	if( son_phy_open(&phy, path, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
		return -1;
	}

	phy_fprintf(&phy, "{\n");
	to_json_recursive(h, store_next(&store), 1, is_array, &phy);
	phy_fprintf(&phy, "}\n");

	return son_phy_close(&phy);
}

void phy_fprintf(son_phy_t * phy, const char * format, ...){
	char buffer[256];
	va_list args;
	va_start (args, format);
	vsnprintf(buffer, 256, format, args);
	va_end (args);
	son_phy_write(phy, buffer, strnlen(buffer, 256));
}

int phy_lseek_current(son_t * h, s32 offset){
	int ret;
	ret = son_phy_lseek(&(h->phy), offset, SON_SEEK_CUR);
	if( ret < 0 ){
		h->err = SON_ERR_SEEK_IO;
	}
	return ret;
}

int phy_lseek_set(son_t * h, s32 offset){
	int ret;
	ret = son_phy_lseek(&(h->phy), offset, SON_SEEK_SET);
	if( ret < 0 ){
		h->err = SON_ERR_SEEK_IO;
	}
	return ret;
}

int phy_lseek_end(son_t * h){
	return son_phy_lseek(&(h->phy), 0, SON_SEEK_END);
}

void to_json_recursive(son_t * h, son_size_t last_pos, int indent, int is_array, son_phy_t * phy){
	son_store_t store;
	son_size_t data_size;
	son_size_t pos;
	u8 type;


	while( store_read(h, &store) > 0 ){

		pos = phy_lseek_current(h, 0);

		data_size = store_next(&store) - pos;
		type = store_type(&store);


		print_indent(indent, phy);
		if( type == SON_OBJ ){
			if( is_array ){
				phy_fprintf(phy, "{\n");
			} else {
				phy_fprintf(phy, "\"%s\" : {\n", store.key.name);
			}
			to_json_recursive(h, store_next(&store), indent+1, 0, phy);
			print_indent(indent, phy);
			phy_fprintf(phy, "}");
			//add a comma?
			if( store_next(&store) != last_pos ){
				phy_fprintf(phy, ",");
			}

			phy_fprintf(phy, "\n");
		} else if( type == SON_ARRAY ){
			if( is_array ){
				phy_fprintf(phy, "[\n");
			} else {
				phy_fprintf(phy, "\"%s\" : [\n", store.key.name);
			}
			to_json_recursive(h, store_next(&store), indent+1, 1, phy);
			print_indent(indent, phy);
			phy_fprintf(phy, "]");
			//add a comma?
			if( store_next(&store) != last_pos ){
				phy_fprintf(phy, ",");
			}

			phy_fprintf(phy, "\n");

		} else {
			char buffer[data_size+1];
			buffer[data_size] = 0;
			son_phy_read(&(h->phy), buffer, data_size);

			if( is_array == 0 ){
				phy_fprintf(phy, "\"%s\" : ", store.key.name);
			}

			if( type == SON_STRING ){
				phy_fprintf(phy, "\"%s\"", buffer);
			} else if ( type == SON_FLOAT ){
				float * value = (float*)buffer;
				phy_fprintf(phy, "%f", *value);
			} else if ( type == SON_NUMBER_U32 ){
				u32 * value = (u32*)buffer;
#if !defined __StratifyOS__
				phy_fprintf(phy, "%d", *value);
#else
				phy_fprintf(phy, "%ld", *value);
#endif
			} else if ( type == SON_NUMBER_S32 ){
				s32 * value = (s32*)buffer;
#if !defined __StratifyOS__
				phy_fprintf(phy, "%d", *value);
#else
				phy_fprintf(phy, "%ld", *value);
#endif
			} else if ( type == SON_TRUE ){
				phy_fprintf(phy, "true");
			} else if ( type == SON_FALSE ){
				phy_fprintf(phy, "false");
			} else if ( type == SON_NULL ){
				phy_fprintf(phy, "null");
			} else if ( type == SON_DATA ){
				phy_fprintf(phy, "DATA");
			}

			if( store_next(&store) != last_pos ){
				phy_fprintf(phy, ",");
			}
			phy_fprintf(phy, "\n");
		}

		if( store_next(&store) == last_pos ){
			return;
		}
	}
}

int son_edit_float(son_t * h, const char * key, float v){
	return edit_raw_data(h, key, &v, sizeof(v), SON_FLOAT);
}

int son_edit_unum(son_t * h, const char * key, u32 v){
	return edit_raw_data(h, key, &v, sizeof(v), SON_NUMBER_U32);
}

int son_edit_num(son_t * h, const char * key, s32 v){
	return edit_raw_data(h, key, &v, sizeof(v), SON_NUMBER_S32);
}

int son_edit_str(son_t * h, const char * key, const char * v){
	return edit_raw_data(h, key, v, strlen(v), SON_STRING);
}

int son_edit_data(son_t * h, const char * key,  void * data, son_size_t size){
	return edit_raw_data(h, key, data, size, SON_DATA);
}

int son_edit_bool(son_t * h, const char * key, son_bool_t v){
	size_t pos;
	son_store_t store;
	son_marker_t type;
	int read_length;
	char buffer[SON_BUFFER_SIZE];

	read_length = son_read_raw_data(h, key, buffer, SON_BUFFER_SIZE, &store);

	if(read_length < 0){
		return -1;
	}

	if (v == TRUE){
		type = SON_TRUE;
	}
	else if (v == FALSE){
		type = SON_FALSE;
	}
	else{
		return -1;
	}

	store_set_type(&store, type);

	pos = phy_lseek_current(h, -1*(s32)sizeof(store));
	store_set_next(&store, pos+sizeof(son_store_t));

	return store_write(h, &store);
}


int edit_raw_data(son_t * h, const char * key, const void * data, son_size_t size, son_marker_t new_data_marker){
	son_size_t data_size;
	son_store_t son;

	if( store_seek(h, key, &son, &data_size) < 0 ){
		return -1;
	}

	if( store_type(&son) != new_data_marker ){
		return -1;
	}


	if( size > data_size ){
		size = data_size;
	}

	return son_phy_write(&(h->phy), data, size);
}
