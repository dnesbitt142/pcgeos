/*
 * OpenSpellGEOS exact spelling backend, Step 2.
 *
 * Exact OSGDIC1 dictionary membership plus small edit-distance suggestions.
 * Dictionary opening uses the no-argument OSGDICTOPEN ASM helper so Watcom C
 * does not pass filename pointers into the old Borland/Pascal Spell wrappers.
 *
 * Suggestions are stored in ICB_altList/ICB_correctPtr and retrieved by the
 * patched ICGetAlternate in spell.asm.
 */

#ifndef OSGSPELL_HOST
# include <geos.h>
# include <heap.h>
# include <file.h>
# define FAR _far
#else
# define FAR
# define _pascal
#endif

#ifndef NULL
# define NULL ((void *)0)
#endif

typedef unsigned char  OSG_u8;
typedef unsigned short OSG_u16;
typedef unsigned long  OSG_u32;

#define OSG_DICT_MAGIC0 'O'
#define OSG_DICT_MAGIC1 'S'
#define OSG_DICT_MAGIC2 'G'
#define OSG_DICT_MAGIC3 'D'
#define OSG_DICT_MAGIC4 'I'
#define OSG_DICT_MAGIC5 'C'
#define OSG_DICT_MAGIC6 '1'
#define OSG_DICT_MAGIC7 0

#define OSG_HEADER_SIZE       24UL
#define OSG_BUCKET_COUNT      256U
#define OSG_BUCKET_TABLE_OFF  24UL
#define OSG_BUCKET_REC_SIZE   8UL
#define OSG_READ_CHUNK        512U
#define OSG_MAX_WORD          64U
#define OSG_MAX_SUGGESTIONS   5U
#define OSG_MAX_EDIT_DISTANCE 3U

/*
 * Diagnostic / bring-up mode.
 * 0 = force all words found
 * 1 = force all words not found, no suggestions
 * 2 = real exact dictionary lookup
 * 3 = dictionary path diagnostic: type pub/user/top/abs
 */
#ifndef OSG_SPELL_MODE
#define OSG_SPELL_MODE 2
#endif

/* Values from Include/Internal/spelllib.def. */
#define IC_RET_OK             0
#define IC_RET_ERR            8
#define IC_RET_FOUND          10
#define IC_RET_NOT_FOUND      11
#define IC_RET_NOMEM          15
#define IC_RET_NO_OPEN        16
#define IC_RET_NO_USER_DICT   21

/* Values from Include/sllang.def. */
#define SL_ENGLISH            16
#define LD_AMERICAN           128

/* SpellTask values from Include/Internal/spelllib.def. */
#define ST_VERIFY             3
#define ST_CORRECT            100

/* Offsets in Include/Internal/icbuff.def for the SBCS spell library build.
 * We use byte offsets instead of a C struct to avoid compiler packing drift
 * across the historical HighC/BorlandC/Watcom targets.
 */
#define ICB_TASK              0
#define ICB_LANGUAGE          2
#define ICB_DIALECT           3
#define ICB_MASTER_FNAME      5
#define ICB_INIT_FLAGS        70
#define ICB_PROCESS_FLAGS     72
#define ICB_ERROR_FLAGS       74
#define ICB_ERROR_FLAGS_HIGH  76
#define ICB_RET_CODE          78
#define ICB_HYPMAP            84
#define ICB_ALTMAP            92
#define ICB_WORD              101
#define ICB_ALT_WORD          166
#define ICB_PREV_WORD         259
#define ICB_LEN               324
#define ICB_LSIDE             326
#define ICB_RSIDE             328
#define ICB_SLASH_MAP         330
#define ICB_HYPHEN_MAP        338
#define ICB_EMDASH_MAP        346
#define ICB_ELDASH_MAP        354
#define ICB_NUM_ALTS          362
#define ICB_NEXT_ALT          364
#define ICB_ALT_LIST          366
#define ICB_CORRECT_PTR       566

#ifdef OSGSPELL_HOST
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
const char *host_dict_dir = ".";
FILE *host_files[16];
int GEOSsetDictPath(void) { return -1; }
int GEOSfileOpen(char FAR *fname, int flags) {
    char path[512];
    int i;
    (void)flags;
    snprintf(path, sizeof(path), "%s/%s", host_dict_dir, fname);
    for (i = 1; i < 16; ++i) {
        if (!host_files[i]) {
            host_files[i] = fopen(path, "rb");
            return host_files[i] ? i : 0;
        }
    }
    return 0;
}
int GEOSopen(char FAR *fname, int flags) { int h = GEOSfileOpen(fname, flags); return h ? h : -1; }
long GEOSlseek(int h, OSG_u32 off, int where) {
    int origin = where == 0 ? SEEK_SET : (where == 1 ? SEEK_CUR : SEEK_END);
    if (!host_files[h] || fseek(host_files[h], (long)off, origin) != 0) return -1L;
    return ftell(host_files[h]);
}
int GEOSfarread(int h, void FAR *buf, OSG_u16 n) {
    if (!host_files[h]) return -1;
    return (int)fread(buf, 1, n, host_files[h]);
}
int OSGclose(int h) { if (host_files[h]) fclose(host_files[h]); host_files[h] = 0; return 0; }
#else
/*
 * The spell library ASM is assembled by this tree with -D__BORLANDC__ even
 * when C sources are compiled by Open Watcom.  geos_asmcalls.asm therefore
 * exports Borland-style uppercase far symbols (GEOSLSEEK, GEOSFARREAD, ...),
 * and icsManager.asm declares ICGEOSPL*, not mixed-case ICGEOSpl*.
 *
 * Use the uppercase identifiers directly.  This avoids relying on compiler-
 * specific object-name pragmas, which were not taking effect in glue.
 */
/*
 * Do not call the historical GEOS* ASM file wrappers from Watcom C.
 * Those wrappers are assembled as Borland/Pascal entry points in this build,
 * while geos.h defines Watcom _pascal with reversed parameters.  Calling them
 * directly corrupts their argument view and trips EC checks such as
 * BAD_GEOS_LSEEK_FLAGS.  Use the normal GEOS C file APIs from file.h instead.
 */
#define ICGEOSplInitICBuff    ICGEOSPLINITICBUFF
#define ICGEOSplExitICBuff    ICGEOSPLEXITICBUFF
#define ICGEOSplExit          ICGEOSPLEXIT
#define ICGEOSpl              ICGEOSPL
#define ICGEOGetAlternate     ICGEOGETALTERNATE
#define IPGEOAddUser          IPGEOADDUSER
#define IPGEODeleteUser       IPGEODELETEUSER
#define IPGEOBuildUserList    IPGEOBUILDUSERLIST
#define UpdateUserDictionary  UPDATEUSERDICTIONARY
#define ICGEOIgnoreString     ICGEOIGNORESTRING
#define ICResetIgnoreUserDict ICRESETIGNOREUSERDICT

#endif

#ifndef OSGSPELL_HOST
/* no-argument ASM dictionary opener from geos_asmcalls.asm */
int _pascal OSGDICTOPEN(void);
#endif

#ifndef OSGSPELL_HOST
/*
 * icsManager.asm declares the legacy spell entrypoints as living in specific
 * GEOS resources (INIT, EXIT, CODE, IPPRINT, IPCODE).  Open Watcom otherwise
 * emits them into osgspell_TEXT, and glue reports "declared in both" errors.
 * Switch the code segment before each exported group so the C definitions match
 * the ASM declarations.
 */
#ifdef __WATCOMC__
#pragma code_seg("CODE")
#endif
#endif

/*
 * In a GEOS multi-resource object, Watcom/static helper routines referenced
 * from another code resource may appear to glue as unresolved.  Keep these
 * helpers public in the GEOS build so INIT/EXIT/CODE/IPCODE references can
 * be resolved.
 */

#ifndef OSGSPELL_HOST
int osg_file_close(int h) {
    return (FileClose((FileHandle)h, FALSE) == 0) ? 0 : -1;
}

long osg_file_seek(int h, OSG_u32 off) {
    dword pos = FilePos((FileHandle)h, (dword)off, FILE_POS_START);
    return (long)pos;
}

int osg_file_read(int h, void FAR *buf, OSG_u16 n) {
    /* FileRead returns the number of bytes read; ThreadGetError carries the
     * detailed error if one occurred.  A short read is treated as failure by
     * read_exact(), but cursor reads may legitimately return fewer bytes near
     * EOF.
     */
    return (int)FileRead((FileHandle)h, buf, n, FALSE);
}
#else
# define osg_file_close(h) OSGclose(h)
# define osg_file_seek(h, off) GEOSlseek((h), (off), 0)
# define osg_file_read(h, buf, n) GEOSfarread((h), (buf), (n))
#endif

OSG_u8 FAR *ic_bytes(void FAR *ic) { return (OSG_u8 FAR *)ic; }

void set8(void FAR *ic, OSG_u16 off, OSG_u8 v) {
    ic_bytes(ic)[off] = v;
}

void set16(void FAR *ic, OSG_u16 off, OSG_u16 v) {
    OSG_u8 FAR *p = ic_bytes(ic) + off;
    p[0] = (OSG_u8)(v & 0xff);
    p[1] = (OSG_u8)((v >> 8) & 0xff);
}

OSG_u16 get16(void FAR *ic, OSG_u16 off) {
    OSG_u8 FAR *p = ic_bytes(ic) + off;
    return (OSG_u16)(p[0] | ((OSG_u16)p[1] << 8));
}

void zero_bytes(void FAR *ic, OSG_u16 off, OSG_u16 n) {
    OSG_u8 FAR *p = ic_bytes(ic) + off;
    while (n--) *p++ = 0;
}

void copy_cstr_to_ic(void FAR *ic, OSG_u16 off, const char *s, OSG_u16 max) {
    OSG_u8 FAR *p = ic_bytes(ic) + off;
    OSG_u16 i = 0;
    while (i + 1 < max && s[i]) {
        p[i] = (OSG_u8)s[i];
        ++i;
    }
    p[i] = 0;
}

OSG_u32 le32(const OSG_u8 *p) {
    return ((OSG_u32)p[0]) | ((OSG_u32)p[1] << 8) | ((OSG_u32)p[2] << 16) | ((OSG_u32)p[3] << 24);
}

int read_exact(int h, OSG_u32 off, OSG_u8 *buf, OSG_u16 n) {
    if (osg_file_seek(h, off) < 0) return 0;
    return osg_file_read(h, buf, n) == (int)n;
}

unsigned c_strlen_far(const char FAR *s) {
    unsigned n = 0;
    while (s[n] && n < OSG_MAX_WORD + 8) ++n;
    return n;
}

int normalize_word(const char FAR *src, char *dst, unsigned *rawLen) {
    unsigned i = 0, o = 0;
    if (!src) return 0;
    while (src[i] && i < OSG_MAX_WORD + 8) {
        unsigned char c = (unsigned char)src[i++];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
        if ((c >= 'a' && c <= 'z') || c == '\'') {
            if (o >= OSG_MAX_WORD) return 0;
            dst[o++] = (char)c;
        } else {
            return 0;
        }
    }
    dst[o] = 0;
    if (rawLen) *rawLen = i;
    return o != 0;
}

typedef struct OSGCursor {
    int h;
    OSG_u8 buf[OSG_READ_CHUNK];
    OSG_u16 pos;
    OSG_u16 len;
} OSGCursor;

int cursor_get(OSGCursor *c, OSG_u8 *out) {
    if (c->pos >= c->len) {
        int got = osg_file_read(c->h, c->buf, OSG_READ_CHUNK);
        if (got <= 0) return 0;
        c->pos = 0;
        c->len = (OSG_u16)got;
    }
    *out = c->buf[c->pos++];
    return 1;
}

int read_next_dict_word(OSGCursor *cur, char *out, unsigned max) {
    unsigned n = 0;
    OSG_u8 ch;
    for (;;) {
        if (!cursor_get(cur, &ch)) return -1;
        if (ch == 0) {
            out[n < max ? n : max] = 0;
            return 1;
        }
        if (n < max) out[n] = (char)ch;
        ++n;
    }
}

int cmp_ascii(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return ((unsigned char)*a) - ((unsigned char)*b);
}

int open_dictionary(void FAR *ic) {
#ifdef OSGSPELL_HOST
    {
        int h = GEOSfileOpen("EN_US.DAT", 0);
        if (h) return h;
        h = GEOSopen("EN_US.DAT", 0);
        return h < 0 ? 0 : h;
    }
#else
    (void)ic;
    return OSGDICTOPEN();
#endif
}


int osg_read_current(int h, OSG_u8 *buf, OSG_u16 n) {
    return osg_file_read(h, buf, n) == (int)n;
}

int osg_skip_current(int h, OSG_u32 n) {
    OSG_u8 scratch[128];
    while (n) {
        OSG_u16 chunk = (n > sizeof(scratch)) ? (OSG_u16)sizeof(scratch) : (OSG_u16)n;
        if (osg_file_read(h, scratch, chunk) != (int)chunk) return 0;
        n -= chunk;
    }
    return 1;
}

int osg_read_header(int h, OSG_u8 *hdr) {
    OSG_u32 version, buckets, dataOff;
    if (!osg_read_current(h, hdr, OSG_HEADER_SIZE)) return 0;
    if (hdr[0] != OSG_DICT_MAGIC0 || hdr[1] != OSG_DICT_MAGIC1 ||
        hdr[2] != OSG_DICT_MAGIC2 || hdr[3] != OSG_DICT_MAGIC3 ||
        hdr[4] != OSG_DICT_MAGIC4 || hdr[5] != OSG_DICT_MAGIC5 ||
        hdr[6] != OSG_DICT_MAGIC6 || hdr[7] != OSG_DICT_MAGIC7) {
        return 0;
    }
    version = le32(hdr + 8);
    buckets = le32(hdr + 16);
    dataOff = le32(hdr + 20);
    return version == 1UL && buckets == OSG_BUCKET_COUNT &&
           dataOff == OSG_BUCKET_TABLE_OFF + OSG_BUCKET_COUNT * OSG_BUCKET_REC_SIZE;
}

int osg_read_bucket_rec_sequential(int h, unsigned first, OSG_u8 *rec, OSG_u32 *posOut) {
    OSG_u32 skip = ((OSG_u32)first * OSG_BUCKET_REC_SIZE);
    if (!osg_skip_current(h, skip)) return 0;
    if (!osg_read_current(h, rec, OSG_BUCKET_REC_SIZE)) return 0;
    if (posOut) *posOut = OSG_HEADER_SIZE + skip + OSG_BUCKET_REC_SIZE;
    return 1;
}

int osg_contains(void FAR *ic, const char *needle) {
    OSG_u8 hdr[OSG_HEADER_SIZE];
    OSG_u8 rec[OSG_BUCKET_REC_SIZE];
    OSG_u32 bucketOff, bucketCount, pos;
    int h;
    OSGCursor cur;
    char word[OSG_MAX_WORD + 1];
    OSG_u32 i;
    unsigned first;

    if (!needle[0]) return IC_RET_NOT_FOUND;
    first = (unsigned char)needle[0];

    h = open_dictionary(ic);
    if (!h) return IC_RET_NO_OPEN;

    if (!osg_read_header(h, hdr)) { osg_file_close(h); return IC_RET_NO_OPEN; }
    if (!osg_read_bucket_rec_sequential(h, first, rec, &pos)) {
        osg_file_close(h);
        return IC_RET_NO_OPEN;
    }
    bucketOff = le32(rec);
    bucketCount = le32(rec + 4);
    if (bucketCount == 0) { osg_file_close(h); return IC_RET_NOT_FOUND; }
    if (bucketOff < pos) { osg_file_close(h); return IC_RET_NO_OPEN; }
    if (!osg_skip_current(h, bucketOff - pos)) { osg_file_close(h); return IC_RET_NO_OPEN; }

    cur.h = h;
    cur.pos = cur.len = 0;
    for (i = 0; i < bucketCount; ++i) {
        int r = read_next_dict_word(&cur, word, OSG_MAX_WORD);
        int c;
        if (r < 0) { osg_file_close(h); return IC_RET_NO_OPEN; }
        c = cmp_ascii(word, needle);
        if (c == 0) { osg_file_close(h); return IC_RET_FOUND; }
        if (c > 0) { osg_file_close(h); return IC_RET_NOT_FOUND; }
    }
    osg_file_close(h);
    return IC_RET_NOT_FOUND;
}



unsigned c_strlen_near(const char *s) {
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

int cmp_alt_word(void FAR *ic, OSG_u16 altOff, const char *s) {
    OSG_u8 FAR *p = ic_bytes(ic) + ICB_ALT_LIST + altOff;
    unsigned i = 0;
    while (p[i] && s[i] && p[i] == (OSG_u8)s[i]) ++i;
    return ((unsigned char)p[i]) - ((unsigned char)s[i]);
}

void reset_suggestions(void FAR *ic) {
    zero_bytes(ic, ICB_ALT_LIST, 200);
    zero_bytes(ic, ICB_CORRECT_PTR, OSG_MAX_SUGGESTIONS * 2);
    set16(ic, ICB_NUM_ALTS, 0);
    set16(ic, ICB_NEXT_ALT, 0);
}

int add_suggestion(void FAR *ic, const char *s) {
    OSG_u16 count = get16(ic, ICB_NUM_ALTS);
    OSG_u16 used = get16(ic, ICB_NEXT_ALT);
    OSG_u16 len = (OSG_u16)c_strlen_near(s);
    OSG_u16 i;
    OSG_u8 FAR *p;

    if (!s[0] || count >= OSG_MAX_SUGGESTIONS) return 0;
    if (used + len + 1 > 200) return 0;
    for (i = 0; i < count; ++i) {
        OSG_u16 off = get16(ic, ICB_CORRECT_PTR + i * 2);
        if (cmp_alt_word(ic, off, s) == 0) return 0;
    }

    p = ic_bytes(ic) + ICB_ALT_LIST + used;
    for (i = 0; i < len; ++i) p[i] = (OSG_u8)s[i];
    p[len] = 0;
    set16(ic, ICB_CORRECT_PTR + count * 2, used);
    set16(ic, ICB_NUM_ALTS, count + 1);
    set16(ic, ICB_NEXT_ALT, used + len + 1);
    return 1;
}

int edit_distance_cutoff(const char *a, const char *b, int cutoff) {
    int prev[OSG_MAX_WORD + 1];
    int cur[OSG_MAX_WORD + 1];
    int la = (int)c_strlen_near(a);
    int lb = (int)c_strlen_near(b);
    int i, j;

    if (la - lb > cutoff || lb - la > cutoff) return cutoff + 1;
    if (lb > OSG_MAX_WORD) return cutoff + 1;

    for (j = 0; j <= lb; ++j) prev[j] = j;
    for (i = 1; i <= la; ++i) {
        int rowMin;
        cur[0] = i;
        rowMin = cur[0];
        for (j = 1; j <= lb; ++j) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = cur[j-1] + 1;
            int sub = prev[j-1] + cost;
            int v = del < ins ? del : ins;
            if (sub < v) v = sub;
            cur[j] = v;
            if (v < rowMin) rowMin = v;
        }
        if (rowMin > cutoff) return cutoff + 1;
        for (j = 0; j <= lb; ++j) prev[j] = cur[j];
    }
    return prev[lb];
}

int common_prefix_len(const char *a, const char *b) {
    int n = 0;
    while (a[n] && b[n] && a[n] == b[n]) ++n;
    return n;
}

int is_subsequence(const char *shorter, const char *longer) {
    unsigned i = 0, j = 0;
    while (shorter[i] && longer[j]) {
        if (shorter[i] == longer[j]) ++i;
        ++j;
    }
    return shorter[i] == 0;
}

int suggestion_rank(const char *needle, const char *word, int dist, int diff) {
    int rank = dist * 100 + diff * 8;
    int prefix = common_prefix_len(needle, word);
    rank -= prefix * 6;
    if (needle[0] && word[0] && needle[0] == word[0]) rank -= 10;
    if (needle[1] && word[1] && needle[1] == word[1]) rank -= 5;
    if (is_subsequence(needle, word) || is_subsequence(word, needle)) rank -= 50;
    return rank;
}

void remember_candidate(char sug[OSG_MAX_SUGGESTIONS][OSG_MAX_WORD + 1],
                        int rank[OSG_MAX_SUGGESTIONS],
                        int lenDiff[OSG_MAX_SUGGESTIONS],
                        const char *word, int candidateRank, int diff) {
    int slot = -1;
    int worst = -1;
    int i;

    for (i = 0; i < OSG_MAX_SUGGESTIONS; ++i) {
        if (rank[i] == 9999) { slot = i; break; }
        if (worst < 0 || rank[i] > rank[worst] ||
            (rank[i] == rank[worst] && lenDiff[i] > lenDiff[worst])) {
            worst = i;
        }
    }
    if (slot < 0) {
        if (candidateRank > rank[worst]) return;
        if (candidateRank == rank[worst] && diff >= lenDiff[worst]) return;
        slot = worst;
    }
    {
        unsigned j;
        for (j = 0; j < OSG_MAX_WORD && word[j]; ++j) sug[slot][j] = word[j];
        sug[slot][j] = 0;
    }
    rank[slot] = candidateRank;
    lenDiff[slot] = diff;
}

void build_suggestions(void FAR *ic, const char *needle) {
    OSG_u8 hdr[OSG_HEADER_SIZE];
    OSG_u8 rec[OSG_BUCKET_REC_SIZE];
    int h;
    unsigned bucket;
    char sug[OSG_MAX_SUGGESTIONS][OSG_MAX_WORD + 1];
    int rank[OSG_MAX_SUGGESTIONS];
    int lenDiff[OSG_MAX_SUGGESTIONS];
    int i;
    int nlen = (int)c_strlen_near(needle);

    reset_suggestions(ic);
    for (i = 0; i < OSG_MAX_SUGGESTIONS; ++i) {
        sug[i][0] = 0;
        rank[i] = 9999;
        lenDiff[i] = 9999;
    }

    h = open_dictionary(ic);
    if (!h) return;
    if (!osg_read_header(h, hdr)) { osg_file_close(h); return; }

    for (bucket = 'a'; bucket <= 'z'; ++bucket) {
        OSG_u32 pos;
        OSG_u32 bucketOff, bucketCount;
        OSGCursor cur;
        char word[OSG_MAX_WORD + 1];
        OSG_u32 wi;

        if (osg_file_seek(h, OSG_BUCKET_TABLE_OFF + ((OSG_u32)bucket * OSG_BUCKET_REC_SIZE)) < 0) break;
        if (osg_file_read(h, rec, OSG_BUCKET_REC_SIZE) != (int)OSG_BUCKET_REC_SIZE) break;
        bucketOff = le32(rec);
        bucketCount = le32(rec + 4);
        if (bucketCount == 0) continue;
        if (osg_file_seek(h, bucketOff) < 0) break;
        cur.h = h;
        cur.pos = cur.len = 0;
        for (wi = 0; wi < bucketCount; ++wi) {
            int r = read_next_dict_word(&cur, word, OSG_MAX_WORD);
            int wlen, diff, dist, cutoff;
            if (r < 0) break;
            wlen = (int)c_strlen_near(word);
            diff = wlen > nlen ? (wlen - nlen) : (nlen - wlen);
            cutoff = (nlen <= 5) ? 2 : 3;
            if (diff > cutoff) continue;
            if (word[0] != needle[0] && diff > 1) continue;
            dist = edit_distance_cutoff(needle, word, cutoff);
            if (dist <= cutoff) {
                int rnk = suggestion_rank(needle, word, dist, diff);
                remember_candidate(sug, rank, lenDiff, word, rnk, diff);
            }
        }
    }
    osg_file_close(h);

    for (;;) {
        int best = -1;
        for (i = 0; i < OSG_MAX_SUGGESTIONS; ++i) {
            if (rank[i] == 9999) continue;
            if (best < 0 || rank[i] < rank[best] ||
                (rank[i] == rank[best] && lenDiff[i] < lenDiff[best])) {
                best = i;
            }
        }
        if (best < 0) break;
        add_suggestion(ic, sug[best]);
        rank[best] = 9999;
    }
}

int osg_init_icbuff_ptr(void FAR *ic) {

    //return IC_RET_OK;

    if (!ic) return IC_RET_ERR;
    set16(ic, ICB_TASK, ST_VERIFY);
    set8(ic, ICB_LANGUAGE, SL_ENGLISH);
    set16(ic, ICB_DIALECT, LD_AMERICAN);
    copy_cstr_to_ic(ic, ICB_MASTER_FNAME, "EN_US.DAT", 65);
    set16(ic, ICB_INIT_FLAGS, 0x0080); /* SIF_INIT_OK */
    set16(ic, ICB_PROCESS_FLAGS, 0);
    set16(ic, ICB_ERROR_FLAGS, 0);
    set16(ic, ICB_ERROR_FLAGS_HIGH, 0);
    set16(ic, ICB_RET_CODE, IC_RET_OK);
    zero_bytes(ic, ICB_HYPMAP, 8);
    zero_bytes(ic, ICB_ALTMAP, 8);
    zero_bytes(ic, ICB_PREV_WORD, 65);
    set16(ic, ICB_LSIDE, 0);
    set16(ic, ICB_RSIDE, 0);
    zero_bytes(ic, ICB_SLASH_MAP, 32);
    reset_suggestions(ic);
    return IC_RET_OK;
}

int osg_exit_icbuff_ptr(void FAR *ic) {
    (void)ic;
    return IC_RET_OK;
}


#ifndef OSGSPELL_HOST
int _pascal ICGEOSplInitICBuff(OSG_u16 icHan);
int _pascal ICGEOSplExitICBuff(OSG_u16 icHan);
int _pascal ICGEOSpl(char FAR *src, OSG_u16 icHan);
int _pascal ICGEOSplExit(void);
void _pascal ICGEOGetAlternate(char FAR *dest, char FAR *src, void FAR *ic, int index);
int _pascal IPGEOAddUser(OSG_u16 icHan, char FAR *word);
int _pascal IPGEODeleteUser(OSG_u16 icHan, char FAR *word);
int _pascal IPGEOBuildUserList(OSG_u16 icHan);
int _pascal UpdateUserDictionary(OSG_u16 icHan);
void _pascal ICGEOIgnoreString(OSG_u16 icHan, char FAR *word);
void _pascal ICResetIgnoreUserDict(OSG_u16 icHan);
#endif

#ifndef OSGSPELL_HOST
#ifdef __WATCOMC__
#pragma code_seg("CODE")
#endif
#endif
int osg_spell_ptr(void FAR *ic, char FAR *src) {
    char norm[OSG_MAX_WORD + 1];
    unsigned rawLen = 0;
    int ret;

    if (!ic || !src) return IC_RET_ERR;

    /*
     * The GEOS UI asks for suggestions by setting ICB_task to ST_CORRECT and
     * then calling ICSpl() with an empty string until the old engine stops
     * returning IC_RET_FOUND.  Our backend builds the suggestion list during
     * the original ST_VERIFY miss, so an ST_CORRECT/empty-string call must not
     * clear ICB_altList/ICB_correctPtr.  Just report end-of-correction and let
     * SuggestListGenerateSuggestions read the preserved alternates.
     */
    if (get16(ic, ICB_TASK) == ST_CORRECT && src[0] == 0) {
        set16(ic, ICB_ERROR_FLAGS, 0);
        set16(ic, ICB_ERROR_FLAGS_HIGH, 0);
        zero_bytes(ic, ICB_SLASH_MAP, 32);
        set16(ic, ICB_RET_CODE, IC_RET_NOT_FOUND);
        return IC_RET_NOT_FOUND;
    }

    set16(ic, ICB_ERROR_FLAGS, 0);
    set16(ic, ICB_ERROR_FLAGS_HIGH, 0);
    zero_bytes(ic, ICB_SLASH_MAP, 32);
    reset_suggestions(ic);

    if (!normalize_word(src, norm, &rawLen)) {
        set16(ic, ICB_RET_CODE, IC_RET_NOT_FOUND);
        set16(ic, ICB_LEN, (OSG_u16)c_strlen_far(src));
        set16(ic, ICB_LSIDE, 0);
        set16(ic, ICB_RSIDE, (OSG_u16)c_strlen_far(src));
        return IC_RET_NOT_FOUND;
    }

    copy_cstr_to_ic(ic, ICB_WORD, norm, OSG_MAX_WORD + 1);
    set16(ic, ICB_LEN, (OSG_u16)rawLen);
    set16(ic, ICB_LSIDE, 0);
    set16(ic, ICB_RSIDE, (OSG_u16)rawLen);

    ret = osg_contains(ic, norm);
    if (ret == IC_RET_NOT_FOUND) {
        build_suggestions(ic, norm);
    }
    set16(ic, ICB_RET_CODE, (OSG_u16)ret);
    return ret;
}

#ifndef OSGSPELL_HOST
int try_dict_path(DiskHandle disk, const char *path)
{
    FileHandle h;

    FilePushDir();
    if (FileSetCurrentPath(disk, path) == 0) {
        FilePopDir();
        return 0;
    }

    h = FileOpen("EN_US.DAT", FILE_ACCESS_R | FILE_DENY_NONE);
    FilePopDir();

    if (h) {
        FileClose(h, FALSE);
        return 1;
    }
    return 0;
}
#endif

int mark_spell_result(void FAR *ic, const char *word, int len, int ret)
{
    if (ic) {
        set16(ic, ICB_ERROR_FLAGS, 0);
        set16(ic, ICB_ERROR_FLAGS_HIGH, 0);
        zero_bytes(ic, ICB_SLASH_MAP, 32);
        set16(ic, ICB_NUM_ALTS, 0);
        set16(ic, ICB_NEXT_ALT, 0);
        copy_cstr_to_ic(ic, ICB_WORD, word ? word : "", OSG_MAX_WORD + 1);
        set16(ic, ICB_LEN, (OSG_u16)len);
        set16(ic, ICB_LSIDE, 0);
        set16(ic, ICB_RSIDE, (OSG_u16)len);
        set16(ic, ICB_RET_CODE, (OSG_u16)ret);
    }
    return ret;
}

#ifndef OSGSPELL_HOST
#ifdef __WATCOMC__
#pragma code_seg("EXIT")
#endif
#endif
int _pascal ICGEOSplExit(void) {
    return IC_RET_OK;
}

#ifndef OSGSPELL_HOST
#ifdef __WATCOMC__
#pragma code_seg("CODE")
#endif
#endif
void _pascal ICGEOGetAlternate(char FAR *dest, char FAR *src, void FAR *ic, int index) {
    (void)src; (void)ic; (void)index;
    if (dest) dest[0] = 0;
}

#ifndef OSGSPELL_HOST
#ifdef __WATCOMC__
#pragma code_seg("IPCODE")
#endif
#endif
int _pascal IPGEOAddUser(OSG_u16 icHan, char FAR *word) {
    (void)icHan;
    (void)word;
    return IC_RET_NO_USER_DICT;
}

int _pascal IPGEODeleteUser(OSG_u16 icHan, char FAR *word) {
    (void)icHan;
    (void)word;
    return IC_RET_NO_USER_DICT;
}

void _pascal ICGEOIgnoreString(OSG_u16 icHan, char FAR *word) {
    (void)icHan; (void)word;
}

void _pascal ICResetIgnoreUserDict(OSG_u16 icHan) {
    (void)icHan;
}

#ifndef OSGSPELL_HOST
#ifdef __WATCOMC__
#pragma code_seg("IPPRINT")
#endif
#endif
int _pascal IPGEOBuildUserList(OSG_u16 icHan) {
    (void)icHan;
    return 0;
}

int _pascal UpdateUserDictionary(OSG_u16 icHan) {
    (void)icHan;
    return IC_RET_OK;
}

#ifdef OSGSPELL_HOST
int _pascal ICGEOSplInitICBuff(void FAR *ic) { return osg_init_icbuff_ptr(ic); }
int _pascal ICGEOSplExitICBuff(void FAR *ic) { return osg_exit_icbuff_ptr(ic); }
int _pascal ICGEOSpl(void FAR *ic, char FAR *src) { return osg_spell_ptr(ic, src); }
#else
#ifdef __WATCOMC__
#pragma code_seg("INIT")
#endif
int _pascal ICGEOSplInitICBuff(OSG_u16 icHan)
{
    void FAR *ic;
    int ret;

    if (icHan == 0) {
        return IC_RET_ERR;
    }

    ic = MemLock((MemHandle)icHan);
    if (ic == 0) {
        return IC_RET_NOMEM;
    }

    ret = osg_init_icbuff_ptr(ic);

    MemUnlock((MemHandle)icHan);
    return ret;
}

#ifdef __WATCOMC__
#pragma code_seg("EXIT")
#endif
int _pascal ICGEOSplExitICBuff(OSG_u16 icHan) {
    void FAR *ic;
    int ret;
    if (!icHan) return IC_RET_ERR;
    ic = MemLock((MemHandle)icHan);
    if (!ic) return IC_RET_NOMEM;
    ret = osg_exit_icbuff_ptr(ic);
    MemUnlock((MemHandle)icHan);
    return ret;
}

#ifdef __WATCOMC__
#pragma code_seg("CODE")
#endif
int _pascal ICGEOSpl(char FAR *src, OSG_u16 icHan)
{
    void FAR *ic;
    int ret;
    char word[64];
    int i;

    if (src == 0 || icHan == 0) {
        return IC_RET_FOUND;
    }

    #if OSG_SPELL_MODE == 0
    (void)icHan;
    return IC_RET_FOUND;
    #else
    ic = MemLock((MemHandle)icHan);
    if (ic == 0) {
        return IC_RET_FOUND;
    }

    #if OSG_SPELL_MODE == 1
    for (i = 0; i < 63 && src[i] != 0; i++) {
        word[i] = src[i];
    }
    word[i] = 0;
    set16(ic, ICB_ERROR_FLAGS, 0);
    set16(ic, ICB_ERROR_FLAGS_HIGH, 0);
    zero_bytes(ic, ICB_SLASH_MAP, 32);
    set16(ic, ICB_NUM_ALTS, 0);
    set16(ic, ICB_NEXT_ALT, 0);
    copy_cstr_to_ic(ic, ICB_WORD, word, OSG_MAX_WORD + 1);
    set16(ic, ICB_LEN, (OSG_u16)i);
    set16(ic, ICB_LSIDE, 0);
    set16(ic, ICB_RSIDE, (OSG_u16)i);
    set16(ic, ICB_RET_CODE, IC_RET_NOT_FOUND);
    ret = IC_RET_NOT_FOUND;
    #elif OSG_SPELL_MODE == 2
    ret = osg_spell_ptr(ic, src);
    #elif OSG_SPELL_MODE == 3
    for (i = 0; i < 63 && src[i] != 0; i++) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        word[i] = c;
    }
    word[i] = 0;

    if (cmp_ascii(word, "diag") == 0) {
        ret = IC_RET_NOT_FOUND;
    } else if (cmp_ascii(word, "pub") == 0) {
        ret = try_dict_path(SP_PUBLIC_DATA, "DICTS") ? IC_RET_NOT_FOUND : IC_RET_FOUND;
    } else if (cmp_ascii(word, "user") == 0) {
        ret = try_dict_path(SP_USER_DATA, "DICTS") ? IC_RET_NOT_FOUND : IC_RET_FOUND;
    } else if (cmp_ascii(word, "top") == 0) {
        ret = try_dict_path(SP_TOP, "USERDATA\\DICTS") ? IC_RET_NOT_FOUND : IC_RET_FOUND;
    } else if (cmp_ascii(word, "abs") == 0) {
        ret = try_dict_path((DiskHandle)0, "\\E166\\USERDATA\\DICTS") ? IC_RET_NOT_FOUND : IC_RET_FOUND;
    } else {
        ret = IC_RET_FOUND;
    }

    /*
     * A non-found spell result must also populate the ICBuff word and
     * position fields.  Returning IC_RET_NOT_FOUND alone can make the
     * outer spell loop believe there is no usable error range and finish.
     */
    ret = mark_spell_result(ic, word, i, ret);
    #else
    ret = IC_RET_FOUND;
    #endif

    MemUnlock((MemHandle)icHan);
    return ret;
    #endif
}
#endif

#ifdef OSGSPELL_HOST_MAIN
int main(int argc, char **argv) {
    unsigned char ic[700];
    int i;
    if (argc < 3) {
        fprintf(stderr, "usage: %s DICT_DIR word...\n", argv[0]);
        return 2;
    }
    memset(ic, 0, sizeof(ic));
    host_dict_dir = argv[1];
    ICGEOSplInitICBuff(ic);
    for (i = 2; i < argc; ++i) {
        int r = ICGEOSpl(ic, argv[i]);
        printf("%s: %s (%d)\n", argv[i], r == IC_RET_FOUND ? "FOUND" : "NOT FOUND", r);
    }
    return 0;
}
#endif
