#include <ruby.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <rubyio.h>
#include <intern.h>
#include "version.h"

#if RUBY_VERSION_CODE < 180
#define StringValue(x) do {				\
    if (TYPE(x) != T_STRING) x = rb_str_to_str(x);	\
} while (0)
#define StringValuePtr(x) STR2CSTR(x)
#define SafeStringValue(x) Check_SafeStr(x)
#endif

#ifndef MADV_NORMAL
#ifdef POSIX_MADV_NORMAL
#define MADV_NORMAL     POSIX_MADV_NORMAL 
#define MADV_RANDOM     POSIX_MADV_RANDOM 
#define MADV_SEQUENTIAL POSIX_MADV_SEQUENTIAL
#define MADV_WILLNEED   POSIX_MADV_WILLNEED
#define MADV_DONTNEED   POSIX_MADV_DONTNEED
#define madvise posix_madvise
#endif
#endif

#include <re.h>

#define BEG(no) regs->beg[no]
#define END(no) regs->end[no]

#ifndef MMAP_RETTYPE
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif /* !_POSIX_C_SOURCE */
#ifdef _POSIX_VERSION
#if _POSIX_VERSION >= 199309
#define MMAP_RETTYPE void *
#endif /* _POSIX_VERSION >= 199309 */
#endif /* _POSIX_VERSION */
#endif /* !MMAP_RETTYPE */

#ifndef MMAP_RETTYPE
#define MMAP_RETTYPE caddr_t
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((caddr_t)-1)
#endif /* !MAP_FAILED */

#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

static VALUE mm_cMap;

#define EXP_INCR_SIZE 4096

typedef struct {
    MMAP_RETTYPE addr;
    int smode, pmode, vscope;
    int advice, flag;
    size_t len, real, incr;
    off_t offset;
    VALUE io;
    char *path;
} mm_mmap;

static void
mm_mark(t_mm)
    mm_mmap *t_mm;
{
    rb_gc_mark(t_mm->io);
}

static void
mm_free(t_mm)
    mm_mmap *t_mm;
{
    if (t_mm->path) {
	munmap(t_mm->addr, t_mm->len);
	if (t_mm->path != (char *)-1) {
	    if (t_mm->real < t_mm->len && t_mm->vscope != MAP_PRIVATE &&
		truncate(t_mm->path, t_mm->real) == -1) {
		free(t_mm->path);
		free(t_mm);
		rb_raise(rb_eTypeError, "truncate");
	    }
	    free(t_mm->path);
	}
    }
    free(t_mm);
}

#define MM_MODIFY 1
#define MM_ORIGIN 2
#define MM_CHANGE (MM_MODIFY | 4)
#define MM_PROTECT 8

#define MM_FROZEN (1<<0)
#define MM_FIXED  (1<<1)
#define MM_ANON   (1<<2)
#define MM_LOCK   (1<<3)

#define GetMmap(obj, t_mm, t_modify)				\
    Data_Get_Struct(obj, mm_mmap, t_mm);			\
    if (!t_mm->path) {						\
	rb_raise(rb_eIOError, "unmapped file");			\
    }								\
    if ((t_modify & MM_MODIFY) && (t_mm->flag & MM_FROZEN)) {	\
	rb_error_frozen("mmap");				\
    }

static VALUE
mm_unmap(obj)
    VALUE obj;
{
    mm_mmap *t_mm;

    GetMmap(obj, t_mm, 0);
    if (t_mm->path) {
	munmap(t_mm->addr, t_mm->len);
	if (t_mm->path != (char *)-1) {
	    if (t_mm->real < t_mm->len && t_mm->vscope != MAP_PRIVATE &&
		truncate(t_mm->path, t_mm->real) == -1) {
		rb_raise(rb_eTypeError, "truncate");
	    }
	    free(t_mm->path);
	}
	t_mm->path = '\0';
    }
    return Qnil;
}

static VALUE
mm_freeze(obj)
    VALUE obj;
{
    mm_mmap *t_mm;
    rb_obj_freeze(obj);
    GetMmap(obj, t_mm, 0);
    t_mm->flag |= MM_FROZEN;
    return obj;
}

static VALUE
mm_str(obj, modify)
    VALUE obj;
    int modify;
{
    mm_mmap *t_mm;
    VALUE ret;

    GetMmap(obj, t_mm, modify & ~MM_ORIGIN);
    if (modify & MM_MODIFY) {
	if (t_mm->flag & MM_FROZEN) rb_error_frozen("mmap");
	if (!OBJ_TAINTED(ret) && rb_safe_level() >= 4)
	    rb_raise(rb_eSecurityError, "Insecure: can't modify mmap");
    }
#if RUBY_VERSION_CODE >= 171
    ret = rb_obj_alloc(rb_cString);
    if (rb_obj_tainted(obj)) {
	OBJ_TAINT(ret);
    }
#else
    if (rb_obj_tainted(obj)) {
	ret = rb_tainted_str_new2("");
    }
    else {
	ret = rb_str_new2("");
    }
    free(RSTRING(ret)->ptr);
#endif
    RSTRING(ret)->ptr = t_mm->addr;
    RSTRING(ret)->len = t_mm->real;
    if (modify & MM_ORIGIN) {
#if RUBY_VERSION_CODE >= 172
	RSTRING(ret)->aux.shared = ret;
	FL_SET(ret, ELTS_SHARED);
#else
	RSTRING(ret)->orig = ret;
#endif
    }
    if (t_mm->flag & MM_FROZEN) {
	ret = rb_obj_freeze(ret);
    }
    return ret;
}

static VALUE
mm_to_str(obj)
    VALUE obj;
{
    return mm_str(obj, MM_ORIGIN);
}
 
extern char *ruby_strdup();

static void
mm_expandf(t_mm, len)
    mm_mmap *t_mm;
    long len;
{
    int fd;

    if (t_mm->vscope == MAP_PRIVATE) {
	rb_raise(rb_eTypeError, "expand for a private map");
    }
    if (t_mm->flag & MM_FIXED) {
	rb_raise(rb_eTypeError, "expand for a fixed map");
    }
    if (!t_mm->path || t_mm->path == (char *)-1) {
	rb_raise(rb_eTypeError, "expand for an anonymous map");
    }
    if (munmap(t_mm->addr, t_mm->len)) {
	rb_raise(rb_eArgError, "munmap failed");
    }
    if ((fd = open(t_mm->path, t_mm->smode)) == -1) {
	rb_raise(rb_eArgError, "Can't open %s", t_mm->path);
    }
    if (len > t_mm->len) {
	if (lseek(fd, len - t_mm->len - 1, SEEK_END) == -1) {
	    rb_raise(rb_eIOError, "Can't lseek %d", len - t_mm->len - 1);
	}
	if (write(fd, "\000", 1) != 1) {
	    rb_raise(rb_eIOError, "Can't extend %s", t_mm->path);
	}
    }
    else if (len < t_mm->len && truncate(t_mm->path, len) == -1) {
	rb_raise(rb_eIOError, "Can't truncate %s", t_mm->path);
    }
    t_mm->addr = mmap(0, len, t_mm->pmode, t_mm->vscope, fd, t_mm->offset);
    close(fd);
    if (t_mm->addr == MAP_FAILED) {
	rb_raise(rb_eArgError, "mmap failed");
    }
#ifdef MADV_NORMAL
    if (t_mm->advice && madvise(t_mm->addr, len, t_mm->advice) == -1) {
	rb_raise(rb_eArgError, "madvise(%d)", errno);
    }
#endif
    if ((t_mm->flag & MM_LOCK) && mlock(t_mm->addr, len) == -1) {
	rb_raise(rb_eArgError, "mlock(%d)", errno);
    }
    t_mm->len  = len;
}

static void
mm_realloc(t_mm, len)
    mm_mmap *t_mm;
    long len;
{
    if (t_mm->flag & MM_FROZEN) rb_error_frozen("mmap");
    if (len > t_mm->len) {
	if ((len - t_mm->len) < t_mm->incr) {
	    len = t_mm->len + t_mm->incr;
	}
	mm_expandf(t_mm, len);
    }
}

static VALUE
mm_extend(obj, a)
    VALUE obj, a;
{
    mm_mmap *t_mm;
    long len;

    GetMmap(obj, t_mm, MM_MODIFY);
    len = NUM2LONG(a);
    if (len > 0) {
	mm_expandf(t_mm, t_mm->len + len);
    }
    return INT2NUM(t_mm->len);
}

static VALUE
mm_i_options(arg, obj)
    VALUE arg, obj;
{
    mm_mmap *t_mm;
    char *options;
    VALUE key, value;

    Data_Get_Struct(obj, mm_mmap, t_mm);
    key = rb_ary_entry(arg, 0);
    value = rb_ary_entry(arg, 1);
    key = rb_obj_as_string(key);
    options = StringValuePtr(key);
    if (strcmp(options, "length") == 0) {
	t_mm->len = NUM2INT(value);
	if (t_mm->len <= 0) {
	    rb_raise(rb_eArgError, "Invalid value for length %d", t_mm->len);
	}
	t_mm->flag |= MM_FIXED;
    }
    else if (strcmp(options, "offset") == 0) {
	t_mm->offset = NUM2INT(value);
	if (t_mm->offset < 0) {
	    rb_raise(rb_eArgError, "Invalid value for offset %d", t_mm->offset);
	}
	t_mm->flag |= MM_FIXED;
    }
    else if (strcmp(options, "advice") == 0) {
	t_mm->advice = NUM2INT(value);
    }
    else if (strcmp(options, "increment") == 0) {
	t_mm->incr = NUM2INT(value);
	if (t_mm->incr < 0) {
	    rb_raise(rb_eArgError, "Invalid value for increment %d", t_mm->incr);
	}
    }
    return Qnil;
}


#if RUBY_VERSION_CODE >= 172
static VALUE
mm_s_alloc(obj)
    VALUE obj;
{
    VALUE res;
    mm_mmap *t_mm;

    res = Data_Make_Struct(obj, mm_mmap, mm_mark, mm_free, t_mm);
    t_mm->incr = EXP_INCR_SIZE;
    return res;
}
#endif

static VALUE
#if RUBY_VERSION_CODE >= 172
mm_init(argc, argv, obj)
#else
mm_s_new(argc, argv, obj)
#endif
    VALUE obj, *argv;
    int argc;
{
    struct stat st;
    int fd, smode, pmode, vscope, perm, init;
    MMAP_RETTYPE addr;
    VALUE res, fname, fdv, vmode, scope, options;
    mm_mmap *t_mm;
    char *path, *mode;
    size_t size = 0;
    off_t offset;
    int anonymous;

    options = Qnil;
    if (argc > 1 && TYPE(argv[argc - 1]) == T_HASH) {
	options = argv[argc - 1];
	argc--;
    }
    rb_scan_args(argc, argv, "12", &fname, &vmode, &scope);
    vscope = 0;
    path = 0;
    fd = -1;
    anonymous = 0;
    fdv = Qnil;
#ifdef MAP_ANON
    if (NIL_P(fname)) {
	vscope = MAP_ANON | MAP_SHARED;
	anonymous = 1;
    }
    else 
#endif
    {
	if (rb_safe_level() > 0 && OBJ_TAINTED(fname)){
	    rb_raise(rb_eSecurityError, "Insecure operation");
	}
	rb_secure(4);
	if (rb_respond_to(fname, rb_intern("fileno"))) {
	    fdv = rb_funcall2(fname, rb_intern("fileno"), 0, 0);
	}
	if (NIL_P(fdv)) {
	    fname = rb_str_to_str(fname);
	    SafeStringValue(fname);
	    path = StringValuePtr(fname);
	}
	else {
	    fd = NUM2INT(fdv);
	    if (fd < 0) {
		rb_raise(rb_eArgError, "invalid file descriptor %d", fd);
	    }
	}
	if (!NIL_P(scope)) {
	    vscope = NUM2INT(scope);
#ifdef MAP_ANON
	    if (vscope & MAP_ANON) {
		rb_raise(rb_eArgError, "filename specified for an anonymous map");
	    }
#endif
	}
    }
    vscope |= NIL_P(scope) ? MAP_SHARED : NUM2INT(scope);
    size = 0;
    perm = 0666;
    if (!anonymous) {
	if (NIL_P(vmode)) {
	    mode = "r";
	}
	else if (TYPE(vmode) == T_ARRAY && RARRAY(vmode)->len >= 2) {
	    VALUE tmp = RARRAY(vmode)->ptr[0];
	    mode = StringValuePtr(tmp);
	    perm = NUM2INT(RARRAY(vmode)->ptr[1]);
	}
	else {
	    mode = StringValuePtr(vmode);
	}
	if (strcmp(mode, "r") == 0) {
	    smode = O_RDONLY;
	    pmode = PROT_READ;
	}
	else if (strcmp(mode, "w") == 0) {
	    smode = O_WRONLY;
	    pmode = PROT_WRITE;
	    if (!NIL_P(fdv)) {
		pmode |= PROT_READ;
	    }
	}
	else if (strcmp(mode, "rw") == 0 || strcmp(mode, "wr") == 0) {
	    smode = O_RDWR;
	    pmode = PROT_READ | PROT_WRITE;
	}
	else if (strcmp(mode, "a") == 0) {
	    smode = O_RDWR | O_CREAT;
	    pmode = PROT_READ | PROT_WRITE;
	}
	else {
	    rb_raise(rb_eArgError, "Invalid mode %s", mode);
	}
	if (NIL_P(fdv)) {
	    if ((fd = open(path, smode, perm)) == -1) {
		rb_raise(rb_eArgError, "Can't open %s", path);
	    }
	}
	if (fstat(fd, &st) == -1) {
	    rb_raise(rb_eArgError, "Can't stat %s", path);
	}
	size = st.st_size;
    }
    else {
	fd = -1;
	if (!NIL_P(vmode) && TYPE(vmode) != T_STRING) {
	    size = NUM2INT(vmode);
	}
    }
#if RUBY_VERSION_CODE >= 172
    Data_Get_Struct(obj, mm_mmap, t_mm);
    res = obj;
#else
    res = Data_Make_Struct(obj, mm_mmap, mm_mark, mm_free, t_mm);
    t_mm->incr = EXP_INCR_SIZE;
#endif
    offset = 0;
    if (options != Qnil) {
	rb_iterate(rb_each, options, mm_i_options, res);
	if (path && (t_mm->len + t_mm->offset) > st.st_size) {
	    rb_raise(rb_eArgError, "invalid value for length (%d) or offset (%d)",
		     t_mm->len, t_mm->offset);
	}
	if (t_mm->len) size = t_mm->len;
	offset = t_mm->offset;
    }
    init = 0;
    if (anonymous) {
	if (size <= 0) {
	    rb_raise(rb_eArgError, "length not specified for an anonymous map");
	}
	if (offset) {
	    rb_warning("Ignoring offset for an anonymous map");
	    offset = 0;
	}
	smode = O_RDWR;
	pmode = PROT_READ | PROT_WRITE;
	t_mm->flag |= MM_FIXED;
    }
    else {
	if (size == 0 && (smode & O_RDWR)) {
	    if (lseek(fd, t_mm->incr - 1, SEEK_END) == -1) {
		rb_raise(rb_eIOError, "Can't lseek %d", t_mm->incr - 1);
	    }
	    if (write(fd, "\000", 1) != 1) {
		rb_raise(rb_eIOError, "Can't extend %s", path);
	    }
	    init = 1;
	    size = t_mm->incr;
	}
	if (!NIL_P(fdv)) {
	    t_mm->flag |= MM_FIXED;
	}
    }
    addr = mmap(0, size, pmode, vscope, fd, offset);
    if (NIL_P(fdv) && !anonymous) {
	close(fd);
    }
    if (addr == MAP_FAILED || !addr) {
	rb_raise(rb_eArgError, "mmap failed (%d)", errno);
    }
#ifdef MADV_NORMAL
    if (t_mm->advice && madvise(addr, size, t_mm->advice) == -1) {
	rb_raise(rb_eArgError, "madvise(%d)", errno);
    }
#endif
    t_mm->addr  = addr;
    t_mm->len = size;
    if (!init) t_mm->real = size;
    t_mm->pmode = pmode;
    t_mm->vscope = vscope;
    t_mm->smode = smode;
    t_mm->path = (path)?ruby_strdup(path):(char *)-1;
    if (!NIL_P(fdv)) {
	t_mm->io = fname;
    }
    if (smode == O_RDONLY) {
	res = rb_obj_freeze(res);
	t_mm->flag |= MM_FROZEN;
    }
    else {
	OBJ_TAINT(res);
    }
#if RUBY_VERSION_CODE < 172
    rb_obj_call_init(res, argc, argv);
#endif
    return res;
}

#if RUBY_VERSION_CODE < 171
static VALUE
mm_init(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return obj;
}
#endif


static VALUE
mm_msync(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    mm_mmap *t_mm;
    VALUE oflag;
    int ret;
    int flag = MS_SYNC;

    if (argc) {
	rb_scan_args(argc, argv, "01", &oflag);
	flag = NUM2INT(oflag);
    }
    GetMmap(obj, t_mm, MM_MODIFY);
    if ((ret = msync(t_mm->addr, t_mm->len, flag)) != 0) {
	rb_raise(rb_eArgError, "msync(%d)", ret);
    }
    if (t_mm->real < t_mm->len && t_mm->vscope != MAP_PRIVATE)
	mm_expandf(t_mm, t_mm->real);
    return obj;
}

static VALUE
mm_mprotect(obj, a)
    VALUE obj, a;
{
    mm_mmap *t_mm;
    int ret, pmode;
    char *smode;

    GetMmap(obj, t_mm, 0);
    if (TYPE(a) == T_STRING) {
	smode = StringValuePtr(a);
	if (strcmp(smode, "r") == 0) pmode = PROT_READ;
	else if (strcmp(smode, "w") == 0) pmode = PROT_WRITE;
	else if (strcmp(smode, "rw") == 0 || strcmp(smode, "wr") == 0)
	    pmode = PROT_READ | PROT_WRITE;
	else {
	    rb_raise(rb_eArgError, "Invalid mode %s", smode);
	}
    }
    else {
	pmode = NUM2INT(a);
    }
    if ((pmode & PROT_WRITE) && (t_mm->flag & MM_FROZEN)) 
	rb_error_frozen("mmap");
    if ((ret = mprotect(t_mm->addr, t_mm->len, pmode)) != 0) {
	rb_raise(rb_eArgError, "mprotect(%d)", ret);
    }
    t_mm->pmode = pmode;
    if (pmode & PROT_READ) {
	if (pmode & PROT_WRITE) t_mm->smode = O_RDWR;
	else {
	    t_mm->smode == O_RDONLY;
	    if (t_mm->vscope == MAP_PRIVATE) {
		obj = rb_obj_freeze(obj);
		t_mm->flag |= MM_FROZEN;
	    }
	}
    }
    else if (pmode & PROT_WRITE) {
	t_mm->smode == O_WRONLY;
    }
    return obj;
}

#ifdef MADV_NORMAL
static VALUE
mm_madvise(obj, a)
    VALUE obj, a;
{
    mm_mmap *t_mm;
    
    GetMmap(obj, t_mm, 0);
    if (madvise(t_mm->addr, t_mm->len, NUM2INT(a)) == -1) {
	rb_raise(rb_eTypeError, "madvise(%d)", errno);
    }
    t_mm->advice = NUM2INT(a);
    return Qnil;
}
#endif

#define StringMmap(b, bp, bl)						   \
do {									   \
    if (TYPE(b) == T_DATA && RDATA(b)->dfree == (RUBY_DATA_FUNC)mm_free) { \
	mm_mmap *b_mm;							   \
	GetMmap(b, b_mm, 0);						   \
	bp = b_mm->addr;						   \
	bl = b_mm->real;						   \
    }									   \
    else {								   \
	bp = StringValuePtr(b);						   \
	bl = RSTRING(b)->len;						   \
    }									   \
} while (0);

static void
mm_update(str, beg, len, val)
    mm_mmap *str;
    VALUE val;
    long beg;
    long len;
{
    char *valp;
    long vall;

    if (str->flag & MM_FROZEN) rb_error_frozen("mmap");
    if (len < 0) rb_raise(rb_eIndexError, "negative length %d", len);
    if (beg < 0) {
	beg += str->real;
    }
    if (beg < 0 || str->real < beg) {
	if (beg < 0) {
	    beg -= str->real;
	}
	rb_raise(rb_eIndexError, "index %d out of string", beg);
    }
    if (str->real < beg + len) {
	len = str->real - beg;
    }

    StringMmap(val, valp, vall);

    if ((str->flag & MM_FIXED) && vall != len) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    if (len < vall) {
	mm_realloc(str, str->real + vall - len);
    }

    if (vall != len) {
	memmove(str->addr + beg + vall,
		str->addr + beg + len,
		str->real - (beg + len));
    }
    if (str->real < beg && len < 0) {
	MEMZERO(str->addr + str->real, char, -len);
    }
    if (vall > 0) {
	memmove(str->addr+beg, valp, vall);
    }
    str->real += vall - len;
}

static VALUE
mm_match(x, y)
    VALUE x, y;
{
    VALUE reg, res;
    long start;

    x = mm_str(x, MM_ORIGIN);
    if (TYPE(y) == T_DATA && RDATA(y)->dfree == (RUBY_DATA_FUNC)mm_free) {
	y = mm_to_str(y);
    }
    switch (TYPE(y)) {
      case T_REGEXP:
	res = rb_reg_match(y, x);
	break;

      case T_STRING:
	reg = rb_reg_regcomp(y);
	start = rb_reg_search(reg, x, 0, 0);
	if (start == -1) res = Qnil;
	else res = INT2NUM(start);
	break;

      default:
	res = rb_funcall(y, rb_intern("=~"), 1, x);
	break;
    }
    return res;
}

static VALUE
get_pat(pat)
    VALUE pat;
{
    switch (TYPE(pat)) {
      case T_REGEXP:
	break;

      case T_STRING:
	pat = rb_reg_regcomp(pat);
	break;

      default:
	/* type failed */
	Check_Type(pat, T_REGEXP);
    }
    return pat;
}

static int
mm_correct_backref(t_mm)
    mm_mmap *t_mm;
{
    VALUE match;
    struct re_registers *regs;
    int i, start;

    match = rb_backref_get();
    if (NIL_P(match)) return 0;
    if (RMATCH(match)->BEG(0) == -1) return 0;
    start = RMATCH(match)->BEG(0);
    RMATCH(match)->str = rb_str_new(StringValuePtr(RMATCH(match)->str) + start,
				    RMATCH(match)->END(0) - start);
    if (OBJ_TAINTED(match)) OBJ_TAINT(RMATCH(match)->str);
    for (i = 0; i < RMATCH(match)->regs->num_regs && RMATCH(match)->BEG(i) != -1; i++) {
	RMATCH(match)->BEG(i) -= start;
	RMATCH(match)->END(i) -= start;
    }
    rb_backref_set(match);
    return start;
}

static VALUE
mm_sub_bang(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE pat, repl, match, str, res;
    struct re_registers *regs;
    int start, iter = 0;
    int tainted = 0;
    long plen;
    mm_mmap *t_mm;

    if (argc == 1 && rb_block_given_p()) {
	iter = 1;
    }
    else if (argc == 2) {
	repl = rb_str_to_str(argv[1]);
	if (OBJ_TAINTED(repl)) tainted = 1;
    }
    else {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)", argc);
    }
    GetMmap(obj, t_mm, MM_MODIFY);
    str = mm_str(obj, MM_MODIFY | MM_ORIGIN);

    pat = get_pat(argv[0]);
    res = Qnil;
    if (rb_reg_search(pat, str, 0, 0) >= 0) {
	start = mm_correct_backref(t_mm);
	match = rb_backref_get();
	regs = RMATCH(match)->regs;
	if (iter) {
	    rb_match_busy(match);
	    repl = rb_obj_as_string(rb_yield(rb_reg_nth_match(0, match)));
	    rb_backref_set(match);
	}
	else {
	    RSTRING(str)->ptr += start;
	    repl = rb_reg_regsub(repl, str, regs);
	    RSTRING(str)->ptr -= start;
	}
	if (OBJ_TAINTED(repl)) tainted = 1;
	plen = END(0) - BEG(0);
	if (RSTRING(repl)->len > plen) {
	    mm_realloc(t_mm, RSTRING(str)->len + RSTRING(repl)->len - plen);
	    RSTRING(str)->ptr = t_mm->addr;
	}
	if (RSTRING(repl)->len != plen) {
	    if (t_mm->flag & MM_FIXED) {
		rb_raise(rb_eTypeError, "try to change the size of a fixed map");
	    }
	    memmove(RSTRING(str)->ptr + start + BEG(0) + RSTRING(repl)->len,
		    RSTRING(str)->ptr + start + BEG(0) + plen,
		    RSTRING(str)->len - start - BEG(0) - plen);
	}
	memcpy(RSTRING(str)->ptr + start + BEG(0),
	       RSTRING(repl)->ptr, RSTRING(repl)->len);
	t_mm->real += RSTRING(repl)->len - plen;
	if (tainted) OBJ_TAINT(obj);

	res = obj;
    }
    rb_gc_force_recycle(str);
    return res;
}

static VALUE
mm_gsub_bang(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE pat, val, repl, match, str;
    struct re_registers *regs;
    long beg, offset;
    int start, iter = 0;
    int tainted = 0;
    long plen;
    mm_mmap *t_mm;

    if (argc == 1 && rb_block_given_p()) {
	iter = 1;
    }
    else if (argc == 2) {
	repl = rb_str_to_str(argv[1]);
	if (OBJ_TAINTED(repl)) tainted = 1;
    }
    else {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)", argc);
    }
    GetMmap(obj, t_mm, MM_MODIFY);
    str = mm_str(obj, MM_MODIFY | MM_ORIGIN);

    pat = get_pat(argv[0]);
    offset = 0;
    beg = rb_reg_search(pat, str, 0, 0);
    if (beg < 0) {
	rb_gc_force_recycle(str);
	return Qnil;
    }
    while (beg >= 0) {
	start = mm_correct_backref(t_mm);
	match = rb_backref_get();
	regs = RMATCH(match)->regs;
	if (iter) {
	    rb_match_busy(match);
	    val = rb_obj_as_string(rb_yield(rb_reg_nth_match(0, match)));
	    rb_backref_set(match);
	}
	else {
	    RSTRING(str)->ptr += start;
	    val = rb_reg_regsub(repl, str, regs);
	    RSTRING(str)->ptr -= start;
	}
	if (OBJ_TAINTED(repl)) tainted = 1;
	plen = END(0) - BEG(0);
	if ((t_mm->real + RSTRING(val)->len - plen) > t_mm->len) {
	    mm_realloc(t_mm, RSTRING(str)->len + RSTRING(val)->len - plen);
	}
	if (RSTRING(val)->len != plen) {
	    if (t_mm->flag & MM_FIXED) {
		rb_raise(rb_eTypeError, "try to change the size of a fixed map");
	    }
	    memmove(RSTRING(str)->ptr + start + BEG(0) + RSTRING(val)->len,
		    RSTRING(str)->ptr + start + BEG(0) + plen,
		    RSTRING(str)->len - start - BEG(0) - plen);
	}
	memcpy(RSTRING(str)->ptr + start + BEG(0),
	       RSTRING(val)->ptr, RSTRING(val)->len);
	RSTRING(str)->len += RSTRING(val)->len - plen;
	t_mm->real = RSTRING(str)->len;
	if (BEG(0) == END(0)) {
	    offset = start + END(0) + mbclen2(RSTRING(str)->ptr[END(0)], pat);
	    offset += RSTRING(val)->len - plen;
	}
	else {
	    offset = start + END(0) + RSTRING(val)->len - plen;
	}
	if (offset > RSTRING(str)->len) break;
	beg = rb_reg_search(pat, str, offset, 0);
    }
    rb_backref_set(match);
    if (tainted) OBJ_TAINT(obj);
    rb_gc_force_recycle(str);
    return obj;
}

static VALUE mm_index __((int, VALUE *, VALUE));

#if RUBY_VERSION_CODE >= 171

static VALUE
mm_subpat_set(obj, re, offset, val)
    VALUE obj, re;
    int offset;
    VALUE val;
{
    VALUE str, match;
    int start, end, len;
    mm_mmap *t_mm;
    
    str = mm_str(obj, MM_MODIFY | MM_ORIGIN);
    if (rb_reg_search(re, str, 0, 0) < 0) {
	rb_raise(rb_eIndexError, "regexp not matched");
    }
    match = rb_backref_get();
    if (offset >= RMATCH(match)->regs->num_regs) {
	rb_raise(rb_eIndexError, "index %d out of regexp", offset);
    }

    start = RMATCH(match)->BEG(offset);
    if (start == -1) {
	rb_raise(rb_eIndexError, "regexp group %d not matched", offset);
    }
    end = RMATCH(match)->END(offset);
    len = end - start;
    GetMmap(str, t_mm, MM_MODIFY);
    mm_update(t_mm, start, len, val);
}

#endif

static VALUE
mm_aset(str, indx, val)
    VALUE str;
    VALUE indx, val;
{
    long idx, beg;
    mm_mmap *t_mm;

    GetMmap(str, t_mm, MM_MODIFY);
    switch (TYPE(indx)) {
      case T_FIXNUM:
      num_index:
	idx = NUM2INT(indx);
	if (idx < 0) {
	    idx += t_mm->real;
	}
	if (idx < 0 || t_mm->real <= idx) {
	    rb_raise(rb_eIndexError, "index %d out of string", idx);
	}
	if (FIXNUM_P(val)) {
	    if (t_mm->real == idx) {
		t_mm->real += 1;
		mm_realloc(t_mm, t_mm->real);
	    }
	    ((char *)t_mm->addr)[idx] = NUM2INT(val) & 0xff;
	}
	else {
	    mm_update(t_mm, idx, 1, val);
	}
	return val;

      case T_REGEXP:
#if RUBY_VERSION_CODE >= 171
	  mm_subpat_set(str, 0, indx, val);
#else 
        {
	    VALUE args[2];
	    args[0] = indx;
	    args[1] = val;
	    mm_sub_bang(2, args, str);
	}
#endif
	return val;

      case T_STRING:
	beg = mm_index(1, &indx, str);
	if (beg != -1) {
	    mm_update(t_mm, beg, RSTRING(indx)->len, val);
	}
	return val;

      default:
	/* check if indx is Range */
	{
	    long beg, len;
	    if (rb_range_beg_len(indx, &beg, &len, t_mm->real, 2)) {
		mm_update(t_mm, beg, len, val);
		return val;
	    }
	}
	idx = NUM2LONG(indx);
	goto num_index;
    }
}

static VALUE
mm_aset_m(argc, argv, str)
    int argc;
    VALUE *argv;
    VALUE str;
{
    mm_mmap *t_mm;

    GetMmap(str, t_mm, MM_MODIFY);
    if (argc == 3) {
	long beg, len;

#if RUBY_VERSION_CODE >= 171
	if (TYPE(argv[0]) == T_REGEXP) {
	    mm_subpat_set(str, argv[0], NUM2INT(argv[1]), argv[2]);
	}
	else
#endif
	{
	    beg = NUM2INT(argv[0]);
	    len = NUM2INT(argv[1]);
	    mm_update(t_mm, beg, len, argv[2]);
	}
	return argv[2];
    }
    if (argc != 2) {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)", argc);
    }
    return mm_aset(str, argv[0], argv[1]);
}

static VALUE
mm_insert(str, idx, str2)
    VALUE str, idx, str2;
{
    mm_mmap *t_mm;
    long pos = NUM2LONG(idx);

    GetMmap(str, t_mm, MM_MODIFY);
    if (pos == -1) {
	pos = RSTRING(str)->len;
    }
    else if (pos < 0) {
	pos++;
    }
    mm_update(t_mm, pos, 0, str2);
    return str;
}

static VALUE mm_aref_m _((int, VALUE *, VALUE));

static VALUE
mm_slice_bang(argc, argv, str)
    int argc;
    VALUE *argv;
    VALUE str;
{
    VALUE result;
    VALUE buf[3];
    int i;

    if (argc < 1 || 2 < argc) {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)", argc);
    }
    for (i = 0; i < argc; i++) {
	buf[i] = argv[i];
    }
    buf[i] = rb_str_new(0,0);
    result = mm_aref_m(argc, buf, str);
#if RUBY_VERSION_CODE >= 168
    if (!NIL_P(result)) {
#endif
	mm_aset_m(argc+1, buf, str);
#if RUBY_VERSION_CODE >= 168
    }
#endif
    return result;
}

static VALUE
mm_cat(str, ptr, len)
    VALUE str;
    const char *ptr;
    long len;
{
    mm_mmap *t_mm;
    char *sptr;

    GetMmap(str, t_mm, MM_MODIFY);
    if (len > 0) {
	int poffset = -1;
	sptr = (char *)t_mm->addr;

	if (sptr <= ptr &&
	    ptr < sptr + t_mm->real) {
	    poffset = ptr - sptr;
	}
	mm_realloc(t_mm, t_mm->real + len);
	sptr = (char *)t_mm->addr;
	if (ptr) {
	    if (poffset >= 0) ptr = sptr + poffset;
	    memcpy(sptr + t_mm->real, ptr, len);
	}
	t_mm->real += len;
    }
    return str;
}

static VALUE
mm_append(str1, str2)
    VALUE str1, str2;
{
    str2 = rb_str_to_str(str2);
    str1 = mm_cat(str1, StringValuePtr(str2), RSTRING(str2)->len);
    return str1;
}

static VALUE
mm_concat(str1, str2)
    VALUE str1, str2;
{
    if (FIXNUM_P(str2)) {
	int i = FIX2INT(str2);
	if (0 <= i && i <= 0xff) { /* byte */
	    char c = i;
	    return mm_cat(str1, &c, 1);
	}
    }
    str1 = mm_append(str1, str2);
    return str1;
}

#if RUBY_VERSION_CODE < 171

static VALUE
mm_strip_bang(str)
    VALUE str;
{
    char *s, *t, *e;
    mm_mmap *t_mm;

    GetMmap(str, t_mm, MM_MODIFY);
    s = (char *)t_mm->addr;
    e = t = s + t_mm->real;
    while (s < t && ISSPACE(*s)) s++;
    t--;
    while (s <= t && ISSPACE(*t)) t--;
    t++;

    if (t_mm->real != (t - s) && (t_mm->flag & MM_FIXED)) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    t_mm->real = t-s;
    if (s > (char *)t_mm->addr) { 
	memmove(t_mm->addr, s, t_mm->real);
	((char *)t_mm->addr)[t_mm->real] = '\0';
    }
    else if (t < e) {
	((char *)t_mm->addr)[t_mm->real] = '\0';
    }
    else {
	return Qnil;
    }

    return str;
}

#else

static VALUE
mm_lstrip_bang(str)
    VALUE str;
{
    char *s, *t, *e;
    mm_mmap *t_mm;

    GetMmap(str, t_mm, MM_MODIFY);
    s = (char *)t_mm->addr;
    e = t = s + t_mm->real;
    while (s < t && ISSPACE(*s)) s++;

    if (t_mm->real != (t - s) && (t_mm->flag & MM_FIXED)) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    t_mm->real = t - s;
    if (s > (char *)t_mm->addr) { 
	memmove(t_mm->addr, s, t_mm->real);
	((char *)t_mm->addr)[t_mm->real] = '\0';
	return str;
    }
    return Qnil;
}

static VALUE
mm_rstrip_bang(str)
    VALUE str;
{
    char *s, *t, *e;
    mm_mmap *t_mm;

    GetMmap(str, t_mm, MM_MODIFY);
    s = (char *)t_mm->addr;
    e = t = s + t_mm->real;
    t--;
    while (s <= t && ISSPACE(*t)) t--;
    t++;
    if (t_mm->real != (t - s) && (t_mm->flag & MM_FIXED)) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    t_mm->real = t - s;
    if (t < e) {
	((char *)t_mm->addr)[t_mm->real] = '\0';
	return str;
    }
    return Qnil;
}

static VALUE
mm_strip_bang(str)
    VALUE str;
{
    VALUE l = mm_lstrip_bang(str);
    VALUE r = mm_rstrip_bang(str);

    if (NIL_P(l) && NIL_P(r)) return Qnil;
    return str;
}

#endif

#define MmapStr(b, recycle)						    \
do {									    \
    recycle = 0;							    \
    if (TYPE(b) == T_DATA &&  RDATA(b)->dfree == (RUBY_DATA_FUNC)mm_free) { \
	recycle = 1;							    \
	b = mm_str(b, MM_ORIGIN);					    \
    }									    \
    else {								    \
	b = rb_str_to_str(b);						    \
    }									    \
} while (0);
 
 
static VALUE
mm_cmp(a, b)
    VALUE a, b;
{
    int result;
    int recycle = 0;

    a = mm_str(a, MM_ORIGIN);
    MmapStr(b, recycle);
    result = rb_str_cmp(a, b);
    rb_gc_force_recycle(a);
    if (recycle) rb_gc_force_recycle(b);
    return INT2FIX(result);
}

#if RUBY_VERSION_CODE >= 171

static VALUE
mm_casecmp(a, b)
    VALUE a, b;
{
    VALUE result;
    int recycle = 0;

    a = mm_str(a, MM_ORIGIN);
    MmapStr(b, recycle);
    result = rb_funcall2(a, rb_intern("casecmp"), 1, &b);
    rb_gc_force_recycle(a);
    if (recycle) rb_gc_force_recycle(b);
    return result;
}

#endif

static VALUE
mm_equal(a, b)
    VALUE a, b;
{
    VALUE result;
    
    if (a == b) return Qtrue;
    if (TYPE(b) != T_DATA || RDATA(b)->dfree != (RUBY_DATA_FUNC)mm_free)
	return Qfalse;

    a = mm_str(a, MM_ORIGIN);
    b = mm_str(b, MM_ORIGIN);
    result = rb_funcall2(a, rb_intern("=="), 1, &b);
    rb_gc_force_recycle(a);
    rb_gc_force_recycle(b);
    return result;
}

static VALUE
mm_eql(a, b)
    VALUE a, b;
{
    VALUE result;
    
    if (a == b) return Qtrue;
    if (TYPE(b) != T_DATA || RDATA(b)->dfree != (RUBY_DATA_FUNC)mm_free)
	return Qfalse;

    a = mm_str(a, MM_ORIGIN);
    b = mm_str(b, MM_ORIGIN);
    result = rb_funcall2(a, rb_intern("eql?"), 1, &b);
    rb_gc_force_recycle(a);
    rb_gc_force_recycle(b);
    return result;
}

static VALUE
mm_hash(a)
    VALUE a;
{
    VALUE b;
    int res;

    b = mm_str(a, MM_ORIGIN);
    res = rb_str_hash(b);
    rb_gc_force_recycle(b);
    return INT2FIX(res);
}

static VALUE
mm_size(a)
    VALUE a;
{
    mm_mmap *t_mm;

    GetMmap(a, t_mm, 0);
    return INT2NUM(t_mm->real);
}

static VALUE
mm_empty(a)
    VALUE a;
{
    mm_mmap *t_mm;

    GetMmap(a, t_mm, 0);
    if (t_mm->real == 0) return Qtrue;
    return Qfalse;
}

static VALUE
mm_protect_bang(t)
    VALUE *t;
{
    return rb_funcall2(t[0], (ID)t[1], (int)t[2], (VALUE *)t[3]);
}

static VALUE
mm_recycle(str)
    VALUE str;
{
    rb_gc_force_recycle(str);
    return str;
}

static VALUE
mm_bang_i(obj, flag, id, argc, argv)
    VALUE obj, *argv;
    int flag, id, argc;
{
    VALUE str, res;
    mm_mmap *t_mm;

    GetMmap(obj, t_mm, 0);
    if ((flag & MM_CHANGE) && (t_mm->flag & MM_FIXED)) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    str = mm_str(obj, flag);
    if (flag & MM_PROTECT) {
	VALUE tmp[4];
	tmp[0] = str;
	tmp[1] = (VALUE)id;
	tmp[2] = (VALUE)argc;
	tmp[3] = (VALUE)argv;
	res = rb_ensure(mm_protect_bang, (VALUE)tmp, mm_recycle, str);
    }
    else {
	res = rb_funcall2(str, id, argc, argv);
	rb_gc_force_recycle(str);
    }
    if (res == Qnil) return res;
    GetMmap(obj, t_mm, 0);
    t_mm->real = RSTRING(str)->len;
    return (flag & MM_ORIGIN)?res:obj;

}

#if RUBY_VERSION_CODE >= 180

static VALUE
mm_match_m(a, b)
    VALUE a, b;
{
    return mm_bang_i(a, MM_ORIGIN, rb_intern("match"), 1, &b);
}

#endif

static VALUE
mm_upcase_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_MODIFY, rb_intern("upcase!"), 0, 0);
}

static VALUE
mm_downcase_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_MODIFY, rb_intern("downcase!"), 0, 0);
}

static VALUE
mm_capitalize_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_MODIFY, rb_intern("capitalize!"), 0, 0);
}

static VALUE
mm_swapcase_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_MODIFY, rb_intern("swapcase!"), 0, 0);
}
 
static VALUE
mm_reverse_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_MODIFY, rb_intern("reverse!"), 0, 0);
}

static VALUE
mm_chop_bang(a)
    VALUE a;
{
    return mm_bang_i(a, MM_CHANGE, rb_intern("chop!"), 0, 0);
}

static VALUE
mm_inspect(a)
    VALUE a;
{
    return rb_any_to_s(a);
}

static VALUE
mm_chomp_bang(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_CHANGE | MM_PROTECT, rb_intern("chomp!"), argc, argv);
}

static VALUE
mm_delete_bang(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_CHANGE | MM_PROTECT, rb_intern("delete!"), argc, argv);
}

static VALUE
mm_squeeze_bang(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_CHANGE | MM_PROTECT, rb_intern("squeeze!"), argc, argv);
}

static VALUE
mm_tr_bang(obj, a, b)
    VALUE obj, a, b;
{
    VALUE tmp[2];
    tmp[0] = a;
    tmp[1] = b;
    return mm_bang_i(obj, MM_MODIFY | MM_PROTECT, rb_intern("tr!"), 2, tmp);
}

static VALUE
mm_tr_s_bang(obj, a, b)
    VALUE obj, a, b;
{
    VALUE tmp[2];
    tmp[0] = a;
    tmp[1] = b;
    return mm_bang_i(obj, MM_CHANGE | MM_PROTECT, rb_intern("tr_s!"), 2, tmp);
}

static VALUE
mm_crypt(a, b)
    VALUE a, b;
{
    return mm_bang_i(a, MM_ORIGIN, rb_intern("crypt"), 1, &b);
}

static VALUE
mm_include(a, b)
    VALUE a, b;
{
    return mm_bang_i(a, MM_ORIGIN, rb_intern("include?"), 1, &b);
}

static VALUE
mm_index(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("index"), argc, argv);
}

static VALUE
mm_rindex(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("rindex"), argc, argv);
}

static VALUE
mm_aref_m(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("[]"), argc, argv);
}

static VALUE
mm_sum(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("sum"), argc, argv);
}

static VALUE
mm_split(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("split"), argc, argv);
}

static VALUE
mm_count(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    return mm_bang_i(obj, MM_ORIGIN, rb_intern("count"), argc, argv);
}

static VALUE
mm_internal_each(tmp)
    VALUE *tmp;
{
    return rb_funcall2(tmp[0], (ID)tmp[1], (int)tmp[2], (VALUE *)tmp[3]);
}

static VALUE
mm_scan(obj, a)
    VALUE obj, a;
{
    VALUE tmp[4];

    if (!rb_block_given_p()) {
	return rb_funcall(mm_str(obj, MM_ORIGIN), rb_intern("scan"), 1, a);
    }
    tmp[0] = mm_str(obj, MM_ORIGIN);
    tmp[1] = (VALUE)rb_intern("scan");
    tmp[2] = (VALUE)1;
    tmp[3] = (VALUE)&a;
    rb_iterate(mm_internal_each, (VALUE)tmp, rb_yield, 0);
    return obj;
}

static VALUE
mm_each_line(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    VALUE tmp[4];

    tmp[0] = mm_str(obj, MM_ORIGIN);
    tmp[1] = (VALUE)rb_intern("each_line");
    tmp[2] = (VALUE)argc;
    tmp[3] = (VALUE)argv;
    rb_iterate(mm_internal_each, (VALUE)tmp, rb_yield, 0);
    return obj;
}

static VALUE
mm_each_byte(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    VALUE tmp[4];

    tmp[0] = mm_str(obj, MM_ORIGIN);
    tmp[1] = (VALUE)rb_intern("each_byte");
    tmp[2] = (VALUE)argc;
    tmp[3] = (VALUE)argv;
    rb_iterate(mm_internal_each, (VALUE)tmp, rb_yield, 0);
    return obj;
}

static VALUE
mm_undefined(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    rb_raise(rb_eNameError, "not yet implemented");
}

static VALUE
mm_mlockall(obj, flag)
    VALUE obj, flag;
{
    if (mlockall(NUM2INT(flag)) == -1) {
	rb_raise(rb_eArgError, "mlockall(%d)", errno);
    }
    return Qnil;
}

static VALUE
mm_munlockall(obj)
    VALUE obj;
{
    if (munlockall() == -1) {
	rb_raise(rb_eArgError, "munlockall(%d)", errno);
    }
    return Qnil;
}

static VALUE
mm_mlock(obj)
    VALUE obj;
{
    mm_mmap *t_mm;

    Data_Get_Struct(obj, mm_mmap, t_mm);
    if (t_mm->flag & MM_LOCK) {
	return obj;
    }
    if (mlock(t_mm->addr, t_mm->len) == -1) {
	rb_raise(rb_eArgError, "mlock(%d)", errno);
    }
    t_mm->flag |= MM_LOCK;
    return obj;
}

static VALUE
mm_munlock(obj)
    VALUE obj;
{
    mm_mmap *t_mm;

    Data_Get_Struct(obj, mm_mmap, t_mm);
    if (!(t_mm->flag & MM_LOCK)) {
	return obj;
    }
    if (munlock(t_mm->addr, t_mm->len) == -1) {
	rb_raise(rb_eArgError, "munlock(%d)", errno);
    }
    t_mm->flag &= ~MM_LOCK;
    return obj;
}

void
Init_mmap()
{
    if (rb_const_defined_at(rb_cObject, rb_intern("Mmap"))) {
	rb_raise(rb_eNameError, "class already defined");
    }
    mm_cMap = rb_define_class("Mmap", rb_cObject);
    rb_define_const(mm_cMap, "MS_SYNC", INT2FIX(MS_SYNC));
    rb_define_const(mm_cMap, "MS_ASYNC", INT2FIX(MS_ASYNC));
    rb_define_const(mm_cMap, "MS_INVALIDATE", INT2FIX(MS_INVALIDATE));
    rb_define_const(mm_cMap, "PROT_READ", INT2FIX(PROT_READ));
    rb_define_const(mm_cMap, "PROT_WRITE", INT2FIX(PROT_WRITE));
    rb_define_const(mm_cMap, "PROT_EXEC", INT2FIX(PROT_EXEC));
    rb_define_const(mm_cMap, "PROT_NONE", INT2FIX(PROT_NONE));
    rb_define_const(mm_cMap, "MAP_SHARED", INT2FIX(MAP_SHARED));
    rb_define_const(mm_cMap, "MAP_PRIVATE", INT2FIX(MAP_PRIVATE));
#ifdef MADV_NORMAL
    rb_define_const(mm_cMap, "MADV_NORMAL", INT2FIX(MADV_NORMAL));
    rb_define_const(mm_cMap, "MADV_RANDOM", INT2FIX(MADV_RANDOM));
    rb_define_const(mm_cMap, "MADV_SEQUENTIAL", INT2FIX(MADV_SEQUENTIAL));
    rb_define_const(mm_cMap, "MADV_WILLNEED", INT2FIX(MADV_WILLNEED));
    rb_define_const(mm_cMap, "MADV_DONTNEED", INT2FIX(MADV_DONTNEED));
#endif
#ifdef MAP_DENYWRITE
    rb_define_const(mm_cMap, "MAP_DENYWRITE", INT2FIX(MAP_DENYWRITE));
#endif
#ifdef MAP_EXECUTABLE
    rb_define_const(mm_cMap, "MAP_EXECUTABLE", INT2FIX(MAP_EXECUTABLE));
#endif
#ifdef MAP_NORESERVE
    rb_define_const(mm_cMap, "MAP_NORESERVE", INT2FIX(MAP_NORESERVE));
#endif
#ifdef MAP_LOCKED
    rb_define_const(mm_cMap, "MAP_LOCKED", INT2FIX(MAP_LOCKED));
#endif
#ifdef MAP_GROWSDOWN
    rb_define_const(mm_cMap, "MAP_GROWSDOWN", INT2FIX(MAP_GROWSDOWN));
#endif
#ifdef MAP_ANON
    rb_define_const(mm_cMap, "MAP_ANON", INT2FIX(MAP_ANON));
#endif
#ifdef MAP_ANONYMOUS
    rb_define_const(mm_cMap, "MAP_ANONYMOUS", INT2FIX(MAP_ANONYMOUS));
#endif
#ifdef MAP_NOSYNC
    rb_define_const(mm_cMap, "MAP_NOSYNC", INT2FIX(MAP_NOSYNC));
#endif
    rb_define_const(mm_cMap, "MCL_CURRENT", INT2FIX(MCL_CURRENT));
    rb_define_const(mm_cMap, "MCL_FUTURE", INT2FIX(MCL_FUTURE));

    rb_include_module(mm_cMap, rb_mComparable);
    rb_include_module(mm_cMap, rb_mEnumerable);

#if RUBY_VERSION_CODE >= 172
#if RUBY_VERSION_CODE >= 180
    rb_define_alloc_func(mm_cMap, mm_s_alloc);
#else
    rb_define_singleton_method(mm_cMap, "allocate", mm_s_alloc, 0);
#endif
#else
    rb_define_singleton_method(mm_cMap, "new", mm_s_new, -1);
#endif
    rb_define_singleton_method(mm_cMap, "mlockall", mm_mlockall, 1);
    rb_define_singleton_method(mm_cMap, "lockall", mm_mlockall, 1);
    rb_define_singleton_method(mm_cMap, "munlockall", mm_munlockall, 0);
    rb_define_singleton_method(mm_cMap, "unlockall", mm_munlockall, 0);

    rb_define_method(mm_cMap, "initialize", mm_init, -1);

    rb_define_method(mm_cMap, "unmap", mm_unmap, 0);
    rb_define_method(mm_cMap, "munmap", mm_unmap, 0);
    rb_define_method(mm_cMap, "msync", mm_msync, -1);
    rb_define_method(mm_cMap, "sync", mm_msync, -1);
    rb_define_method(mm_cMap, "flush", mm_msync, -1);
    rb_define_method(mm_cMap, "mprotect", mm_mprotect, 1);
    rb_define_method(mm_cMap, "protect", mm_mprotect, 1);
#ifdef MADV_NORMAL
    rb_define_method(mm_cMap, "madvise", mm_madvise, 1);
    rb_define_method(mm_cMap, "advise", mm_madvise, 1);
#endif
    rb_define_method(mm_cMap, "mlock", mm_mlock, 0);
    rb_define_method(mm_cMap, "lock", mm_mlock, 0);
    rb_define_method(mm_cMap, "munlock", mm_munlock, 0);
    rb_define_method(mm_cMap, "unlock", mm_munlock, 0);

    rb_define_method(mm_cMap, "extend", mm_extend, 1);
    rb_define_method(mm_cMap, "freeze", mm_freeze, 0);
    rb_define_method(mm_cMap, "clone", mm_undefined, -1);
#if RUBY_VERSION_CODE >= 180
    rb_define_method(mm_cMap, "initialize_copy", mm_undefined, -1);
#endif
    rb_define_method(mm_cMap, "dup", mm_undefined, -1);
    rb_define_method(mm_cMap, "<=>", mm_cmp, 1);
    rb_define_method(mm_cMap, "==", mm_equal, 1);
    rb_define_method(mm_cMap, "===", mm_equal, 1);
    rb_define_method(mm_cMap, "eql?", mm_eql, 1);
    rb_define_method(mm_cMap, "hash", mm_hash, 0);
#if RUBY_VERSION_CODE >= 171
    rb_define_method(mm_cMap, "casecmp", mm_casecmp, 1);
#endif
    rb_define_method(mm_cMap, "+", mm_undefined, -1);
    rb_define_method(mm_cMap, "*", mm_undefined, -1);
    rb_define_method(mm_cMap, "%", mm_undefined, -1);
    rb_define_method(mm_cMap, "[]", mm_aref_m, -1);
    rb_define_method(mm_cMap, "[]=", mm_aset_m, -1);
#if RUBY_VERSION_CODE >= 171
    rb_define_method(mm_cMap, "insert", mm_insert, 2);
#endif
    rb_define_method(mm_cMap, "length", mm_size, 0);
    rb_define_method(mm_cMap, "size", mm_size, 0);
    rb_define_method(mm_cMap, "empty?", mm_empty, 0);
    rb_define_method(mm_cMap, "=~", mm_match, 1);
    rb_define_method(mm_cMap, "~", mm_undefined, -1);
#if RUBY_VERSION_CODE >= 180
    rb_define_method(mm_cMap, "match", mm_match_m, 1);
#endif
    rb_define_method(mm_cMap, "succ", mm_undefined, -1);
    rb_define_method(mm_cMap, "succ!", mm_undefined, -1);
    rb_define_method(mm_cMap, "next", mm_undefined, -1);
    rb_define_method(mm_cMap, "next!", mm_undefined, -1);
    rb_define_method(mm_cMap, "upto", mm_undefined, -1);
    rb_define_method(mm_cMap, "index", mm_index, -1);
    rb_define_method(mm_cMap, "rindex", mm_rindex, -1);
    rb_define_method(mm_cMap, "replace", mm_undefined, -1);

    rb_define_method(mm_cMap, "to_i", mm_undefined, -1);
    rb_define_method(mm_cMap, "to_f", mm_undefined, -1);
    rb_define_method(mm_cMap, "to_sym", mm_undefined, -1);
    rb_define_method(mm_cMap, "to_s", rb_any_to_s, 0);
    rb_define_method(mm_cMap, "to_str", mm_to_str, 0);
    rb_define_method(mm_cMap, "inspect", mm_inspect, 0);
    rb_define_method(mm_cMap, "dump", mm_undefined, -1);

    rb_define_method(mm_cMap, "upcase", mm_undefined, -1);
    rb_define_method(mm_cMap, "downcase", mm_undefined, -1);
    rb_define_method(mm_cMap, "capitalize", mm_undefined, -1);
    rb_define_method(mm_cMap, "swapcase", mm_undefined, -1);

    rb_define_method(mm_cMap, "upcase!", mm_upcase_bang, 0);
    rb_define_method(mm_cMap, "downcase!", mm_downcase_bang, 0);
    rb_define_method(mm_cMap, "capitalize!", mm_capitalize_bang, 0);
    rb_define_method(mm_cMap, "swapcase!", mm_swapcase_bang, 0);

    rb_define_method(mm_cMap, "hex", mm_undefined, -1);
    rb_define_method(mm_cMap, "oct", mm_undefined, -1);
    rb_define_method(mm_cMap, "split", mm_split, -1);
    rb_define_method(mm_cMap, "reverse", mm_undefined, -1);
    rb_define_method(mm_cMap, "reverse!", mm_reverse_bang, 0);
    rb_define_method(mm_cMap, "concat", mm_concat, 1);
    rb_define_method(mm_cMap, "<<", mm_concat, 1);
    rb_define_method(mm_cMap, "crypt", mm_crypt, 1);
    rb_define_method(mm_cMap, "intern", mm_undefined, -1);

    rb_define_method(mm_cMap, "include?", mm_include, 1);

    rb_define_method(mm_cMap, "scan", mm_scan, 1);

    rb_define_method(mm_cMap, "ljust", mm_undefined, -1);
    rb_define_method(mm_cMap, "rjust", mm_undefined, -1);
    rb_define_method(mm_cMap, "center", mm_undefined, -1);

    rb_define_method(mm_cMap, "sub", mm_undefined, -1);
    rb_define_method(mm_cMap, "gsub", mm_undefined, -1);
    rb_define_method(mm_cMap, "chop", mm_undefined, -1);
    rb_define_method(mm_cMap, "chomp", mm_undefined, -1);
    rb_define_method(mm_cMap, "strip", mm_undefined, -1);
#if RUBY_VERSION_CODE >= 171
    rb_define_method(mm_cMap, "lstrip", mm_undefined, -1);
    rb_define_method(mm_cMap, "rstrip", mm_undefined, -1);
#endif

    rb_define_method(mm_cMap, "sub!", mm_sub_bang, -1);
    rb_define_method(mm_cMap, "gsub!", mm_gsub_bang, -1);
    rb_define_method(mm_cMap, "strip!", mm_strip_bang, 0);
#if RUBY_VERSION_CODE >= 171
    rb_define_method(mm_cMap, "lstrip!", mm_lstrip_bang, 0);
    rb_define_method(mm_cMap, "rstrip!", mm_rstrip_bang, 0);
#endif
    rb_define_method(mm_cMap, "chop!", mm_chop_bang, 0);
    rb_define_method(mm_cMap, "chomp!", mm_chomp_bang, -1);

    rb_define_method(mm_cMap, "tr", mm_undefined, -1);
    rb_define_method(mm_cMap, "tr_s", mm_undefined, -1);
    rb_define_method(mm_cMap, "delete", mm_undefined, -1);
    rb_define_method(mm_cMap, "squeeze", mm_undefined, -1);
    rb_define_method(mm_cMap, "count", mm_count, -1);

    rb_define_method(mm_cMap, "tr!", mm_tr_bang, 2);
    rb_define_method(mm_cMap, "tr_s!", mm_tr_s_bang, 2);
    rb_define_method(mm_cMap, "delete!", mm_delete_bang, -1);
    rb_define_method(mm_cMap, "squeeze!", mm_squeeze_bang, -1);

    rb_define_method(mm_cMap, "each_line", mm_each_line, -1);
    rb_define_method(mm_cMap, "each", mm_each_line, -1);
    rb_define_method(mm_cMap, "each_byte", mm_each_byte, -1);

    rb_define_method(mm_cMap, "sum", mm_sum, -1);

    rb_define_method(mm_cMap, "slice", mm_aref_m, -1);
    rb_define_method(mm_cMap, "slice!", mm_slice_bang, -1);
}
