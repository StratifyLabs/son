//Copyright 2011-2016 Tyler Gilbert; All Rights Reserved

#ifndef SON_PHY_H_
#define SON_PHY_H_

#if defined __StratifyOS__
#include <mcu/types.h>
#else
//need to define u32, s32, u16, s16, etc

#if defined __link
#include <sos/link.h>
#else
#include <stdint.h>
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
#define MCU_PACK __attribute__((packed))
#endif
#endif

#include <sys/types.h>
#include <stdio.h>

#if !defined __StratifyOS__

typedef struct MCU_PACK {
	FILE * f;
	int fd;
#if defined __link
	link_transport_mdriver_t * driver;
#else
	void * driver;
#endif
	void * message;
	u16 message_size;
	u16 message_offset;
} son_phy_t;

#if defined __link
#define SON_SEEK_SET LINK_SEEK_SET
#define SON_SEEK_CUR LINK_SEEK_CUR
#define SON_SEEK_END LINK_SEEK_END
#define SON_O_RDONLY LINK_O_RDONLY
#define SON_O_WRONLY LINK_O_WRONLY
#define SON_O_RDWR LINK_O_RDWR
#define SON_O_APPEND LINK_O_APPEND
#define SON_O_CREAT LINK_O_CREAT
#define SON_O_TRUNC LINK_O_TRUNC
#else
#define SON_SEEK_SET SEEK_SET
#define SON_SEEK_CUR SEEK_CUR
#define SON_SEEK_END SEEK_END
#define SON_O_RDONLY 0
#define SON_O_WRONLY 1
#define SON_O_RDWR 2
#define SON_O_APPEND 0x0008
#define SON_O_CREAT 0x0200
#define SON_O_TRUNC 0x0400
#endif

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

typedef struct MCU_PACK {
	int fd;
	void * message;
	u16 message_size;
	u16 message_offset;
} son_phy_t;

#endif


#if defined __cplusplus
extern "C" {
#endif

void son_phy_set_driver(son_phy_t * phy, void * driver);
void son_phy_msleep(int ms);
int son_phy_open_message(son_phy_t * phy, void * message, u32 size);
int son_phy_open(son_phy_t * phy, const char * name, int32_t flags, int32_t mode);
int son_phy_read(son_phy_t * phy, void * buffer, u32 nbyte);
int son_phy_write(son_phy_t * phy, const void * buffer, u32 nbyte);
int son_phy_read_fileno(son_phy_t * phy, int fd, void * buffer, u32 nbyte);
int son_phy_write_fileno(son_phy_t * phy, int fd, const void * buffer, u32 nbyte);
int son_phy_lseek(son_phy_t * phy, int32_t offset, int whence);
int son_phy_close(son_phy_t * phy);

#if defined __cplusplus
}
#endif

#endif /* SON_PHY_H_ */
