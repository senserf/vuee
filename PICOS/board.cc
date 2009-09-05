#ifndef __picos_board_c__
#define __picos_board_c__

#include "stdattr_undef.h"

#include "board.h"
#include "rwpmm.cc"
#include "wchansh.cc"
#include "encrypt.cc"
#include "nvram.cc"
#include "agent.h"

//
// Size of the number table for extracting table contents:
// make it divisible by 2 and 3
//
#define	NPTABLE_SIZE	66

const char	zz_hex_enc_table [] = {
				'0', '1', '2', '3', '4', '5', '6', '7',
				'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
			      };

// Legitimate UART rates ( / 100)
static const word urates [] = { 12, 24, 48, 96, 144, 192, 288, 384, 768, 1152,
                              2560 };
struct strpool_s {

	const byte *STR;
	int Len;
	struct strpool_s *Next;
};

typedef	struct strpool_s strpool_t;

typedef struct {

	const char *Tag;
	IPointer Value;

} preitem_t;

struct preinit_s {

	Long NodeId;
	preitem_t *PITS;
	int NPITS;

	struct preinit_s *Next;
};

typedef struct preinit_s preinit_t;

static	preinit_t *PREINITS = NULL;

static	strpool_t *STRPOOL = NULL;

static	const byte *find_strpool (const byte *str, int len, Boolean cp) {

	strpool_t *p;
	const byte *s0, *s1;
	int l;

	for (p = STRPOOL; p != NULL; p = p->Next) {
		if (len != p->Len)
			continue;
		s0 = str;
		s1 = p->STR;
		l = len;
		while (l) {
			if (*s0++ != *s1++)
				break;
			l--;
		}
		if (l)
			continue;
		// Found
		return p->STR;
	}

	// Not found
	p = new strpool_t;
	if (cp) {
		// Copy the string
		p->STR = new byte [len];
		memcpy ((void*)(p->STR), str, len);
	} else {
		p->STR = str;
	}
	p->Len = len;
	p->Next = STRPOOL;
	STRPOOL = p;

	return p->STR;
}

static const char *xname (int nn) {

	return (nn < 0) ? "<defaults>" : form ("node %1d", nn);
}

void _dad (PicOSNode, diag) (const char *s, ...) {

	va_list ap;

	va_start (ap, s);

	trace ("DIAG: %s", ::vform (s, ap));
}

void zz_dbg (int x, word v) {

	trace ("DBG: %1d == %04x / %1u", x, v, v);
}

void syserror (int p, const char *s) {

	excptn (::form ("SYSERROR [%1d]: %1d, %s", TheStation->getId (), p, s));
}

void PicOSNode::stopall () {
//
// Cleanup all activities at the node, as for halt
//
	MemChunk *mc;

	terminate ();

	uart_abort ();

	// Clean up memory
	while (MHead != NULL) {
		delete [] (byte*)(MHead->PTR);
		mc = MHead -> Next;
		delete MHead;
		MHead = mc;
	}

	if (RFInt != NULL)
		RFInt->abort ();

	if (lcdg != NULL)
		lcdg->m_lcdg_off ();

	Halted = YES;
}

void _dad (PicOSNode, reset) () {
//
// This is the actual reset method callable by the praxis
//
	stopall ();
	reset ();
	Halted = NO;
	init ();
	sleep;
}

void _dad (PicOSNode, halt) () {
//
// This halts the node
//
	stopall ();
	// Signal panel status change; note: no need to do that for reset
	// because the status change is momentary and not perceptible by
	// agents
	zz_panel_signal (getId ());
	sleep;
}

void _dad (PicOSNode, powerdown) () {

	pwrt_change (PWRT_CPU, PWRT_CPU_LP);
}

void _dad (PicOSNode, powerup) () {

	pwrt_change (PWRT_CPU, PWRT_CPU_FP);
}

void PicOSNode::uart_reset () {
//
// Reset the UART based on its interface mode
//
	uart_dir_int_t *f;

	if (uart == NULL)
		// Nothing to do
		return;

	switch (uart->IMode) {

		case UART_IMODE_D:

			UART_INTF_D (uart) -> init ();
			break;

		case UART_IMODE_P:
		case UART_IMODE_L:

			UART_INTF_P (uart) -> init ();
			break;

		default:

			excptn ("PicOSNode->uart_reset: illegal mode %1d",
				uart->IMode);
	}

	uart->U->rst ();
}

void PicOSNode::uart_abort () {
//
// Stops the UART based on its interface mode (for halt)
//
	uart_dir_int_t *f;

	if (uart == NULL)
		// Nothing to do
		return;

	switch (uart->IMode) {

		case UART_IMODE_D:

			UART_INTF_D (uart) -> abort ();
			break;

		case UART_IMODE_P:
		case UART_IMODE_L:

			UART_INTF_P (uart) -> abort ();
			break;

		default:

			excptn ("PicOSNode->uart_abort: illegal mode %1d",
				uart->IMode);
	}
	// If there is a need to abort the low-level (common) UART interface,
	// the code should go here
}

void PicOSNode::reset () {

	assert (Halted, "reset at %s: should be Halted", getSName ());
	assert (MHead == NULL, "reset at %s: MHead should be NULL",
		getSName ());

	NFree = MFree = MTotal;
	MTail = NULL;
	SecondOffset = (long) ituToEtu (Time);

	uart_reset ();

	if (pins != NULL)
		pins->rst ();

	if (snsrs != NULL)
		snsrs->rst ();

	if (ledsm != NULL)
		ledsm->rst ();

	if (pwr_tracker != NULL)
		pwr_tracker->rst ();

	initParams ();

#include "lib_attributes_init.h"

}

void PicOSNode::initParams () {

	// Process tally
	NPcss = 0;
	// Entropy source
	_da (entropy) = 0;

	// Initialize the transceiver
	if (RFInt != NULL)
		RFInt->init ();

	// This will do the dynamic initialization of the static stuff in TCV
	_da (tcv_init) ();
}

// === rfm_intd_t =============================================================

rfm_intd_t::rfm_intd_t (const data_no_t *nd) {

	const data_rf_t *rf = nd->rf;

	// These two survive reset. We assume that they are never
	// changed by the praxis.
	min_backoff = (word) (rf->BCMin);

	// This is turned into the argument for 'toss' to generate the
	// proper offset. The consistency has been verified by
	// readNodeParams.
	max_backoff = (word) (rf->BCMax) - min_backoff + 1;

	// Same about these two
	if (rf->LBTDel == 0) {
		// Disable it
		lbt_threshold = HUGE;
		lbt_delay = 0;
	} else {
		lbt_threshold = dBToLin (rf->LBTThs);
		lbt_delay = (word) (rf->LBTDel);
	}

	DefXPower   = rf->Power;		// Index
	DefRPower   = dBToLin (rf->Boost);	// Value in dB
	DefRate     = rf->Rate;			// Index
	DefChannel  = rf->Channel;

	RFInterface = create Transceiver (
				1,			// Dummy
				(Long)(rf->Pre),
				1.0,			// Dummy
				1.0,			// Dummy
				nd->X, nd->Y );
	setrfrate (DefRate);
	setrfchan (DefChannel);

	Ether->connect (RFInterface);

}

void rfm_intd_t::init () {
//
// After reset
//

	RFInterface->rcvOn ();
	setrfpowr (DefXPower);
	RFInterface->setRPower (DefRPower);

	setrfrate (DefRate);
	setrfchan (DefChannel);
	
	statid = 0;

	Receiving = Xmitting = NO;
	TXOFF = RXOFF = YES;

	OBuffer.fill (NONE, NONE, 0, 0, 0);
}

void rfm_intd_t::setrfpowr (word ix) {
//
// Set X power
//
	IVMapper *m;

	m = SEther -> PS;

	assert (m->exact (ix), "RFInt->setrfpowr: illegal power index %1d", ix);

	RFInterface -> setXPower (m->setvalue (ix));
}

void rfm_intd_t::setrfrate (word ix) {
//
// Set RF rate
//
	double rate;
	IVMapper *m;

	m = SEther -> Rates;

	assert (m->exact (ix), "RFInt->setrfrate: illegal rate index %1d", ix);

	rate = m->setvalue (ix);
	RFInterface->setTRate ((RATE) round ((double)etuToItu (1.0) / rate));
	RFInterface->setTag ((RFInterface->getTag () & 0xffff) | (ix << 16));
}

void rfm_intd_t::setrfchan (word ch) {
//
// Set RF channel
//
	MXChannels *m;

	m = SEther -> Channels;

	assert (ch <= m->max (), "RFInt->setrfchan: illegal channel %1d", ch);

	RFInterface->setTag ((RFInterface->getTag () & ~0xffff) | ch);
}

void rfm_intd_t::abort () {
//
// For reset
//
	// Abort the transceiver if transmitting
	if (RFInterface->transmitting ())
		RFInterface->abort ();

	RFInterface->rcvOff ();
}

// ============================================================================

void uart_tcv_int_t::abort () {

	// This is an indication that the driver isn't running; note that
	// memory allocated to r_buffer is returned by stopall (as it is
	// taken from the node's pool)
	r_buffer = NULL;
}
	
void PicOSNode::setup (data_no_t *nd) {

	// Turn this into a trigger mailbox
	TB.setLimit (-1);

	NFree = MFree = MTotal = (nd->Mem + 3) / 4; // This is in full words
	MHead = MTail = NULL;

	// Radio

	if (nd->rf == NULL) {
		// No radio
		RFInt = NULL;
	} else {

		// Impossible
		assert (Ether != NULL, "Root: RF interface without RF channel");

		RFInt = new rfm_intd_t (nd);
	}

	if (nd->ua == NULL) {
		// No UART
		uart = NULL;
	} else {
		uart = new uart_t;
		uart->U = new UART (nd->ua);
		uart->IMode = nd->ua->iface;
		switch (uart->IMode) {
			case UART_IMODE_P:
			case UART_IMODE_L:
				uart->Int = (void*) new uart_tcv_int_t;
				break;
			default:
				uart->Int = (void*) new uart_dir_int_t;
		}
	}

	if (nd->pn == NULL) {
		// No PINS module
		pins = NULL;
	} else {
		pins = new PINS (nd->pn);
	}

	if (nd->sa == NULL) {
		// No sensors/actuators
		snsrs = NULL;
	} else {
		snsrs = new SNSRS (nd->sa);
	}

	if (nd->le == NULL) {
		ledsm = NULL;
	} else {
		ledsm = new LEDSM (nd->le);
	}

	if (nd->pt == NULL) {
		pwr_tracker = NULL;
	} else {
		pwr_tracker = new pwr_tracker_t (nd->pt);
	}

	NPcLim = nd->PLimit;

	// EEPROM and IFLASH: note that they are not resettable
	eeprom = NULL;
	iflash = NULL;
	if (nd->ep != NULL) {
		data_ep_t *EP = nd->ep;
		if (EP->EEPRS) {
			eeprom = new NVRAM (EP->EEPRS, EP->EEPPS, EP->EFLGS,
				EP->EECL, EP->bounds, EP->EPINI, EP->EPIF);
		}
		if (EP->IFLSS) {
			iflash = new NVRAM (EP->IFLSS, EP->IFLPS, 
				NVRAM_TYPE_NOOVER | NVRAM_TYPE_ERPAGE,
					EP->EECL, NULL, EP->IFINI, EP->IFIF);
		}
	}

	if (nd->Lcdg)
		lcdg = new LCDG;
	else
		lcdg = NULL;

	initParams ();

	// This can be optional based on whether the node is supposed to be
	// initially on or off 

	if (nd->On == 0) {
		// This value means OFF (it can be WNONE - for default - or 1)
		Halted = YES;
		return;
	}

	Halted = NO;
	// This is TIME_0
	SecondOffset = (long) ituToEtu (Time);
	init ();

#include "lib_attributes_init.h"

}

lword _dad (PicOSNode, seconds) () {

	// FIXME: make those different at different stations
	return (lword)(((lword) ituToEtu (Time)) + SecondOffset);
};

void _dad (PicOSNode, setseconds) (lword nv) {

	SecondOffset = (long)((long)nv - (lword)ituToEtu (Time));
};

word _dad (PicOSNode, sectomin) () {
//
// This is a stub. It makes no sense to try to do it right as the minute
// clock will probably go
//
	return 1;
}

void _dad (PicOSNode, ldelay) (word d, int state) {
/*
 * Minute wait
 */
	if (d == 0)
		syserror (ENEVENTS, "ldelay");

	Timer->delay (64.0 * d, state);
}

void _dad (PicOSNode, lhold) (int st, lword *nsec) {
/*
 * Long second wait:
 */
	if (*nsec == 0)
		return;

	Timer->delay ((double)(*nsec), st);
	*nsec = 0;
	sleep;
};

address PicOSNode::memAlloc (int size, word lsize) {
/*
 * size  == real size 
 * lsize == simulated size
 */
	MemChunk 	*mc;
	address		*op;

	lsize = (lsize + 3) / 4;		// Convert to 4-tuples
	if (lsize > MFree) {
		//trace ("MEMALLOC OOM");
		return NULL;
	}

	mc = new MemChunk;
	mc -> Next = NULL;
	mc -> PTR = (address) new byte [size];
	mc -> Size = lsize;

	MFree -= lsize;
	if (NFree > MFree)
		NFree = MFree;

	if (MHead == NULL)
		MHead = mc;
	else
		MTail->Next = mc;
	MTail = mc;
	//trace ("MEMALLOC %x [%1d, %1d]", mc->PTR, size, lsize);

	return mc->PTR;
}

void PicOSNode::memFree (address p) {

	MemChunk *mc, *pc;

	if (p == NULL)
		return;

	for (pc = NULL, mc = MHead; mc != NULL; pc = mc, mc = mc -> Next) {

		if (p == mc->PTR) {
			// Found
			if (pc == NULL)
				MHead = mc->Next;
			else
				pc->Next = mc->Next;

			if (mc->Next == NULL)
				MTail = pc;

			delete [] (byte*) (mc->PTR);
			MFree += mc -> Size;
			assert (MFree <= MTotal,
				"PicOSNode->memFree: corrupted memory");
			//trace ("MEMFREE OK: %x %1d", p, mc->Size);
			delete mc;
			TB.signal (N_MEMEVENT);
			return;
		}

	}

	excptn ("PicOSNode->memFree: chunk %x not found", p);
}

word _dad (PicOSNode, memfree) (int pool, word *res) {
/*
 * This one is for stats
 */
	if (res != NULL)
		*res = NFree << 1;

	// This is supposed to be in words, if I remember correctly. What
	// a mess!!!
	return MFree << 1;
}

word _dad (PicOSNode, actsize) (address p) {

	MemChunk *mc;

	for (mc = MHead; mc != NULL; mc = mc->Next)
		if (p == mc->PTR)
			// Found
			return mc->Size * 4;

	excptn ("PicOSNode->actsize: incorrect chunk pointer");
	return 0;
}

Boolean PicOSNode::memBook (word lsize) {

	lsize = (lsize + 3) / 4;
	if (lsize > MFree) {
		return NO;
	}
	MFree -= lsize;
	if (NFree > MFree)
		NFree = MFree;
	return YES;
}

void PicOSNode::memUnBook (word lsize) {

	lsize = (lsize + 3) / 4;
	MFree += lsize;
	assert (MFree <= MTotal, "PicOSNode->memUnBook: corrupted memory");
}

void PicOSNode::no_pin_module (const char *fn) {

	excptn ("%s: no PINS module at %s", fn, getSName ());
}

void PicOSNode::no_sensor_module (const char *fn) {

	excptn ("%s: no SENSORS module at %s", fn, getSName ());
}

char* _dad (PicOSNode, form) (char *buf, const char *fm, ...) {

	va_list	ap;
	va_start (ap, fm);

	return _da (vform) (buf, fm, ap);
}

int _dad (PicOSNode, scan) (const char *buf, const char *fmt, ...) {

	va_list ap;
	va_start (ap, fmt);

	return _da (vscan) (buf, fmt, ap);
}

static word vfparse (char *res, word n, const char *fm, va_list ap) {

	char c;
	word d;

#define	outc(c)	do { if (res && (d < n)) res [d] = (char)(c); d++; } while (0)

#define enci(b)	i = (b); \
		while (1) { \
			c = (char) (val / i); \
			if (c || i == 1) \
				break; \
			i /= 10; \
		} \
		while (1) { \
			outc (c + '0'); \
			val = val - (c * i); \
			i /= 10; \
			if (i == 0) \
				break; \
			c = (char) (val / i); \
		}

	d = 0;

	while (1) {

		c = *fm++;

		if (c == '\\') {
			/* Escape the next character unless it is 0 */
			if ((c = *fm++) == '\0') {
				outc ('\\');
				goto Eol;
			}
			outc (c);
			continue;
		}

		if (c == '%') {
			/* Something special */
			c = *fm++;
			switch (c) {
			    case 'x' : {
				word val; int i;
				val = va_arg (ap, int);
				for (i = 12; ; i -= 4) {
					outc (zz_hex_enc_table [(val>>i)&0xf]);
					if (i == 0)
						break;
				}
				break;
			    }
			    case 'd' :
			    case 'u' : {
				word val, i;
				val = va_arg (ap, int);
				if (c == 'd' && (val & 0x8000) != 0) {
					/* Minus */
					outc ('-');
					val = (~val) + 1;
				}
				enci (10000);
				break;
			    }
#if	CODE_LONG_INTS
			    case 'l' :
				c = *fm;
				if (c == 'd' || c == 'u') {
					lword val, i;
					fm++;
					val = va_arg (ap, lword);
					if (c == 'd' &&
					    (val & 0x80000000L) != 0) {
						/* Minus */
						outc ('-');
						val = (~val) + 1;
					}
					enci (1000000000L);
				} else if (c == 'x') {
					lword val;
					int i;
					fm++;
					val = va_arg (ap, lword);
					for (i = 28; ; i -= 4) {
						outc (zz_hex_enc_table
							[(val>>i)&0xf]);
						if (i == 0)
							break;
					}
				} else {
					outc ('%');
					outc ('l');
				}
				break;
#endif
			    case 'c' : {
				word val;
				val = va_arg (ap, int);
				outc (val);
				break;
			    }

		  	    case 's' : {
				char *st;
				st = va_arg (ap, char*);
				while (*st != '\0') {
					outc (*st);
					st++;
				}
				break;
			    }
		  	    default:
				outc ('%');
				outc (c);
				if (c == '\0')
					goto Ret;
			}
		} else {
			// Regular character
Eol:
			outc (c);
			if (c == '\0')
Ret:
				return d;
		}
	}
}

char* _dad (PicOSNode, vform) (char *res, const char *fm, va_list aq) {

	word fml, d;

	if (res != NULL) {
		// We trust the caller
		vfparse (res, MAX_UINT, fm, aq);
		return res;
	}

	// Size unknown; guess a decent size
	fml = strlen (fm) + 17;
	// Sentinel included (it is counted by outc)
Again:
	if ((res = (char*) umalloc (fml)) == NULL)
		/* There is not much we can do */
		return NULL;

	if ((d = vfparse (res, fml, fm, aq)) > fml) {
		// No luck, reallocate
		ufree (res);
		fml = d;
		goto Again;
	}
	return res;
}

word _dad (PicOSNode, vfsize) (const char *fm, va_list aq) {

	return vfparse (NULL, 0, fm, aq);
}

word _dad (PicOSNode, fsize) (const char *fm, ...) {

	va_list	ap;
	va_start (ap, fm);

	return _da (vfsize) (fm, ap);
}

int _dad (PicOSNode, vscan) (const char *buf, const char *fmt, va_list ap) {

	int nc;

#define	scani(at)	{ unsigned at *vap; Boolean mf; \
			Retry_d_ ## at: \
			while (!isdigit (*buf) && *buf != '-' && *buf != '+') \
				if (*buf++ == '\0') \
					return nc; \
			mf = NO; \
			if (*buf == '-' || *buf == '+') { \
				if (*buf++ == '-') \
					mf = YES; \
				if (!isdigit (*buf)) \
					goto Retry_d_ ## at; \
			} \
			nc++; \
			vap = va_arg (ap, unsigned at *); \
			*vap = 0; \
			while (isdigit (*buf)) { \
				*vap = (*vap) * 10 - \
				     (unsigned at)(unsigned int)(*buf - '0'); \
				buf++; \
			} \
			if (!mf) \
				*vap = (unsigned at)(-((at)(*vap))); \
			}
#define scanu(at)	{ unsigned at *vap; \
			while (!isdigit (*buf)) \
				if (*buf++ == '\0') \
					return nc; \
			nc++; \
			vap = va_arg (ap, unsigned at *); \
			*vap = 0; \
			while (isdigit (*buf)) { \
				*vap = (*vap) * 10 + \
				     (unsigned at)(unsigned int)(*buf - '0'); \
				buf++; \
			} \
			}
#define	scanx(at)	{ unsigned at *vap; int dc; char c; \
			while (!isxdigit (*buf)) \
				if (*buf++ == '\0') \
					return nc; \
			nc++; \
			vap = va_arg (ap, unsigned at *); \
			*vap = 0; \
			dc = 0; \
			while (isxdigit (*buf) && dc < 2 * sizeof (at)) { \
				c = *buf++; dc++; \
				if (isdigit (c)) \
					c -= '0'; \
				else if (c <= 'f' && c >= 'a') \
					c -= (char) ('a' - 10); \
				else \
					c -= (char) ('A' - 10); \
				*vap = ((*vap) << 4) | (at) c; \
			} \
			}

	if (buf == NULL || fmt == NULL)
		return 0;

	nc = 0;
	while (*fmt != '\0') {
		if (*fmt++ != '%')
			continue;
		switch (*fmt++) {
		    case '\0': return nc;
		    case 'd': scani (short); break;
		    case 'u': scanu (short); break;
		    case 'x': scanx (short); break;
#if	CODE_LONG_INTS
		    case 'l':
			switch (*fmt++) {
			    case '\0':	return nc;
		    	    case 'd': scani (long); break;
		    	    case 'u': scanu (long); break;
		    	    case 'x': scanx (long); break;
			}
			break;
#endif
		    case 'c': {
			char c, *sap;
			/* One character exactly where we are */
			if ((c = *buf++) == '\0')
				return nc;
			nc++;
			sap = va_arg (ap, char*);
			*sap = c;
			break;
		    }
		    case 's': {
			char *sap;
			while (isspace (*buf)) buf++;
			if (*buf == '\0')
				return nc;
			nc++;
			sap = va_arg (ap, char*);

			if (*buf != ',') {
				while (!isspace (*buf) && *buf != ',' &&
					*buf != '\0')
						*sap++ = *buf++;
			}
			while (isspace (*buf)) buf++;
			if (*buf == ',') buf++;
			*sap = '\0';
			break;
		    }
		}
	}
	return nc;
}

int _dad (PicOSNode, ser_in) (word st, char *buf, int len) {

	int prcs;
	uart_dir_int_t *f;

	assert (uart->IMode == UART_IMODE_D, "PicOSNode->ser_in: not allowed "
		"in mode %1d", uart->IMode);

	if (len == 0)
		return 0;

	f = UART_INTF_D (uart);

	if (f->__inpline == NULL) {
		if (f->pcsInserial == NULL) {
			if (tally_in_pcs ()) {
				create d_uart_inp_p;
			} else {
				npwait (st);
				sleep;
			}
		}
		f->pcsInserial->wait (DEATH, st);
		sleep;
	}

	/* Input available */
	if (*(f->__inpline) == 0) // bin cmd
		prcs = f->__inpline [1] + 3; // 0x00, len, 0x04
	else
		prcs = strlen (f->__inpline);
	if (prcs >= len)
		prcs = len-1;

	memcpy (buf, f->__inpline, prcs);

	ufree (f->__inpline);
	f->__inpline = NULL;

	if (*buf) // if it's NULL, it's a bin cmd
		buf [prcs] = '\0';

	return 0;
}

int _dad (PicOSNode, ser_out) (word st, const char *m) {

	int prcs;
	char *buf;
	uart_dir_int_t *f;

	assert (uart->IMode == UART_IMODE_D, "PicOSNode->ser_out: not allowed "
		"in mode %1d", uart->IMode);

	f = UART_INTF_D (uart);

	if (f->pcsOutserial != NULL) {
		f->pcsOutserial->wait (DEATH, st);
		sleep;
	}
	
	if (*m)
		prcs = strlen (m) + 1;
	else
		prcs =  m [1] + 3;

	if ((buf = (char*) umalloc (prcs)) == NULL) {
		/*
		 * We have to wait for memory
		 */
		umwait (st);
		sleep;
	}

	if (*m)
		strcpy (buf, m);
	else
		memcpy (buf, m, prcs);

	if (tally_in_pcs ()) {
		create d_uart_out_p (buf);
	} else {
		ufree (buf);
		npwait (st);
		sleep;
	}

	return 0;
}

int _dad (PicOSNode, ser_outb) (word st, const char *m) {

	int prcs;
	char *buf;
	uart_dir_int_t *f;

	assert (uart->IMode == UART_IMODE_D, "PicOSNode->ser_outb: not allowed "
		"in mode %1d", uart->IMode);

	assert (st != WNONE, "PicOSNode->ser_outb: NONE state unimplemented");

	f = UART_INTF_D (uart);

	if (m == NULL)
		return 0;

	if (f->pcsOutserial != NULL) {
		f->pcsOutserial->wait (DEATH, st);
		sleep;
	}
	if (tally_in_pcs ()) {
		create d_uart_out_p (m);
	} else {
		ufree (buf);
		npwait (st);
		sleep;
	}

	return 0;
}

int _dad (PicOSNode, ser_inf) (word st, const char *fmt, ...) {
/* ========= */
/* Formatted */
/* ========= */

	int prcs;
	va_list	ap;
	uart_dir_int_t *f;

	assert (uart->IMode == UART_IMODE_D, "PicOSNode->ser_inf: not allowed "
		"in mode %1d", uart->IMode);

	if (fmt == NULL)
		return 0;

	f = UART_INTF_D (uart);

	if (f->__inpline == NULL) {
		if (f->pcsInserial == NULL) {
			if (tally_in_pcs ()) {
				create d_uart_inp_p;
			} else {
				npwait (st);
				sleep;
			}
		}
		f->pcsInserial->wait (DEATH, st);
		sleep;
	}

	/* Input available */
	va_start (ap, fmt);

	prcs = _da (vscan) (f->__inpline, fmt, ap);

	ufree (f->__inpline);
	f->__inpline = NULL;

	return 0;
}

int _dad (PicOSNode, ser_outf) (word st, const char *m, ...) {

	int prcs;
	char *buf;
	va_list ap;
	uart_dir_int_t *f;

	assert (uart->IMode == UART_IMODE_D, "PicOSNode->ser_outf: not allowed "
		"in mode %1d", uart->IMode);

	assert (st != WNONE, "PicOSNode->ser_outf: NONE state unimplemented");

	if (m == NULL)
		return 0;

	f = UART_INTF_D (uart);

	if (f->pcsOutserial != NULL) {
		f->pcsOutserial->wait (DEATH, st);
		sleep;
	}
	
	va_start (ap, m);

	if ((buf = _da (vform) (NULL, m, ap)) == NULL) {
		/*
		 * This means we are out of memory
		 */
		umwait (st);
		sleep;
	}

	if (tally_in_pcs ()) {
		create d_uart_out_p (buf);
	} else {
		ufree (buf);
		npwait (st);
		sleep;
	}

	return 0;
}

word _dad (PicOSNode, ee_open) () {

	sysassert (eeprom != NULL, "ee_open no eeprom");
	return eeprom->nvopen ();
};

void _dad (PicOSNode, ee_close) () {

	sysassert (eeprom != NULL, "ee_close no eeprom");
	eeprom->nvclose ();
};

lword _dad (PicOSNode, ee_size) (Boolean *er, lword *rt) {

	sysassert (eeprom != NULL, "ee_size no eeprom");
	return eeprom->size (er, rt);
};

word _dad (PicOSNode, ee_read) (lword adr, byte *buf, word n) {

	sysassert (eeprom != NULL, "ee_read no eeprom");
	return eeprom->get (adr, buf, (lword) n);
};

word _dad (PicOSNode, ee_write) (word st, lword adr, const byte *buf, word n) {

	sysassert (eeprom != NULL, "ee_write no eeprom");
	eeprom->put (st, adr, buf, (lword) n);
};

word _dad (PicOSNode, ee_erase) (word st, lword fr, lword up) {

	sysassert (eeprom != NULL, "ee_erase no eeprom");
	eeprom->erase (st, fr, up);
};

word _dad (PicOSNode, ee_sync) (word st) {

	sysassert (eeprom != NULL, "ee_sync no eeprom");
	eeprom->sync (st);
};

int _dad (PicOSNode, if_write) (word adr, word w) {

	sysassert (iflash != NULL, "if_write no iflash");
	iflash->put (WNONE, (lword) adr << 1, (const byte*) (&w), 2);
	return 0;
};

word _dad (PicOSNode, if_read) (word adr) {

	word w;

	sysassert (iflash != NULL, "if_read no iflash");
	iflash->get ((lword) adr << 1, (byte*) (&w), 2);
	return w;
};

void _dad (PicOSNode, if_erase) (int a) {

	sysassert (iflash != NULL, "if_erase no iflash");

	if (a < 0) {
		iflash->erase (WNONE, 0, 0);
	} else {
		a <<= 1;
		iflash->erase (WNONE, (lword)a, (lword)a);
	}
};

// =====================================
// Root stuff: input data interpretation
// =====================================

static void xenf (const char *s, const char *w) {
	excptn ("Root: %s specification not found within %s", s, w);
}

static void xemi (const char *s, const char *w) {
	excptn ("Root: %s attribute missing from %s", s, w);
}

static void xeai (const char *s, const char *w, const char *v) {
	excptn ("Root: attribute %s in %s has invalid value: %s", s, w, v);
}

static void xevi (const char *s, const char *w, const char *v) {
	excptn ("Root: illegal %s value in %s: %s", s, w, v);
}

static void xeni (const char *s) {
	excptn ("Root: %s table too large, increase NPTABLE_SIZE", s);
}

static void xesi (const char *s, const char *w) {
	excptn ("Root: a single integer number required in %s in %s", s, w);
}

static void xefi (const char *s, const char *w) {
	excptn ("Root: a single FP number required in %s in %s", s, w);
}

static void xmon (int nr, const word *wt, const double *dta, const char *s) {
//
// Validates the monotonicity of data to be put into an IVMapper
//
	int j;
	Boolean dec;

	if (nr < 2)
		return;

	for (j = 1; j < nr; j++) {
		if (wt [j] <= wt [j-1])
			excptn ("Root: representation entries in mapper %s "
				"are not strictly increasing", s);
	}

	dec = dta [1] < dta [0];
	for (j = 1; j < nr; j++) {
		if (( dec && dta [j] >= dta [j-1]) ||
	            (!dec && dta [j] <= dta [j-1])  )
			excptn ("Root: value entries in mapper %s are not "
				"strictly monotonic", s);
	}
}

static void xpos (int nr, const double *dta, const char *s, Boolean nz = NO) {

	int i;

	for (i = 1; i < nr; i++) {
		if (dta [i] <= 0.0) {
			if (nz == NO && dta [i] == 0.0)
				continue;
			excptn ("Root: illegal value in mapper %s", s);
		}
	}
}

static void oadj (const char *s, int n) {
//
// Tabulate output to the specified position
//
	n -= strlen (s);
	while (n > 0) {
		Ouf << ' ';
		n--;
	}
}

BoardRoot::perform {

	state Start:

		initAll ();
		Kernel->wait (DEATH, Stop);

	state Stop:

		terminate;
};

static int sanitize_string (char *str) {
/*
 * Strip leading and trailing spaces, process UNIX escape sequences, return
 * the actual length of the string. Note that sanitize_string is NOT
 * idempotent.
 */
	char c, *sptr, *optr;
	int len, k, n;

	sptr = str;

	// The first pass: skip leading and trailing spaces
	while (*sptr != '\0' && isspace (*sptr))
		sptr++;

	optr = str + strlen (str) - 1;
	while (optr >= sptr && isspace (*optr))
		optr--;

	len = optr - sptr + 1;

	if (len == 0)
		return 0;

	// Move the string to the front
	for (k = 0; k < len; k++)
		str [k] = *sptr++;

	// Handle escapes
	sptr = optr = str;
	while (len--) {
		if (*sptr != '\\') {
			*optr++ = *sptr++;
			continue;
		}
		// Skip the backslash
		sptr++;
		if (len == 0)
			// Backslash at the end - ignore it
			break;

		// Check for octal escape
		if (*sptr >= '0' && *sptr <= '7') {
			n = 0;
			k = 3;
			while (len && k) {
				if (*sptr < '0' || *sptr > '7')
					break;
				n = (n << 3) + (*sptr - '0');
				sptr++;
				len--;
				k--;
			}
			*optr++ = (char) n;
			continue;
		}
		
		switch (*sptr) {

		    case 't' :	*optr = '\t'; break;
		    case 'n' :	*optr = '\n'; break;
		    case 'r' :	*optr = '\r'; break;
		    default :
			// Regular character
			*optr = *sptr;
		}

		len--; sptr++; optr++;
	}

	return optr - str;
}
	
void BoardRoot::initTiming (sxml_t xml) {

	const char *att;
	double grid = 1.0;
	nparse_t np [1];
	sxml_t data;
	int qual;

	np [0].type = TYPE_double;

	if ((data = sxml_child (xml, "grid")) != NULL) {
		att = sxml_txt (data);
		if (parseNumbers (att, 1, np) != 1)
			excptn ("Root: <grid> parameter error");
		grid = np [0].DVal;
	}

	// ITU is equal to the propagation time across grid unit, assuming 1
	// ETU == 1 second
	setEtu (SOL_VACUUM / grid);

	// DU is equal to the propagation time across 1m
	setDu (1.0/grid);

	// Clock tolerance
	if ((data = sxml_child (xml, "tolerance")) != NULL) {
		att = sxml_txt (data);
		if (parseNumbers (att, 1, np) != 1)
			excptn ("Root: <tolerance> parameter error: %s",
				att);
		grid = np [0].DVal;
		if ((att = sxml_attr (data, "quality")) != NULL) {
			np [0].type = TYPE_LONG;
			if (parseNumbers (att, 1, np) != 1)
				excptn ("Root: <tolerance> 'quality' format "
					"error: %s", att);
			qual = (int) (np [0].LVal);
		} else
			qual = 2;	// This is the default

		setTolerance (grid, qual);
	}
}

static void packetCleaner (Packet *p) {

	// Assumes there are no other packet types
	delete [] ((PKT*)p)->Payload;

}

int BoardRoot::initChannel (sxml_t data, int NN, Boolean nc) {

	const char *att, *xnam;
	double bn_db, beta, dref, sigm, loss_db, psir, pber, cutoff;
	nparse_t np [NPTABLE_SIZE];
	int nb, nr, i, j, syncbits, bpb, frml;
	sxml_t cur;
	sir_to_ber_t	*STB;
	IVMapper	*ivc [4];
	MXChannels	*mxc;
	word wn, *wt;
	Boolean rmo;
	double *dta, *dtb;

	Ether = NULL;

	if (NN == 0)
		// Ignore the channel, no stations have radio
		return NN;

	if ((data = sxml_child (data, "channel")) == NULL) {
		if (nc)
			// We are allowed to force NN to zero
			return 0;
		// This is an error
		excptn ("Root: no <channel> element with %1d wireless nodes",
			NN);
		// No continuation
	}

	// At the moment, we handle shadowing models only
	if ((cur = sxml_child (data, "shadowing")) == NULL)
		xenf ("<shadowing>", "<channel>");

	if ((att = sxml_attr (cur, "bn")) == NULL)
		xemi ("bn", "<shadowing> for <channel>");

	np [0].type = TYPE_double;
	if (parseNumbers (att, 1, np) != 1)
		xeai ("bn", "<shadowing> for <channel>", att);

	bn_db = np [0].DVal;

	if ((att = sxml_attr (cur, "syncbits")) == NULL)
		xemi ("syncbits", "<shadowing> for <channel>");

	np [0].type = TYPE_LONG;
	if (parseNumbers (att, 1, np) != 1)
		xeai ("syncbits", "<shadowing> for <channel>", att);

	syncbits = np [0].LVal;
	
	// Now for the model paramaters
	att = sxml_txt (cur);
	for (i = 0; i < NPTABLE_SIZE; i++)
		np [i].type = TYPE_double;
	if ((nb = parseNumbers (att, 5, np)) != 5) {
		if (nb < 0)
			excptn ("Root: illegal number in <shadowing>");
		else
			excptn ("Root: expected 5 numbers in <shadowing>,"
				" found %1d", nb);
	}

	if (np [0].DVal != -10.0)
		excptn ("Root: the factor in <shadowing> must be -10, is %f",
			np [0].DVal);

	beta = np [1].DVal;
	dref = np [2].DVal;
	sigm = np [3].DVal;
	loss_db = np [4].DVal;

	// Decode the BER table
	if ((cur = sxml_child (data, "ber")) == NULL)
		xenf ("<ber>", "<channel>");

	att = sxml_txt (cur);
	nb = parseNumbers (att, NPTABLE_SIZE, np);
	if (nb > NPTABLE_SIZE)
		excptn ("Root: <ber> table too large, increase NPTABLE_SIZE");

	if (nb < 4 || (nb & 1) != 0) {
		if (nb < 0)
			excptn ("Root: illegal numerical value in <ber> table");
		else
			excptn ("Root: illegal size of <ber> table (%1d), "
				"must be an even number >= 4", nb);
	}

	psir = HUGE;
	pber = -1.0;
	// This is the size of BER table
	nb /= 2;
	STB = new sir_to_ber_t [nb];

	for (i = 0; i < nb; i++) {
		// The SIR is stored as a linear ratio
		STB [i].sir = dBToLin (np [2 * i] . DVal);
		STB [i].ber = np [2 * i + 1] . DVal;
		// Validate
		if (STB [i] . sir >= psir)
			excptn ("Root: SIR entries in <ber> must be "
				"monotonically decreasing, %f and %f aren't",
					psir, STB [i] . sir);
		psir = STB [i] . sir;
		if (STB [i] . ber < 0)
			excptn ("Root: BER entries in <ber> must not be "
				"negative, %f is", STB [i] . ber);
		if (STB [i] . ber <= pber)
			excptn ("Root: BER entries in <ber> must be "
				"monotonically increasing, %f and %f aren't",
					pber, STB [i] . ber);
		pber = STB [i] . ber;
	}

	// The cutoff threshold wrt to background noise: the default means no
	// cutoff
	cutoff = -HUGE;
	if ((cur = sxml_child (data, "cutoff")) != NULL) {
		att = sxml_txt (cur);
		if (parseNumbers (att, 1, np) != 1)
			xevi ("<cutoff>", "<channel>", att);
		cutoff = np [0].DVal;
	}

	// Frame parameters
	if ((cur = sxml_child (data, "frame")) == NULL)
		xenf ("<frame>", "<channel>");
	att = sxml_txt (cur);
	np [0] . type = np [1] . type = TYPE_LONG;
	if (parseNumbers (att, 2, np) != 2)
		xevi ("<frame>", "<channel>", att);

	bpb = (int) (np [0].LVal);

	frml = (int) (np [1].LVal);
	if (bpb <= 0 || frml < 0)
		xevi ("<frame>", "<channel>", att);

	// Prepare np for reading value mappers
	for (i = 0; i < NPTABLE_SIZE; i += 2) {
		np [i  ] . type = TYPE_int;
		np [i+1] . type = TYPE_double;
	}

	print ("Channel:\n");
	print (form ("  RP(d)/XP [dB] = -10 x %g x log(d/%gm) + X(%g) - %g\n",
			beta, dref, sigm, loss_db));
	print ("\n  BER table:           SIR         BER\n");
	for (i = 0; i < nb; i++) {
 		print (form ("             %11gdB %11g\n",
			linTodB (STB[i].sir), STB[i].ber));
	}

	memset (ivc, 0, sizeof (ivc));

	if ((cur = sxml_child (data, "rates")) == NULL)
		xenf ("<rates>", "<network>");

	// This tells us whether we should expect boost factors
	rmo = (att = sxml_attr (cur, "boost")) != NULL && strcmp (att, "no");
	att = sxml_txt (cur);

	if (rmo) {
		// Expect sets of triplets: int int doubls
		for (i = 0; i < NPTABLE_SIZE; i += 3) {
			np [i  ] . type = np [i+1] . type = TYPE_int;
			np [i+2] . type = TYPE_double;
		}
		nr = parseNumbers (att, NPTABLE_SIZE, np);
		if (nr > NPTABLE_SIZE)
			xeni ("<rates>");

		if ((nr < 3) || (nr % 3) != 0)
			excptn ("Root: number of items in <rates> must be a"
				" nonzero multiple of 3");
		nr /= 3;
		wt = new word [nr];
		dta = new double [nr];
		dtb = new double [nr];

		for (j = 0; j < nr; j++) {
			wt [j] = (word) (np [3*j] . IVal);
			// Actual rates go first (stored as double)
			dta [j] = (double) (np [3*j + 1] . IVal);
			// Boost
			dtb [j] = np [3*j + 2] . DVal;
		}

		xmon (nr, wt, dta, "<rates>");
		xmon (nr, wt, dtb, "<rates>");

		att = form ("\n  Rates: ", xnam);
		print (att);
		oadj (att, 24);

		print ("REP        RATE        BOOST\n");
		for (j = 0; j < nr; j++)
 			print (form ("                %10d %11g %10gdB\n",
				wt [j], dta [j], dtb [j]));

		ivc [0] = new IVMapper (nr, wt, dta);
		ivc [1] = new IVMapper (nr, wt, dtb, YES);

	} else {

		// No boost specified, the boost IVMapper is null, which
		// translates into the boost of 1.0

		for (i = 0; i < NPTABLE_SIZE; i++)
			np [i] . type = TYPE_int;

		nr = parseNumbers (att, NPTABLE_SIZE, np);
		if (nr > NPTABLE_SIZE)
			xeni ("<rates>");

		if (nr < 2) {
			if (nr < 1) {
RVErr:
				excptn ("Root: number of items in <rates> must "
					"be either 1, or a nonzero multiple of "
						"2");
			}
			// Single entry - a special case
			wt = new word [1];
			dta = new double [1];
			wt [0] = 0;
			dta [0] = (double) (np [0] . IVal);
		} else {
			if ((nr % 2) != 0)
				goto RVErr;

			nr /= 2;
			wt = new word [nr];
			dta = new double [nr];

			for (j = 0; j < nr; j++) {
				wt [j] = (word) (np [2*j] . IVal);
				dta [j] = (double) (np [2*j + 1] . IVal);
			}
			xmon (nr, wt, dta, "<rates>");
		}

		att = form ("\n  Rates: ", xnam);
		print (att);
		oadj (att, 24);

		print ("REP        RATE\n");
		for (j = 0; j < nr; j++)
 			print (form ("                %10d %11g\n",
				wt [j], dta [j]));

		ivc [0] = new IVMapper (nr, wt, dta);
	}

	// Power

	if ((cur = sxml_child (data, "power")) == NULL)
		xenf ("<power>", "<network>");

	att = sxml_txt (cur);

	// Check for a single double value first
	np [0] . type = TYPE_double;
	if (parseNumbers (att, 2, np) == 1) {
		// We have a single entry case
		wt = new word [1];
		dta = new double [1];
		wt [0] = 0;
		dta [0] = np [0] . DVal;
	} else {
		for (i = 0; i < NPTABLE_SIZE; i += 2) {
			np [i ]  . type = TYPE_int;
			np [i+1] . type = TYPE_double;
		}
		nr = parseNumbers (att, NPTABLE_SIZE, np);
		if (nr > NPTABLE_SIZE)
			xeni ("<power>");

		if (nr < 2 || (nr % 2) != 0) 
			excptn ("Root: number of items in <power> must "
					"be either 1, or a nonzero multiple of "
						"2");
		nr /= 2;
		wt = new word [nr];
		dta = new double [nr];

		for (j = 0; j < nr; j++) {
			wt [j] = (word) (np [2*j] . IVal);
			dta [j] = np [2*j + 1] . DVal;
		}
		xmon (nr, wt, dta, "<power>");
	}

	att = form ("\n  Power: ", xnam);
	print (att);
	oadj (att, 24);

	print ("REP       POWER\n");
	for (j = 0; j < nr; j++)
 		print (form ("                %10d %8gdBm\n",
			wt [j], dta [j]));

	ivc [3] = new IVMapper (nr, wt, dta, YES);

	// RSSI map (optional)

	if ((cur = sxml_child (data, "rssi")) != NULL) {

		att = sxml_txt (cur);

		for (i = 0; i < NPTABLE_SIZE; i += 2) {
			np [i ]  . type = TYPE_int;
			np [i+1] . type = TYPE_double;
		}

		nr = parseNumbers (att, NPTABLE_SIZE, np);
		if (nr > NPTABLE_SIZE)
			xeni ("<rssi>");

		if (nr < 2 || (nr % 2) != 0) 
			excptn ("Root: number of items in <rssi> must "
					"be a nonzero multiple of 2");
		nr /= 2;
		wt = new word [nr];
		dta = new double [nr];

		for (j = 0; j < nr; j++) {
			wt [j] = (word) (np [2*j] . IVal);
			dta [j] = np [2*j + 1] . DVal;
		}
		xmon (nr, wt, dta, "<rssi>");

		att = form ("\n  RSSI: ", xnam);
		print (att);
		oadj (att, 24);

		print ("REP      SIGNAL\n");
		for (j = 0; j < nr; j++)
 			print (form ("                %10d %8gdBm\n",
				wt [j], dta [j]));

		ivc [2] = new IVMapper (nr, wt, dta, YES);
	}

	// Channels

	if ((cur = sxml_child (data, "channels")) == NULL) {
		mxc = new MXChannels (1, 0, NULL);
	} else {
		if ((att = sxml_attr (cur, "number")) != NULL ||
		    (att = sxml_attr (cur, "n")) != NULL ||
		    (att = sxml_attr (cur, "count")) != NULL) {
			// Extract the number of channels
			np [0] . type = TYPE_LONG;
			if (parseNumbers (att, 1, np) != 1)
				xevi ("<channels>", "<channel>", att);
			if (np [0] . LVal < 1 || np [0] . LVal > MAX_UINT)
				xevi ("<channels>", "<channel>", att);
			wn = (word) (np [0] . LVal);
			if (wn == 0)
				xeai ("number", "<channels>", att);

			att = sxml_txt (cur);

			for (i = 0; i < NPTABLE_SIZE; i++)
				np [i] . type = TYPE_double;

			j = parseNumbers (att, NPTABLE_SIZE, np);

			if (j > NPTABLE_SIZE)
				excptn ("Root: <channels> separation table too"
					" large, increase NPTABLE_SIZE");

			if (j < 0)
				excptn ("Root: illegal numerical value in the "
					"<channels> separation table");

			if (j == 0) {
				// No separations
				dta = NULL;
			} else {
				dta = new double [j];
				for (i = 0; i < j; i++)
					dta [i] = np [i] . DVal;
			}

			print (form ("\n  %1d channels", wn));
			if (j) {
				print (", separation: ");
				for (i = 0; i < j; i++)
					print (form (" %gdB", dta [i]));
			}
			print ("\n");
			mxc = new MXChannels (wn, j, dta);
		} else
			xemi ("number", "<channels>");
	}

	print ("\n");

	// Create the channel (this sets SEther)
	create RFShadow (NN, STB, nb, dref, loss_db, beta, sigm, bn_db, bn_db,
		cutoff, syncbits, bpb, frml, ivc, mxc, NULL);

	// Packet cleaner
	SEther->setPacketCleaner (packetCleaner);

	return NN;
}

void BoardRoot::initPanels (sxml_t data) {

	sxml_t cur;
	const char *att;
	char *str, *sts;
	int CNT, len;
	Dev *d;
	Boolean lf;

	TheStation = System;

	for (lf = YES, CNT = 0, data = sxml_child (data, "panel");
					data != NULL; data = sxml_next (data)) {

		if (lf) {
			print ("\n");
			lf = NO;
		}

		print (form ("Panel %1d: source = ", CNT));

		if ((cur = sxml_child (data, "input")) == NULL)
			excptn ("Root: <input> missing for panel %1d", CNT);

		str = (char*) sxml_txt (cur);
		len = sanitize_string (str);

		if ((att = sxml_attr (cur, "source")) == NULL)
			// Shouldn't we have a default?
			excptn ("Root: <source> missing from <input> in panel "
				"%1d", CNT);

		if (strcmp (att, "device") == 0) {
			if (len == 0)
				excptn ("Root: device name missing in panel "
					"%1d", CNT);
			str [len] = '\0';

			print (form ("device '%s'\n", str));

			d = create Dev;

			if (d->connect (DEVICE+READ, str, 0, XTRN_MBX_BUFLEN) ==
			    ERROR)
				excptn ("Root: panel %1d, cannot open device "
					"%s", str);
			create PanelHandler (d, XTRN_IMODE_DEVICE);
			continue;
		}

		if (strcmp (att, "string") == 0) {
			if (len == 0)
				excptn ("Root: empty input string in panel "
					"%1d", CNT);
			sts = (char*) find_strpool ((const byte*) str, len + 1,
				YES);

			print (form ("string '%4s ...'\n", sts));

			create PanelHandler ((Dev*)sts, XTRN_IMODE_STRING|len);
			continue;
		}

		if (strcmp (att, "socket") == 0) {
			print ("socket (redundant)\n");
			continue;
		}

		excptn ("Root: illegal input type '%s' in panel %1d", att,
			CNT);
	}
}

void BoardRoot::initRoamers (sxml_t data) {

	sxml_t cur;
	const char *att;
	char *str, *sts;
	int CNT, len;
	Dev *d;
	Boolean lf;

	TheStation = System;

	for (lf = YES, CNT = 0, data = sxml_child (data, "roamer");
					data != NULL; data = sxml_next (data)) {

		if (lf) {
			print ("\n");
			lf = NO;
		}

		print (form ("Roamer %1d: source = ", CNT));

		if ((cur = sxml_child (data, "input")) == NULL)
			excptn ("Root: <input> missing for roamer %1d", CNT);

		str = (char*) sxml_txt (cur);
		len = sanitize_string (str);

		if ((att = sxml_attr (cur, "source")) == NULL)
			// Shouldn't we have a default?
			excptn ("Root: <source> missing from <input> in roamer "
				"%1d", CNT);

		if (strcmp (att, "device") == 0) {
			if (len == 0)
				excptn ("Root: device name missing in roamer "
					"%1d", CNT);
			str [len] = '\0';

			print (form ("device '%s'\n", str));

			d = create Dev;

			if (d->connect (DEVICE+READ, str, 0, XTRN_MBX_BUFLEN) ==
			    ERROR)
				excptn ("Root: roamer %1d, cannot open device "
					"%s", str);
			create MoveHandler (d, XTRN_IMODE_DEVICE);
			continue;
		}

		if (strcmp (att, "string") == 0) {
			if (len == 0)
				excptn ("Root: empty input string in roamer "
					"%1d", CNT);
			sts = (char*) find_strpool ((const byte*) str, len + 1,
				YES);

			print (form ("string '%4s ...'\n", sts));

			create MoveHandler ((Dev*)sts, XTRN_IMODE_STRING | len);
			continue;
		}

		if (strcmp (att, "socket") == 0) {
			print ("socket (redundant)\n");
			continue;
		}

		excptn ("Root: illegal input type '%s' in roamer %1d", att,
			CNT);
	}
}

void BoardRoot::readPreinits (sxml_t data, int nn) {

	const char *att;
	sxml_t chd, che;
	preinit_t *P;
	int i, j, d, tp;
	nparse_t np [1];

	chd = sxml_child (data, "preinit");

	if (chd == NULL)
		// No preinits for this node
		return;

	P = new preinit_t;

	P->NodeId = nn;

	// Calculate the number of preinits at this level
	for (P->NPITS = 0, che = chd; che != NULL; che = sxml_next (che))
		P->NPITS++;

	P->PITS = new preitem_t [P->NPITS];

	print ("  Preinits:\n");

	for (i = 0; chd != NULL; chd = sxml_next (chd), i++) {

		if ((att = sxml_attr (chd, "tag")) == NULL)
			excptn ("Root: <preinit> for %s, tag missing",
				xname (nn));

		if ((d = strlen (att)) == 0)
			excptn ("Root: <preinit> for %s, empty tag",
				xname (nn));

		// Check for uniqueness
		for (j = 0; j < i; j++)
			if (strcmp (att, P->PITS [j].Tag) == 0)
				excptn ("Root: <preinit> for %s, duplicate tag "
					"%s", xname (nn), att);

		// Allocate storage for the tag
		P->PITS [i] . Tag = (char*) find_strpool ((const byte*) att,
			d + 1, YES);

		print (form ("    Tag: %s, Value: ", P->PITS [i] . Tag));

		if ((att = sxml_attr (chd, "type")) == NULL || *att == 'w')
			// Expect a short number (decimal or hex)
			tp = 0;
		else if (*att == 'l')
			// Long size
			tp = 1;
		else if (*att == 's')
			// String
			tp = 2;
		else
			excptn ("Root: <preinit> for %s, illegal type %s",
				xname (nn), att);

		att = sxml_txt (chd);

		if (tp < 2) {
			// A single number, hex or int (signed or unsigned)
			print (att);
			while (isspace (*att))
				att++;
			if (*att == '0' && (*(att+1) == 'x' ||
							    *(att+1) == 'X')) {
				// Hex
				np [0] . type = TYPE_hex;
				if (parseNumbers (att, 1, np) != 1)
					excptn ("Root: <preinit> for %s, "
						"illegal value for tag %s",
							xname (nn),
							P->PITS [i] . Tag);
				P->PITS [i] . Value = (IPointer) np [0]. IVal;

			} else {

				np [0] . type = TYPE_LONG;
				if (parseNumbers (att, 1, np) != 1)
					excptn ("Root: <preinit> for %s, "
						"illegal value for tag %s",
							xname (nn),
							P->PITS [i] . Tag);

				P->PITS [i] . Value = (IPointer) np [0]. LVal;
			}
		} else {
			// Collect the string
			d = sanitize_string ((char*) att);
			print (att);
			if (d == 0) {
				P->PITS [i] . Value = 0;
			} else {
				P->PITS [i] . Value = (IPointer)
					find_strpool ((const byte*) att, d + 1,
						YES);
			}
		}
		print ("\n");
	}

	// Add the preinit to the list. We create them in the increasing order
	// of node numbers, with <default> going first. This is ASSUMED here.
	// The list will start with the largest numbered node and end with the
	// <default> entry.

	print ("\n");

	P->Next = PREINITS;

	PREINITS = P;
}

IPointer PicOSNode::preinit (const char *tag) {

	preinit_t *P;
	int i;

	P = PREINITS;

	while (P != NULL) {
		if (P->NodeId < 0 || P->NodeId == getId ()) {
			// Search for the tag
			for (i = 0; i < P->NPITS; i++)
				if (strcmp (P->PITS [i] . Tag, tag) == 0)
					return P->PITS [i] . Value;
		}
		P = P -> Next;
	}

	return 0;
}

static data_epini_t *get_nv_inits (sxml_t nvs, const char *w, const char *esn) {
//
// Extract the list of initializers for NVRAM
//
	char es [64];
	data_epini_t *hd, *cc, *ta;
	sxml_t cur;
	const char *att;
	nparse_t np [1];
	int len;
	const char *sp, *ep;

	strcpy (es, "<chunk> for ");
	strcat (es, w);
	strcat (es, " at ");
	strcat (es, esn);


	hd = NULL;
	for (cur = sxml_child (nvs, "chunk"); cur != NULL;
							cur = sxml_next (cur)) {
		if ((att = sxml_attr (cur, "address")) == NULL)
			att = sxml_attr (cur, "at");

		if (att == NULL)
			xemi ("address", es);

		np [0].type = TYPE_LONG;
		if (parseNumbers (att, 1, np) != 1)
			xeai ("address", es, att);

		ta = new data_epini_t;

		ta->Address = (lword) (np [0].LVal);
		ta->chunk = NULL;
		ta->Size = 0;
		ta->Next = NULL;

		if ((att = sxml_attr (cur, "file")) != NULL) {
			// The file name
			len = strlen (att);
			// No need to do this via a string pool, as the
			// name will be deallocated after initialization
			ta->chunk = (byte*) (new char [len + 1]);
			strcpy ((char*)(ta->chunk), att);
		} else {
			// Count the bytes
			att = sxml_txt (cur);
			for (len = 0, sp = att; *sp != '\0'; len++) {
				while (isspace (*sp)) sp++;
				for (ep = sp; isxdigit (*ep) || *ep == 'x' ||
					*ep == 'X'; ep++);
				if (ep == sp)
					// Something weird
					break;
				sp = ep;
				while (isspace (*sp)) sp++;
				if (*sp == ',')
					sp++;
			}

			if (*sp != '\0')
				excptn ("Root: a %s contains "
					"an illegal character '%c'",
						es, *sp);
			if (len == 0)
				excptn ("Root: a %s is empty", es);
			// Allocate that many bytes
			ta->chunk = new byte [ta->Size = len];

			// And go through the second round of actually decoding
			// the stuff
			len = 0;
			while (len < ta->Size) {
				ta->chunk [len] = (byte) strtol (att,
					(char**)&ep, 16);
				if (ep == att) {
					excptn ("Root: a %s contains garbage: "
						"%16s", es, att);
				}
				while (isspace (*ep)) ep++;
				if (*ep == ',')
					ep++;
				att = ep;
				len++;
			}
		}

		// Append at the end of current list
		if (hd == NULL)
			hd = cc = ta;
		else {
			cc->Next = ta;
			cc = ta;
		}
	}

	return hd;
}

static void print_nv_inits (const data_epini_t *in) {
//
// Print info regarding EEPROM/IFLASH initializers
//
	while (in != NULL) {
		print (in->Size ?
			form ("              Init: @%1d, chunk size %1d\n",
				in->Address, in->Size)
			:
			form ("              Init: @%1d, file %s\n",
				in->Address, (char*)(in->chunk))
			);
		in = in->Next;
	}
}

static void deallocate_ep_def (data_ep_t *ep) {
//
// Deallocate EEPROM definition
//
	data_epini_t *c, *q;

	if ((ep->EFLGS & NVRAM_NOFR_EPINIT) == 0) {
		// Deallocate arrays
		c = ep->EPINI;
		while (c != NULL) {
			c = (q = c)->Next;
			delete q->chunk;
			delete q;
		}
		if (ep->EPIF)
			delete ep->EPIF;
	}

	if ((ep->EFLGS & NVRAM_NOFR_IFINIT) == 0) {
		c = ep->IFINI;
		while (c != NULL) {
			c = (q = c)->Next;
			delete q->chunk;
			delete q;
		}
		if (ep->IFIF)
			delete ep->IFIF;
	}
	
	delete ep;
}
		 
data_no_t *BoardRoot::readNodeParams (sxml_t data, int nn, const char *ion) {

	nparse_t np [2 + EP_N_BOUNDS];
	sxml_t cur, mai;
	const char *att;
	char *str, *as;
	int i, len;
	data_rf_t *RF;
	data_ep_t *EP;
	data_no_t *ND;
	Boolean ppf;

	ND = new data_no_t;
	ND->Mem = 0;
	// These ones are not set here, placeholders only to be set by
	// the caller
	ND->X = ND->Y = 0.0;

	if (ion == NULL)
		ND->On = WNONE;
	else if (strcmp (ion, "on") == 0)
		ND->On = 1;
	else if (strcmp (ion, "off") == 0)
		ND->On = 0;
	else
		xeai ("start", "node or defaults", ion);

	ND->PLimit = WNONE;

	// This is just a flag for now
	ND->Lcdg = WNONE;

	// The optionals
	ND->rf = NULL;
	ND->ua = NULL;
	ND->ep = NULL;
	ND->pn = NULL;
	ND->sa = NULL;
	ND->le = NULL;
	ND->pt = NULL;

	if (data == NULL)
		// This is how we stand so far
		return ND;

	print ("Node configuration [");

	if (nn < 0)
		print ("default");
	else
		print (form ("    %3d", nn));
	print ("]:\n\n");

/* ======== */
/* Preinits */
/* ======== */

	readPreinits (data, nn);

	ppf = NO;

/* ====== */
/* MEMORY */
/* ====== */

	if ((cur = sxml_child (data, "memory")) != NULL) {
		np [0].type = np [1].type = TYPE_LONG;
		if (parseNumbers (sxml_txt (cur), 1, np) != 1)
			xevi ("<memory>", xname (nn), sxml_txt (cur));
		ND->Mem = (word) (np [0] . LVal);
		if (ND->Mem > 0x00008000)
			excptn ("Root: <memory> too large (%1d) in %s; the "
				"maximum is 32768", ND->Mem, xname (nn));
		print (form ("  Memory:     %1d bytes\n", ND->Mem));
		ppf = YES;
	}

	np [0] . type = np [1] . type = TYPE_LONG;

	/* PLIMIT */
	if ((cur = sxml_child (data, "processes")) != NULL) {
		if (parseNumbers (sxml_txt (cur), 1, np) != 1)
			xesi ("<power>", xname (nn));
		ND->PLimit = (word) (np [0] . LVal);
		print (form ("  Processes:  %1d\n", ND->PLimit));
		ppf = YES;
	}

	/* LCDG */
	if ((cur = sxml_child (data, "lcdg")) != NULL) {
		att = sxml_attr (cur, "type");
		if (att == NULL)
			ND->Lcdg = 0;
		else if (strcmp (att, "n6100p") == 0)
			ND->Lcdg = 1;
		else
			excptn ("Root: 'type' for <lcdg> (%s) in %s can only "
				"be 'n6100p' at present", att, xname (nn));
		if (ND->Lcdg)
			print (form ("  LCDG display: %s\n", att));
	}

/* ========= */
/* RF MODULE */
/* ========= */

	// ====================================================================

	if ((mai = sxml_child (data, "radio")) != NULL) {

		// The node has a radio interface

		assert (Ether != NULL, "Root: <radio> (in %s) illegal if there"
			" is no RF <channel>", xname (nn));

		RF = ND->rf = new data_rf_t;
		RF->LBTThs = RF->Boost = HUGE;
		RF->Power = RF->Rate = RF->Channel = RF->Pre = RF->LBTDel =
			RF->BCMin = RF->BCMax = WNONE;

		RF->absent = YES;

		/* POWER */
		if ((cur = sxml_child (mai, "power")) != NULL) {
			// This is the index
			if (parseNumbers (sxml_txt (cur), 1, np) != 1)
				xesi ("<power>", xname (nn));
			RF->Power = (word) (np [0] . LVal);
			if (!(Ether->PS->exact (RF->Power)))
				excptn ("Root: power index %1d (in %s) does not"
					" occur in <channel><power>",
						RF->Power, xname (nn));
			print (form ("  Power idx:  %1d\n", RF->Power));
			ppf = YES;
			RF->absent = NO;
		}
	
		/* RATE */
		if ((cur = sxml_child (mai, "rate")) != NULL) {
			if (parseNumbers (sxml_txt (cur), 1, np) != 1)
				xesi ("<rate>", xname (nn));
			RF->Rate = (word) (np [0].LVal);
			// Check if the rate index is legit
			if (!(Ether->Rates->exact (RF->Rate))) 
				excptn ("Root: rate index %1d (in %s) does not"
					" occur in <channel><rates>",
						RF->Rate, xname (nn));
			print (form ("  Rate idx:   %1d\n", RF->Rate));
			RF->absent = NO;
		}
	
		/* CHANNEL */
		if ((cur = sxml_child (mai, "channel")) != NULL) {
			if (parseNumbers (sxml_txt (cur), 1, np) != 1)
				xesi ("<channel>", xname (nn));
			RF->Channel = (word) (np [0].LVal);
			// Check if the channel number is legit
			if (Ether->Channels->max () < RF->Channel) 
				excptn ("Root: channel number %1d (in %s) is "
					"illegal (see <channel><channels>)",
						RF->Channel, xname (nn));
			print (form ("  Channel:    %1d\n", RF->Channel));
			RF->absent = NO;
		}
	
		/* BACKOFF */
		if ((cur = sxml_child (mai, "backoff")) != NULL) {
			// Both are int
			if (parseNumbers (sxml_txt (cur), 2, np) != 2)
				excptn ("Root: two int numbers required in "
					"<backoff> in %s", xname (nn));
			RF->BCMin = (word) (np [0].LVal);
			RF->BCMax = (word) (np [1].LVal);
	
			if (RF->BCMax < RF->BCMin)
				xevi ("<backoff>", xname (nn), sxml_txt (cur));
	
			print (form ("  Backoff:    min=%1d, max=%1d\n",
				RF->BCMin, RF->BCMax));
			ppf = YES;
			RF->absent = NO;
		}
	
		/* PREAMBLE */
		if ((cur = sxml_child (mai, "preamble")) != NULL) {
			// Both are int
			if (parseNumbers (sxml_txt (cur), 1, np) != 1)
				xevi ("<preamble>", xname (nn), sxml_txt (cur));
			RF->Pre = (word) (np [0].LVal);
			print (form ("  Preamble:   %1d bits\n", RF->Pre));
			ppf = YES;
			RF->absent = NO;
		}
	
		// ============================================================
	
		np [1] . type = TYPE_double;
	
		/* LBT */
		if ((cur = sxml_child (mai, "lbt")) != NULL) {
			if (parseNumbers (sxml_txt (cur), 2, np) != 2)
				xevi ("<lbt>", xname (nn), sxml_txt (cur));
			RF->LBTDel = (word) (np [0].LVal);
			RF->LBTThs = np [1].DVal;
	
			print (form ("  LBT:        del=%1d, ths=%g\n",
				RF->LBTDel, RF->LBTThs));
			ppf = YES;
			RF->absent = NO;
		}
	
		// ============================================================
	
		np [0] . type = TYPE_double;
	
		if ((cur = sxml_child (mai, "boost")) != NULL) {
			if (parseNumbers (sxml_txt (cur), 1, np) != 1)
				xefi ("<boost>", xname (nn));
			RF->Boost = np [0] . DVal;
			print (form ("  Boost:      %gdB\n", RF->Boost));
			ppf = YES;
			RF->absent = NO;
		}
	
		// ============================================================

	}

	if (ppf)
		print ("\n");

/* ============ */
/* EEPROM & FIM */
/* ============ */

	EP = NULL;
	/* EEPROM */
	np [0].type = np [1].type = TYPE_LONG;
	for (i = 0; i < EP_N_BOUNDS; i++)
		np [i + 2] . type = TYPE_double;

	if ((cur = sxml_child (data, "eeprom")) != NULL) {

		lword pgsz;

		EP = ND->ep = new data_ep_t;
		memset (EP, 0, sizeof (data_ep_t));

		// Flag: FIM still inheritable from defaults
		EP->IFLSS = WNONE;

		EP->EEPPS = 0;
		att = sxml_attr (cur, "size");

		if ((len = parseNumbers (att, 2, np)) < 0)
			excptn ("Root: illegal value in <eeprom> size for %s",
				xname (nn));

		// No size or size=0 means no EEPROM
		EP->EEPRS = (len == 0) ? 0 : (lword) (np [0] . LVal);

		// Number of pages
		if (EP->EEPRS) {
			// EEPRS == 0 means no EEPROM, no inheritance from
			// defaults

			if ((att = sxml_attr (cur, "erase")) != NULL) {
				if (strcmp (att, "block") == 0 ||
				    strcmp (att, "page") == 0)
					EP->EFLGS |= NVRAM_TYPE_ERPAGE;
				else if (strcmp (att, "byte") != 0)
					xeai ("erase", "eeprom", att);
			}

			if ((att = sxml_attr (cur, "overwrite")) != NULL) {
				if (strcmp (att, "no") == 0)
					EP->EFLGS |= NVRAM_TYPE_NOOVER;
				else if (strcmp (att, "yes") != 0)
					xeai ("overwrite", "eeprom", att);
			}

			if (len > 1) {
				pgsz = (lword) (np [1] . LVal);
				if (pgsz) {
					// This is the number of pages, so turn
					// it into a page size
					if (pgsz > EP->EEPRS ||
					    (EP->EEPRS % pgsz) != 0)
						excptn ("Root: number of eeprom"
 						    " pages, %1d, is illegal "
						    "in %s", pgsz, xname (nn));
					pgsz = EP->EEPRS / pgsz;
				}
				EP->EEPPS = pgsz;
			} 

			if ((att = sxml_attr (cur, "clean")) != NULL) {
				if (parseNumbers (att, 1, np) != 1)
					xeai ("clean", "eeprom", att);
				EP->EECL = (byte) (np [0]. LVal);
			} else 
				// The default
				EP->EECL = 0xff;

			// Timing bounds
			for (i = 0; i < EP_N_BOUNDS; i++)
				EP->bounds [i] = 0.0;

			if ((att = sxml_attr (cur, "timing")) != NULL) {
				if ((len = parseNumbers (att, EP_N_BOUNDS,
					np + 2)) < 0)
						excptn ("Root: illegal timing "
							"value in <eeprom> for "
							"%s", xname (nn));

				for (i = 0; i < len; i++) {
					EP->bounds [i] = np [i + 2] . DVal;
				}
			} 
		
			for (i = 0; i < EP_N_BOUNDS; i += 2) {

				if (EP->bounds [i] != 0.0 &&
				    EP->bounds [i+1] == 0.0)
					EP->bounds [i+1] = EP->bounds [i];

				if (EP->bounds [i] < 0.0 ||
				    EP->bounds [i+1] < EP->bounds [i] )
					excptn ("Root: timing distribution "
						"parameters for eeprom: %1g %1g"
						" are illegal in %s",
							EP->bounds [i],
							EP->bounds [i+1],
							xname (nn));
			}

			if ((att = sxml_attr (cur, "image")) != NULL) {
				// Image file
				EP->EPIF = new char [strlen (att) + 1];
				strcpy (EP->EPIF, att);
			}

			EP->EPINI = get_nv_inits (cur, "EEPROM", xname (nn));

		   	print (form (
		"  EEPROM:     %1d bytes, page size: %1d, clean: %02x\n",
					EP->EEPRS, EP->EEPPS, EP->EECL));
			if (EP->EPIF)
				print (form (
				"              Image file: %s\n", EP->EPIF));
			print (form (
				"              R: [%1g,%1g], W: [%1g,%1g], "
					     " E: [%1g,%1g], S: [%1g,%1g]\n",
					EP->bounds [0],
					EP->bounds [1],
					EP->bounds [2],
					EP->bounds [3],
					EP->bounds [4],
					EP->bounds [5],
					EP->bounds [6],
					EP->bounds [7]));

			print_nv_inits (EP->EPINI);

		} else
			      print ("  EEPROM:     none\n");
	}

	if ((cur = sxml_child (data, "iflash")) != NULL) {

		Long ifsz, ifps;

		if (EP == NULL) {
			EP = ND->ep = new data_ep_t;
			memset (EP, 0, sizeof (data_ep_t));
			// Flag: EEPROM still inheritable from defaults
			EP->EEPRS = LWNONE;
		}

		// Get the size (as for EEPROM)
		ifsz = 0;
		att = sxml_attr (cur, "size");
		len = parseNumbers (att, 2, np);
		if (len != 1 && len != 2)
			xevi ("<iflash>", xname (nn), sxml_txt (cur));
		ifsz = (Long) (np [0].LVal);
		if (ifsz < 0 || ifsz > 65536)
			excptn ("Root: iflash size must be >= 0 and <= 65536, "
				"is %1d, in %s", ifsz, xname (nn));
		ifps = ifsz;
		if (len == 2) {
			ifps = (Long) (np [1].LVal);
			if (ifps) {
			    if (ifps < 0 || ifps > ifsz || (ifsz % ifps) != 0)
				excptn ("Root: number of iflash pages, %1d, is "
					"illegal in %s", ifps, xname (nn));
			    ifps = ifsz / ifps;
			}
		}
		EP->IFLSS = (word) ifsz;
		EP->IFLPS = (word) ifps;

		if (ifsz) {
			// There is an IFLASH
			EP->IFINI = get_nv_inits (cur, "IFLASH", xname (nn));

			if ((att = sxml_attr (cur, "clean")) != NULL) {
				if (parseNumbers (att, 1, np) != 1)
					xeai ("clean", "iflash", att);
				EP->IFCL = (byte) (np [0] . LVal);
			} else 
				// The default
				EP->IFCL = 0xff;

			if ((att = sxml_attr (cur, "image")) != NULL) {
				// Image file
				EP->IFIF = new char [strlen (att) + 1];
				strcpy (EP->IFIF, att);
			}

		    	print (form (
		"  IFLASH:     %1d bytes, page size: %1d, clean: %02x\n",
					ifsz, ifps, EP->IFCL));
			if (EP->IFIF)
				print (form (
				"              Image file: %s\n", EP->IFIF));
			print_nv_inits (EP->IFINI);
		} else
			print ("  IFLASH:     none\n");
	}
		
	if (EP != NULL) {
		// Make this flag consistent
		EP->absent = (EP->EEPRS == 0 && EP->IFLSS == 0);
		print ("\n");
	}

/* ==== */
/* LEDS */
/* ==== */

	ND->le = readLedsParams (data, xname (nn));

/* ======== */
/* PTRACKER */
/* ======== */

	ND->pt = readPwtrParams (data, xname (nn));

/* ==== */
/* UART */
/* ==== */

	ND->ua = readUartParams (data, xname (nn));

/* ==== */
/* PINS */
/* ==== */

	ND->pn = readPinsParams (data, xname (nn));

/* ================= */
/* Sensors/Actuators */
/* ================= */

	ND->sa = readSensParams (data, xname (nn));

	return ND;
}

Boolean zz_validate_uart_rate (word rate) {

	int i;

	for (i = 0; i < sizeof (urates)/sizeof (word); i++)
		if (urates [i] == rate)
			return YES;

	return NO;
}

data_ua_t *BoardRoot::readUartParams (sxml_t data, const char *esn) {
/*
 * Decodes UART parameters
 */
	sxml_t cur;
	nparse_t np [2];
	const char *att;
	char *str, *sts;
	char es [48];
	data_ua_t *UA;
	int len;

	if ((data = sxml_child (data, "uart")) == NULL)
		return NULL;

	strcpy (es, "<uart> for ");
	strcat (es, esn);

	UA = new data_ua_t;

	np [0].type = np [1].type = TYPE_LONG;

	/* The rate */
	UA->URate = 0;
	if ((att = sxml_attr (data, "rate")) != NULL) {
		if (parseNumbers (att, 1, np) != 1 || np [0].LVal <= 0)
			xeai ("rate", es, att);
		len = (int) (np [0].LVal);
		// Make sure it is decent
		if (len < 0 || (len % 100) != 0)
			xeai ("rate", es, att);
		// We store it in hundreds
		UA->URate = (word) (len / 100);
		if (!zz_validate_uart_rate (UA->URate))
			xeai ("rate", es, att);
	}

	// The default is "direct"
	UA->iface = UART_IMODE_D;
	if ((att = sxml_attr (data, "mode")) != NULL) {
		if (*att == 'p' || *att == 'n')
			// Packet (the 'n' means non-persistent)
			UA->iface = UART_IMODE_P;
		else if (*att == 'l')
			// Line packet
			UA->iface = UART_IMODE_L;
		else if (*att != 'd')
			// For now, it can only be "direct", "packet", "line"
			xeai ("mode", es, att);
	}

	/* Buffer size */
	UA->UIBSize = UA->UOBSize = 0;

	if ((att = sxml_attr (data, "bsize")) != NULL) {
		len = parseNumbers (att, 2, np);
		if ((len != 1 && len != 2) || np [0].LVal < 0)
			xeai ("bsize", es, att);
		UA->UIBSize = (word) (np [0].LVal);
		if (len == 2) {
			if (np [1].LVal < 0)
				xeai ("bsize", es, att);
			UA->UOBSize = (word) (np [1].LVal);
		}
	}
	print (form ("  UART [rate = %1d bps, mode = %s, bsize i = %1d, "
		"o = %d bytes]:\n", (int)(UA->URate) * 100,
			UA->iface == UART_IMODE_P ? "packet" : "direct",
				UA->UIBSize, UA->UOBSize));

	UA->UMode = 0;
	UA->UIDev = UA->UODev = NULL;

	/* The INPUT spec */
	if ((cur = sxml_child (data, "input")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "source")) == NULL)
			xemi ("source", es);
		if (strcmp (att, "none") == 0) {
			// Equivalent to 'no uart input spec'
			goto NoUInput;
		}

		print ("    INPUT:  ");

		if (strcmp (att, "device") == 0) {
			// Preprocess the string (in place, as it can only
			// shrink). Unfortunately, we cannot have exotic
			// characters in it because 0 is the sentinel.
			len = sanitize_string (str);
			if (len == 0)
				xevi ("<input> device string", es, "-empty-");
			// This is a device name
			str [len] = '\0';
			UA->UMode |= XTRN_IMODE_DEVICE;
			UA->UIDev = str;
			print (form ("device '%s'", str));
		} else if (strcmp (att, "socket") == 0) {
			// No string
			UA->UMode |= XTRN_IMODE_SOCKET | XTRN_OMODE_SOCKET;
			print ("socket");
		} else if (strcmp (att, "string") == 0) {
			len = sanitize_string (str);
			// We shall copy the string, such that the UART
			// constructor won't have to. This should be more
			// economical.
			sts = (char*) find_strpool ((const byte*) str, len + 1,
				YES);
			UA->UIDev = sts;
			UA->UMode |= (XTRN_IMODE_STRING | len);
			print (form ("string '%c%c%c%c ...'",
				sts [0],
				sts [1],
				sts [2],
				sts [3] ));
		} else {
			xeai ("source", es, att);
		}

		// Now for the 'type'
		if ((att = sxml_attr (cur, "type")) != NULL) {
			if (strcmp (att, "timed") == 0) {
				UA->UMode |= XTRN_IMODE_TIMED;
				print (", TIMED");
			} else if (strcmp (att, "untimed"))
				xeai ("type", es, att);
		}
		// And the coding
		if ((att = sxml_attr (cur, "coding")) != NULL) {
			if (strcmp (att, "hex") == 0) {
				print (", HEX");
				UA->UMode |= XTRN_IMODE_HEX;
			} else if (strcmp (att, "ascii"))
				xeai ("coding", es, att);
		}
			
		print ("\n");
	}

NoUInput:

	// The OUTPUT spec
	if ((cur = sxml_child (data, "output")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "target")) == NULL)
			xemi ("target", es);
		if ((UA->UMode & XTRN_OMODE_MASK) == XTRN_OMODE_SOCKET) {
			// This must be a socket
			if (strcmp (att, "socket"))
				// but isn't
				excptn ("Root: 'target' for <uart> <output> "
					"(%s) in %s must be 'socket'",
						att, es);
			print ("    OUTPUT: ");
			print ("socket (see INPUT)");
			goto CheckOType;
		} else if (strcmp (att, "none") == 0)
			// Equivalent to 'no uart output spec'
			goto NoUOutput;

		print ("    OUTPUT: ");

		if (strcmp (att, "device") == 0) {
			len = sanitize_string (str);
			if (len == 0)
				xevi ("<output> device string", es, "-empty-");
			// This is a device name
			str [len] = '\0';
			UA->UMode |= XTRN_OMODE_DEVICE;
			UA->UODev = str;
			print (form ("device '%s'", str));
		} else if (strcmp (att, "socket") == 0) {
			if ((UA->UMode & XTRN_IMODE_MASK) != XTRN_IMODE_NONE)
				excptn ("Root: 'target' in <uart> <output> (%s)"
					" for %s conflicts with <input> source",
						att, es);
			UA->UMode |= (XTRN_OMODE_SOCKET | XTRN_IMODE_SOCKET);
			print ("socket");
CheckOType:
			// Check the type attribute
			if ((att = sxml_attr (cur, "type")) != NULL) {
				if (strcmp (att, "held") == 0 ||
				    strcmp (att, "hold") == 0 ||
				    strcmp (att, "wait") == 0 ) {
					print (", HELD");
					// Hold output until connected
					UA->UMode |= XTRN_OMODE_HOLD;
				}
				// Ignore other types for now; we may need more
				// later
			}
		} else {
			xeai ("target", es, att);
		}

		// The coding
		if ((att = sxml_attr (cur, "coding")) != NULL) {
			if (strcmp (att, "hex") == 0) {
				UA->UMode |= XTRN_OMODE_HEX;
				print (", HEX");
			} else if (strcmp (att, "ascii"))
				xeai ("coding", es, att);
		}
		print ("\n\n");
	}

NoUOutput:

	// Check if the UART is there after all this parsing
	UA->absent = ((UA->UMode & (XTRN_OMODE_MASK | XTRN_IMODE_MASK)) == 0);

	if (!UA->absent && UA->URate == 0)
		xemi ("rate", es);

	return UA;
}

data_pn_t *BoardRoot::readPinsParams (sxml_t data, const char *esn) {
/*
 * Decodes PINS parameters
 */
	double d;
	sxml_t cur;
	nparse_t np [3], *npp;
	const char *att;
	char es [48];
	char *str, *sts;
	data_pn_t *PN;
	byte *BS;
	short *SS;
	int len, ni, nj;
	byte pn;

	if ((data = sxml_child (data, "pins")) == NULL)
		return NULL;

	strcpy (es, "<pins> for ");
	strcat (es, esn);

	PN = new data_pn_t;

	PN->PMode = 0;
	PN->NA = 0;
	PN->MPIN = PN->NPIN = PN->D0PIN = PN->D1PIN = BNONE;
	PN->ST = PN->IV = PN->BN = NULL;
	PN->VO = NULL;
	PN->PIDev = PN->PODev = NULL;
	PN->absent = NO;

	/* Total number of pins */
	if ((att = sxml_attr (data, "total")) == NULL &&
	    (att = sxml_attr (data, "number")) == NULL) {
		PN->absent = YES;
		return PN;
	}

	np [0].type = np [1].type = np [2].type = TYPE_LONG;

	for (len = 0; len < 7; len++)
		PN->DEB [len] = 0;
	
	if (parseNumbers (att, 1, np) != 1 || np [0].LVal < 0 ||
	    np [0].LVal > 254)
		xeai ("total", es, att);

	if (np [0].LVal == 0) {
		// An explicit way to say that there are no PINS
		PN->absent = YES;
		return PN;
	}
	PN->NP = (byte) (np [0].LVal);

	/* ADC pins */
	if ((att = sxml_attr (data, "adc")) != NULL) {
		if (parseNumbers (att, 1, np) != 1 || np [0].LVal < 0 ||
	    	  np [0].LVal > PN->NP)
		    xeai ("adc", es, att);
		PN->NA = (byte) (np [0].LVal);
	}

	/* Counter */
	if ((att = sxml_attr (data, "counter")) != NULL) {
		if ((ni = parseNumbers (att, 3, np)) < 1) {
CntErr:
		      xeai ("counter", es, att);
		}
		if (np [0].LVal < 0 || np [0].LVal >= PN->NP)
			goto CntErr;
		PN->MPIN = (byte) (np [0].LVal);
		if (ni > 1) {
			// Debouncers
			if ((PN->DEB [0] = (Long)(np [1].LVal)) < 0)
				goto CntErr;
			if (ni > 2) {
				if ((PN->DEB [1] = (Long)(np [2].LVal)) < 0)
					goto CntErr;
			}
		}
	}

	/* Notifier */
	if ((att = sxml_attr (data, "notifier")) != NULL) {
		if ((ni = parseNumbers (att, 3, np)) < 1) {
NotErr:
		      xeai ("notifier", es, att);
		}
		if (np [0].LVal < 0 || np [0].LVal >= PN->NP)
			goto NotErr;
		PN->NPIN = (byte) (np [0].LVal);
		if (PN->NPIN == PN->MPIN)
			goto NotErr;
		if (ni > 1) {
			// Debouncers
			if ((PN->DEB [2] = (Long)(np [1].LVal)) < 0)
				goto NotErr;
			if (ni > 2) {
				if ((PN->DEB [3] = (Long)(np [2].LVal)) < 0)
					goto NotErr;
			}
		}
	}

	/* DAC */
	if ((att = sxml_attr (data, "dac")) != NULL) {
		len = parseNumbers (att, 2, np);
		if (len < 1 || len > 2)
	        	xeai ("dac", es, att);
		if (np [0].LVal < 0 || np [0].LVal >= PN->NP ||
		     np [0].LVal == PN->MPIN || np [0].LVal == PN->NPIN)
	        	xeai ("dac", es, att);
		// The firs one is OK
		PN->D0PIN = (byte) (np [0].LVal);
		if (len == 2) {
			// Verify the second one
			if (np [1].LVal < 0 || np [1].LVal >= PN->NP ||
			  np [0].LVal == np [1].LVal || np [1].LVal == PN->MPIN
			    || np [1].LVal == PN->NPIN)
	        	      xeai ("dac", es, att);
			PN->D1PIN = (byte) (np [1].LVal);
		}
	}

	print (form ("  PINS [total = %1d, adc = %1d", PN->NP, PN->NA));
	if (PN->MPIN != BNONE) {
		print (form (", PM = %1d", PN->MPIN));
		if (PN->DEB [0] != 0 || PN->DEB [1] != 0)
			print (form (" /%1d,%1d/", PN->DEB [0], PN->DEB [1]));
	}
	if (PN->NPIN != BNONE) {
		print (form (", EN = %1d", PN->NPIN));
		if (PN->DEB [2] != 0 || PN->DEB [3] != 0)
			print (form (" /%1d,%1d/", PN->DEB [2], PN->DEB [3]));
	}
	if (PN->D0PIN != BNONE) {
		print (form (", DAC = %1d", PN->D0PIN));
		if (PN->D1PIN != BNONE)
			print (form ("+%1d", PN->D1PIN));
	}
	print ("]:\n");

	/* I/O */
			  
	/* The INPUT spec */
	if ((cur = sxml_child (data, "input")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "source")) == NULL)
			xemi ("source", es);
		if (strcmp (att, "none") == 0) {
			// No input
			goto NoPInput;
		}

		print ("    INPUT:  ");

		if (strcmp (att, "device") == 0) {
			// Preprocess the string (in place, as it can only
			// shrink). Unfortunately, we cannot have exotic
			// characters in it because 0 is the sentinel.
			len = sanitize_string (str);
			if (len == 0)
				xevi ("<input> device string", es, "-empty-");
			// This is a device name
			str [len] = '\0';
			PN->PMode |= XTRN_IMODE_DEVICE;
			PN->PIDev = str;
			print (form ("device '%s'", str));
		} else if (strcmp (att, "socket") == 0) {
			// No string
			PN->PMode |= XTRN_IMODE_SOCKET | XTRN_OMODE_SOCKET;
			print ("socket");
		} else if (strcmp (att, "string") == 0) {
			len = sanitize_string (str);
			sts = (char*) find_strpool ((const byte*) str, len + 1,
				YES);
			PN->PIDev = sts;
			PN->PMode |= XTRN_IMODE_STRING | len;
			print (form ("string '%c%c%c%c ...'",
				sts [0],
				sts [1],
				sts [2],
				sts [3] ));
		} else {
			xeai ("source", es, att);
		}

		print ("\n");
	}

NoPInput:

	// The OUTPUT spec
	if ((cur = sxml_child (data, "output")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "target")) == NULL)
			xemi ("target", es);
		if ((PN->PMode & XTRN_OMODE_MASK) == XTRN_OMODE_SOCKET) {
			// This must be a socket
			if (strcmp (att, "socket"))
				// but isn't
				excptn ("Root: 'target' for <pins> <output> "
					"(%s) in %s must be 'socket'",
						att, es);
			print ("    OUTPUT: socket (see INPUT)\n");
		} else {

			print ("    OUTPUT: ");

			if (strcmp (att, "device") == 0) {

				len = sanitize_string (str);
				if (len == 0)
					xevi ("<output> device string", es,
						"-empty-");
				// This is a device name
				str [len] = '\0';
				PN->PMode |= XTRN_OMODE_DEVICE;
				PN->PODev = str;
				print (form ("device '%s'", str));

			} else if (strcmp (att, "socket") == 0) {

				if ((PN->PMode & XTRN_IMODE_MASK) !=
				    XTRN_IMODE_NONE)
					excptn ("Root: 'target' in <pins> "
					    "<output> (%s) for %s conflicts "
						"with <input> source", att, es);

				PN->PMode |= (XTRN_OMODE_SOCKET |
					XTRN_IMODE_SOCKET);
				print ("socket");
	
			} else if (strcmp (att, "none") != 0) {
	
				xeai ("target", es, att);
			}
			print ("\n");
		}
	}

	/* Pin status */
	if ((cur = sxml_child (data, "status")) != NULL) {
		BS = new byte [len = ((PN->NP + 7) >> 3)];
		// The default is "pin availalble"
		memset (BS, 0xff, len);
		str = (char*)sxml_txt (cur);
		if (sanitize_string (str) == 0)
			xevi ("<status> string", es, "-empty-");
		
		sts = str;
		for (pn = 0; pn < PN->NP; pn++) {
			// Find next digit in sts
			while (isspace (*sts))
				sts++;
			if (*sts == '\0')
				break;
			if (*sts == '1')
				PINS::sbit (BS, pn);
			else if (*sts != '0')
				xevi ("<status>", es, str);
			sts++;
		}
		print ("    STATUS: ");
		for (pn = 0; pn < PN->NP; pn++)
			print (PINS::gbit (BS, pn) ? "1" : "0");
		print ("\n");
		PN->ST = find_strpool ((const byte*)BS, len, NO);
		if (PN->ST != BS)
			// Recycled
			delete [] BS;
	}

	/* Buttons */
	if ((cur = sxml_child (data, "buttons")) != NULL) {

		// Polarity: default == high
		PN->BPol = 1;
		if ((att = sxml_attr (cur, "polarity")) != NULL &&
			(*att == '0' || strcmp (att, "low") == 0))
				PN->BPol = 0;

		// Debounce/repeat
		nj = 0;
		if ((att = sxml_attr (cur, "timing")) != NULL) {
			len = parseNumbers (att, 3, np);
			if (len < 1 || len > 3)
				xeai ("timing", es, att);
			for (pn = 0; pn < len; pn++) {
				if (np [pn].LVal < 0 || np [pn].LVal > MAX_UINT)
					xeai ("timing", es, att);
				PN->DEB [4 + pn] = (Long)(np [pn].LVal);
			}
			nj = 1;
		}
		// We expect at most NP integer (byte-sized) values
		// identifying button numbers
		BS = new byte [PN->NP];
		npp = new nparse_t [PN->NP];
		for (pn = 0; pn < PN->NP; pn++) {
			// This means "not assigned to a button"
			BS [pn] = BNONE;
			npp [pn] . type = TYPE_int;
		}
		str = (char*) sxml_txt (cur);
		if ((len = parseNumbers (str, PN->NP, npp)) < 0)
			excptn ("Root: illegal int value in <buttons> for %s",
				es);
		if (len > PN->NP)
			excptn ("Root: too many values in <buttons> for %s",
				es);
		for (pn = 0; pn < len; pn++) {
			if ((ni = npp [pn] . IVal) < 0)
				// This means skip
				continue;
			// Check if not taken
			if (PN->ST != NULL && PINS::gbit (PN->ST, pn) == 0)
				// Absent
				excptn ("Root: pin %1d in <buttons> for %s is "
					"declared as absent", pn, es);
			if (ni > 254)
				excptn ("Root: button number %1d in <buttons> "
					"for %s is too big (254 is the max)",
						ni, es);
			BS [pn] = (byte) ni;
		}

		delete [] npp;

		print ("    BUTTONS: ");
		for (pn = 0; pn < PN->NP; pn++)
			print (BS [pn] == BNONE ? "- " :
				form ("%1d ", BS [pn]));
		print ("\n");
		if (nj)
			// Timing
			print (form ("      Timing: %1d, %1d, %1d\n",
				PN->DEB [4], PN->DEB [5], PN->DEB [6]));

		PN->BN = find_strpool ((const byte*)BS, PN->NP, NO);
		if (PN->BN != BS)
			// Recycled
			delete [] BS;
	}

	/* Default (initial) pin values */
	if ((cur = sxml_child (data, "values")) != NULL) {
		BS = new byte [len = ((PN->NP + 7) >> 3)];
		memset (BS, 0, len);
		str = (char*)sxml_txt (cur);
		if (sanitize_string (str) == 0)
			xevi ("<values> string", es, "-empty-");
		
		sts = str;
		for (pn = 0; pn < PN->NP; pn++) {
			// Find next digit in sts
			while (isspace (*sts))
				sts++;
			if (*sts == '\0')
				break;
			if (*sts == '1')
				PINS::sbit (BS, pn);
			else if (*sts != '0')
				xevi ("<values>", es, str);
			sts++;
		}
		print ("    VALUES: ");
		for (pn = 0; pn < PN->NP; pn++)
			print (PINS::gbit (BS, pn) ? "1" : "0");
		print ("\n");
		PN->IV = find_strpool ((const byte*)BS, len, NO);
		if (PN->IV != BS)
			// Recycled
			delete [] BS;
	}

	/* Default (initial) ADC input voltage */
	if (PN->NA != 0 && ((cur = sxml_child (data, "voltages")) != NULL ||
				(cur = sxml_child (data, "voltage")) != NULL)) {
		SS = new short [PN->NA];
		memset (SS, 0, PN->NA * sizeof (short));
		npp = new nparse_t [PN->NA];
		for (pn = 0; pn < PN->NA; pn++)
			npp [pn] . type = TYPE_double;
		str = (char*)sxml_txt (cur);
		len = parseNumbers (str, PN->NA, npp);
		if (len > PN->NA)
			excptn ("Root: too many FP values in <voltages> for %s",
				es);
		if (len < 0)
			excptn ("Root: illegal FP value in <voltages> for %s",
				es);
		for (pn = 0; pn < len; pn++) {
			d = (npp [pn] . DVal * 32767.0) / 3.3;
			if (d < -32768.0)
				SS [pn] = 0x8000;
			else if (d > 32767.0)
				SS [pn] = 0x7fff;
			else
				SS [pn] = (short) d;
		}

		PN->VO = (const short*) find_strpool ((const byte*) SS,
			(int)(PN->NA) * sizeof (short), NO);

		if (PN->VO != SS)
			// Recycled
			delete [] SS;

		print ("    ADC INPUTS: ");
		for (pn = 0; pn < PN->NA; pn++)
			print (form ("%5.2f ", (double)(PN->VO [pn]) *
				3.3/32768.0));
		print ("\n");

		delete [] npp;
	}

	print ("\n");

	return PN;
}

static int sa_smax (const char *what, const char *err, sxml_t root) {
/*
 * Calculates the maximum number of sensors/actuators
 */
	int max, last;
	const char *att;
	nparse_t np [1];

	np [0].type = TYPE_LONG;
	max = last = -1;
	// No consistency checks yet - just the maximum
	for (root = sxml_child (root, what); root != NULL;
						root = sxml_next (root)) {
		
		if ((att = sxml_attr (root, "number")) != NULL) {
			if (parseNumbers (att, 1, np) != 1 ||
				np [0].LVal < 0 || np [0].LVal > 255)
					// This is an error
					xeai ("number", err, att);
			last = (int) (np [0].LVal);
		} else
			last++;

		if (last > max)
			max = last;
	}

	return max + 1;
}

int SensActDesc::bsize (lword bound) {
/*
 * Determines the number size for sensor value bound. Note that the
 * value returned by a sensor is normalized and unsigned, such that
 * Min == 0
 */
	if ((bound & 0xFF000000))
		return 4;
	if ((bound & 0x00FF0000))
		return 3;
	if ((bound & 0x0000FF00))
		return 2;
	return 1;
}

Boolean SensActDesc::expand (address vp) {
/*
 * Expand and store the value
 */
	lword v;

	switch (Length) {
	
		case 1:
			v = (lword) *((byte*) vp);
			break;

		case 2:

			v = (lword) *((word*) vp);
			break;

		case 3:
			// This one assumes little endianness (avoid!!!)
			v = 	((lword)(*((byte*)vp))) |
				(((lword)(*(((byte*)vp) + 1))) <<  8) |
				(((lword)(*(((byte*)vp) + 2))) << 16);
			break;
		case 4:
			v = *((lword*)vp);
			break;

		default:
			excptn ("Illegal length in sensor: %1d", Length);
	}
	return set (v);
}

void SensActDesc::get (address vp) {
/*
 * Retrieve a properly sized value
 */
	switch (Length) {

		case 1:
			*((byte*) vp) = (byte) Value;
			return;
		case 2:
			*((word*) vp) = (word) Value;
			return;
		case 3:
			// This one assumes little endianness (avoid!!!)
			*(((byte*)vp) + 0) = (byte) (Value      );
			*(((byte*)vp) + 1) = (byte) (Value >>  8);
			*(((byte*)vp) + 2) = (byte) (Value >> 16);
			return;
		case 4:
			*((lword*) vp) = Value;
			return;

		default:
			excptn ("Illegal length in sensor: %1d", Length);
	}
}

static SensActDesc *sa_doit (const char *what, const char *erc, sxml_t root,
									int n) {
/*
 * Initialize the sensor/actuator array
 */
	int last, i, j, rqs, mns, fill;
	lword ival;
	const char *att;
	nparse_t np [2];
	SensActDesc *res;
	byte Type;

	Type = strcmp (what, "sensor") ? SEN_TYPE_ACTUATOR : SEN_TYPE_SENSOR;
	
	res = new SensActDesc [n];
	memset (res, 0, sizeof (SensActDesc) * n);
	np [0].type = TYPE_LONG;
	np [1].type = TYPE_LONG;

	last = -1;
	fill = 0;

	for (root = sxml_child (root, what); root != NULL;
						root = sxml_next (root)) {
		np [0].type = np [1].type = TYPE_LONG;
		
		if ((att = sxml_attr (root, "number")) != NULL) {
			if (parseNumbers (att, 1, np) != 1 ||
				np [0].LVal < 0 || np [0].LVal > 255)
					// This has been verified already, but
					// let us play it safe
					xeai ("number", erc, att);
			last = (int) (np [0].LVal);
		} else
			last++;

		if (last >= n || res [last] . Length != 0)
			xeai ("number", erc, att);

		fill++;

		res [last] . Type = Type;
		res [last] . Id = last;

		// Check for range
		att = sxml_txt (root);
		i = parseNumbers (att, 1, np);
		if (i == 0) {
			// Assume unsigned full size
			res [last] . Max = 0xffffffff;
		} else if (i < 0) {
			excptn ("Root: illegal numerical value for %s in %s",
				what, erc);
		} else {
			if (np [0] . LVal <= 0)
			   excptn ("Root: max value for %s in %s: is <= 0 (%s)",
			      what, erc, att);
			res [last] . Max = (lword) (np [0] . LVal);
		}

		// Check for initial value
		if ((att = sxml_attr (root, "init")) != NULL) {
			if (parseNumbers (att, 1, np) != 1 || np [0] . LVal < 0)
				xeai ("init", erc, att);
			ival = (lword)(np [0].LVal);
		} else
			ival = 0;

		// Check for explicit length indication
		if ((att = sxml_attr (root, "vsize")) != NULL) {
			if (parseNumbers (att, 1, np) != 1 ||
				np [0].LVal < 1 || np [0].LVal > 4)
					excptn ("Root: illegal value size in "
						"%s in %s: %s", what, erc, att);
			rqs = (int) (np [0].LVal);
		} else
			// Requested size, if any
			rqs = 0;

		// Size derived from Max
		mns = SensActDesc::bsize (res [last] . Max);

		if (i == 0) {
			// Derive bound from size
			if (rqs == 0)
				// This is the default
				rqs = 4;	
			else if (rqs < 4)
				// Actually derive the upper bound
				res [last] . Max = (1 << (rqs*8)) - 1;
		} else if (rqs == 0) {
			// Derive size from Max
			rqs = mns;
		} else {
			// Verify
			if (rqs < mns)
				excptn ("Root: requested vsize for %s in %s is "
					"less than minimum required: %s",
						what, erc, att);
		}

		// Timing
		np [0].type = np [1].type = TYPE_double;
		if ((att = sxml_attr (root, "delay")) != NULL) {
			if ((j = parseNumbers (att, 2, np)) < 1 || j > 2)
				xeai ("delay", erc, att);
			if ((res [last] . MinTime = np [0] . DVal) < 0.0)
				xeai ("delay", erc, att);
			if (j > 1) {
				if ((res [last] . MaxTime = np [1] . DVal) <
							  res [last] . MinTime)
					xeai ("delay", erc, att);
			} else {
				res [last] . MaxTime = res [last] . MinTime;
			}
		}

		res [last] . Length = rqs;
		res [last] . set (ival);
		res [last] . RValue = res [last] . Value;
	}

	print (form ("  %s [total = %1d, used = %1d]:\n",
		strcmp (what, "sensor") ? "ACTUATORS" : "SENSORS", n, fill));

	for (i = 0; i < n; i++)
		if (res [i] . Length)
			print (form ("    NUMBER %1d, VSize %1d, "
				"Range %1u, Init: %1u, "
					"Delay: [%f7.4,%f7.4]\n",
						i, res [i] . Length,
						res [i] . Max,
						res [i] . Value,
						res [i] . MinTime,
						res [i] . MaxTime));
	return res;
}

data_sa_t *BoardRoot::readSensParams (sxml_t data, const char *esn) {
/*
 * Decodes Sensors/Actuators
 */
	data_sa_t *SA;
	sxml_t cur;
	int len;
	const char *att;
	char es [48];
	char *str, *sts;

	if ((data = sxml_child (data, "sensors")) == NULL)
		return NULL;

	strcpy (es, "<sensors> for ");
	strcat (es, esn);

	SA = new data_sa_t;

	SA->Sensors = SA->Actuators = NULL;
	SA->SIDev = SA->SODev = NULL;
	SA->absent = NO;
	SA->SMode = 0;

	SA->NS = (byte) sa_smax ("sensor", es, data);
	SA->NA = (byte) sa_smax ("actuator", es, data);

	if (SA->NS == 0 && SA->NA == 0) {
		// That's it
		SA->absent = YES;
		return SA;
	}

	if (SA->NS != 0)
		SA->Sensors = sa_doit ("sensor", es, data, SA->NS);
	if (SA->NA != 0)
		SA->Actuators = sa_doit ("actuator", es, data, SA->NA);

	/*
	 * I/O FIXME: these operations look similar to PINS, perhaps we
	 * encapsulate them out. Note that this also applies to the Agent
	 * interface.
	 */
			  
	/* The INPUT spec */
	if ((cur = sxml_child (data, "input")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "source")) == NULL)
			xemi ("source", es);
		if (strcmp (att, "none") == 0) {
			// No input
			goto NoSInput;
		}

		print ("    INPUT:  ");

		if (strcmp (att, "device") == 0) {
			len = sanitize_string (str);
			if (len == 0)
				xevi ("<input> device string", es, "-empty-");
			// This is a device name
			str [len] = '\0';
			SA->SMode |= XTRN_IMODE_DEVICE;
			SA->SIDev = str;
			print (form ("device '%s'", str));
		} else if (strcmp (att, "socket") == 0) {
			// No string
			SA->SMode |= XTRN_IMODE_SOCKET | XTRN_OMODE_SOCKET;
			print ("socket");
		} else if (strcmp (att, "string") == 0) {
			len = sanitize_string (str);
			sts = (char*) find_strpool ((const byte*) str, len + 1,
				YES);
			SA->SIDev = sts;
			SA->SMode |= XTRN_IMODE_STRING | len;
			print (form ("string '%c%c%c%c ...'",
				sts [0],
				sts [1],
				sts [2],
				sts [3] ));
		} else {
			xeai ("source", es, att);
		}

		print ("\n");
	}

NoSInput:

	// The OUTPUT spec
	if ((cur = sxml_child (data, "output")) != NULL) {
		str = (char*) sxml_txt (cur);
		if ((att = sxml_attr (cur, "target")) == NULL)
			xemi ("target", es);
		if ((SA->SMode & XTRN_OMODE_MASK) == XTRN_OMODE_SOCKET) {
			// This must be a socket
			if (strcmp (att, "socket"))
				// but isn't
				excptn ("Root: 'target' for <sensors> <output> "
					"(%s) in %s must be 'socket'",
						att, es);
			print ("    OUTPUT: socket (see INPUT)\n");
		} else {
			print ("    OUTPUT: ");
			if (strcmp (att, "device") == 0) {

				len = sanitize_string (str);
				if (len == 0)
					xevi ("<output> device string", es,
						"-empty-");
				// This is a device name
				str [len] = '\0';
				SA->SMode |= XTRN_OMODE_DEVICE;
				SA->SODev = str;
				print (form ("device '%s'", str));

			} else if (strcmp (att, "socket") == 0) {

				if ((SA->SMode & XTRN_IMODE_MASK) !=
				    XTRN_IMODE_NONE)
					excptn ("Root: 'target' in <sensors> "
					    "<output> (%s) for %s conflicts "
						"with <input> source", att, es);

				SA->SMode |= (XTRN_OMODE_SOCKET |
					XTRN_IMODE_SOCKET);
				print ("socket");
	
			} else if (strcmp (att, "none") != 0) {
	
				xeai ("target", es, att);
			}
			print ("\n");
		}
	}

	print ("\n");

	return SA;
}

data_le_t *BoardRoot::readLedsParams (sxml_t data, const char *esn) {
/*
 * Decodes LEDs parameters
 */
	sxml_t cur;
	nparse_t np [1];
	data_le_t *LE;
	const char *att;
	char *str;
	int len;
	
	char es [48];

	strcpy (es, "<leds> for ");
	strcat (es, esn);

	if ((data = sxml_child (data, "leds")) == NULL)
		return NULL;

	LE = new data_le_t;
	LE->LODev = NULL;

	if ((att = sxml_attr (data, "number")) == NULL &&
	    (att = sxml_attr (data, "total")) == NULL    ) {
		LE->absent = YES;
		return LE;
	} else {
		LE->absent = NO;
		np [0].type = TYPE_LONG;
		if (parseNumbers (att, 1, np) != 1 || np [0].LVal < 0 ||
		    np [0].LVal > 64)
			xeai ("number", es, att);
		if (np [0].LVal == 0) {
			// Explicit NO
			print ("  LEDS: none\n");
			LE->absent = YES;
			return LE;
		}
	}

	LE->NLeds = (word) (np [0].LVal);

	print (form ("  LEDS: %1d\n", LE->NLeds));

	// Output mode

	if ((cur = sxml_child (data, "output")) == NULL) {
		print ("    OUTPUT: none\n\n");
		LE->absent = YES;
		return LE;
	}

	if ((att = sxml_attr (cur, "target")) == NULL)
		xemi ("target", es);
	if (strcmp (att, "none") == 0) {
		// Equivalent to no leds
		LE->absent = YES;
		print ("    OUTPUT: none\n\n");
		return LE;
	} 

	print ("    OUTPUT: ");

	if (strcmp (att, "device") == 0) {
		str = (char*) sxml_txt (cur);
		len = sanitize_string (str);
		if (len == 0)
			xevi ("<output> device string", es, "-empty-");
		// This is a device name
		str [len] = '\0';
		LE->LODev = str;
		print (form ("device '%s'\n\n", str));
	} else if (strcmp (att, "socket") == 0) {
		print (form ("socket\n\n", str));
	} else
		xeai ("target", es, att);

	return LE;
}

data_pt_t *BoardRoot::readPwtrParams (sxml_t data, const char *esn) {
/*
 * Decodes power tracker parameters
 */
	sxml_t cur;
	nparse_t np [16];
	double lv [16];
	data_pt_t *PT;
	pwr_mod_t *md;
	int i, nm, cm;
	const char *att;
	char *str;
	char es [48];

	const char *mids [] = { "cpu", "radio", "storage", "sensors", "rf",
				"eeprom", "sd", "sensor" };

	strcpy (es, "<ptracker> for ");
	strcat (es, esn);

	if ((data = sxml_child (data, "ptracker")) == NULL)
		return NULL;

	PT = new data_pt_t;
	PT->PODev = NULL;
	PT->absent = YES;

	for (i = 0; i < PWRT_N_MODULES; i++)
		PT->Modules [i] = NULL;

	for (i = 0; i < 16; i++)
		np [i].type = TYPE_double;

	for (cur = sxml_child (data, "module"); cur != NULL;
							cur = sxml_next (cur)) {

		if ((att = sxml_attr (cur, "id")) == NULL)
			xemi ("id", es);

		for (cm = 0; cm < 8; cm++)
			if (strcmp (att, mids [cm]) == 0)
				break;
		if (cm == 8)
			xeai ("id of <module>", es, att);

		switch (cm) {

			case 0:
			case 1:
			case 2:
			case 3:
				break;
			case 4:
				cm = 1;
				break;
			case 5:
			case 6:
				cm = 2;
				break;
			default:
				cm = 3;
		}

		if (PT->Modules [cm] != NULL)
			excptn ("Root: duplicate module id: %s in %s", att, es);
		
		PT->Modules [cm] = md = new pwr_mod_t;

		// Levels
		str = (char*) sxml_txt (cur);
		sanitize_string (str);
		if ((nm = parseNumbers (str, 16, np)) > 16)
			excptn ("Root: too many levels (%1d) in module %s in"
				"%s, max is 16", nm, att, es);
		if (nm < 2)
			excptn ("Root: less than two levels in module %s in %s",
				att, es);

		for (i = 0; i < nm; i++)
			lv [i] = np [i].DVal;

		md->Levels = (double*) find_strpool ((const byte*)lv, 
			nm * sizeof (double), YES);

		md->NStates = nm;
		PT->absent = NO;
	}

	if (PT->absent) {
		// Explicitly absent
		return PT;
	}

	print ("  PTRACKER:\n");

	// Output mode
	if ((cur = sxml_child (data, "output")) == NULL) {
		xenf ("<output>", es);
	}

	if ((att = sxml_attr (cur, "target")) == NULL)
		xemi ("target", es);

	if (strcmp (att, "none") == 0) {
		xevi ("target", es, "none");
	} 

	print ("    OUTPUT: ");

	if (strcmp (att, "device") == 0) {
		str = (char*) sxml_txt (cur);
		cm = sanitize_string (str);
		if (cm == 0)
			xevi ("<output> device string", es, "-empty-");
		// This is a device name
		str [cm] = '\0';
		PT->PODev = str;
		print (form ("device '%s'\n\n", str));
	} else if (strcmp (att, "socket") == 0) {
		print (form ("socket\n", str));
	} else
		xeai ("target", es, att);

	for (cm = 0; cm < PWRT_N_MODULES; cm++) {
		if ((md = PT->Modules [cm]) == NULL)
			continue;
		print (form ("    MODULE %s, Levels:", mids [cm]));
		for (i = 0; i < md->NStates; i++)
			print (form (" %g", md->Levels [i]));
		print ("\n");
	}
	print ("\n");

	return PT;
}

void BoardRoot::initNodes (sxml_t data, int NT, int NN) {

	data_no_t *DEF, *NOD;
	const char *def_type, *nod_type, *att, *start;
	sxml_t cno, cur, *xnodes;
	Long i, j, last, fill;
	int tq;
	nparse_t np [2];
	data_rf_t *NRF, *DRF;
	data_ep_t *NEP, *DEP;

	print ("Timing:\n\n");
	print ((double) etuToItu (1.0),
		   "  ETU (1s) = ", 11, 14);
	print ((double) duToItu (1.0),
		   "  DU  (1m) = ", 11, 14);
	print (getTolerance (&tq),
	           "  TOL      = ", 11, 14);
	print (tq, "  TOL QUAL = ", 11, 14);
	print ("\n");

	TheStation = System;

	if ((data = sxml_child (data, "nodes")) == NULL)
		xenf ("<nodes>", "<network>");

	// Check for the defaults
	cno = sxml_child (data, "defaults");
	start = NULL;
	if (cno != NULL) {
		def_type = sxml_attr (cno, "type");
		start = sxml_attr (cno, "start");
	} else
		def_type = NULL;

	// OK if NULL, readNodeParams will initialize them
	DEF = readNodeParams (cno, -1, start);
	// DEF is never NULL, remember to deallocate it
	
	// A temporary
	xnodes = new sxml_t [NT];
	for (i = 0; i < NT; i++)
		xnodes [i] = NULL;

	last = -1;
	fill = 0;
	np [0] . type = TYPE_LONG;

	for (cno = sxml_child (data, "node"); cno != NULL;
							cno = sxml_next (cno)) {
		att = sxml_attr (cno, "number");
		if (att != NULL) {
			if (parseNumbers (att, 1, np) != 1 ||
			    np [0].LVal < 0 || np [0].LVal >= NT)
				xeai ("number", "<node>", att);
			last = (Long) (np [0].LVal);
		} else
			last++;

		if (last >= NT)
			excptn ("Root: implict node number reaches the limit"
				" of %1d", NT);

		if (xnodes [last] != NULL)
			excptn ("Root: node %1d multiply defined", last);

		xnodes [last] = cno;

		fill++;
	}

	if (fill < NT) {
		for (i = 0; i < NT; i++)
			if (xnodes [i] == NULL)
				break;
		excptn ("Root: some nodes undefined, first is %1d", i);
	}

	for (tq = i = 0; i < NT; i++) {
		cno = xnodes [i];
		start = sxml_attr (cno, "start");
		NOD = readNodeParams (cno, i, start);

		if ((nod_type = sxml_attr (cno, "type")) == NULL)
			nod_type = def_type;

		// Substitute defaults as needed; validate later
		if (NOD->Mem == 0)
			NOD->Mem = DEF->Mem;

		if (NOD->On == WNONE)
			NOD->On = DEF->On;

		if (NOD->PLimit == WNONE)
			NOD->PLimit = DEF->PLimit;

		if (NOD->Lcdg == WNONE)
			NOD->Lcdg = DEF->Lcdg;

		// === radio ==================================================

		NRF = NOD->rf;
		DRF = DEF->rf;

		if (NRF == NULL) {
			// Inherit the defaults
			if (DRF != NULL && !(DRF->absent)) {
				NRF = NOD->rf = DRF;
			}
		} else if (NRF->absent)  {
			// Explicit absent
			delete NRF;
			NRF = NOD->rf = NULL;
		} else if (DRF != NULL && !(DRF->absent)) {

			// Partial defaults

			if (NRF->Boost == HUGE)
				NRF->Boost = DRF->Boost;

			if (NRF->Rate == WNONE)
				NRF->Rate = DRF->Rate;

			if (NRF->Power == WNONE)
				NRF->Power = DRF->Power;

			if (NRF->Channel == WNONE)
				NRF->Channel = DRF->Channel;

			if (NRF->BCMin == WNONE) {
				NRF->BCMin = DRF->BCMin;
				NRF->BCMax = DRF->BCMax;
			}

			if (NRF->Pre == WNONE)
				NRF->Pre = DRF->Pre;

			if (NRF->LBTDel == WNONE) {
				NRF->LBTDel = DRF->LBTDel;
				NRF->LBTThs = DRF->LBTThs;
			}
		}

		// === EEPROM =================================================

		NEP = NOD->ep;
		DEP = DEF->ep;

		if (NEP == NULL) {
			// Inherit the defaults
			if (DEP != NULL && !(DEP->absent)) {
				NEP = NOD->ep = DEP;
			}
		} else if (NEP->absent) {
			// Explicit "no", ignore the default
			delete NEP;
			NEP = NOD->ep = NULL;
		} else if (DEP != NULL && !(DEP->absent)) {
			// Partial defaults?
			if (NEP->EEPRS == LWNONE) {
				// FIXME: provide a function to copy this
				NEP->EEPRS = DEP->EEPRS;
				NEP->EEPPS = DEP->EEPPS;
				NEP->EPIF = DEP->EPIF;
				// Do not deallocate arrays
				NEP->EFLGS = DEP->EFLGS | NVRAM_NOFR_EPINIT;
				NEP->EPINI = DEP->EPINI;
				for (j = 0; j < EP_N_BOUNDS; j++)
					NEP->bounds [j] =
						DEP->bounds [j];
				if (NEP->EPIF != NULL) {
				}
			}
			if (NEP->IFLSS == WNONE) {
				NEP->IFLSS = DEP->IFLSS;
				NEP->IFLPS = DEP->IFLPS;
				NEP->IFIF = DEP->IFIF;
				// Do not deallocate arrays
				NEP->EFLGS |= NVRAM_NOFR_IFINIT;
				NEP->IFINI = DEP->IFINI;
			}
		}

		if (NEP) {
			// FIXME: this desperately calls to be cleaned up
			if (NEP->EEPRS == LWNONE)
				NEP->EEPRS = 0;
			if (NEP->IFLSS == WNONE)
				NEP->IFLSS = 0;
		}

		// === UART ===================================================

		if (NOD->ua == NULL) {
			// Inherit the defaults
			if (DEF->ua != NULL && !(DEF->ua->absent))
				NOD->ua = DEF->ua;
		} else if (NOD->ua->absent) {
			// Explicit "no", ignore the default
			delete NOD->ua;
			NOD->ua = NULL;
		}

		// === PINS ===================================================

		if (NOD->pn == NULL) {
			// Inherit the defaults
			if (DEF->pn != NULL && !(DEF->pn->absent))
				NOD->pn = DEF->pn;
		} else if (NOD->pn->absent) {
			// Explicit "no", ignore the default
			delete NOD->pn;
			NOD->pn = NULL;
		}

		// === Sensors/Actuators ======================================

		if (NOD->sa == NULL) {
			// Inherit the defaults
			if (DEF->sa != NULL && !(DEF->sa->absent))
				NOD->sa = DEF->sa;
		} else if (NOD->sa->absent) {
			// Explicit "no", ignore the default
			delete NOD->sa;
			NOD->sa = NULL;
		}

		// === LED ====================================================

		if (NOD->le == NULL) {
			// Inherit the defaults
			if (DEF->le != NULL && !(DEF->le->absent))
				NOD->le = DEF->le;
		} else if (NOD->le->absent) {
			// Explicit "no", ignore the default
			delete NOD->le;
			NOD->le = NULL;
		}

		// === PTRACKER ===============================================

		if (NOD->pt == NULL) {
			// Inherit the defaults
			if (DEF->pt != NULL && !(DEF->pt->absent))
				NOD->pt = DEF->pt;
		} else if (NOD->pt->absent) {
			// Explicit "no", ignore the default
			delete NOD->pt;
			NOD->pt = NULL;
		}

		// A few checks; some of this stuff is checked (additionally) at
		// the respective constructors

		if (NOD->Mem == 0)
			excptn ("Root: memory for node %1d is undefined", i);

		if (NOD->PLimit == WNONE)
			NOD->PLimit = 0;

		if (NOD->Lcdg == WNONE)
			NOD->Lcdg = 0;

		if (NRF != NULL) {

			// Count those with RF interface
			tq++;
			if (tq > NN)
				excptn ("Root: too many nodes with RF interface"
					", %1d expected", NN);

			if (NRF->Boost == HUGE)
				// The default is no boost (0dB)
				NRF->Boost = 0.0;

			if (NRF->Rate == WNONE)
				// This one has a reasonable default
				NRF->Rate = Ether->Rates->lower ();

			if (NRF->Power == WNONE)
				// And so does this one
				NRF->Power = Ether->PS->lower ();

			if (NRF->Channel == WNONE)
				// And so does this
				NRF->Channel = 0;

			if (NRF->BCMin == WNONE) 
				excptn ("Root: backoff for node %1d is "
					"undefined", i);

			if (NRF->Pre == WNONE)
				excptn ("Root: preamble for node %1d is "
					"undefined", i);

			if (NRF->LBTDel == WNONE)
				excptn ("Root: LBT parameters for node %1d are "
					"undefined", i);
		}
				
		// Location: there is no definable default for it, but it
		// defaults to <0, 0> if no radif
		if ((cur = sxml_child (cno, "location")) == NULL) {

			if (NRF != NULL)
			  	excptn ("Root: no location for node %1d (which"
					"is equipped with radio)", i);

			NOD->X = NOD->Y = 0.0;
		} else {
			att = sxml_txt (cur);
			np [0].type = np [1].type = TYPE_double;
			if (parseNumbers (att, 2, np) != 2 ||
			    np [0].DVal < 0.0 || np [1].DVal < 0.0)
				excptn ("Root: illegal location (%s) for node "
					"%1d", att, i);
			NOD->X = np [0].DVal;
			NOD->Y = np [1].DVal;
		}

		buildNode (nod_type, NOD);

		// Deallocate the data spec. Note that ua and pn may contain
		// arrays, but those arrays need not (must not) be deallocated.
		// They are either strings pointed to in the sxmtl tree, which
		// is deallocated elsewhere, or intentionally copied (constant)
		// strings that have been linked to by the respective objects.

		if (NOD->rf != NULL && NOD->rf != DEF->rf)
			delete NOD->rf;
		if (NOD->ep != NULL && NOD->ep != DEF->ep)
			deallocate_ep_def (NOD->ep);
		if (NOD->ua != NULL && NOD->ua != DEF->ua)
			delete NOD->ua;
		if (NOD->pn != NULL && NOD->pn != DEF->pn)
			delete NOD->pn;
		if (NOD->sa != NULL && NOD->sa != DEF->sa)
			delete NOD->sa;
		if (NOD->le != NULL && NOD->le != DEF->le)
			delete NOD->le;
		if (NOD->pt != NULL && NOD->pt != DEF->pt)
			delete NOD->pt;
		delete NOD;
	}

	// Delete the scratch node list
	delete [] xnodes;

	// Delete the default data block
	if (DEF->rf != NULL)
		delete DEF->rf;
	if (DEF->ep != NULL)
		deallocate_ep_def (DEF->ep);
	if (DEF->ua != NULL)
		delete DEF->ua;
	if (DEF->pn != NULL)
		delete DEF->pn;
	if (DEF->sa != NULL)
		delete DEF->sa;
	if (DEF->pt != NULL)
		delete DEF->pt;

	delete DEF;

	if (tq < NN)
		excptn ("Root: too few (%1d) nodes with RF interface, %1d"
			" expected", tq, NN);

	if (SEther != NULL) {
		// Minimum distance
		SEther->setMinDistance (SEther->RDist);
		// We need this for ANYEVENT
		SEther->setAevMode (NO);
	}
}

void BoardRoot::initAll () {

	sxml_t xml;
	const char *att;
	nparse_t np [1];
	Boolean nc;
	int NN, NT;

	settraceFlags (	TRACE_OPTION_TIME +
			TRACE_OPTION_ETIME +
			TRACE_OPTION_STATID +
			TRACE_OPTION_PROCESS );

	xml = sxml_parse_input ();
	if (!sxml_ok (xml))
		excptn ("Root: XML input data error, %s",
			(char*)sxml_error (xml));
	if (strcmp (sxml_name (xml), "network"))
		excptn ("Root: <network> data expected");

	np [0] . type = TYPE_LONG;

	// Decode the number of stations
	if ((att = sxml_attr (xml, "nodes")) == NULL)
		xemi ("nodes", "<network>");

	if (parseNumbers (att, 1, np) != 1)
		xeai ("nodes", "<network>", att);

	NT = (int) (np [0] . LVal);
	if (NT <= 0)
		excptn ("Root: 'nodes' in <network> must be strictly positive, "
			"is %1d", NT);

	// How many interfaced to radio?
	if ((att = sxml_attr (xml, "radio")) == NULL) {
		// All of them by default
		NN = NT;
		nc = YES;
	} else {
		if (parseNumbers (att, 1, np) != 1)
			xeai ("radio", "<network>", att);

		NN = (int) (np [0] . LVal);
		if (NN < 0 || NN > NT)
			excptn ("Root: the 'radio' attribute in <network> must "
				"be >= 0 <= 'nodes', " "is %1d", NN);
		nc = NO;
	}
	// Check for the non-standard port
	if ((att = sxml_attr (xml, "port")) != NULL) {
		if (parseNumbers (att, 1, np) != 1 || np [0] . LVal < 1 ||
		    np [0] . LVal > 0x0000ffff)
			xeai ("port", "<network>", att);
		ZZ_Agent_Port = (word) (np [0] . LVal);
	}

	initTiming (xml);
	// The third argument tells whether NN is a default copy of NT or not;
	// in the former case, we can force it to zero, if there is no channel
	// definition in the input data set
	NN = initChannel (xml, NN, nc);

	initNodes (xml, NT, NN);
	initPanels (xml);
	initRoamers (xml);

	sxml_free (xml);

	print ("\n");

	setResync (500, 0.5);

	create (System) AgentInterface;
}

// ======================================================== //
// Here is the part to run under the fake PicOS environment //
// ======================================================== //

#include "stdattr.h"

// === UART direct ============================================================

void d_uart_inp_p::setup () {

	assert (S->uart->IMode == UART_IMODE_D,
		"d_uart_inp_p->setup: illegal mode");
	f = UART_INTF_D (S->uart);
	assert (f->pcsInserial == NULL,
		"d_uart_inp_p->setup: duplicated process");
	f->pcsInserial = this;
}

void d_uart_inp_p::close () {
	f->pcsInserial = NULL;
	S->tally_out_pcs ();
	terminate ();
	sleep;
}

d_uart_inp_p::perform {

    int quant;

    state IM_INIT:

	if (f->__inpline != NULL)
		/* Never overwrite previous unclaimed stuff */
		close ();

	if ((tmp = ptr = (char*) umalloc (MAX_LINE_LENGTH + 1)) == NULL) {
		/*
		 * We have to wait for memory
		 */
		umwait (IM_INIT);
		sleep;
	}
	len = MAX_LINE_LENGTH;

    transient IM_READ:

	io (IM_READ, 0, READ, ptr, 1);
	if (ptr == tmp) { // new line
		if (*ptr == '\0') { // bin cmd
			ptr++;
			len--;
			proceed IM_BIN;
		}

		if (*ptr < 0x20)
			/* Ignore codes below space at the beginning of line */
			proceed IM_READ;
	}
	if (*ptr == '\n' || *ptr == '\r') {
		*ptr = '\0';
		f->__inpline = tmp;
		close ();
	}

	if (len) {
		ptr++;
		len--;
	}

	proceed IM_READ;

    state IM_BIN:

	io (IM_BIN, 0, READ, ptr, 1);
	if (--len > *ptr +1) // 1 for 0x04
		len = *ptr +1;
	ptr++;

    transient IM_BIN1:

	quant = io (IM_BIN1, 0, READ, ptr, len);
	len -= quant;
	if (len == 0) {
		f->__inpline = tmp;
		close ();
	}
	ptr += quant;
	proceed IM_BIN1;

}

void d_uart_out_p::setup (const char *d) {

	assert (S->uart->IMode == UART_IMODE_D,
		"d_uart_out_p->setup: illegal mode");
	f = UART_INTF_D (S->uart);
	assert (f->pcsOutserial == NULL,
		"d_uart_out_p->setup: duplicated process");
	assert (d != NULL, "d_uart_out_p->setup: string pointer is NULL");
	f->pcsOutserial = this;
	ptr = data = d;
}

void d_uart_out_p::close () {
	f->pcsOutserial = NULL;
	S->tally_out_pcs ();
	terminate ();
	sleep;
}

d_uart_out_p::perform {

    int quant;

    state OM_INIT:

	if (*ptr)
		len = strlen (ptr);
	else
		len = ptr [1] +3; // 3: 0x00, len, 0x04

    transient OM_WRITE:

	quant = io (OM_WRITE, 0, WRITE, (char*)ptr, len);
    	ptr += quant;
	len -= quant;
	if (len == 0) {
		/* This is always a fresh buffer allocated dynamically */
		ufree (data);
		close ();
	}
	proceed OM_WRITE;
}

int zz_running (void *tid) {

	Process *P [1];

	return zz_getproclist (TheNode, tid, P, 1) ? __cpint (P [0]) : 0;
}

int zz_killall (void *tid) {

	Process *P [16];
	Long np, i, tot;

	tot = 0;
	while (1) {
		np = zz_getproclist (TheNode, tid, P, 16);
		tot += np;
		for (i = 0; i < np; i++) {
			P [i] -> terminate ();
			TheNode->tally_out_pcs ();
		}
		if (np < 16)
			return tot;
	}
}

int zz_crunning (void *tid) {

	Process **P;
	int np;

	if (TheNode->NPcLim == 0)
		excptn ("crunning: function requires a limit on process "
			"population size");

	np = TheNode->NPcss;

	if (tid == NULL)
		return TheNode->NPcLim - np;

	P = new Process* [np];

	// FIXME: a special function would help to speed it up; we don't
	// actually have to collect all those processes
	np = zz_getproclist (TheNode, tid, P, np);
	delete P;

	return np;
}

identify (VUEE VUEE_VERSION);

#include "lcdg_n6100p.cc"

#include "lib_modules.h"

#include "stdattr_undef.h"

#include "agent.cc"
#include "rfmodule.cc"
#include "uart_phys.cc"

#endif
