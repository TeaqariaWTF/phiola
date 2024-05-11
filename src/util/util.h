/** phiola: utility functions
2023, Simon Zolin */

#include <ffbase/stringz.h>

static inline int ffbit_test_array32(const uint *ar, uint bit)
{
	return ffbit_test32(&ar[bit / 32], bit % 32);
}


/** Free aligned memory region. */
static inline void ffvec_free_align(ffvec *v)
{
	if (v->cap != 0) {
		FF_ASSERT(v->ptr != NULL);
		FF_ASSERT(v->len <= v->cap);
		ffmem_alignfree(v->ptr);
		v->cap = 0;
	}
	v->ptr = NULL;
	v->len = 0;
}

/** Allocate aligned memory region; call ffvec_free_align() to free.
WARNING: don't call ffvec_free() */
static inline void* ffvec_alloc_align(ffvec *v, ffsize n, ffsize align, ffsize elsize)
{
	ffvec_free_align(v);

	if (n == 0)
		n = 1;
	ffsize bytes;
	if (__builtin_mul_overflow(n, elsize, &bytes))
		return NULL;

	if (NULL == (v->ptr = ffmem_align(bytes, align)))
		return NULL;

	v->cap = n;
	return v->ptr;
}

#define ffvec_alloc_alignT(v, n, align, T)  ((T*)ffvec_alloc_align(v, n, align, sizeof(T)))


/** Process input string of the format "...text...$var...text...".
out: either text chunk or variable name */
static inline int ffstr_var_next(ffstr *in, ffstr *out, char c)
{
	if (in->ptr[0] != c) {
		ffssize pos = ffstr_findchar(in, c);
		if (pos < 0)
			pos = in->len;
		ffstr_set(out, in->ptr, pos);
		ffstr_shift(in, pos);
		return 't';
	}

	ffstr_shift(in, 1);
	ffssize i = ffs_skip_ranges(in->ptr, in->len, "\x30\x39\x41\x5a\x5f\x5f\x61\x7a", 8); // "0-9A-Z_a-z"
	if (i < 0)
		i = in->len;
	ffstr_set(out, in->ptr - 1, i + 1);
	ffstr_shift(in, i);
	return 'v';
}


static inline ffssize ffcharr_findsorted_padding(const void *ar, ffsize n, ffsize elsize, ffsize padding, const char *search, ffsize search_len)
{
	if (search_len > elsize)
		return -1; // the string's too large for this array

	ffsize start = 0;
	while (start != n) {
		ffsize i = start + (n - start) / 2;
		const char *ptr = (char*)ar + i * (elsize + padding);
		int r = ffmem_cmp(search, ptr, search_len);

		if (r == 0
			&& search_len != elsize
			&& ptr[search_len] != '\0')
			r = -1; // found "01" in {0,1,2}

		if (r == 0)
			return i;
		else if (r < 0)
			n = i;
		else
			start = i + 1;
	}
	return -1;
}

struct map_sz_vptr {
	char key[16];
	const void *val;
};
static inline const void* map_sz_vptr_find(const struct map_sz_vptr *m, const char *name)
{
	for (ffsize i = 0;  m[i].key[0] != '\0';  i++) {
		if (ffsz_eq(m[i].key, name))
			return m[i].val;
	}
	return NULL;
}
static inline const void* map_sz_vptr_findz2(const struct map_sz_vptr *m, ffsize n, const char *name)
{
	ffssize i = ffcharr_findsorted_padding(m, n, sizeof(m->key), sizeof(m->val), name, ffsz_len(name));
	if (i < 0)
		return NULL;
	return m[i].val;
}
static inline const void* map_sz_vptr_findstr(const struct map_sz_vptr *m, ffsize n, ffstr name)
{
	ffssize i = ffcharr_findsorted_padding(m, n, sizeof(m->key), sizeof(m->val), name.ptr, name.len);
	if (i < 0)
		return NULL;
	return m[i].val;
}


#include <ffsys/path.h>
static inline void ffpath_split3_str(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffpath_splitname_str(nm, name, ext);
}

/** Treat ".ext" file name as just an extension without a name */
static inline void ffpath_split3_output(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffstr_rsplitby(&nm, '.', name, ext);
}

/** Replace uncommon/invalid characters in file name component
Return N bytes written */
static inline ffsize ffpath_makefn(char *dst, ffsize dstcap, ffstr src, char replace_char, const uint *char_bitmask)
{
	ffstr_skipchar(&src, ' ');
	ffstr_rskipchar(&src, ' ');
	ffsize len = ffmin(src.len, dstcap);

	for (ffsize i = 0;  i < len;  i++) {
		dst[i] = src.ptr[i];
		if (!ffbit_test_array32(char_bitmask, (ffbyte)src.ptr[i]))
			dst[i] = replace_char;
	}
	return len;
}


#define samples_to_msec(samples, rate)   ((uint64)(samples) * 1000 / (rate))
#define msec_to_samples(time_ms, rate)   ((uint64)(time_ms) * (rate) / 1000)

/** bits per sample */
#define pcm_bits(format)  ((format) & 0xff)

/** Get size of 1 sample (in bytes). */
#define pcm_size(format, channels)  (pcm_bits(format) / 8 * (channels))
#define pcm_size1(f)  pcm_size((f)->format, (f)->channels)

#define phi_af_size(af)  (((af)->format & 0xff) / 8 * (af)->channels)
#define phi_af_bits(af)  ((af)->format & 0xff)

static inline void phi_af_update(struct phi_af *dst, const struct phi_af *src)
{
	if (src->format)
		dst->format = src->format;
	if (src->rate)
		dst->rate = src->rate;
	if (src->channels)
		dst->channels = src->channels;
}
