/*! \file */ //Copyright 2011-2017 Tyler Gilbert; All Rights Reserved



#include <errno.h>

#include <sos/dev/cfifo.h>

#include "son_local.h"

#if defined __StratifyOS__
#include <cortexm/cortexm.h>
#define error_number errno
#define ERROR_AGAIN EAGAIN
#define COUNT_MULT 1
#else
#define CORTEXM_ZERO_SUM32_COUNT(x) (sizeof(x)/sizeof(u32))

#if defined __link
#define error_number link_errno
#else
#define error_number errno
#endif

#define ERROR_AGAIN 11
#define COUNT_MULT 50

void cortexm_assign_zero_sum32(void * data, int count){
	u32 sum = 0;
	u32 * ptr = data;
	int i;
	for(i=0; i < count-1; i++){
		sum += ptr[i];
	}
	ptr[i] = (u32)(0 - sum);
}

int cortexm_verify_zero_sum32(void * data, int count){
	u32 sum = 0;
	u32 * ptr = data;
	int i;
	for(i=0; i < count; i++){
		sum += ptr[i];
	}
	return sum == 0;
}
#endif

enum {
	SON_MESSAGE_START = 0x01234567
};

typedef struct MCU_PACK {
	u32 start;
	u32 size;
	u32 checksum;
} son_message_t;

typedef int (*son_transfer_t)(son_phy_t * phy, int, void*, size_t);

static int son_message_transfer_data(son_t * h, int fd, void *  data, int nbytes, int timeout, son_transfer_t transfer);
static int son_message_recv_start(son_t * h, int fd, int timeout);
static int son_is_message(son_t * h);

int son_get_message_size(son_t * h){
	son_store_t * root;
	u32 next;
	int ret;
	if( son_is_message(h) < 0 ){ return -1; }
	root = (son_store_t *)(h->phy.message + sizeof(son_hdr_t));
	next = son_local_store_next(root);
	if( next ){
		ret = next + sizeof(son_hdr_t);
	} else {
		h->err = SON_ERR_INCOMPLETE_MESSAGE;
		son_local_assign_checksum(h);
		ret = -1;
	}
	return ret;
}

int son_send_message(son_t * h, int fd, int timeout){
	son_message_t msg;
	int nbytes;

	if( son_is_message(h) < 0 ){ return -1; }

	nbytes = son_get_message_size(h);
	if( nbytes <  0 ){ return -1; }

	if( nbytes > 0 ) {

		nbytes = nbytes < h->phy.message_size ? nbytes : h->phy.message_size;
		msg.start = SON_MESSAGE_START;
		msg.size = nbytes;
		cortexm_assign_zero_sum32(&msg, CORTEXM_ZERO_SUM32_COUNT(son_message_t));

		if( son_message_transfer_data(h, fd, &msg, sizeof(msg), timeout, (son_transfer_t)son_phy_write_fileno) < 0 ){
			//h->err is set by son_message_transfer_data()
			nbytes = -1;
		} else {
			if( son_message_transfer_data(h, fd, h->phy.message, msg.size, timeout, (son_transfer_t)son_phy_write_fileno) < 0 ){
				//h->err is set by son_message_transfer_data()
				nbytes = -1;
			}
		}
	}
	son_local_assign_checksum(h);

	return nbytes;
}

int son_recv_message(son_t * h, int fd, int timeout){
	son_message_t msg;
	int ret = -1;
	int s;

	if( son_is_message(h) < 0 ){ return -1; }

	if( son_message_recv_start(h, fd, timeout) ){
		msg.start = SON_MESSAGE_START;
		if( son_message_transfer_data(h, fd, &msg.size, sizeof(msg)-sizeof(u32), timeout, (son_transfer_t)son_phy_read_fileno) >= 0 ){
			if( cortexm_verify_zero_sum32(&msg, CORTEXM_ZERO_SUM32_COUNT(son_message_t)) ){ //see if msg.checksum is valid
				memset(h->phy.message, 0, h->phy.message_size);
				//now receive the actual data
				s = msg.size < h->phy.message_size ? msg.size : h->phy.message_size;

				if( son_message_transfer_data(h, fd, h->phy.message, s, timeout, (son_transfer_t)son_phy_read_fileno) < 0 ){
					ret = -1;
				} else {
					//successful reception of the message
					ret = s;
				}
			}
		}
	}
	son_local_assign_checksum(h);

	return ret;
}

int son_message_transfer_data(son_t * h, int fd, void *  data, int nbytes, int timeout, son_transfer_t transfer){
	int ret;
	int bytes = 0;
	int count = 0;
	do {
		error_number = 0;
		ret = transfer(&(h->phy), fd, data + bytes, nbytes - bytes);
		if( ret < 0 ){
			if( error_number == ERROR_AGAIN ){
				son_phy_msleep(COUNT_MULT);
				count++;
				if( count*COUNT_MULT >= timeout ){
					h->err = SON_ERR_MESSAGE_TIMEOUT;
					return -1;
				}
			} else {
				h->err = SON_ERR_MESSAGE_IO;
				return -1;
			}
		} else {
			count = 0;
			bytes += ret;
		}
	} while( bytes < nbytes );
	return nbytes;
}

int son_is_message(son_t * h){
	if( son_local_verify_checksum(h) < 0 ){ return -1; }

	if( h->phy.message == 0 ){
		h->err = SON_ERR_NO_MESSAGE;
		return -1;
	}
	return 0;
}

int son_message_recv_start(son_t * h, int fd, int timeout){
	int i;
	u8 c;
	u8 start;
	i = 0;
	do {
		if( son_message_transfer_data(h, fd, &c, 1, 0, (son_transfer_t)son_phy_read_fileno) < 0 ){
			return 0;
		}
		start = ((SON_MESSAGE_START >> (i*8)) & 0xff);
		if( c == start ){
			i++;
		} else {
			i = 0;
		}
	} while( i < 4 );
	return 1;
}
