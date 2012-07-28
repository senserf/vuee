#ifndef __rfmodule_dm2200_h__
#define	__rfmodule_dm2200_h__

#include "board.h"

#define	rfi	(rf->RFInterface)
#define	mxpl	(rf->MaxPL)
#define	xbf	(rf->__pi_x_buffer)
#define	rbf	(rf->__pi_r_buffer)
#define	obf	(rf->OBuffer)
#define	bkf	(rf->backoff)
#define	txe	(rf->tx_event)
#define	rxe	rf
#define	rxoff	(rf->RXOFF)
#define	sid	(rf->statid)
#define	minbkf	(rf->min_backoff)
#define	maxbkf	(rf->max_backoff)
#define	lbtth	(rf->lbt_threshold)
#define	lbtdl	(rf->lbt_delay)
#define	xmtg	(rf->Xmitting)
#define	rcvg	(rf->Receiving)
#define	defxp	(rf->DefXPower)
#define	defrt	(rf->DefRate)
#define	defch	(rf->DefChannel)
#define	physid	(rf->phys_id)
#define	rerr	(rf->rerror)
#define	retr	(rf->retrcnt)

process Receiver : _PP_ (PicOSNode) {

	rfm_intd_t *rf;

	states { RCV_GETIT, RCV_START, RCV_RECEIVE, RCV_GOTIT };

	byte get_rssi (byte&);

	perform;

	void setup () {
		rf = TheNode->RFInt;
	};
};

process ADC (PicOSNode) {
//
// This one is not _PP_
//
	double		ATime,		// Accumulated sampling time
			Average,	// Average signal so far
			CLevel;		// Current (last) signal level
	TIME		Last;		// Last sample time
	rfm_intd_t 	*rf;

	double sigLevel () {

		double DT, NA, res;

		DT = (double)(Time - Last);
		NA = ATime + DT;
		res = ((Average * ATime) / NA) + (CLevel * DT) / NA;
// diag ("ADC: %d / %g / %g", S->getId (), res, S->lbt_threshold);
		return res;
	};

	states { ADC_WAIT, ADC_RESUME, ADC_UPDATE, ADC_STOP };

	perform;

	void setup () {
		rf = TheNode->RFInt;
	};

};

process Xmitter : _PP_ (PicOSNode) {

	int		buflen;
	ADC		*RSSI;
	rfm_intd_t	*rf;

	states { XM_LOOP, XM_TXDONE, XM_LBS };

	perform;

	void setup () {
		rf = TheNode->RFInt;
		xbf = NULL;
		RSSI = create ADC;
	};

	inline void gbackoff () {
		bkf = minbkf + toss (maxbkf);
	};

	inline void pwr_on () {
		TheNode->pwrt_change (PWRT_RADIO, 
			rxoff ? PWRT_RADIO_XMT : PWRT_RADIO_XCV);
	};

	inline void pwr_off () {
		TheNode->pwrt_change (PWRT_RADIO, 
			rxoff ? PWRT_RADIO_OFF : PWRT_RADIO_RCV);
	};

	// Copied almost directly from PICOS; will be optimized out if the body
	// is empty
	inline void set_congestion_indicator (word v) {

#if (RADIO_OPTIONS & 0x04)

		if ((rerr [RERR_CONG] = (rerr [RERR_CONG] * 3 + v) >> 2) >
		    0x0fff)
			rerr [RERR_CONG] = 0xfff;

		if (v) {
			if (rerr [RERR_CURB] + v < rerr [RERR_CURB])
				// Overflow
				rerr [RERR_CURB] = 0xffff;
			else
				rerr [RERR_CURB] += v;
		} else {
			// Update max
			if (rerr [RERR_MAXB] < rerr [RERR_CURB])
				rerr [RERR_MAXB] = rerr [RERR_CURB];
			rerr [RERR_CURB] = 0;
		}
#endif
	};
};

#endif
