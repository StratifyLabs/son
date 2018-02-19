/*! \file */ //Copyright 2011-2017 Tyler Gilbert; All Rights Reserved


#include <stdlib.h>
#include <string.h>

#include "son_phy.h"

static int calc_bytes_left(son_phy_t * phy, int nbyte);
static int phy_read_message(son_phy_t * phy, void * buffer, u32 nbyte);
static int phy_write_message(son_phy_t * phy, const void * buffer, u32 nbyte);
static int phy_lseek_message(son_phy_t * phy, int32_t offset, int whence);
static int phy_close_message(son_phy_t * phy);

int son_phy_open_message(son_phy_t * phy, void * message, u32 size){
	phy->message = 0;
	phy->message_size = 0;
	phy->message_offset = 0;
	phy->fd = -1;
	if( message ){
		phy->message = message;
	} else {
		return -1;
	}
	phy->message_size = size;
	return 0;
}

int calc_bytes_left(son_phy_t * phy, int nbyte){
	if( phy->message_offset + nbyte >= phy->message_size ){
		nbyte = phy->message_size - phy->message_offset;
	}
	return nbyte;
}

int phy_read_message(son_phy_t * phy, void * buffer, u32 nbyte){
	int bytes = calc_bytes_left(phy, nbyte);
	if( bytes ){
		memcpy(buffer, phy->message + phy->message_offset, bytes);
		phy->message_offset += bytes;
		return bytes;
	}
	return 0;
}

int phy_write_message(son_phy_t * phy, const void * buffer, u32 nbyte){
	int bytes = calc_bytes_left(phy, nbyte);
	if( bytes ){
		memcpy(phy->message + phy->message_offset, buffer, bytes);
		phy->message_offset += bytes;
		return bytes;
	}
	return 0;
}

int phy_lseek_message(son_phy_t * phy, int32_t offset, int whence){
	switch(whence){
	case SEEK_SET: phy->message_offset = offset; break;
	case SEEK_CUR: phy->message_offset += offset; break;
	case SEEK_END: phy->message_offset = phy->message_size + offset; break;
	}
	if( phy->message_offset > phy->message_size ){
		phy->message_offset = phy->message_size;
	}
	return phy->message_offset;
}

int phy_close_message(son_phy_t * phy){
	if( phy->fd == -2 ){
		free(phy->message);
	}
	phy->fd = -1;
	phy->message = 0;
	phy->message_size = 0;
	phy->message_offset = 0;
	return 0;
}



#if !defined __StratifyOS__

#include <stdio.h>
#if defined __win32 || defined __win64
#include <windows.h>
#else
#include <unistd.h>
#endif

void son_phy_msleep(int ms){
#if defined __win32 || defined __win64
    Sleep(ms);
#else
	usleep(ms*1000);
#endif
}

void son_phy_set_driver(son_phy_t * phy, void * driver){
	phy->driver = driver;
}

int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode){
	phy->message = 0;
	phy->message_offset = 0;
	phy->message_size = 0;
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
	if( phy->message ){
		return phy_read_message(phy, buffer, nbyte);
	}
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
	if( phy->message ){
		return phy_write_message(phy, buffer, nbyte);
	}
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

int son_phy_read_fileno(son_phy_t * phy, int fd, void * buffer, u32 nbyte){
#if defined __link
	if( phy->driver ){
		return link_read(phy->driver, fd, buffer, nbyte);
	}
#endif
	return -1;
}

int son_phy_write_fileno(son_phy_t * phy, int fd, const void * buffer, u32 nbyte){
#if defined __link
	if( phy->driver ){
		return link_write(phy->driver, fd, buffer, nbyte);
	}
#endif
	return -1;
}

int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence){
	if( phy->message ){
		return phy_lseek_message(phy, offset, whence);
	}
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
	if( phy->message ){
		return phy_close_message(phy);
	}
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
#include <string.h>

void son_phy_msleep(int ms){
	usleep(ms*1000);
}

int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode){
	phy->message = 0;
	phy->message_offset = 0;
	phy->message_size = 0;
	phy->fd = open(name, flags, mode);
	if( phy->fd < 0 ){
		return -1;
	}
	return 0;
}

int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte){
	if( phy->message ){
		return phy_read_message(phy, buffer, nbyte);
	}
	return read(phy->fd, buffer, nbyte);
}

int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte){
	if( phy->message ){
		return phy_write_message(phy, buffer, nbyte);
	}
	return write(phy->fd, buffer, nbyte);
}

int son_phy_read_fileno(son_phy_t * phy, int fd, void * buffer, u32 nbyte){
	return read(fd, buffer, nbyte);
}

int son_phy_write_fileno(son_phy_t * phy, int fd, const void * buffer, u32 nbyte){
	return write(fd, buffer, nbyte);
}

int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence){
	if( phy->message ){
		return phy_lseek_message(phy, offset, whence);
	}
	return lseek(phy->fd, offset, whence);
}

int son_phy_close(son_phy_t * phy){
	if( phy->message ){
		return phy_close_message(phy);
	}
	if( phy->fd >= 0 ){
		return close(phy->fd);
	}
	return 0;
}

#endif
