/*
MIT License

Copyright (c) 2018 Anirban Chakraborty

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef _CMN_H_
#define _CMN_H_

#define SHM_METADATA_GUARD_SIZE	2u
#define SHM_METADATA_IDX_SIZE	4u
#define SHM_METADATA_KEY_SIZE	4u
#define MAX_NO_BUFFER	1024u
#define BUFFER_SIZE		1024
#define	GUARD_PATTERN	0xDEADDEAD
#define	SLEEP_TIME	1u

#define SHM_MDATA_INIT	1u
#define SHM_BUFF_INIT	(1u << 1)

#define	PRODUCER_INIT	1
#define	CONSUMER_INIT	2

#define	MINI_DUMP	0
#define	FULL_DUMP	1

typedef struct {
	unsigned int ver;
	unsigned int nbuffs;
	unsigned int guard[SHM_METADATA_GUARD_SIZE];
	unsigned int flag;
	unsigned int st_size;
	unsigned long wrt_bmap[MAX_NO_BUFFER/sizeof(unsigned long)];
} m_data_t;

typedef struct {
	void *base;
	int fd;
	unsigned int size;
} shm_hndl_t;

typedef struct {
	shm_hndl_t fd_rd; /* shm handle for buffer wr pointers */
	shm_hndl_t fd_wr; /* shm handle for buffer wr pointers */
	shm_hndl_t fd_buff; /* shm handle for main data bufferrs */
} idx_ptr_t;

typedef struct {
	unsigned int buf_flag;
	unsigned int rd_flag;
	unsigned int wr_flag;
	unsigned int sflag;
} flag_t;

typedef struct {
	m_data_t *metadata;
	unsigned int input_buffs;
	unsigned int flag;
	shm_hndl_t fd; /* shm handle for metadata */
} desc_data_t;

typedef struct {
	unsigned int *rd;
	unsigned int *wr;
	unsigned char *buf;
} io_t;

typedef enum {
	D_PROCESS_ROLE_CONSUMER = 0,
	D_PROCESS_ROLE_PRODUCER
} process_type_t;

/* Setup core functionality */
int setup_core(desc_data_t *mdata, process_type_t role, idx_ptr_t *idx);
/* Get an io construct to do IO */
void get_io(int, const idx_ptr_t *, io_t *);
/* Dump metadata */
void dump(const m_data_t *, const idx_ptr_t *, unsigned char);
/* Cleanup everything */
void cleanup(idx_ptr_t *, desc_data_t *);
/* set buffer state */
void buf_state(m_data_t *data, int i, int set);
#endif
