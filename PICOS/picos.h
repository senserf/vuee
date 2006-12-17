#ifndef	__picos_h__
#define	__picos_h__

#define	__UNIX__	1

/* ========================================================================== */

#define	MILLISECOND	0.0009765625	// 1/1024 s

typedef	unsigned short	word;
typedef	word		*address;
typedef	short int	sint;
typedef	unsigned int	lword;
typedef	unsigned char	byte;

#define	MAX_UINT	((word)0xFFFF)
#define	MAX_ULONG	((lword)0xFFFFFFFF)

#define	sysassert(a,b)	do { if (!(a)) syserror (EASSERT, b); } while (0)
#define	halt()		excptn ("HALTED!!!")
#define	CNOP		do { } while (0)

#define	byteaddr(p)	((char*)(p))
#define	entry(s)	transient s :
#define	process(a,b)	a::perform {
#define	endprocess(a)	}
#define	nodata		CNOP
#define	FORK(a,b)	create a
#define fork(a,b)	(int) (create a (b))
#define	RELEASE		sleep
#define	nodefun(t,n,s)	t Node::n

/* ========================================================================== */

#define	PHYSID			0
#define	MINIMUM_PACKET_LENGTH	8
#define	RADIO_DEF_BUF_LEN	48
#define	INFO_PHYS_DM2200	0x0700

#include "sysio.h"

#endif
