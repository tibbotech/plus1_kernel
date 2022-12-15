#ifndef CYGONCE_HAL_TYPE_H
#define CYGONCE_HAL_TYPE_H

typedef unsigned char		BYTE;
typedef unsigned short		WORD;
typedef unsigned long		DWORD;

typedef unsigned long long 	UINT64;
typedef unsigned int   		UINT32;
typedef unsigned short 		UINT16;
typedef unsigned char 		UINT8;

typedef long long 		INT64;
typedef int   			INT32;
typedef short 			INT16;
typedef signed char		INT8;

typedef unsigned char		__u8;
typedef unsigned short		__u16;
typedef unsigned int		__u32;
typedef unsigned long long	__u64;
typedef signed char		__s8;
typedef signed short		__s16;
typedef signed int		__s32;
typedef signed long long	__s64;

typedef UINT32              	FOURCC;

#ifdef BIG_ENDIAN
#undef BIG_ENDIAN
#endif
#define	BIG_ENDIAN  4321

#ifndef NULL
#define NULL    ((void *)0)
#endif

#endif // CYGONCE_HAL_TYPE_H

