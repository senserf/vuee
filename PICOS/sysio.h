/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#ifndef	__sysio_h__
#define	__sysio_h__

#include "picos.h"

#ifndef	DONT_INCLUDE_OPTIONS_SYS
// This is a legacy feature to provide for the brute-force inclusion of
// options.sys where the module options can be defined for those programs
// that are not compiled with picomp.
#include "options.sys"
#endif

#include "modsyms.h"
#include "bma250.h"
#include "bmp085.h"
#include "dbgtrc.h"

// ============================================================================

#ifdef	TCV_PRESENT
#undef	TCV_PRESENT
#endif

// This must be overriden unconditionally because TCV is always compiled in
#define	TCV_PRESENT	1

// ============================================================================

#define	CRC_ISO3309	1

#define	PHYSID			0
#define	MINIMUM_PACKET_LENGTH	4
#define	RADIO_DEF_BUF_LEN	48
#define	UART_DEF_BUF_LEN	82

#if RADIO_CRC_MODE < 4
#define	CC1100_MAXPLEN		62
#else
#define	CC1100_MAXPLEN		60
#endif

#define	CC2420_MAXPLEN		126
#define	CC1350_MAXPLEN		250

// ----------------------------------------------------------------------- //
// NOTE: this will be replaced in PicOS as an isolated fragment of sysio.h //
// ----------------------------------------------------------------------- //

#define	UART_A			0
#define	UART_B			1
#define	UART			UART_A

// Note: READ/WRITE are defined in SMURPH (their values are different than in
// PicOS, but they can be used for 'io'); here's one more
#define	CONTROL			2

#define	UART_CNTRL_LCK		1	/* UART lock/unlock */
#define	UART_CNTRL_SETRATE	2
#define	UART_CNTRL_GETRATE	3
#define	UART_CNTRL_CALIBRATE	4	/* For UARTs driven by flimsy clocks */

#if	TCV_PRESENT

#include "tcv_defs.h"

typedef	int (*ctrlfun_t) (int option, address);
typedef	int (*revkfun_t) (address);

/* Disposition codes */
#define	TCV_DSP_PASS	0
#define	TCV_DSP_DROP	1
#define	TCV_DSP_RCV	2
#define	TCV_DSP_RCVU	3
#define	TCV_DSP_XMT	4
#define	TCV_DSP_XMTU	5

/* ========================= */
/* End of TCV specific stuff */
/* ========================= */
#endif

/* LEDs operations */
#define	LED_OFF		0
#define	LED_ON		1
#define	LED_BLINK	2

/* malloc shortcut */
#define	tmalloc(s)	(TheNode->memAlloc (s, (word)(s)))
#define	tfree(s)	(TheNode->memFree ((address)(s)))
#define tmwait(s)	(TheNode->waitMem((int)(s)))

#define	stackfree()	256

#define	JIFFIES		1024	/* Clock ticks in a second */

#define	umalloc(s)	tmalloc (s)
#define	ufree(s)	tfree (s)
#define	umwait(s)	tmwait (s)
#define	npwait(s)	tmwait (s)

#define	trigger(a)	((void)(TheNode->TB.signal (__cpint(a))))
#define	ptrigger(a,b)	trigger(b)

#define	hexcode(a)	(isdigit(a) ? ((a) - '0') : ( ((a)>='a'&&(a)<='f') ?\
	    ((a) - 'a' + 10) : (((a)>='A'&&(a)<='F') ? ((a) - 'A' + 10) : 0) ) )

// Byte/word swap
#define	__swabw(w)	((((w)&0xff)<<8)|(((w)>>8)&0xff))
#define __swabl(w)	((((w)&0xff)<<24)|(((w)&0xff00)<<8)|\
				(((w)>>8)&0xff00)|(((w)>>24)&0xff))
#define	__swawl(w)	((((w) & 0xffff) << 16) | (((w) >> 16) & 0xffff))

#if	BYTE_ORDER == LITTLE_ENDIAN

#define	ntowl(w)	__swawl (w)
#define	re_endian_w(w)	CNOP
#define	re_endian_lw(w)	CNOP

#else

#define ntowl(w)	(w)

#define	re_endian_w(w)	do { (w) = __swabw ((word)(w)); } while (0)
#define	re_endian_lw(w)	do { (w) = __swabl ((lword)(w)); } while (0)

#endif	/* LITTLE_ENDIAN */

#define	wtonl(w)	ntowl (w)

#define	HEX_TO_ASCII(p)		(__pi_hex_enc_table [(p) & 0xf])

/* ======================================================================== */

#define	ptrtoint(a)			__cpint (a)

#define	_da(a)				_na_ ## a
#define	_dac(a,b)			(((a *)TheStation)-> _na_ ## b)
#define	_dan(a,b)			(((a *)TheStation)-> b)
#define	_dad(t,a)			t::_na_ ## a
#define	_dap(b)				_dan (PicOSNode, b)

#define	__PRIVF(ot,tp,nam)		tp ot::nam
#define	__PUBLS(ot,tp,nam)		__PUBLF (ot, tp, nam)
#define	__PUBLF(ot,tp,nam)		tp ot::_na_ ## nam

#define	__STATIC
#define	__EXTERN
#define	__CONST
#define	__VIRTUAL	virtual
#define	__ABSTRACT	= 0
#define	__sinit(...)	

#ifndef	THREADNAME
#define	THREADNAME(a)	a
#endif

#define	PREINIT(a,b)	preinit (b)

#define	threadhdr(a,b)	process THREADNAME(a) : _PP_ (b)
#define	strandhdr(a,b)	threadhdr (a, b)

#define	thread(a)	THREADNAME(a)::perform { _pp_enter_ ();
#define	runthread(a)	(TheNode->tally_in_pcs () ? \
				(create THREADNAME(a)) -> _pp_apid_ () : 0)

#define	runstrand(a,b)	(TheNode->tally_in_pcs () ? \
				(create THREADNAME(a) (b)) -> _pp_apid_ () : 0)

#define	strand(a,b)	thread(a)
#define	endthread	}
#define	endstrand	endthread

// Generated by picomp
// #define	call(p,d,s)	do { if (join (runstrand (p, d), s)) sleep; } while (0)

#define	staticsize()	0

#define	praxis_starter(nt) \
		void nt::appStart () { runthread (root); }

//#define	__NA(a,b)		((a*)TheNode->b)

/* running + killall */

#ifdef getcpid
#undef getcpid
#endif

// Some of them are prefixed by __pi_ ... because their names are popular, so
// they may collide with some library functions

// Note: this prefix (zz_) is required by SIDE
#define	_pt_id_(pt)	(&zz_!!THREADNAME(pt)!!_prcs)

#define	running(pt)	__pi_running (_pt_id_ (pt))
#define	crunning(pt)	__pi_crunning (_pt_id_ (pt))
#define	ptleft()	(TheNode->tally_left ())
#define	killall(pt)	__pi_killall (_pt_id_ (pt))
#define	kill(p)		__pi_kill (p)
#define	joinall(pt,st)	__pi_joinall (_pt_id_ (pt), st)
#define	join(p,st)	__pi_join (p, st)
#define	getcpid()	__pi_getcpid ()

#define	seconds()	((lword)(((lword) ituToEtu (Time)) - \
							TheNode->SecondOffset))
#define	setseconds(a)	(TheNode->SecondOffset = (long)((long)(a) - \
				(lword)ituToEtu (Time)))

#define	when(e,st)	__pi_when (__cpint (e), st)

#define	ee_panic()	CNOP

#define	sd_open()	ee_open ()
#define	sd_close()	ee_close ()
#define	sd_panic()	ee_panic ()
#define	sd_read(a,b,c)	ee_read (a, b, c)
#define sd_write(a,b,c)	ee_write (WNONE, a, b, c)
#define	sd_erase(a,b)	ee_erase (WNONE, a, b)
#define	sd_sync()	ee_sync (WNONE)
#define	sd_size()	ee_size (NULL, NULL)
#define	sd_idle()	CNOP

// ============================================================================

#define	_no_m_(m,t,f)	( (TheNode->m == NULL) ? _no_module_ (t, f) : 1 )

// ============================================================================

#define	leds(le,op)	do { \
				_no_m_ (ledsm, "LEDS", "leds"); \
				TheNode->ledsm->leds_op (le, op); \
			} while (0)

#define	fastblink(a)	do { \
				_no_m_ (ledsm, "LEDS", "fastblink"); \
				TheNode->ledsm->setfast (a); \
			} while (0)

#define	is_fastblink	( \
				_no_m_ (ledsm, "LEDS", "is_fastblink"), \
				TheNode->ledsm->isfast () \
			)

// ============================================================================

#define	lcdg_on(cn)	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_on"); \
				TheNode->lcdg->m_lcdg_on (cn); \
			} while (0)

#define	lcdg_off()	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_off"); \
				TheNode->lcdg->m_lcdg_off (); \
			} while (0)

#define	lcdg_set(a,b,c,d) \
			do { \
				_no_m_ (lcdg, "LCGD", "lcdg_set"); \
				TheNode->lcdg->m_lcdg_set (a, b, c, d); \
			} while (0)

#define	lcdg_get(a,b,c,d) \
			do { \
				_no_m_ (lcdg, "LCGD", "lcdg_get"); \
				TheNode->lcdg->m_lcdg_get (a, b, c, d); \
			} while (0)

#define	lcdg_setc(a,b)  do { \
				_no_m_ (lcdg, "LCGD", "lcdg_setc"); \
				TheNode->lcdg->m_lcdg_setc (a, b); \
			} while (0)

#define	lcdg_clear() 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_clear"); \
				TheNode->lcdg->m_lcdg_clear (); \
			} while (0)

#define	lcdg_render(a,b,c,d) \
		 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_render"); \
				TheNode->lcdg->m_lcdg_render (a, b, c, d); \
			} while (0)

#define	lcdg_end() 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_end"); \
				TheNode->lcdg->m_lcdg_end (); \
			} while (0)

#define lcdg_font(a)	( \
				_no_m_ (lcdg, "LCGD", "lcdg_font"), \
				TheNode->lcdg->m_lcdg_font (a) \
			)
			
#define lcdg_cwidth()	( \
				_no_m_ (lcdg, "LCGD", "lcdg_cwidth"), \
				TheNode->lcdg->m_lcdg_cwidth () \
			)
			
#define lcdg_cheight()	( \
				_no_m_ (lcdg, "LCGD", "lcdg_cheight"), \
				TheNode->lcdg->m_lcdg_cheight () \
			)
			
#define lcdg_sett(a,b,c,d) \
			( \
				_no_m_ (lcdg, "LCGD", "lcdg_sett"), \
				TheNode->lcdg->m_lcdg_sett (a, b, c, d) \
			)
			
#define	lcdg_ec(a,b,c) 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_ec"); \
				TheNode->lcdg->m_lcdg_ec (a, b, c); \
			} while (0)

#define	lcdg_el(a,b) 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_el"); \
				TheNode->lcdg->m_lcdg_el (a, b); \
			} while (0)

#define	lcdg_wl(a,b,c,d) \
		 	do { \
				_no_m_ (lcdg, "LCGD", "lcdg_wl"); \
				TheNode->lcdg->m_lcdg_wl (a, b, c, d); \
			} while (0)

// ============================================================================

void __pi_dbg (int, word);

#ifndef	dbg_0
#define	dbg_0(a)	__pi_dbg (0, (word)(a))
#endif
#ifndef	dbg_1
#define	dbg_1(a)	__pi_dbg (1, (word)(a))
#endif
#ifndef	dbg_2
#define	dbg_2(a)	__pi_dbg (2, (word)(a))
#endif
#ifndef	dbg_3
#define	dbg_3(a)	__pi_dbg (3, (word)(a))
#endif
#ifndef	dbg_4
#define	dbg_4(a)	__pi_dbg (4, (word)(a))
#endif
#ifndef	dbg_5
#define	dbg_5(a)	__pi_dbg (5, (word)(a))
#endif
#ifndef	dbg_6
#define	dbg_6(a)	__pi_dbg (6, (word)(a))
#endif
#ifndef	dbg_7
#define	dbg_7(a)	__pi_dbg (7, (word)(a))
#endif
#ifndef	dbg_8
#define	dbg_8(a)	__pi_dbg (8, (word)(a))
#endif
#ifndef	dbg_9
#define	dbg_9(a)	__pi_dbg (9, (word)(a))
#endif
#ifndef	dbg_a
#define	dbg_a(a)	__pi_dbg (10, (word)(a))
#endif
#ifndef	dbg_b
#define	dbg_b(a)	__pi_dbg (11, (word)(a))
#endif
#ifndef	dbg_c
#define	dbg_c(a)	__pi_dbg (12, (word)(a))
#endif
#ifndef	dbg_d
#define	dbg_d(a)	__pi_dbg (13, (word)(a))
#endif
#ifndef	dbg_e
#define	dbg_e(a)	__pi_dbg (14, (word)(a))
#endif
#ifndef	dbg_f
#define	dbg_f(a)	__pi_dbg (15, (word)(a))
#endif

#define	AB_MODE_OFF	0
#define	AB_MODE_PASSIVE	1
#define	AB_MODE_ACTIVE	2

#define	_BIS(a,b)	(a) |= (b)
#define	_BIC(a,b)	(a) &= ~(b)
#define	_BIX(a,b)	(a) ^= (b)

#endif
