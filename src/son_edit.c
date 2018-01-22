//Copyright 2011-2017 Tyler Gilbert; All Rights Reserved

#include "son_local.h"

static int edit_raw_data(son_t * h, const char * key, const void * data, son_size_t size, son_value_t new_data_marker);


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
	return edit_raw_data(h, key, v, strlen(v)+1, SON_STRING);
}

int son_edit_data(son_t * h, const char * key, const void * data, son_size_t size){
	return edit_raw_data(h, key, data, size, SON_DATA);
}

int son_edit_bool(son_t * h, const char * key, int v){
	size_t pos;
	son_store_t store;
	son_value_t type;
	int read_length;
	char buffer[SON_BUFFER_SIZE];
	int ret = 0;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	read_length = son_local_read_raw_data(h, key, buffer, SON_BUFFER_SIZE, &store);

	if(read_length < 0){
		ret = -1;
	} else {

		if (v == 0){
			type = SON_FALSE;
		} else {
			type = SON_TRUE;
		}

		son_local_store_set_type(&store, type);

		pos = son_local_phy_lseek_current(h, -1*(s32)sizeof(store));
		son_local_store_set_next(&store, pos+sizeof(son_store_t));

		ret = son_local_store_write(h, &store);

	}

	son_local_assign_checksum(h);

	return ret;
}

int edit_raw_data(son_t * h, const char * key, const void * data, son_size_t size, son_value_t new_data_marker){
	son_size_t data_size;
	son_store_t son;
	int ret;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	if( son_local_store_seek(h, key, &son, &data_size) < 0 ){
		ret = -1;
	} else {
		if( son_local_store_type(&son) != new_data_marker ){
			h->err = SON_ERR_EDIT_TYPE_MISMATCH;
			ret = -1;
		} else {


			if( size > data_size ){
				size = data_size;
			}

			ret =  son_phy_write(&(h->phy), data, size);

		}
	}

	son_local_assign_checksum(h);
	return ret;

}
