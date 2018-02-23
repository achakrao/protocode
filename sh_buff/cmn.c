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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>

#include "cmn.h"

const char *g_metadata_key = "0x600DD006";
const char *g_data_buff_key = "0x600DDA1A";
const char *g_rd_idx_zone_key = "0x600DDE01";
const char *g_wr_idx_zone_key = "0x600DDE02";

static void init_metadata(m_data_t *, unsigned int);
static int get_metadata(desc_data_t *, process_type_t, flag_t *);
static int setup_buffers(desc_data_t *, idx_ptr_t *, flag_t *);
static void get_role_flags(process_type_t, flag_t *);

int setup_core(desc_data_t *mdata, process_type_t role, idx_ptr_t *idx)
{
	flag_t flag;
	if (get_metadata(mdata, role, &flag)) {
		perror("consumer: can't setup metadata area");
		return -1;
	}

	if (role == D_PROCESS_ROLE_PRODUCER) {
		printf("init metadata, input buffs:%d\n", mdata->input_buffs);
		init_metadata(mdata->metadata, mdata->input_buffs);
	}

	if (setup_buffers(mdata, idx, &flag)) {
		perror("consumer: error setting up buffers");
		return -1;
	}

	/* Initialize buffers */
	if (role == D_PROCESS_ROLE_PRODUCER) {
		if (!(mdata->metadata->flag & SHM_BUFF_INIT)) {
			memset(idx->fd_buff.base, 0, idx->fd_buff.size);
			memset(idx->fd_rd.base, 0, idx->fd_rd.size);
			memset(idx->fd_wr.base, 0, idx->fd_wr.size);
			mdata->metadata->flag |= SHM_BUFF_INIT;
		}
		mdata->flag |= PRODUCER_INIT;
	} else {
		if (mdata->metadata->nbuffs) {
			mdata->flag |= CONSUMER_INIT;
		}
	}
	return 0;
}

/* Given an index i of the buffer #, return an IO construct */
void get_io(int i, const idx_ptr_t *idx, io_t *ptr)
{
	if (!ptr || !idx) {
		return;
	}
	ptr->wr = (unsigned int *) (idx->fd_wr.base + i * sizeof(unsigned int*));
	ptr->rd = (unsigned int *) (idx->fd_rd.base + i * sizeof(unsigned int*));
	ptr->buf = (unsigned char *) (idx->fd_buff.base + i * BUFFER_SIZE);
}

void cleanup(idx_ptr_t *idx, desc_data_t *data)
{
	unsigned int flag = data->metadata->flag & SHM_BUFF_INIT;

	if (data->metadata->flag & SHM_MDATA_INIT) { 
		shm_unlink((const char *)data->metadata);
		close(data->fd.fd);
	}

	if (flag) {
		shm_unlink((const char *)idx->fd_buff.base);
		close(idx->fd_buff.fd);

		shm_unlink((const char *)idx->fd_rd.base);
		close(idx->fd_rd.fd);

		shm_unlink((const char *)idx->fd_wr.base);
		close(idx->fd_wr.fd);
	}
}

void dump(const m_data_t *data, const idx_ptr_t *idx, unsigned char mode)
{
	io_t io;

	if (!data) {
		return;
	}
	memset(&io, 0, sizeof(io));
	printf("metadata dump\n");
	printf("ver: %d\n", data->ver);
	printf("nbuffs: %d\n", data->nbuffs);
	printf("guard: 0x%x 0x%x\n", data->guard[0], data->guard[1]);
	printf("flag: 0x%x\n", data->flag);
	printf("st_size: %d\n", data->st_size);

	printf("Rd indices\n");
	for (unsigned int i = 0; i < data->nbuffs; i++) {
		get_io(i, idx, &io);
		printf("[%d] rd:0x%x wr:0x%x\n", i, *io.rd, *io.wr);
	}
	if (mode == FULL_DUMP) {
		printf("bmap: \n");
		for (unsigned int i = 0; i < MAX_NO_BUFFER/sizeof(unsigned long); i++) {
			printf("wr:0x%08lx\n", data->wrt_bmap[i]);
		}
	}
}

static void get_role_flags(process_type_t role, flag_t *flags)
{
	if (role == D_PROCESS_ROLE_CONSUMER) {
		flags->buf_flag = PROT_READ;
		flags->rd_flag = PROT_READ | PROT_WRITE;
		flags->wr_flag = PROT_READ;
		flags->sflag = O_RDWR;
	} else {
		flags->buf_flag = PROT_WRITE;
		flags->rd_flag = PROT_READ | PROT_WRITE;
		flags->wr_flag = PROT_WRITE;
		flags->sflag = O_CREAT | O_RDWR;
	}
}

static int get_metadata(desc_data_t *data, process_type_t role, flag_t *flag)
{
	void *reg = NULL;

	if (!data) {
		perror("null data passed!\n");
		return -1;
	}

	get_role_flags(role, flag);
	/* setup metadata area */
	data->fd.fd = shm_open(g_metadata_key, flag->sflag, 0666);

	if (data->fd.fd == -1) {
		perror("shm_open");
		return -1;
	}

	data->fd.size = sizeof(*data->metadata);
	ftruncate(data->fd.fd, data->fd.size);

	reg = mmap(NULL, sizeof(*data->metadata), flag->buf_flag, MAP_SHARED, data->fd.fd, 0);
	if (reg == NULL) {
		perror("mmapfailure");
		return -1;
	}
	data->metadata = (m_data_t *)reg;

	return 0;
}

static int setup_mbufs(const char *key, unsigned int flag, unsigned int sflag,
	shm_hndl_t *hndl)
{
	hndl->fd = shm_open(key, sflag, 0666);
	if (hndl->fd == -1) {
		perror("cant open shm for RD indices");
		return -1;
	}
	ftruncate(hndl->fd, hndl->size);

	hndl->base = mmap(NULL, hndl->size, flag, MAP_SHARED, hndl->fd, 0);

	return 0;
}

static int setup_rdwr_indices(desc_data_t *desc, idx_ptr_t *idx_ptrs, flag_t *flag)
{
	/* setup read pointers */
	idx_ptrs->fd_rd.size = desc->metadata->nbuffs * sizeof(unsigned int);
	if (setup_mbufs(g_rd_idx_zone_key, flag->rd_flag, flag->sflag, &idx_ptrs->fd_rd)) {
		perror("error setting up read pointers");
		return -1;
	}

	/* setup write pointers */
	idx_ptrs->fd_wr.size = desc->metadata->nbuffs * sizeof(unsigned int);
	if (setup_mbufs(g_wr_idx_zone_key, flag->wr_flag, flag->sflag, &idx_ptrs->fd_wr)) {
		perror("error setting up write pointers");
		return -1;
	}
	return 0;
}

static int setup_buffers(desc_data_t *desc, idx_ptr_t *idx_ptrs, flag_t *flag)
{
	/* setup read write indices */
	if (setup_rdwr_indices(desc, idx_ptrs, flag)) {
		perror("setup_indices failed");
		cleanup(idx_ptrs, desc);
		return -1;
	}
	/* setup main data buffers */
	idx_ptrs->fd_buff.size = desc->metadata->nbuffs * BUFFER_SIZE;
	if (setup_mbufs(g_data_buff_key, flag->buf_flag, flag->sflag, &idx_ptrs->fd_buff)) {
		perror("error setting up data buffers");
		cleanup(idx_ptrs, desc);
		return -1;
	}
	/* set the segment start addresses and fill the guard area */
	return 0;
}

static void init_metadata(m_data_t *data, unsigned int nbuf)
{
	unsigned long tmp = GUARD_PATTERN;

	if (nbuf && data->nbuffs != nbuf) {
		memset(data, 0, sizeof(*data));
		data->ver = 1u;
		data->nbuffs = nbuf;
		data->st_size = SHM_METADATA_KEY_SIZE;
		memcpy(&data->guard, (void *)&tmp, sizeof(tmp));
		data->flag |= SHM_MDATA_INIT;
	}
}

/* Use 1 to set and 0 for clear */
void buf_state(m_data_t *data, int i, int set)
{
	int k = 0;
	unsigned int mask;

	k = i/(sizeof(unsigned long));
	mask = 1ul << i;
	if (set) {
		data->wrt_bmap[k] |= mask; 
	} else {
		data->wrt_bmap[k] &= ~mask;
	}
}

