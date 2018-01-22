/*! \file */ //Copyright 2011-2017 Tyler Gilbert; All Rights Reserved


#include "son_local.h"

int son_read_str(son_t * h, const char * access, char * str, son_size_t capacity){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_local_read_raw_data(h, access, str, capacity, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = str;
	memset(buffer, 0, SON_BUFFER_SIZE);

	switch(son_local_store_type(&son)){
	case SON_FLOAT: snprintf(buffer, 32, "%f", *(ptype.f)); break;
	case SON_NUMBER_U32: snprintf(buffer, 32, SON_PRINTF_INT, *(ptype.n_u32)); break;
	case SON_NUMBER_S32: snprintf(buffer, 32, SON_PRINTF_INT, *(ptype.n_s32)); break;
	case SON_TRUE: strcpy(buffer, "true"); break;
	case SON_FALSE: strcpy(buffer, "false"); break;
	case SON_NULL: strcpy(buffer, "null"); break;
	case SON_DATA:
	case SON_STRING:
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

	data_size = son_local_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(son_local_store_type(&son)){
	case SON_FLOAT: return (int)*(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1;
	case SON_FALSE: return 0;
	case SON_NULL: return 0;
	case SON_STRING: return atoi(buffer);
	case SON_DATA:
		h->err = SON_ERR_CANNOT_CONVERT;
		return 0;
	}

	return 0;
}

u32 son_read_unum(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_local_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(son_local_store_type(&son)){
	case SON_FLOAT: return (int)*(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1;
	case SON_FALSE: return 0;
	case SON_NULL: return 0;
	case SON_STRING: return atoi(buffer);
	case SON_DATA:
		h->err = SON_ERR_CANNOT_CONVERT;
		return 0;
	}

	return 0;
}

float son_read_float(son_t * h, const char * access){
	int data_size;
	son_store_t son;
	son_type_t ptype;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_local_read_raw_data(h, access, buffer, SON_BUFFER_SIZE, &son);
	if( data_size < 0 ){
		return -1;
	}

	ptype.cdata = buffer;

	switch(son_local_store_type(&son)){
	case SON_FLOAT: return *(ptype.f);
	case SON_NUMBER_U32: return *(ptype.n_u32);
	case SON_NUMBER_S32: return *(ptype.n_s32);
	case SON_TRUE: return 1.0;
	case SON_FALSE: return 0.0;
	case SON_NULL: return 0.0;
#if defined __StratifyOS__
	case SON_STRING: return atoff(buffer);
#else
	case SON_STRING: return atof(buffer);
#endif
	case SON_DATA: return 0.0;
	}

	return 0.0;
}

int son_read_data(son_t * h, const char * access, void * data, son_size_t size){
	son_store_t son;
	return son_local_read_raw_data(h, access, data, size, &son);
}

int son_read_bool(son_t *h, const char * key){
	int data_size;
	son_store_t son;
	char buffer[SON_BUFFER_SIZE];

	data_size = son_local_read_raw_data(h,key,buffer,SON_BUFFER_SIZE,&son);

	if(data_size < 0){
		return -1;
	}

	//ptype.cdata = buffer;

	switch(son_local_store_type(&son)){
	case SON_TRUE: return TRUE;
	case SON_FALSE: return FALSE;
	}

	return 0;
}

int son_local_read_raw_data(son_t * h, const char * access, void * data, son_size_t size, son_store_t * son){
	son_size_t data_size;
	int ret = 0;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	//check to see if h is open for reading
	if( h->stack_size != 0 ){
		h->err = SON_ERR_CANNOT_READ;
		ret = -1;
	} else {
		if( son_local_store_seek(h, access, son, &data_size) < 0 ){
			ret = -1;
		} else {
			memset(data, 0, size);
			if( data_size > size ){
				data_size = size;
			}

			if( son_phy_read(&(h->phy), data, data_size) != data_size ){
				h->err = SON_ERR_READ_IO;
				ret = -1;
			} else {
				ret = data_size;
			}
		}
	}

	son_local_assign_checksum(h);

	return ret;
}
