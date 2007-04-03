#ifndef	__picos_board_h__
#define	__picos_board_h__

#include "picos.h"
#include "ndata.h"
#include "agent.h"
#include "nvram.h"
#include "tcv.h"
#include "tcvphys.h"
#include "wchan.h"
#include "tarp.h"
#include "plug_tarp.h"
#include "plug_null.h"

#define	N_MEMEVENT	0xFFFF0001
#define	PMON_CNTEVENT	0xFFFF0002
#define	PMON_NOTEVENT	0xFFFF0003

#define	PMON_STATE_NOT_RISING	0x01
#define	PMON_STATE_NOT_ON	0x02
#define	PMON_STATE_NOT_PENDING	0x04
#define	PMON_STATE_CNT_RISING	0x10
#define	PMON_STATE_CNT_ON	0x20
#define	PMON_STATE_CMP_ON	0x40
#define	PMON_STATE_CMP_PENDING	0x80

#define	TheNode		((PicOSNode*)TheStation)
#define	ThePckt		((PKT*)ThePacket)

#define	MAX_LINE_LENGTH	63	// For Inserial

int zz_running (void*);
int zz_killall (void*);

extern	const char zz_hex_enc_table [];

void	syserror (int, const char*);

struct mem_chunk_struct	{

	struct	mem_chunk_struct	*Next;

	address	PTR;		// The address
	word	Size;		// The simulated size in full words
};

typedef	struct mem_chunk_struct	MemChunk;

typedef	struct {
/*
 * The UART stuff. Instead of having all these as individual node attributes,
 * we create a structure encapsulating them. This is because most nodes will
 * have no UARTs, and the attributes would be mostly wasted.
 */
	UART	*U;
	char	*__inpline;
	Process	*pcsInserial, *pcsOutserial;
} uart_t;

packet	PKT {

	word	*Payload;
	word	PaySize;

	void load (word *pay, int paysize) {
		// Note that paysize is in bytes and must be even. This is
		// called just before transmission.
		Payload = new word [(PaySize = paysize)/2];
		memcpy (Payload, pay, paysize);
		// Assuming the checksum falls into the payload (but excluding
		// the preamble). This is in bits, according to the rules of
		// SMURPH. Perhaps some day we will take advantage of some of
		// its statistics collection tools?
		ILength = TLength = (paysize << 3);
	};
};

station PicOSNode {

	void		_da (phys_dm2200) (int, int);
	void		_da (phys_cc1100) (int, int);
	void		phys_rfmodule_init (int);

	Mailbox	TB;		// For trigger

	/*
	 * Defaults needed for reset
	 */
	double		_da (DefXPower), _da (DefRPower);

	/*
	 * Memory allocator
	 */
	MemChunk	*MHead, *MTail;
	word		MTotal, MFree,
			NFree;		// Minimum free so far - for stats
	/*
	 * RF interface
	 */
	PKT		_da (OBuffer);	// Output buffer
	Transceiver	*_da (RFInterface);

	/*
	 * RF interface component. We may want to modify it later, if it turns
	 * out to be dependent on RFModule.
	 */
	Boolean		_da (Receiving), _da (Xmitting),
			_da (TXOFF), _da (RXOFF);
	int		_da (tx_event);
	lword		_da (entropy);
	word		_da (statid);		// Station/network ID
	word		_da (min_backoff), _da (max_backoff), _da (backoff);
	word		_da (lbt_delay);
	double		_da (lbt_threshold);
	
	/*
	 * This is NULL if the node has no UART
	 */
	uart_t		*uart;

	/*
	 * Pins
	 */
	PINS		*pins;

	/*
	 * Leds
	 */
	LEDSM		*ledsm;

	/*
	 * This is EEPROM and FIM (IFLASH)
	 */
	NVRAM		*eeprom, *iflash;

	void _da (diag) (const char*, ...);
	void reset ();
	int _da (getpid) () { return __cpint (TheProcess); };
	lword _da (seconds) ();
	address	memAlloc (int, word);
	void memFree (address);
	word _da (actsize) (address);
	Boolean memBook (word);
	void memUnBook (word);
	word memfree (int pool, word *faults);
	inline void waitMem (int state) { TB.wait (N_MEMEVENT, state); };
	inline void _da (delay) (word msec, int state) {
		Timer->delay (msec * MILLISECOND, state);
	};
	inline void _da (when) (int ev, int state) { TB.wait (ev, state); };
	inline void _da (gbackoff) () {
		_da (backoff) = _da (min_backoff) + toss (_da (max_backoff));
	};

	inline void _da (leds) (word led, word op) {
		if (ledsm != NULL)
			// Ignore otherwise
			ledsm->leds_op (led, op);
	};

	inline void _da (fastblink) (Boolean a) {
		if (ledsm != NULL)
			ledsm->fastblink (a);
	};

	inline int _da (io) (int state, int dev, int ope, char *buf, int len) {
		// Note: 'dev' is ignored: it exists for compatibility with
		// PicOS; io only works for the (single) UART.
		assert (uart != NULL, "PicOSNode->io: node %s has no UART",
			getSName ());
		return uart->U->ioop (state, ope, buf, len);
	};

	/*
	 * I/O formatting
	 */
	char * _da (vform) (char*, const char*, va_list);
	int    _da (vscan) (const char*, const char*, va_list);
	char * _da (form) (char*, const char*, ...);
	int    _da (scan) (const char*, const char*, ...);
	int    _da (ser_out) (word, const char*);
	int    _da (ser_in) (word, char*, int);
	int    _da (ser_outf) (word, const char*, ...);
	int    _da (ser_inf) (word, const char*, ...);

	/*
	 * Operations on pins
	 */
	void no_pin_module (const char*);

	inline word  _da (pin_read) (word pn) {
		if (pins == NULL)
			no_pin_module ("pin_read");
		return pins->pin_read (pn);
	};

	inline int   _da (pin_write) (word pn, word val) {
		if (pins == NULL)
			no_pin_module ("pin_write");
		return pins->pin_write (pn, val);
	};

	inline int   _da (pin_read_adc) (word st, word pn, word ref, word smt) {
		if (pins == NULL)
			no_pin_module ("pin_read_adc");
		return pins->pin_read_adc (st, pn, ref, smt);
	};

	inline int   _da (pin_write_dac) (word pn, word val, word ref) {
		if (pins == NULL)
			no_pin_module ("pin_write_dac");
		return pins->pin_write_dac (pn, val, ref);
	};

	// The pulse monitor

	inline void  _da (pmon_start_cnt) (long cnt, Boolean edge) {
		if (pins == NULL)
			no_pin_module ("pmon_start_cnt");
		pins->pmon_start_cnt (cnt, edge);
	};

	inline void  _da (pmon_stop_cnt) () {
		if (pins == NULL)
			no_pin_module ("pmon_stop_cnt");
		pins->pmon_stop_cnt ();
	};

	inline void  _da (pmon_set_cmp) (long cnt) {
		if (pins == NULL)
			no_pin_module ("pmon_set_cmp");
		pins->pmon_set_cmp (cnt);
	};

	inline lword _da (pmon_get_cnt) () {
		if (pins == NULL)
			no_pin_module ("pmon_get_cnt");
		return pins->pmon_get_cnt ();
	};

	inline lword _da (pmon_get_cmp) () {
		if (pins == NULL)
			no_pin_module ("pmon_get_cmp");
		return pins->pmon_get_cmp ();
	};

	inline void  _da (pmon_start_not) (Boolean edge) {
		if (pins == NULL)
			no_pin_module ("pmon_start_not");
		pins->pmon_start_not (edge);
	};

	inline void  _da (pmon_stop_not) () {
		if (pins == NULL)
			no_pin_module ("pmon_stop_not");
		pins->pmon_stop_not ();
	};

	inline word  _da (pmon_get_state) () {
		if (pins == NULL)
			no_pin_module ("pmon_get_state");
		return pins->pmon_get_state ();
	};

	inline Boolean  _da (pmon_pending_not) () {
		if (pins == NULL)
			no_pin_module ("pmon_pending_not");
		return pins->pmon_pending_not ();
	};

	inline Boolean  _da (pmon_pending_cmp) () {
		if (pins == NULL)
			no_pin_module ("pmon_pending_cmp");
		return pins->pmon_pending_cmp ();
	};

	inline void  _da (pmon_dec_cnt) () {
		if (pins == NULL)
			no_pin_module ("pmon_dec_cnt");
		pins->pmon_dec_cnt ();
	};

	inline void  _da (pmon_sub_cnt) (long decr) {
		if (pins == NULL)
			no_pin_module ("pmon_sub_cnt");
		pins->pmon_sub_cnt (decr);
	};

	inline void  _da (pmon_add_cmp) (long incr) {
		if (pins == NULL)
			no_pin_module ("pmon_add_cmp");
		pins->pmon_add_cmp (incr);
	};

	/*
	 * EEPROM + FIM (IFLASH)
	 */
	word _da (ee_read)  (lword, byte*, word);
	word _da (ee_erase) (word, lword, lword);
	word _da (ee_write) (word, lword, const byte*, word);
	word _da (ee_sync) (word);
	int  _da (if_write) (word, word);
	word _da (if_read)  (word);
	void _da (if_erase) (int);

#include "encrypt.h"
	// Note: static TCV data is initialized in tcv_init.
#include "tcv_node_data.h"

	void setup (data_no_t*);

	IPointer preinit (const char*);
};

process Inserial (PicOSNode) {

	uart_t *uart;
	char *tmp, *ptr;
	int len;

	states { IM_INIT, IM_READ, IM_BIN, IM_BIN1 };

	void setup ();
	void close ();

	perform;
};

process Outserial (PicOSNode) {

	uart_t	*uart;
	const char *data, *ptr;
	int len;

	states { OM_INIT, OM_WRITE, OM_RETRY };

	void setup (const char*);
	void close ();

	perform;
};

station NNode : PicOSNode {
/*
 * A node equipped with NULL plugin
 */
#include "plug_null_node_data.h"

	void setup ();
	void reset ();
};

station TNode : PicOSNode {
/*
 * A node equipped with TARP stuff
 */
	// Application-level parameters for TARP
	virtual int _da (tr_offset) (headerType *mb) {
		excptn ("TNode->tr_offset undefined");
	};
	virtual Boolean _da (msg_isBind) (msg_t m) {
		excptn ("TNode->msg_isBind undefined");
	};
	virtual Boolean _da (msg_isTrace) (msg_t m) {
		excptn ("TNode->msg_isTrace undefined");
	};
	virtual Boolean _da (msg_isMaster) (msg_t m) {
		excptn ("TNode->msg_isMaster undefined");
	};
	virtual Boolean _da (msg_isNew) (msg_t m) {
		excptn ("TNode->msg_isNew undefined");
	};
	virtual Boolean _da (msg_isClear) (byte o) {
		excptn ("TNode->msg_isClear undefined");
	};
	virtual void _da (set_master_chg) () {
		excptn ("TNode->set_master_chg undefined");
	};

#include "net_node_data.h"
#include "plug_tarp_node_data.h"
#include "tarp_node_data.h"

	void setup ();
	void reset ();
};

process	BoardRoot {

	data_no_t *readNodeParams (sxml_t, int);
	data_ua_t *readUartParams (sxml_t, const char*);
	data_pn_t *readPinsParams (sxml_t, const char*);
	data_le_t *readLedsParams (sxml_t, const char*);

	void initTiming (sxml_t);
	void initChannel (sxml_t, int);
	void initNodes (sxml_t, int);
	void initRoamers (sxml_t);
	void initAll ();

	void readPreinits (sxml_t, int);
	
	virtual void buildNode (const char *tp, data_no_t *nddata) {
		excptn ("BoardRoot: buildNode undefined");
	};

	states { Start, Stop } ;

	perform;
};

process MoveHandler {

	TIME TimedRequestTime;

	union	{
		Dev	   *Agent;	// May be string
		const char *String;
	};

	int Left;
	char *BP;
	char *RBuf;
	word RBSize;
	FLAGS Flags;

	states { AckMove, Loop, ReadRq, Reply, Delay };

	void setup (Dev*, FLAGS);

	~MoveHandler ();

	perform;
};

#endif
