/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved

#ifndef SON_PHY_H_
#define SON_PHY_H_

#if defined __StratifyOS__
#include <mcu/types.h>
#else
//need to define u32, s32, u16, s16, etc
#include <stdint.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
#endif

#include <sys/types.h>
#if defined __link
#include <stdio.h>
#include <iface/link.h>

typedef struct {
	FILE * f;
	int fd;
	link_transport_mdriver_t * driver;
} son_phy_t;

#define SON_SEEK_SET LINK_SEEK_SET
#define SON_SEEK_CUR LINK_SEEK_CUR
#define SON_SEEK_END LINK_SEEK_END
#define SON_O_RDONLY LINK_O_RDONLY
#define SON_O_WRONLY LINK_O_WRONLY
#define SON_O_RDWR LINK_O_RDWR
#define SON_O_CREAT LINK_O_CREAT
#define SON_O_TRUNC LINK_O_TRUNC
#define SON_O_ACCESS (SON_O_RDWR|SON_O_WRONLY)


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

typedef struct {
	int fd;
} son_phy_t;

#endif


#if defined __cplusplus
extern "C" {
#endif

void son_phy_set_handle(son_phy_t * phy, void * handle);
int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode);
int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte);
int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte);
int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence);
int son_phy_close(son_phy_t * phy);

#if defined __cplusplus
}
#endif

#endif /* SON_PHY_H_ */
