/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved


#include "son_local.h"

#if defined __StratifyOS__
#include <cortexm/cortexm.h>
#else
#define cortexm_assign_zero_sum32(x,y)
#define cortexm_verify_zero_sum32(x,y) 1
#endif

static void phy_fputs(son_phy_t * phy, son_to_json_callback_t callback, void * context, const char * str);

static void phy_fprintf(son_phy_t * phy, son_to_json_callback_t callback, void * context, const char * format, ...);
static void print_indent(int indent, son_phy_t * phy, son_to_json_callback_t callback, void * context);
static void to_json_recursive(son_t * h,
		son_size_t last_pos,
		int indent,
		int is_array,
		son_phy_t * phy,
		son_to_json_callback_t callback,
		void * context);

static int open_from_phy(son_t * h);
static int create_from_phy(son_t * h, son_stack_t * stack, size_t stack_size);
static int edit_from_phy(son_t * h);

static void store_set_checksum(son_store_t * store);

static char * is_token_array(char * tok, son_size_t * index);
static int seek_array_key(son_t * h, son_size_t ind, son_store_t * store, son_size_t * size);
static int seek_key(son_t * h, const char * name, son_store_t * ob, son_size_t * size);

static int base64_encode(char * dest, const void * src, int nbyte);
static int base64_calc_encoded_size(int nbyte);
static char base64_encode_six(uint8_t six_bit_value);

#if !defined __StratifyOS__
void son_set_driver(son_t * h, void * handle){
	son_phy_set_driver(&(h->phy), handle);
}
#endif

int son_get_error(son_t * h){
	int err = h->err;
	if( err != SON_ERR_HANDLE_CHECKSUM ){
		h->err = SON_ERR_NONE;
		son_local_assign_checksum(h);
	}
	return err;
}

int son_append(son_t * h, const char * name, son_stack_t * stack, son_size_t stack_size){
	son_store_t store;
	int ret = 0;

	if( son_phy_open(&(h->phy), name, SON_O_RDWR, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		ret = -1;
	} else {

		if( son_local_phy_lseek_set(h, sizeof(son_hdr_t)) < 0 ){
			ret = -1;
		} else {

			//check to see if parent object size is 0
			if( son_local_store_read(h, &store) > 0 ){
				h->stack = stack;
				h->stack_size = stack_size;
				h->stack_loc = 0;

				//push the root object location onto the stack
				if( h->stack_loc < h->stack_size ){
					h->stack[h->stack_loc].pos = sizeof(son_hdr_t);
					h->stack_loc++;
				} else {
					h->err = SON_ERR_STACK_OVERFLOW;
				}

				son_local_phy_lseek_set(h, son_local_store_next(&store));

			} else {
				son_phy_close(&(h->phy));
				ret = -1;
			}
		}
	}

	son_local_assign_checksum(h);
	return ret;
}

int son_create_message(son_t * h, void * message, int nbyte, son_stack_t * stack, son_size_t stack_size){
	if( son_phy_open_message(&(h->phy), message, nbyte) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return create_from_phy(h, stack, stack_size);
}

int son_create(son_t * h, const char * name, son_stack_t * stack, son_size_t stack_size){
	if( son_phy_open(&(h->phy), name, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return create_from_phy(h, stack, stack_size);
}

int son_open(son_t * h, const char * name){
	//open for read only -- stack is not used
	if( son_phy_open(&(h->phy), name, SON_O_RDONLY, 0666) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}
	return open_from_phy(h);
}

int son_open_message(son_t * h, void * message, int nbyte){
	//open for read only -- stack is not used
	if( son_phy_open_message(&(h->phy), message, nbyte) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}
	return open_from_phy(h);
}

int son_edit_message(son_t * h, void * message, int nbyte){
	if( son_phy_open_message(&(h->phy), message, nbyte) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return edit_from_phy(h);
}

int son_edit(son_t * h, const char * name){

	if( son_phy_open(&(h->phy), name, SON_O_RDWR, 0) < 0 ){
		h->err = SON_ERR_OPEN_IO;
		return -1;
	}

	return edit_from_phy(h);
}

int son_seek(son_t * h, const char * access, son_size_t * data_size){
	son_store_t son;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	int ret = 0;
	if( son_local_store_seek(h, access, &son, data_size) < 0 ){
		ret = -1;
	} else {
		ret = son_phy_lseek(&(h->phy), 0, SEEK_CUR);
	}

	son_local_assign_checksum(h);
	return ret;
}

int son_seek_next(son_t * h, char * name, son_value_t * type){
	son_store_t store;
	u32 next;
	int ret = 0;
	int current;
	u8 tmp;

	if( son_local_verify_checksum(h) < 0 ){ return 0; }

	current = son_local_phy_lseek_current(h, 0);
	if( son_local_store_read(h, &store) > 0 ){
		tmp = son_local_store_type(&store);
		if( type ){ *type = tmp; }

		//copy the name of the current object
		if( strncmp("$", (const char *)store.key.name, SON_KEY_NAME_SIZE) == 0 ){
			name[0] = 0; //empty string for root
		} else {
			strncpy(name, (const char *)store.key.name, SON_KEY_NAME_SIZE);
		}

		//The file needs to seek to the next sibling so the next call works
		next = son_local_store_next(&store);
		son_local_phy_lseek_set(h, next);
		ret = next - current; //size of the advance

	}

	son_local_assign_checksum(h);
	return ret;
}

int son_to_json(son_t * h, const char * path, int (*callback)(void*, const char *), void * context){
	int is_array;
	son_store_t store;
	son_phy_t phy;
	u8 type;


	memset(&phy, 0, sizeof(phy));
	son_local_phy_lseek_set(h, sizeof(son_hdr_t));

	if( son_local_store_read(h, &store) <= 0 ){
		return -1;
	}

	type = son_local_store_type(&store);

	if( type == SON_OBJ ){
		is_array = 0;
	} else if( type == SON_ARRAY ){
		is_array = 1;
	} else {
		h->err = SON_ERR_INVALID_ROOT;
		return -1;
	}

	//create a new file
	if( path != 0 ){
		if( son_phy_open(&phy, path, SON_O_CREAT | SON_O_RDWR | SON_O_TRUNC, 0666) < 0 ){
			h->err = SON_ERR_OPEN_IO;
			return -1;
		}

		phy_fprintf(&phy, callback, context, "{\n");
		to_json_recursive(h, son_local_store_next(&store), 1, is_array, &phy, callback, context);
		phy_fprintf(&phy, callback, context, "}\n");

		return son_phy_close(&phy);
	} else {
		phy_fprintf(0, callback, context, "{\n");
		to_json_recursive(h, son_local_store_next(&store), 1, is_array, 0, callback, context);
		phy_fprintf(0, callback, context, "}\n");
	}

	return 0;
}

void son_local_assign_checksum(son_t * h){
	cortexm_assign_zero_sum32(h, CORTEXM_ZERO_SUM32_COUNT(son_t));
}

int son_local_verify_checksum(son_t * h){
	if( cortexm_verify_zero_sum32(h, CORTEXM_ZERO_SUM32_COUNT(son_t)) == 0){
		h->err = SON_ERR_HANDLE_CHECKSUM;
		return -1;
	}
	return 0;
}

void son_local_store_insert_key(son_store_t * son, const char * key){
	memset(son->key.name, 0, SON_KEY_NAME_CAPACITY);
	strncpy((char*)son->key.name, key, SON_KEY_NAME_SIZE);
}

int son_local_store_read(son_t * h, son_store_t * store){
	int ret;

	ret = son_phy_read(&(h->phy), store, sizeof(son_store_t));

	if( ret < 0 ){
		h->err = SON_ERR_READ_IO;
		return -1;
	}

	if( son_local_store_calc_checksum(store) != 0 ){
		h->err = SON_ERR_READ_CHECKSUM;
		return -1;
	}

	return (ret == sizeof(son_store_t));
}

int son_local_store_write(son_t * h, son_store_t * store){
	store_set_checksum(store);

	if( son_phy_write(&(h->phy), store, sizeof(son_store_t)) != sizeof(son_store_t) ){
		h->err = SON_ERR_WRITE_IO;
		return -1;
	}
	return 0;
}

void store_set_checksum(son_store_t * store){
	u32 * p = (u32*)store;
	u32 other_sum = 0;
	store->checksum = 0;
	u32 i;
	for(i=0; i < (sizeof(son_store_t)/sizeof(u32)); i++){
		other_sum += p[i];
	}
	store->checksum = ~other_sum+1;
}

u32 son_local_store_calc_checksum(son_store_t * store){
	u32 * p = (u32*)store;
	u32 checksum = 0;
	u32 i;
	for(i=0; i < (sizeof(son_store_t)/sizeof(u32)); i++){
		checksum += p[i];
	}

	return checksum;
}

int open_from_phy(son_t * h){
	if( son_local_phy_lseek_set(h, sizeof(son_hdr_t)) < 0 ){
		return -1;
	}

	h->stack_loc = 0;
	h->stack = 0;
	h->stack_size = 0;

	son_local_assign_checksum(h);
	return 0;
}

int edit_from_phy(son_t * h){
	if( son_local_phy_lseek_set(h, sizeof(son_hdr_t)) < 0 ){
		return -1;
	}

	//open for edit only -- stack is not used
	h->stack_loc = 0;
	h->stack = 0;
	h->stack_size = 0;

	son_local_assign_checksum(h);
	return 0;
}

int create_from_phy(son_t * h, son_stack_t * stack, size_t stack_size){
	son_hdr_t hdr;
	hdr.version = SON_VERSION;

	if( son_phy_write(&(h->phy), &hdr, sizeof(hdr)) != sizeof(hdr) ){
		h->err = SON_ERR_WRITE_IO;
		son_phy_close(&(h->phy));
		return -1;
	}

	h->stack = stack;
	h->stack_size = stack_size;
	h->stack_loc = 0;

	son_local_assign_checksum(h);
	return 0;
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

	pos = son_local_phy_lseek_current(h, 0);

	for(i=0; i <= ind; i++){

		if( son_local_store_read(h, store) <= 0 ){
			//this is an error which is set by store_read()
			return 0;
		}

		pos = son_local_phy_lseek_current(h, 0);

		next = son_local_store_next(store);

		if( next == 0 ){
			h->err = SON_ERR_ARRAY_INDEX_NOT_FOUND;
			return 0;
		}

		if( i != ind ){
			son_local_phy_lseek_set(h, next);
		}
	}

	*size = son_local_store_next(store) - pos;
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

	while( (ret = son_local_store_read(h, &store)) > 0 ){

		next = son_local_store_next(&store);

		//check to see if son is a valid object
		if( store.key.name[0] == 0 ){
			//this is not a valid son object or is the end of the file
			h->err = SON_ERR_INVALID_KEY;
			return 0;
		}

		pos = son_local_phy_lseek_current(h, 0);

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
			son_local_phy_lseek_set(h, next);
		}

	}

	if( ret == 0 ){
		h->err = SON_ERR_KEY_NOT_FOUND;
	}
	return 0;
}

int son_local_store_seek(son_t * h, const char * access, son_store_t * son, son_size_t * data_size){
	char acc[SON_ACCESS_NAME_CAPACITY];
	char * str;
	char * tok;
	char * nested_array_tok;
	char acc_item[SON_ACCESS_NAME_CAPACITY];
	son_size_t ind;

	if( son_local_phy_lseek_set(h, sizeof(son_hdr_t)) < 0 ){
		return -1;
	}

	if( (access == 0) || (access[0] == 0) ){
		//return the root value
		if( son_local_store_read(h, son) < 0 ){
			return -1;
		}
		*data_size = son_local_store_next(son) - sizeof(son_hdr_t);
		return 0;
	}

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

void print_indent(int indent, son_phy_t * phy, son_to_json_callback_t callback, void * context){
	int i;
	for(i=0; i < indent; i++){ phy_fputs(phy, callback, context, " "); }
}

void phy_fputs(son_phy_t * phy, son_to_json_callback_t callback, void * context, const char * str){
	if( phy != 0 ){
		son_phy_write(phy, str, strlen(str));
	}

	if( callback != 0 ){
		callback(context, str);
	}
}

void phy_fprintf(son_phy_t * phy, son_to_json_callback_t callback, void * context, const char * format, ...){
	char buffer[128];
	va_list args;
	va_start (args, format);
	vsnprintf(buffer, 128, format, args);
	va_end (args);

	if( phy != 0 ){
		son_phy_write(phy, buffer, strnlen(buffer, 128));
	}

	if( callback != 0 ){
		callback(context, buffer);
	}
}

int son_local_phy_lseek_current(son_t * h, s32 offset){
	int ret;
	ret = son_phy_lseek(&(h->phy), offset, SON_SEEK_CUR);
	if( ret < 0 ){
		h->err = SON_ERR_SEEK_IO;
	}
	return ret;
}

int son_local_phy_lseek_set(son_t * h, s32 offset){
	int ret;
	ret = son_phy_lseek(&(h->phy), offset, SON_SEEK_SET);
	if( ret < 0 ){
		h->err = SON_ERR_SEEK_IO;
	}
	return ret;
}

void to_json_recursive(son_t * h,
		son_size_t last_pos,
		int indent,
		int is_array,
		son_phy_t * phy,
		son_to_json_callback_t callback,
		void * context){
	son_store_t store;
	son_size_t data_size;
	son_size_t pos;
	son_size_t next;
	u8 type;


	while( son_local_store_read(h, &store) > 0 ){

		pos = son_local_phy_lseek_current(h, 0);
		next = son_local_store_next(&store);
		data_size = next - pos;
		type = son_local_store_type(&store);

		print_indent(indent, phy, callback, context);
		if( type == SON_OBJ ){

			if( is_array ){
				phy_fputs(phy, callback, context, "{\n");
			} else {
				phy_fprintf(phy, callback, context, "\"%s\" : {\n", store.key.name);
			}
			if( data_size > 0 ){
				to_json_recursive(h, next, indent+1, 0, phy, callback, context);
			}
			print_indent(indent, phy, callback, context);
			phy_fputs(phy, callback, context, "}");

		} else if( type == SON_ARRAY ){

			if( is_array ){
				phy_fputs(phy, callback, context, "[\n");
			} else {
				phy_fprintf(phy, callback, context, "\"%s\" : [\n", store.key.name);
			}
			if( data_size > 0 ){
				to_json_recursive(h, next, indent+1, 1, phy, callback, context);
			}
			print_indent(indent, phy, callback, context);
			phy_fputs(phy, callback, context, "]");

		} else {
			char buffer[data_size+1];
			buffer[data_size] = 0;
			son_phy_read(&(h->phy), buffer, data_size);


			if( is_array == 0 ){
				phy_fprintf(phy, callback, context, "\"%s\" : ", store.key.name);
			}

			if( type == SON_STRING ){
				phy_fputs(phy, callback, context, "\"");
				phy_fputs(phy, callback, context, buffer);
				phy_fputs(phy, callback, context, "\"");
			} else if ( type == SON_FLOAT ){
				float * value = (float*)buffer;
				phy_fprintf(phy, callback, context, "%f", *value);
			} else if ( type == SON_NUMBER_U32 ){
				u32 * value = (u32*)buffer;
#if !defined __StratifyOS__
				phy_fprintf(phy, callback, context, "%d", *value);
#else
				phy_fprintf(phy, callback, context, "%ld", *value);
#endif
			} else if ( type == SON_NUMBER_S32 ){
				s32 * value = (s32*)buffer;
#if !defined __StratifyOS__
				phy_fprintf(phy, callback, context, "%d", *value);
#else
				phy_fprintf(phy, callback, context, "%ld", *value);
#endif
			} else if ( type == SON_TRUE ){
				phy_fputs(phy, callback, context, "true");
			} else if ( type == SON_FALSE ){
				phy_fputs(phy, callback, context, "false");
			} else if ( type == SON_NULL ){
				phy_fputs(phy, callback, context, "null");
			} else if ( type == SON_DATA ){
				//write base64 encoded data
				int encoded_size = base64_calc_encoded_size(data_size);
				char dest[encoded_size+1]; //add a byte for the zero terminator
				base64_encode(dest, buffer, data_size);
				phy_fputs(phy, callback, context, "\"");
				phy_fputs(phy, callback, context, dest);
				phy_fputs(phy, callback, context, "\"");
			}
		}

		//add a comma?
		if( next != last_pos ){
			phy_fprintf(phy, callback, context, ",\n");
		} else {
			phy_fprintf(phy, callback, context, "\n");
			return;
		}
	}
}

int base64_encode(char * dest, const void * src, int nbyte){
	int i;
	int j;
	int k;
	int len;
	uint8_t six_bits[4];
	len = nbyte;
	const char * data;
	data = (const char *)src;

	k = 0;
	//We need to encode three bytes at a time in to four encoded bytes
	for(i=0; i < len; i+=3){
		//First the thress bytes are broken down into six-bit sections
		six_bits[0] = (data[i] >> 2) & 0x3F;
		six_bits[1] = ((data[i] << 4) & 0x30) + ((data[i+1]>>4) & 0x0F);
		six_bits[2] = ((data[i+1] << 2) & 0x3C) + ((data[i+2]>>6) & 0x03);
		six_bits[3] = data[i+2] & 0x3F;
		//now we use the helper function to convert from six-bits to base64
		for(j=0; j < 4; j++){
			dest[k+j] = base64_encode_six(six_bits[j]);
		}
		k+=4;
	}

	//at the end, we add = if the input is not divisible by 3
	if( (len % 3) == 1 ){
		//two equals at end
		dest[k-2] = '=';
		dest[k-1] = '=';
	} else if ( (len %3 ) == 2 ){
		dest[k-1] = '=';
	}

	//finally, zero terminate the output string
	dest[k] = 0;

	return strlen(dest);
}

int base64_calc_encoded_size(int nbyte){
	return ((((nbyte*4+2)/3) + 3)/4)*4;
}

//This is a helper function to convert a six-bit value to base64
char base64_encode_six(uint8_t six_bit_value){
	uint8_t x;
	char c = -1;
	x = six_bit_value & ~0xC0; //remove top two bits (should be zero anyway)
	if( x < 26 ){
		c = 'A' + x;
	} else if ( x < 52 ){
		c = 'a' + (x - 26);
	} else if( x < 62 ){
		c = '0' + (x - 52);
	} else if( x == 62 ){
		c = '+';
	} else if (x == 63 ){
		c = '/';
	}
	return c;
}
