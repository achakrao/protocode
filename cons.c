/* Consumer code
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmn.h"

static int read_buff(io_t *, const char *, unsigned int);
static void core_read(m_data_t *, idx_ptr_t *, const char *);
static int search_string(char *, const char *);

int main(int nargs, char **args)
{
	char *pattern = NULL;
	desc_data_t mdata;
	idx_ptr_t idxs;

	memset(&mdata, 0, sizeof(mdata));
	memset(&idxs, 0, sizeof(idxs));

	if (nargs > 1) {
		pattern = args[1];
	} else {
		printf("Usage: consumer <\"substring to search\">\n");
		exit(0);
	}	
	if (setup_core(&mdata, D_PROCESS_ROLE_CONSUMER, &idxs)) {
		dump(mdata.metadata, &idxs, MINI_DUMP);
		exit(1);
	}
	while(1) {
		/* Read from buffer */
		core_read(mdata.metadata, &idxs, pattern);
	}
	printf("All buffers are empty, bailing out\n");
	if (mdata.flag & CONSUMER_INIT) {
		cleanup(&idxs, &mdata);
	}
    exit(0);
}

/* validate before reading */
static int valid_rd_buff(const io_t *io, char *str, const char* pattern,
	unsigned int st_size)
{
	unsigned int len = 0;
	char *ptr = NULL;

	/* read the length of the sentence */
	len = *(unsigned int*)str;
	if (len + *io->rd <= BUFFER_SIZE) {
		ptr = str + st_size;
		printf("Data Read [%d] bytes: %s\n", len, ptr);
		/* search for a pattern */
		if (!search_string(ptr, pattern)) {
			printf("%s\n", ptr);
		}
		/* move rd pointer */
		*io->rd += len + st_size;
	} else {
		len = 0;
	}
	return len;
}

/* Read from a buffer */
static int read_buff(io_t *io, const char *pattern, unsigned int st_size)
{
	char str[BUFFER_SIZE];
	char *ptr = NULL;
	int len = 0, buf_len = 0, mv = 0;

	memset(str, 0, sizeof(str));
	buf_len = *io->wr - *io->rd;

	if (buf_len > 0 && buf_len < BUFFER_SIZE) {
		memcpy(str, (io->buf + *io->rd), buf_len);
	} else if (buf_len < 0) {
		/* Copy till end of buffer */
		memcpy(str, (io->buf + *io->rd), BUFFER_SIZE - *io->rd);	
		buf_len = BUFFER_SIZE - *io->rd;
	}
	ptr = str;
	mv = buf_len;
	while (buf_len > 0) {
		if (!(len = valid_rd_buff(io, ptr, pattern, st_size))) {
			break;
		}
		buf_len -= len + (int)st_size;
		ptr += len + (int)st_size;
	}
	return mv;
}

static void core_read(m_data_t *mdata, idx_ptr_t *idxs, const char *pattern)
{
	int i = 0;
	unsigned int k = 0;
	io_t io;

	memset(&io, 0, sizeof(io));

	for (k = 0; k < MAX_NO_BUFFER/sizeof(unsigned long); k++) {
		unsigned long word = mdata->wrt_bmap[k];
		while ((i = ffsl(word)) > 0) {
			get_io((i - 1), idxs, &io);
			if (!read_buff(&io, pattern, mdata->st_size)) {
				/* Reached end, roll over */
				if (*io.rd == BUFFER_SIZE || *io.wr < *io.rd) {
					*io.rd = 0;
				}
				/* nothing to read from this buffer */
				word &= ~(1ul << (i - 1));
			}
			if (!word) {
				break;
			}
		}
	}
}

static int search_string(char *str, const char *pattern)
{
    unsigned int cnt, i = 0;
    
    char *tokens[64];
    char *tok = NULL;
	char *next = NULL;

	memset(tokens, 0, sizeof(tokens));
    if (strlen(pattern) > strlen(str)) {
        return -1;
    }

	/* tokenize the pattern string */
	tok = strtok((char *) pattern, " ");
	while (tok != NULL) {
        tokens[i++] = tok;
		tok = strtok(NULL, " ");
    }
    cnt = i;
    i = 0;

	/* find the starting point in the searched string */
    if ((tok = strstr(str, tokens[i]))) {
		tok += strlen(tokens[i]);
		while (i < cnt) {
			next = strtok(tok, " ");
			while (next != NULL) {
				next = strtok(NULL, " ");
				i++;
				if (i == cnt) {
					break;
				}
				if (!next) {
					return (cnt - i);
				}
				if (strncmp(next, tokens[i], strlen(tokens[i]))) {
					return (cnt - i);
				}
			}
		}
    }
    return (cnt -i);
}
