/*
 * son_phy.c
 *
 *  Created on: Dec 17, 2015
 *      Author: tgil
 */

#include "son_phy.h"


#if !defined __StratifyOS__

#include <stdio.h>


void son_phy_set_driver(son_phy_t * phy, void * driver){
	phy->driver = driver;
}

int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode){
	if( phy->driver == 0 ){
		//create using fopen()
		char open_code[8];

		if( (flags & SON_O_ACCESS) == SON_O_RDONLY ){
			sprintf(open_code, "r");
		} else if( ((flags & SON_O_ACCESS) == SON_O_RDWR) && (flags & SON_O_CREAT) ){
			sprintf(open_code, "wb+");
		} else {
			sprintf(open_code, "r+");
		}

		phy->f = fopen(name, open_code);
		if( phy->f != 0 ){
			return 0;
		}

		return -1;
	} else {
#if defined __link
		phy->fd = link_open(phy->driver, name, flags, mode);
		if( phy->fd < 0 ){
			return -1;
		}
		return 0;
#else
		return -1;
#endif
	}
}

int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte){
	if( phy->driver == 0 ){
		//read using fread
		return fread(buffer, 1, nbyte, phy->f);
	} else {
#if defined __link
		return link_read(phy->driver, phy->fd, buffer, nbyte);
#else
		return -1;
#endif
	}
}

int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte){
	if( phy->driver == 0 ){
		//write using fwrite
		return fwrite(buffer, 1, nbyte, phy->f);
	} else {
#if defined __link
		return link_write(phy->driver, phy->fd, buffer, nbyte);
#else
		return -1;
#endif
	}
}

int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence){
	if( phy->driver == 0 ){
		if( fseek(phy->f, offset, whence) == 0 ){
			return ftell(phy->f);
		}
		return -1;
	} else {
#if defined __link
		return link_lseek(phy->driver, phy->fd, offset, whence);
#else
		return -1;
#endif
	}
}

int son_phy_close(son_phy_t * phy){
	if( phy->driver == 0 ){
		int ret;
		ret = fclose(phy->f);
		phy->f = 0;
		return ret;
	} else {
#if defined __link
		return link_close(phy->driver, phy->fd);
#else
		return -1;
#endif
	}
}


#else

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static int calc_bytes_left(son_phy_t * phy, int nbyte);
static int phy_read_image(son_phy_t * phy, void * buffer, u32 nbyte);
static int phy_write_image(son_phy_t * phy, const void * buffer, u32 nbyte);
static int phy_lseek_image(son_phy_t * phy, int32_t offset, int whence);
static int phy_close_image(son_phy_t * phy);

int son_phy_open_image(son_phy_t * phy, void * image, u32 size){
	phy->image = 0;
	phy->image_size = 0;
	phy->image_offset = 0;
	phy->fd = -1;
	if( image ){
		phy->image = image;
	} else {
		phy->fd = -2;
		phy->image = malloc(size);
		if( phy->image == 0 ){
			return -1;
		}
	}
	phy->image_size = size;
	return 0;
}

int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode){
	phy->image = 0;
	phy->image_offset = 0;
	phy->image_size = 0;
	phy->fd = open(name, flags, mode);
	if( phy->fd < 0 ){
		return -1;
	}
	return 0;
}

int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte){
	if( phy->image ){
		return phy_read_image(phy, buffer, nbyte);
	}
	return read(phy->fd, buffer, nbyte);
}

int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte){
	if( phy->image ){
		return phy_write_image(phy, buffer, nbyte);
	}
	return write(phy->fd, buffer, nbyte);
}

int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence){
	if( phy->image ){
		return phy_lseek_image(phy, offset, whence);
	}
	return lseek(phy->fd, offset, whence);
}

int son_phy_close(son_phy_t * phy){
	if( phy->image ){
		return phy_close_image(phy);
	}
	return close(phy->fd);
}

int calc_bytes_left(son_phy_t * phy, int nbyte){
	if( phy->image_offset + nbyte >= phy->image_size ){
		nbyte = phy->image_size - phy->image_offset;
	}
	return nbyte;
}

int phy_read_image(son_phy_t * phy, void * buffer, u32 nbyte){
	int bytes = calc_bytes_left(phy, nbyte);
	if( bytes ){
		memcpy(buffer, phy->image + phy->image_offset, bytes);
		phy->image_offset += bytes;
		return bytes;
	}
	return 0;
}

int phy_write_image(son_phy_t * phy, const void * buffer, u32 nbyte){
	int bytes = calc_bytes_left(phy, nbyte);
	if( bytes ){
		memcpy(phy->image + phy->image_offset, buffer, bytes);
		phy->image_offset += bytes;
		return bytes;
	}
	return 0;
}

int phy_lseek_image(son_phy_t * phy, int32_t offset, int whence){
	switch(whence){
	case SEEK_SET: phy->image_offset = offset; break;
	case SEEK_CUR: phy->image_offset += offset; break;
	case SEEK_END: phy->image_offset = phy->image_size + offset; break;
	}
	if( phy->image_offset > phy->image_size ){
		phy->image_offset = phy->image_size;
	}
	return phy->image_offset;
}

int phy_close_image(son_phy_t * phy){
	if( phy->fd == -2 ){
		free(phy->image);
	}
	phy->image = 0;
	phy->image_size = 0;
	phy->image_offset = 0;
	return 0;
}


#endif
