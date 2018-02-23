/* Producer code
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmn.h"

static int write_data(m_data_t *, io_t *, const char *, unsigned int);
static int setup_io(desc_data_t *, idx_ptr_t *, const char *);
static const char* get_arguments(int, desc_data_t *, const char **);
static int consumer_reading(m_data_t *, idx_ptr_t *);

int main(int nargs, const char **argv)
{
	const char *file_name;
	idx_ptr_t idx;
	desc_data_t mdata;

	memset(&mdata, 0, sizeof(mdata));
	memset(&idx, 0, sizeof(idx));

	/* Get program arguments */
	file_name = get_arguments(nargs, &mdata, argv);
	if (file_name == NULL) {
		printf("Usage: producer <no_of_buffs> <input text file>\n");
		exit(0);
	}

	/* Setup core infrastructure */
	if (setup_core(&mdata, D_PROCESS_ROLE_PRODUCER, &idx)) {
		exit(1);
	}

	if (mdata.metadata->nbuffs == 0) {
		perror("no buffer exists, restart with a valid number of buffers");
		exit(0);
	}

	/* setup IO */
	if (setup_io(&mdata, &idx, file_name)) {
		dump(mdata.metadata, &idx, FULL_DUMP);
	}
	/* Check if consumer has finished processing all data */
	while (consumer_reading(mdata.metadata, &idx)) {
		sleep(SLEEP_TIME);
	}

	cleanup(&idx, &mdata);
    exit(0);
}

static int setup_io(desc_data_t *mdata, idx_ptr_t *idx, const char *file_n)
{
	char *input;
	size_t line_len = 0;
	io_t *io = NULL;
	int ret = 0;

	io = malloc(mdata->metadata->nbuffs * sizeof(*io));
	if (!io) {
		perror("malloc failuer in io construct creation");
		return -1;
	}

	for (unsigned int i = 0; i < mdata->metadata->nbuffs; i++) {
		get_io(i, idx, &io[i]);
	}

	input = malloc(BUFFER_SIZE * sizeof(char));
	if (!input) {
		free(io);
		return -1;
	}
	FILE *fp = fopen(file_n, "r");
	if (!fp) {
		ret = -1;
		goto err;
	}
	/* get user input and write to buffer */
	while (getline(&input, &line_len, fp) != -1) {
		printf("Data write[%lu] bytes: %s\n", strlen(input) + 1, input);
		if (write_data(mdata->metadata, io, input, strlen(input) + 1)) {
			break;
		}
	}
	fclose(fp);
err:
	free(input);
	free(io);

	return ret;
}

static const char* get_arguments(int nargs, desc_data_t *mdata, const char **args)
{
	char *str = NULL;
	if (nargs == 3) {
		mdata->input_buffs = strtoul(args[1], &str, 10);
		if (mdata->input_buffs > MAX_NO_BUFFER) {
			printf("invalid number of buffers, %d\n", mdata->input_buffs);
			mdata->input_buffs = 0;
		}
		return args[2];
	}
	return NULL;
}

/* write data to a buffer */
static inline void write_buff(int i, io_t *io, const char *str,
	m_data_t *mdata, unsigned int len)
{
	memcpy((io->buf + *io->wr), &len, mdata->st_size);
	*io->wr += mdata->st_size;
	memcpy((io->buf + *io->wr), str, len);
	*io->wr += len;

	buf_state(mdata, i, 1);
}

/* find space for the given write size, 'sz' in a buffer */
static unsigned int find_room(io_t *io, unsigned int sz)
{
	unsigned int len = 0;

	if (*io->wr >= *io->rd) {
		len = BUFFER_SIZE - *io->wr;
	} else {
		len = *io->rd - *io->wr;
	}
	if (len > sz) {
		return 1;
	}
	return 0;
}

/* Find a buffer to write */
static int find_buffer(io_t *io, m_data_t *mdata, unsigned int sz)
{
	unsigned int i = 0;

	for (i = 0; i < mdata->nbuffs; i++) {
		if (find_room(&io[i], sz + mdata->st_size)) {
			break;
		}
	}
	if (i == mdata->nbuffs) {
		return -1;
	}
	return i;
}

/* write data input  */
static int write_data(m_data_t *mdata, io_t *io, const char *str, unsigned int len)
{
	int ret = 0;
	int i = find_buffer(io, mdata, len);

	if (i >= 0) {
		write_buff(i, &io[i], str, mdata, len);
		if (*io[i].wr == BUFFER_SIZE) {
		/* Reached end, roll over */
			*io[i].wr = 0;
		} else if (*io[i].wr == *io[i].rd && *io[i].rd != 0) {
			buf_state(mdata, i, 0);
		}
	} else {
		printf("coulnd't find a buffer to fit the string\n");
		ret = -1;
	}
	return ret;
}

static int consumer_reading(m_data_t *mdata, idx_ptr_t *ptr)
{
	io_t io;
	int cnt = 0;

	memset(&io, 0, sizeof(io));
	for (unsigned int i = 0; i < mdata->nbuffs; i++) {
		get_io(i, ptr, &io);
		if (*io.rd != *io.wr) {
			cnt++;
		}
	}
	return cnt;
}
