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


static void son_to_json_recursive(son_t * h, son_size_t last_pos, int indent, int is_array, son_phy_t * phy);
static int son_seek_store(son_t * h, const char * access, son_store_t * son, son_size_t * data_size);
static void son_insert_key(son_store_t * son, const char * key);
static int son_write_raw_data(son_t * h, const char * key, son_marker_t type, const void * v, son_size_t size);
static int son_write_open_marker(son_t * h, const char * key, u8 type, u8 fixed_size);
static int son_write_close_marker(son_t * h);
static void son_fprintf(son_phy_t * phy, const char * format, ...);

#define SON_FIXED_ARRAY_FLAG (1<<7)
#define SON_TYPE_MASK (0x0F)

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



u8 son_type(const son_store_t * son){
	return son->o_flags & SON_TYPE_MASK;
}

u8 son_fixed_array_size(const son_store_t * son){
	return (son->o_flags & SON_FIXED_ARRAY_FLAG) != 0;
}

void son_set_type(son_store_t * son, u8 type, u8 fixed_array_size){
	son->o_flags = (type & SON_TYPE_MASK) | (fixed_array_size ? SON_FIXED_ARRAY_FLAG : 0);
}


u32 son_next(const son_store_t * son){
	return son->pos.page*65536 + son->pos.page_offset;
}

void son_set_next(son_store_t * son, u32 offset){
	son->pos.page = offset >> 16;
	son->pos.page_offset = offset & 0xFFFF;
}

int son_write_open_marker(son_t * h, const char * key, u8 type, u8 fixed_size){
	son_store_t son_store;
	size_t pos;


	if( h->stack_loc < h->stack_size ){

		son_insert_key(&son_store, key);

		pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);
		h->stack[h->stack_loc].pos = pos;
		h->stack_loc++;

		son_set_type(&son_store, type, fixed_size);
		son_set_next(&son_store, 0);

		return son_phy_write(&(h->phy), &son_store, sizeof(son_store_t));
	}

	return -1;
}

static int son_write_close_marker(son_t * h){
	son_size_t pos;
	son_size_t current;
	son_store_t son;
	//write the size of the object
	if( h->stack_loc > 0 ){
		h->stack_loc--;
		pos = h->stack[h->stack_loc].pos;
		current = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);

		son_set_next(&son, current);

		if( son_phy_lseek(&(h->phy), pos + offsetof(son_store_t, pos), SON_SEEK_SET) < 0 ){
			return -1;
		}
		if( son_phy_write(&(h->phy), &son.pos, sizeof(son_pos_t)) < 0 ){
			return -1;
		}
		if( son_phy_lseek(&(h->phy), current, SON_SEEK_SET) < 0 ){
			return -1;
		}
	}
	return 0;
}

int son_open(son_t * h, const char * name){

	h->stack_loc = 0;
	h->stack = 0;
	h->stack_size = 0;

	if( son_phy_open(&(h->phy), name, SON_O_RDONLY, 0666) < 0 ){
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
		return -1;
	}

	return 0;
}

int son_append(son_t * h, const char * name, son_stack_t * stack, size_t stack_size){
	son_store_t store;

	if( son_phy_open(&(h->phy), name, SON_O_RDWR, 0666) < 0 ){
		return -1;
	}

	//check to see if parent object size is 0
	if( son_phy_read(&(h->phy), &store, sizeof(son_store_t)) ==  sizeof(son_store_t)){

		//check to see if file can be appended
		if( son_next(&store) == 0 ){
			//now check to see if the final obj is still open for writing

			h->stack = stack;
			h->stack_size = stack_size;
			h->stack_loc = 0;

			son_phy_lseek(&(h->phy), 0, SON_SEEK_END);
			return 0;
		}
	}

	son_phy_close(&(h->phy));
	return -1;
}


int son_create(son_t * h, const char * name, son_stack_t * stack, size_t stack_size){

	if( son_phy_open(&(h->phy), name, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
		return -1;
	}

	h->stack = stack;
	h->stack_size = stack_size;
	h->stack_loc = 0;
	return 0;
}

int son_close(son_t * h, int close_all){
	if( h->phy.fd < 0 ){
		return 0;
	}

	if( h->stack != 0 ){
		if( close_all ){
			while( h->stack_loc > 0 ){
				son_write_close_marker(h);
			}
		}
	}

	son_phy_close(&(h->phy));
	h->phy.fd = -1;
	return 0;
}


int son_open_obj(son_t * h, const char * key){
	return son_write_open_marker(h, key, SON_OBJ, 0);
}

int son_close_obj(son_t * h){
	return son_write_close_marker(h);
}

int son_open_data(son_t * h, const char * key){
	return son_write_open_marker(h, key, SON_DATA, 0);
}

int son_close_data(son_t * h){
	return son_write_close_marker(h);
}

void son_insert_key(son_store_t * son, const char * key){
	memset(son->key.name, 0, SON_KEY_NAME_CAPACITY);
	strncpy((char*)son->key.name, key, SON_KEY_NAME_SIZE);
}

int son_open_array(son_t * h, const char * key, int fixed_size){
	return son_write_open_marker(h, key, SON_ARRAY, fixed_size);
}

int son_close_array(son_t * h){
	return son_write_close_marker(h);
}

int son_write_str(son_t * h, const char * key, const char * v){
	return son_write_raw_data(h, key, SON_STRING, v, strlen(v));
}

int son_write_num(son_t * h, const char * key, int32_t v){
	return son_write_raw_data(h, key, SON_NUMBER_S32, &v, sizeof(v));
}


int son_write_unum(son_t * h, const char * key, u32 v){
	return son_write_raw_data(h, key, SON_NUMBER_U32, &v, sizeof(v));
}

int son_write_float(son_t * h, const char * key, float v){
	return son_write_raw_data(h, key, SON_FLOAT, &v, sizeof(float));
}

int son_write_true(son_t * h, const char * key){
	return son_write_raw_data(h, key, SON_TRUE, 0, 0);
}

int son_write_false(son_t * h, const char * key){
	return son_write_raw_data(h, key, SON_FALSE, 0, 0);
}

int son_write_null(son_t * h, const char * key){
	return son_write_raw_data(h, key, SON_NULL, 0, 0);
}

int son_write_data(son_t * h, const char * key, const void * v, son_size_t size){
	return son_write_raw_data(h, key, SON_DATA, v, size);
}

int son_write_open_data(son_t * h, const void * v, son_size_t size){
	return son_phy_write(&(h->phy), v, size);
}

int son_write_raw_data(son_t * h, const char * key, son_marker_t type, const void * v, son_size_t size){
	size_t pos;
	son_store_t son;

	if( h->stack_size == 0 ){
		return -1;
	}

	son_insert_key(&son, key);
	son_set_type(&son, type, 0);


	pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);
	son_set_next(&son, pos + sizeof(son_store_t) + size);

	if( son_phy_write(&(h->phy), &son, sizeof(son_store_t)) < 0 ){
		return -1;
	}


	return son_phy_write(&(h->phy), v, size);
}

int son_token_is_array(char * tok, son_size_t * index){
	char * arr;
	arr = strstr(tok, "[");
	if( arr == 0 ){
		return 0;
	}
#if !defined __StratifyOS__
	sscanf(arr, "[%d]", index);
#else
	sscanf(arr, "[%ld]", index);
#endif


	arr[0] = 0;
	return 1;
}

int son_seek_array_key(son_t * h, son_size_t ind, u8 fixed_size, son_store_t * son, son_size_t * size){
	son_size_t pos;
	son_size_t data_size;
	son_size_t array_data_size;
	son_size_t i;
	son_size_t next;

	pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);
	array_data_size = son_next(son) - pos;

	if( fixed_size ){
		if( son_phy_read(&(h->phy), son, sizeof(son_store_t)) < 0 ){
			return 0;
		}
		data_size = son_next(son) - pos;
		if( ind > 0 ){
			if( ((ind+1) * data_size) < array_data_size){
				son_phy_lseek(&(h->phy), pos + ind*data_size, SON_SEEK_SET);
				son_phy_read(&(h->phy), son, sizeof(son_store_t));
			} else {
				return 0;
			}
		}

		*size = data_size;
		return 1;
	} else {
		for(i=0; i <= ind; i++){
			if( son_phy_read(&(h->phy), son, sizeof(son_store_t)) <= 0 ){
				return 0;
			}
			pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);


			next = son_next(son);

			if( next == 0 ){
				return 0;
			}

			if( i != ind ){
				son_phy_lseek(&(h->phy), next, SON_SEEK_SET);
			}
		}

		*size = son_next(son) - pos;

		return 1;
	}
	return 0;
}

int son_seek_key(son_t * h, const char * name, son_store_t * ob, son_size_t * size){
	son_store_t son;
	size_t pos;
	u32 next;
	int ret;

	*size = 0;

	while( (ret = son_phy_read(&(h->phy), &son, sizeof(son_store_t))) > 0 ){

		next = son_next(&son);

		//check to see if son is a valid object
		if( (son.key.name[0] == 0) ){
			//this is not a valid son object or is the end of the file
			return 0;
		}

		pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);

		//if next is 0, then the object hasn't been closed yet (and must be an array or object marker)
		if( next > 0 ){
			*size = next - pos;
		}

		if( strcmp(name, (char*)son.key.name) == 0 ){
			*ob = son;
			return 1;
		}

		//seek to the next object
		if( next > 0 ){
			son_phy_lseek(&(h->phy), next, SON_SEEK_SET);
		}
	}

	return 0;
}

int son_seek_store(son_t * h, const char * access, son_store_t * son, son_size_t * data_size){
	char acc[SON_ACCESS_NAME_CAPACITY];
	char * str;
	char * tok;
	char acc_item[SON_ACCESS_NAME_CAPACITY];
	son_size_t ind;

	son_phy_lseek(&(h->phy), 0, SON_SEEK_SET);

	strcpy(acc, "root.");
	strncat(acc, access, SON_ACCESS_NAME_SIZE - 5);
	str = acc;

	//peel off object names or index values
	while( (tok = strtok_r(str, ".", &str)) != 0 ){

		strncpy(acc_item, tok, SON_ACCESS_NAME_SIZE);
		if( son_token_is_array(acc_item, &ind) ){

			//search for token then array index
			if( son_seek_key(h, acc_item, son, data_size) ){
				//now found the array object

				if( son_seek_array_key(h, ind, son_fixed_array_size(son), son, data_size) == 0){
					return -1;
				}

			} else {
				return -1;
			}

		} else {
			//search for token
			if( son_seek_key(h, tok, son, data_size) == 0 ){
				return -1;
			}
		}
	}

	return 0;
}

int son_seek(son_t * h, const char * access, son_size_t * data_size){
	son_store_t son;
	if( son_seek_store(h, access, &son, data_size) < 0 ){
		return -1;
	}

	return son_phy_lseek(&(h->phy), 0, SEEK_CUR);
}

int son_read_raw_data(son_t * h, const char * access, void * data, son_size_t size, son_store_t * son){
	son_size_t data_size;

	if( son_seek_store(h, access, son, &data_size) < 0 ){
		return -1;
	}

	memset(data, 0, size);
	if( data_size > size ){
		data_size = size;
	}

	if( son_phy_read(&(h->phy), data, data_size) != data_size ){
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

	switch(son_type(&son)){
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

int32_t son_read_num(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(son_type(&son)){
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

	switch(son_type(&son)){
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

	switch(son_type(&son)){
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
	//son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE,&son);

	if(data_size < 0){
		return -1;
	}

	//ptype.cdata = buffer;

	switch(son_type(&son)){
	case SON_TRUE: return TRUE;
	case SON_FALSE: return FALSE;
	}

	return 0;
}
static void print_indent(int indent, son_phy_t * phy){
	int i;
	for(i=0; i < indent; i++){ son_fprintf(phy, "\t"); }
}

int son_to_json(son_t * h, const char * path){
	int is_array;

	son_store_t son;
	son_phy_t phy;

	memset(&phy, 0, sizeof(phy));

	u8 type;

	son_phy_read(&(h->phy), &son, sizeof(son_store_t));

	type = son_type(&son);

	if( type == SON_OBJ ){
		is_array = 0;
	} else if( type == SON_ARRAY ){
		is_array = 1;
	} else {
		return -1;
	}

	//create a new file
	if( son_phy_open(&phy, path, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
		return -1;
	}

	son_fprintf(&phy, "{\n");
	son_to_json_recursive(h, son_next(&son), 1, is_array, &phy);
	son_fprintf(&phy, "}\n");

	return son_phy_close(&phy);
}

void son_fprintf(son_phy_t * phy, const char * format, ...){
	char buffer[256];
	va_list args;
	va_start (args, format);
	vsnprintf(buffer, 256, format, args);
	va_end (args);
	son_phy_write(phy, buffer, strnlen(buffer, 256));
}

void son_to_json_recursive(son_t * h, son_size_t last_pos, int indent, int is_array, son_phy_t * phy){
	son_store_t son;
	son_size_t data_size;
	son_size_t pos;
	u8 type;


	while( son_phy_read(&(h->phy), &son, sizeof(son_store_t)) > 0 ){

		pos = son_phy_lseek(&(h->phy), 0, SON_SEEK_CUR);

		data_size = son_next(&son) - pos;
		type = son_type(&son);


		print_indent(indent, phy);
		if( type == SON_OBJ ){
			if( is_array ){
				son_fprintf(phy, "{\n");
			} else {
				son_fprintf(phy, "\"%s\" : {\n", son.key.name);
			}
			son_to_json_recursive(h, son_next(&son), indent+1, 0, phy);
			print_indent(indent, phy);
			son_fprintf(phy, "}");
			//add a comma?
			if( son_next(&son) != last_pos ){
				son_fprintf(phy, ",");
			}

			son_fprintf(phy, "\n");
		} else if( type == SON_ARRAY ){
			if( is_array ){
				son_fprintf(phy, "[\n");
			} else {
				son_fprintf(phy, "\"%s\" : [\n", son.key.name);
			}
			son_to_json_recursive(h, son_next(&son), indent+1, 1, phy);
			print_indent(indent, phy);
			son_fprintf(phy, "]");
			//add a comma?
			if( son_next(&son) != last_pos ){
				son_fprintf(phy, ",");
			}

			son_fprintf(phy, "\n");

		} else {
			char buffer[data_size+1];
			buffer[data_size] = 0;
			son_phy_read(&(h->phy), buffer, data_size);

			if( is_array == 0 ){
				son_fprintf(phy, "\"%s\" : ", son.key.name);
			}

			if( type == SON_STRING ){
				son_fprintf(phy, "\"%s\"", buffer);
			} else if ( type == SON_FLOAT ){
				float * value = (float*)buffer;
				son_fprintf(phy, "%f", *value);
			} else if ( type == SON_NUMBER_U32 ){
				u32 * value = (u32*)buffer;
#if !defined __StratifyOS__
				son_fprintf(phy, "%d", *value);
#else
				son_fprintf(phy, "%ld", *value);
#endif
			} else if ( type == SON_NUMBER_S32 ){
				int32_t * value = (int32_t*)buffer;
#if !defined __StratifyOS__
				son_fprintf(phy, "%d", *value);
#else
				son_fprintf(phy, "%ld", *value);
#endif
			} else if ( type == SON_TRUE ){
				son_fprintf(phy, "true");
			} else if ( type == SON_FALSE ){
				son_fprintf(phy, "false");
			} else if ( type == SON_NULL ){
				son_fprintf(phy, "null");
			} else if ( type == SON_DATA ){
				son_fprintf(phy, "DATA");
			}

			if( son_next(&son) != last_pos ){
				son_fprintf(phy, ",");
			}
			son_fprintf(phy, "\n");
		}

		if( son_next(&son) == last_pos ){
			return;
		}
	}
}

int son_edit_bool(son_t * h, const char * key, son_bool_t v){
	size_t pos;
	son_store_t son;
	son_marker_t type;
	int read_length;
	char buffer[SON_BUFFER_SIZE];

	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE,&son);

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

	son_set_type(&son,type,0);

	pos = son_phy_lseek(&(h->phy),-1*(s32)sizeof(son),SON_SEEK_CUR);
	son_set_next(&son,pos+sizeof(son_store_t));

	return son_phy_write(&(h->phy),&son,sizeof(son_store_t));

}

float son_edit_float(son_t * h, const char * key, float v){
	son_store_t son;
	son_size_t data_size;

	int read_length;
	char buffer[SON_BUFFER_SIZE];

	float  t = v;

	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE, &son);

	if(read_length < 0){
		return -1;
	}

	if(son_seek_store(h,key,&son,&data_size) < 0){
		return -1;
	}

	return son_phy_write(&(h->phy),&t,read_length);
}

int son_edit_unum(son_t * h, const char * key, u32 v){
	son_store_t son;
	son_size_t data_size;

	int read_length;
	char buffer[SON_BUFFER_SIZE];

	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE, &son);

	if(read_length < 0){
		return -1;
	}

	if(son_seek_store(h,key,&son,&data_size) < 0){
		return -1;
	}

	return son_phy_write(&(h->phy),&v,read_length);

}

int son_edit_num(son_t * h, const char * key, int32_t v){
	son_store_t son;
	son_size_t data_size;
	int read_length;
	char buffer[SON_BUFFER_SIZE];


	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE, &son);


	if(read_length < 0){
		return -1;
	}


	if(son_seek_store(h,key,&son,&data_size)<0){
		return -1;
	}

	return son_phy_write(&(h->phy),&v,read_length);

}

int son_edit_str(son_t * h, const char * key, const char * v){
	son_store_t son;
	son_size_t data_size;

	int read_length;
	int write_length;
	int read_write_diff;
	char buffer_empty[SON_BUFFER_SIZE];
	char buffer[SON_BUFFER_SIZE];

	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE, &son);

	if (read_length < 0) {
		return -1;
	}

	write_length = strlen(v);

	if(write_length > read_length){
		return -1;
	}

	read_write_diff = abs(read_length-write_length);

	if( son_seek_store(h,key,&son,&data_size ) < 0){
		return -1;
	}

	memset(buffer_empty,' ',read_write_diff);
	memset(buffer,0,read_length);
	strcpy(buffer,(char*) v);
	strncpy(buffer,buffer_empty,read_write_diff);

	return son_phy_write(&(h->phy),buffer,strlen(buffer));
}

int son_edit_data(son_t * h, const char * key,  void * data, son_size_t size ){
	son_size_t data_size;
	son_store_t son;
	char buffer[SON_BUFFER_SIZE];

	int read_length;
	int write_length;
	int read_write_diff;
	char empty_buffer[SON_BUFFER_SIZE];

	read_length = son_read_raw_data(h,key,buffer,SON_BUFFER_SIZE, &son);
	write_length = strlen(data);
	if (read_length < 0){
		return -1;
	}

	read_write_diff = abs(write_length-read_length);

	if(write_length > read_length){
		return -1;
	}

	if(son_seek_store(h,key,&son,&data_size) < 0){
		return -1;
	}

	memset(empty_buffer,' ',read_write_diff);
	memset(buffer,0,read_length);
	strcpy(buffer,data);
	strncat(buffer,empty_buffer,read_write_diff);

	return son_phy_write(&(h->phy),buffer,strlen(buffer));
}

int log_values(char * error,char * val){
	FILE *fp;

	fp = fopen("/Users/nkgau/Desktop/sonlog.txt","a+");
	if(fp == NULL){
		return -1;
	}
	fprintf(fp,"\n%s - %s\n",error,val);
	fclose(fp);
	return 0;
}
