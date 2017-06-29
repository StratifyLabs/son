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


int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode){
	phy->fd = open(name, flags, mode);
	if( phy->fd < 0 ){
		return -1;
	}
	return 0;
}
int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte){ return read(phy->fd, buffer, nbyte); }
int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte){ return write(phy->fd, buffer, nbyte); }
int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence){ return lseek(phy->fd, offset, whence); }
int son_phy_close(son_phy_t * phy){ return close(phy->fd); }





#endif
