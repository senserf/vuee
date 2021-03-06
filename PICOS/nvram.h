/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#ifndef	__picos_nvram_h__
#define	__picos_nvram_h__

typedef struct {
	lword start, length;
	byte *ptr;
} nvram_chunk_t;

typedef	struct {
	TIME	UnHang;
	double	Bounds [EP_N_BOUNDS];
} nvram_timing_t;

#define	NVRAM_TYPE_NOOVER	0x01		// Writing over zero is void
#define	NVRAM_TYPE_ERPAGE	0x02		// Erase by pages (blocks)
#define	NVRAM_FLAG_FIMAGE	0x04		// File image
#define	NVRAM_FLAG_WEHANG	0x80000000	// Write delayed
#define	NVRAM_FLAG_UNSNCD	0x40000000	// Unsynced

#define	NVRAM_NOFR_EPINIT	0x10		// Do not deallocate inits
#define	NVRAM_NOFR_IFINIT	0x20

#define	NVRAM_FTIME_READ	0		// Indices of timing params
#define	NVRAM_FTIME_WRITE	2
#define	NVRAM_FTIME_ERASE	4
#define	NVRAM_FTIME_SYNC	6

class NVRAM {
	
	lword tsize;		// Total size in bytes
	lword esize;		// Current formal size of chunks
	lword asize;		// Number of valid entries in chunks
	lword pmask;		// Page mask (needed for erase with argument)

	byte clean;		// The erase code (ff, 00, ...)

	nvram_timing_t	*ftimes;

	FLAGS	TP;		// Type flags, e.g., writing 1 to 0 is void

	nvram_chunk_t	*chunks;

	void merge (byte*, const byte*, lword len);
	void flush (const byte*, lword len);
	void fetch (byte*, lword len);
	void empty (lword len);
	void preset (const data_epini_t*);
	void grow ();
	void timed (word, lword, int, word);

	inline double otime (int tp, lword len) {
		// Operation timing
		return (ftimes == NULL) ? 0.0 :
			dRndUniform (ftimes->Bounds [tp],
				     ftimes->Bounds [tp + 1])
			* (double) len;
	};

	public:

#if 0
	void dump ();
#endif
	lword size (Boolean*, lword*);
	word nvopen ();
	void nvclose ();
	word get (lword, byte*, lword);
	word put (word, lword, const byte*, lword);
	word erase (word, lword, lword);
	word sync (word);
	NVRAM (lword, lword, FLAGS, byte, double*, const data_epini_t*,
		const char*);
	~NVRAM ();
};

#endif
