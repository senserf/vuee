#ifndef	__picos_rfmattr_h__
#define __picos_rfmattr_h__

// Attribute conversion for RF module drivers

#define	OBuffer			_dac (PicOSNode, OBuffer)
#define	RFInterface		_dac (PicOSNode, RFInterface)
#define	Receiving		_dac (PicOSNode, Receiving)
#define	TXOFF			_dac (PicOSNode, TXOFF)
#define	RXOFF			_dac (PicOSNode, RXOFF)
#define	DefXPower		_dac (PicOSNode, DefXPower)
#define	DefRPower		_dac (PicOSNode, DefRPower)
#define	DefRate			_dac (PicOSNode, DefRate)
#define	DefChannel		_dac (PicOSNode, DefChannel)
#define	Xmitting		_dac (PicOSNode, Xmitting)
#define	tx_event		_dac (PicOSNode, tx_event)
#define	zzx_buffer		_dac (PicOSNode, zzx_buffer)
#define	zzr_buffer		_dac (PicOSNode, zzr_buffer)
#define	statid			_dac (PicOSNode, statid)
#define	gbackoff()		(  ((PicOSNode*)TheStation)->_na_gbackoff ()  )
#define	setrfpowr(a)		(  ((PicOSNode*)TheStation)->_na_setrfpowr (a) )
#define	setrfrate(a)		(  ((PicOSNode*)TheStation)->_na_setrfrate (a) )
#define	setrfchan(a)		(  ((PicOSNode*)TheStation)->_na_setrfchan (a) )
#define	rx_event		RFInterface

#endif
