/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#ifndef	__picos_stdattr_h__
#define __picos_stdattr_h__

// ====================================================
// Attribute conversion for praxis programs and plugins
// ====================================================

// ============================================================================

#define	release		sleep
#define	finish		kill (0)
#define	hang		kill (-1)

#define	entropy		_dac (PicOSNode, entropy)

#define	phys_dm2200	_dac (PicOSNode, phys_dm2200)
#define	phys_cc1100	_dac (PicOSNode, phys_cc1100)
#define	phys_cc1350	_dac (PicOSNode, phys_cc1350)
#define	phys_cc2420	_dac (PicOSNode, phys_cc2420)
#define	phys_uart	_dac (PicOSNode, phys_uart)

#define	rnd()		((word)toss (65536))
#define	lrnd()		((lword)((toss(65536) << 16) | toss(65536)))
#define	diag(...)	(  ((PicOSNode*)TheStation)->_na_diag ( __VA_ARGS__)  )
#define	emul(...)	(  ((PicOSNode*)TheStation)->_na_emul ( __VA_ARGS__)  )

// ============================================================================

#define	host_id		(  ((PicOSNode*)TheStation)->__host_id ()  )
#define	reset()		(  ((PicOSNode*)TheStation)->_na_reset ()  )
#define	halt()		(  ((PicOSNode*)TheStation)->_na_halt ()  )
#define	hibernate()	halt ()
#define	actsize(a)	(  ((PicOSNode*)TheStation)->_na_actsize (a)  )
#define	memfree(a,b)	(  ((PicOSNode*)TheStation)->_na_memfree (a,b)  )
#define	maxfree(a,b)	(  ((PicOSNode*)TheStation)->_na_maxfree (a,b)  )
#define io(a,b,c,d,e)	(  ((PicOSNode*)TheStation)->_na_io (a,b,c,d,e)  )
#define	ion(a,b,c,d)	io (NONE, a, b, c, d)
#define	read_sensor(a,b,c) \
		(  ((PicOSNode*)TheStation)->_na_read_sensor (a,b,c)  )
#define	write_actuator(a,b,c) \
		(  ((PicOSNode*)TheStation)->_na_write_actuator (a,b,c)  )
#define	wait_sensor(a,b) \
		(  ((PicOSNode*)TheStation)->_na_wait_sensor (a,b)  )

// ============================================================================

#define vform(a,b,c)	(  ((PicOSNode*)TheStation)->_na_vform (a,b,c)  )
#define fsize(a, ...) \
	(  ((PicOSNode*)TheStation)->_na_fsize (a, ## __VA_ARGS__)  )
#define vfsize(a,b) 	(  ((PicOSNode*)TheStation)->_na_vfsize (a, b)  )
#define vscan(a,b,c)	(  ((PicOSNode*)TheStation)->_na_vscan (a,b,c)  )
#define form(a, ...) \
	(  ((PicOSNode*)TheStation)->_na_form (a, ## __VA_ARGS__)  )
#define scan(a, ...) \
	(  ((PicOSNode*)TheStation)->_na_scan (a, ## __VA_ARGS__)  )
#define ser_out(a,b)	(  ((PicOSNode*)TheStation)->_na_ser_out (a,b)  )
#define ser_outb(a,b)   (  ((PicOSNode*)TheStation)->_na_ser_outb (a,b)  )
#define ser_in(a,b,c)	(  ((PicOSNode*)TheStation)->_na_ser_in (a,b,c)  )
#define ser_outf(a, ...) \
	(  ((PicOSNode*)TheStation)->_na_ser_outf (a, ## __VA_ARGS__)  )
#define ser_inf(a, ...) \
	(  ((PicOSNode*)TheStation)->_na_ser_inf (a, ## __VA_ARGS__)  )

// ============================================================================

#define buttons_action(a) \
			(  ((PicOSNode*)TheStation)->_na_buttons_action (a)  )
#define button_down(a)  (  ((PicOSNode*)TheStation)->_na_button_down (a)  )
#define pin_read(a)	(  ((PicOSNode*)TheStation)->_na_pin_read (a)  )
#define pin_write(a,b)	(  ((PicOSNode*)TheStation)->_na_pin_write (a,b)  )
#define pin_read_adc(a,b,c,d) \
	(  ((PicOSNode*)TheStation)->_na_pin_read_adc (a,b,c,d)  )
#define pin_write_dac(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_pin_write_dac (a,b,c)  )
#define pmon_start_cnt(a,b) \
	(  ((PicOSNode*)TheStation)->_na_pmon_start_cnt (a,b)  )
#define pmon_stop_cnt() (  ((PicOSNode*)TheStation)->_na_pmon_stop_cnt ()  )
#define pmon_set_cmp(a) (  ((PicOSNode*)TheStation)->_na_pmon_set_cmp (a)  )
#define pmon_get_cnt() 	(  ((PicOSNode*)TheStation)->_na_pmon_get_cnt ()  )
#define pmon_get_cmp() 	(  ((PicOSNode*)TheStation)->_na_pmon_get_cmp ()  )
#define pmon_start_not(a) \
	(  ((PicOSNode*)TheStation)->_na_pmon_start_not (a)  )
#define pmon_stop_not() (  ((PicOSNode*)TheStation)->_na_pmon_stop_not ()  )
#define pmon_get_state() \
	(  ((PicOSNode*)TheStation)->_na_pmon_get_state ()  )
#define pmon_pending_not() \
	(  ((PicOSNode*)TheStation)->_na_pmon_pending_not ()  )
#define pmon_pending_cmp() \
	(  ((PicOSNode*)TheStation)->_na_pmon_pending_cmp ()  )
#define pmon_dec_cnt() 	(  ((PicOSNode*)TheStation)->_na_pmon_dec_cnt ()  )
#define pmon_sub_cnt(a) (  ((PicOSNode*)TheStation)->_na_pmon_sub_cnt (a)  )
#define pmon_add_cmp(a) (  ((PicOSNode*)TheStation)->_na_pmon_add_cmp (a)  )

// ============================================================================

#define ee_open()	(  ((PicOSNode*)TheStation)->_na_ee_open ()  )
#define ee_close()	(  ((PicOSNode*)TheStation)->_na_ee_close ()  )
#define ee_size(a,b)	(  ((PicOSNode*)TheStation)->_na_ee_size (a,b)  )
#define ee_read(a,b,c)	(  ((PicOSNode*)TheStation)->_na_ee_read (a,b,c)  )
#define ee_write(a,b,c,d) \
	(  ((PicOSNode*)TheStation)->_na_ee_write (a,b,c,d)  )
#define ee_erase(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_ee_erase (a,b,c)  )
#define ee_sync(a)	(  ((PicOSNode*)TheStation)->_na_ee_sync (a)  )
#define	if_write(a,b)	(  ((PicOSNode*)TheStation)->_na_if_write (a,b)  )
#define	if_read(a)	(  ((PicOSNode*)TheStation)->_na_if_read (a)  )
#define	if_erase(a)	(  ((PicOSNode*)TheStation)->_na_if_erase (a)  )

// ============================================================================

#define	getSpdCacheSize() \
	(  ((PicOSNode*)TheStation)->_na_getSpdCacheSize ()  )
#define	getDdCacheSize() \
	(  ((PicOSNode*)TheStation)->_na_getDdCacheSize ()  )
#define	getDd(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_getDd (a,b,c)  )
#define	getSpd(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_getSpd (a,b,c)  )
#define	getDdM(a) \
	(  ((PicOSNode*)TheStation)->_na_getDdM (a)  )
#define	getSpdM(a) \
	(  ((PicOSNode*)TheStation)->_na_getSpdM (a)  )

// ============================================================================

#define	tcv_endp(a) \
	(  ((PicOSNode*)TheStation)->_na_tcv_endp (a)  )
#define	tcv_open(a,b,c, ...) \
	(  ((PicOSNode*)TheStation)->_na_tcv_open (a,b,c, ## __VA_ARGS__)  )
#define	tcv_close(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcv_close (a,b)  )
#define	tcv_plug(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcv_plug (a,b)  )
#define	tcv_rnp(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcv_rnp (a,b)  )
#define	tcv_qsize(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcv_qsize (a,b)  )
#define	tcv_erase(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcv_erase (a,b)  )
#define	tcv_wnps(a,b,c,d) \
	(  ((PicOSNode*)TheStation)->_na_tcv_wnps (a,b,c,d)  )
#define	tcv_read(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcv_read (a,b,c)  )
#define	tcv_write(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcv_write (a,b,c)  )
#define	tcv_drop(a) \
	(  ((PicOSNode*)TheStation)->_na_tcv_drop (a)  )
#define	tcv_control(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcv_control (a,b,c)  )

#define	tcvp_control(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_control (a,b,c)  )
#define	tcvp_assign(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_assign (a,b)  )
#define	tcvp_attach(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_attach (a,b)  )
#define	tcvp_clone(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_clone (a,b)  )
#define	tcvp_dispose(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_dispose (a,b)  )
#define	tcvp_new(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_new (a,b,c)  )
#define	tcvp_hook(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_hook (a,b)  )
#define	tcvp_unhook(a) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_unhook (a)  )
#define	tcvp_settimer(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_settimer (a,b)  )
#define	tcvp_cleartimer(a) \
	(  ((PicOSNode*)TheStation)->_na_tcvp_cleartimer (a)  )
#define	tcvphy_reg(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_reg (a,b,c)  )
#define	tcvphy_rcv(a,b,c) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_rcv (a,b,c)  )
#define	tcvphy_get(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_get (a,b)  )
#define	tcvphy_top(a) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_top (a)  )
#define	tcvphy_end(a) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_end (a)  )
#define	tcvphy_erase(a,b) \
	(  ((PicOSNode*)TheStation)->_na_tcvphy_erase (a,b)  )

#define	tcv_init() \
	(  ((PicOSNode*)TheStation)->_na_tcv_init ()  )
#define	tcv_dumpqueues() \
	(  ((PicOSNode*)TheStation)->_na_tcv_dumpqueues ()  )

// ============================================================================

#endif
