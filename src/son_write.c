/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved


#include "son_local.h"

static int write_raw_data(son_t * h, const char * key, son_value_t type, const void * v, son_size_t size);
static int write_open_type(son_t * h, const char * key, u8 type);
static int write_close_type(son_t * h);

int son_close(son_t * h){
	int ret;

	if( h->stack != 0 ){
		while( h->stack_loc > 0 ){
			if( write_close_type(h) < 0 ){
				return -1;
			}
		}
	}

	ret = son_phy_close(&(h->phy));
	h->phy.fd = -1;
	if( ret < 0 ){
		h->err = SON_ERR_CLOSE_IO;
	}

	return ret;
}

int son_open_obj(son_t * h, const char * key){
	return write_open_type(h, key, SON_OBJ);
}

int son_close_obj(son_t * h){
	return write_close_type(h);
}

int son_open_data(son_t * h, const char * key){
	return write_open_type(h, key, SON_DATA);
}

int son_close_data(son_t * h){
	return write_close_type(h);
}

int son_open_array(son_t * h, const char * key){
	return write_open_type(h, key, SON_ARRAY);
}

int son_close_array(son_t * h){
	return write_close_type(h);
}

int son_write_str(son_t * h, const char * key, const char * v){
	//add one to the string length so that the zero terminator is saved
	return write_raw_data(h, key, SON_STRING, v, strlen(v)+1);
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

int write_open_type(son_t * h, const char * key, u8 type){
	son_store_t store;
	size_t pos;
	int ret = 0;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }


	if( h->stack_loc < h->stack_size ){

		if( h->stack_loc == 0 ){
			//need to write the root object first
			if( (key[0] == 0) || (key == 0) ){
				//empty key -- make root
				memset(store.key.name, 0, SON_KEY_NAME_CAPACITY);
				strncpy((char*)store.key.name, "$", SON_KEY_NAME_SIZE);
			} else {
				h->err = SON_ERR_NO_ROOT;
				ret = -1;
			}
		} else {
			son_local_store_insert_key(&store, key);
		}

		if( ret == 0 ){
			pos = son_local_phy_lseek_current(h, 0);
			h->stack[h->stack_loc].pos = pos;
			h->stack_loc++;
			son_local_store_set_type(&store, type);
			son_local_store_set_next(&store, 0);
			ret = son_local_store_write(h, &store);
		}
	} else {
		h->err = SON_ERR_STACK_OVERFLOW;
	}

	son_local_assign_checksum(h);

	return ret;
}

int write_close_type(son_t * h){
	son_size_t pos;
	son_size_t current;
	son_store_t store;
	int ret = 0;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }


	//write the size of the object
	if( h->stack_loc > 0 ){
		h->stack_loc--;
		pos = h->stack[h->stack_loc].pos;
		current = son_local_phy_lseek_current(h, 0);

		//seek to the store of the open marker
		if( son_local_phy_lseek_set(h, pos) < 0 ){
			ret = -1;
		} else {
			//read the current store
			if( son_local_store_read(h, &store) < 0 ){
				ret = -1;
			} else {

				//seek back to the store position
				if( son_local_phy_lseek_set(h, pos) < 0 ){
					ret = -1;
				} else {

					//update the store position
					son_local_store_set_next(&store, current);

					//save the store
					if( son_local_store_write(h, &store) < 0 ){
						ret = -1;
					} else {

						if( son_local_phy_lseek_set(h, current) < 0 ){
							ret = -1;
						}

					}
				}
			}
		}

	} else {
		h->err = SON_ERR_STACK_OVERFLOW;
		ret = -1;
	}

	son_local_assign_checksum(h);
	return ret;
}

int write_raw_data(son_t * h, const char * key, son_value_t type, const void * v, son_size_t size){
	size_t pos;
	son_store_t store;
	int ret;

	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	if( h->stack_size == 0 ){
		//stack size is set to zero when the file is opened for read only
		h->err = SON_ERR_CANNOT_WRITE;
		ret = -1;
	} else if ( (key == 0) || (key[0] == 0) ){
		h->err = SON_ERR_INVALID_KEY;
		ret = -1;
	} else {

		if( h->stack_loc == 0 ){
			h->err = SON_ERR_NO_ROOT;
			ret = -1;
		} else {

			son_local_store_insert_key(&store, key);
			son_local_store_set_type(&store, type);


			pos = son_local_phy_lseek_current(h, 0);
			son_local_store_set_next(&store, pos + sizeof(son_store_t) + size);

			if( son_local_store_write(h, &store) != 0 ){
				ret = -1;
			} else {

				ret = son_phy_write(&(h->phy), v, size);
				if( ret < 0 ){
					h->err = SON_ERR_WRITE_IO;
				}
			}
		}
	}

	son_local_assign_checksum(h);
	return ret;
}
