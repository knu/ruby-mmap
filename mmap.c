#include <ruby.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <rubyio.h>
#include <intern.h>

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

static VALUE mm_cMap;

typedef struct {
    MMAP_RETTYPE addr;
    int smode, pmode, vscope;
    int advice, frozen;
    size_t len, real;
    size_t size;
    off_t offset;
    char *path;
} mm_mmap;

static void
mm_free(t_mm)
    mm_mmap *t_mm;
{
    if (t_mm->path) {
	munmap(t_mm->addr, t_mm->len);
	if (t_mm->real < t_mm->len && t_mm->vscope != MAP_PRIVATE &&
	    truncate(t_mm->path, t_mm->real) == -1) {
            rb_raise(rb_eTypeError, "truncate");
	}
	free(t_mm->path);
    }
    free(t_mm);
}

#define MM_MODIFY 1
#define MM_ORIGIN 2
#define MM_CHANGE (MM_MODIFY | 4)
#define MM_PROTECT 8

#define MM_FROZEN 1
#define MM_FIXED  2

#define GetMmap(obj, t_mm, t_modify)				\
    Data_Get_Struct(obj, mm_mmap, t_mm);			\
    if (!t_mm->path) {						\
	rb_raise(rb_eIOError, "unmapped file");			\
    }								\
    if ((t_modify & MM_MODIFY) && (t_mm->frozen & MM_FROZEN)) {	\
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
	if (t_mm->real < t_mm->len && t_mm->vscope != MAP_PRIVATE &&
	    truncate(t_mm->path, t_mm->real) == -1) {
            rb_raise(rb_eTypeError, "truncate");
	}
	free(t_mm->path);
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
    t_mm->frozen |= MM_FROZEN;
}

static VALUE
mm_str(obj, modify)
    VALUE obj;
    int modify;
{
    mm_mmap *t_mm;
    VALUE ret;

    GetMmap(obj, t_mm, modify & ~MM_ORIGIN);
    if (rb_obj_tainted(obj)) {
	ret = rb_tainted_str_new2("");
    }
    else {
	ret = rb_str_new2("");
    }
    if (t_mm->frozen & MM_FROZEN) ret = rb_obj_freeze(ret);
    free(RSTRING(ret)->ptr);
    RSTRING(ret)->ptr = t_mm->addr;
    RSTRING(ret)->len = t_mm->real;
    if (modify & MM_ORIGIN)
	RSTRING(ret)->orig = ret;
    if (modify & MM_MODIFY) {
	if (OBJ_FROZEN(ret)) rb_error_frozen("mmap");
	if (!OBJ_TAINTED(ret) && rb_safe_level() >= 4)
	    rb_raise(rb_eSecurityError, "Insecure: can't modify mmap");
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
    options = RSTRING(key)->ptr;
    if (strcmp(options, "length") == 0) {
	t_mm->size == NUM2INT(value);
	if (t_mm->size <= 0) {
	    rb_raise(rb_eArgError, "Invalid value for length %d", t_mm->size);
	}
	t_mm->frozen |= MM_FIXED;
    }
    else if (strcmp(options, "offset") == 0) {
	t_mm->offset == NUM2INT(value);
	if (t_mm->offset < 0) {
	    rb_raise(rb_eArgError, "Invalid value for offset %d", t_mm->offset);
	}
	t_mm->frozen |= MM_FIXED;
    }
    else if (strcmp(options, "advice") == 0) {
	t_mm->advice == NUM2INT(value);
    }
    return Qnil;
}

static VALUE
mm_map(argc, argv, obj)
    VALUE obj, *argv;
    int argc;
{
    struct stat st;
    int fd, smode, pmode, vscope;
    MMAP_RETTYPE addr;
    VALUE res, fname, vmode, scope, options;
    mm_mmap *t_mm;
    char *path, *mode;
    size_t size;
    off_t offset;

    options = Qnil;
    if (argc > 1 && TYPE(argv[argc - 1]) == T_HASH) {
	options = argv[argc - 1];
	argc--;
    }
    rb_scan_args(argc, argv, "12", &fname, &vmode, &scope);
    Check_SafeStr(fname);
    path = RSTRING(fname)->ptr;
    mode = NIL_P(vmode) ? "r" : STR2CSTR(vmode);
    if (strcmp(mode, "r") == 0) {
	smode = O_RDONLY;
	pmode = PROT_READ;
    }
    else if (strcmp(mode, "w") == 0) {
	smode = O_WRONLY;
	pmode = PROT_WRITE;
    }
    else if (strcmp(mode, "rw") == 0 || strcmp(mode, "wr") == 0) {
	smode = O_RDWR;
	pmode = PROT_READ | PROT_WRITE;
    }
    else {
	rb_raise(rb_eArgError, "Invalid mode %s", mode);
    }
    vscope = NIL_P(scope) ? MAP_SHARED : NUM2INT(scope);
    if ((fd = open(path, smode)) == -1) {
	rb_raise(rb_eArgError, "Can't open %s", path);
    }
    if (fstat(fd, &st) == -1) {
	rb_raise(rb_eArgError, "Can't stat %s", path);
    }
    res = Data_Make_Struct(mm_cMap, mm_mmap, 0, mm_free, t_mm);
    size = st.st_size;
    offset = 0;
    if (options != Qnil) {
	rb_iterate(rb_each, options, mm_i_options, res);
	if ((t_mm->size + t_mm->offset) > st.st_size) {
	    rb_raise(rb_eArgError, "invalid value for size (%d) or offset (%d)",
		     t_mm->size, t_mm->offset);
	}
	if (t_mm->size) size = t_mm->size;
	offset = t_mm->offset;
    }
    addr = mmap(0, size, pmode, vscope, fd, offset);
    close(fd);
    if (addr == MAP_FAILED || !addr) {
	rb_raise(rb_eArgError, "mmap failed (%x)", addr);
    }
    if (t_mm->advice && madvise(addr, size, t_mm->advice) == -1) {
	rb_raise(rb_eArgError, "madvise(%d)", errno);
    }
    t_mm->addr  = addr;
    t_mm->real = t_mm->len = size;
    t_mm->pmode = pmode;
    t_mm->vscope = vscope;
    t_mm->smode = smode;
    t_mm->path = ruby_strdup(path);
    if (smode == O_RDONLY) {
	res = rb_obj_freeze(res);
	t_mm->frozen |= MM_FROZEN;
    }
    else {
	OBJ_TAINT(res);
    }
    return res;
}

static void
mm_expandf(t_mm, len)
    mm_mmap *t_mm;
    long len;
{
    int fd;

    if (t_mm->vscope == MAP_PRIVATE) {
	rb_raise(rb_eTypeError, "expand for a private map");
    }
    if (t_mm->frozen & MM_FIXED) {
	rb_raise(rb_eTypeError, "expand for a fixed map");
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
    if (t_mm->addr == MAP_FAILED) {
	rb_raise(rb_eArgError, "mmap failed");
    }
    if (t_mm->advice && madvise(t_mm->addr, len, t_mm->advice) == -1) {
	rb_raise(rb_eArgError, "madvise(%d)", errno);
    }
    close(fd);
    t_mm->len  = len;
}

static void
mm_realloc(t_mm, len)
    mm_mmap *t_mm;
    long len;
{
    if (t_mm->frozen & MM_FROZEN) rb_error_frozen("mmap");
    if (len > t_mm->len) mm_expandf(t_mm, len);
}

static VALUE
mm_extend(obj, a)
    VALUE obj, a;
{
    mm_mmap *t_mm;

    GetMmap(obj, t_mm, MM_MODIFY);
    mm_realloc(t_mm, t_mm->len + NEW2LONG(a));
    return INT2NUM(t_mm->len);
}

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
	smode = STR2CSTR(a);
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
    if ((pmode & PROT_WRITE) && (t_mm->frozen & MM_FROZEN)) 
	rb_error_frozen("mmap");
    if ((ret = mprotect(t_mm->addr, t_mm->len, pmode)) != 0) {
	rb_raise(rb_eArgError, "msync(%d)", ret);
    }
    t_mm->pmode = pmode;
    if (pmode & PROT_READ) {
	if (pmode & PROT_WRITE) t_mm->smode = O_RDWR;
	else {
	    t_mm->smode == O_RDONLY;
	    if (t_mm->vscope == MAP_PRIVATE) {
		obj = rb_obj_freeze(obj);
		t_mm->frozen |= MM_FROZEN;
	    }
	}
    }
    else if (pmode & PROT_WRITE) {
	t_mm->smode == O_WRONLY;
    }
    return obj;
}

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

static void
mm_replace(str, beg, len, val)
    mm_mmap *str;
    VALUE val;
    long beg;
    long len;
{
    if (str->frozen & MM_FROZEN) rb_error_frozen("mmap");
    if (str->real < beg + len) {
	len = str->real - beg;
    }
    if ((str->frozen & MM_FIXED) && RSTRING(val)->len != len) {
	rb_raise(rb_eTypeError, "try to change the size of a fixed map");
    }
    if (len < RSTRING(val)->len) {
	mm_realloc(str, str->real+RSTRING(val)->len-len);
    }

    if (RSTRING(val)->len != len) {
	memmove(str->addr + beg + RSTRING(val)->len,
		str->addr + beg + len,
		str->real - (beg + len));
    }
    if (str->real < beg && len < 0) {
	MEMZERO(str->addr + str->real, char, -len);
    }
    if (RSTRING(val)->len > 0) {
	memmove(str->addr+beg, RSTRING(val)->ptr, RSTRING(val)->len);
    }
    str->real += RSTRING(val)->len - len;
}

static VALUE
mm_match(x, y)
    VALUE x, y;
{
    VALUE reg, res;
    long start;

    x = mm_str(x, MM_ORIGIN);
    if (TYPE(y) == T_DATA && rb_obj_is_kind_of(y, mm_cMap)) {
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
mm_correct_backref()
{
    VALUE match;
    struct re_registers *regs;
    int i, start;

    match = rb_backref_get();
    if (NIL_P(match)) return 0;
    if (RMATCH(match)->BEG(0) == -1) return 0;
    start = RMATCH(match)->BEG(0);
    RMATCH(match)->str = rb_str_new(RSTRING(RMATCH(match)->str)->ptr + start,
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
	repl = rb_obj_as_string(argv[1]);;
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
	start = mm_correct_backref();
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
	plen = END(0);
	if (RSTRING(repl)->len > plen) {
	    mm_realloc(t_mm, RSTRING(str)->len + RSTRING(repl)->len - plen);
	    t_mm->real = t_mm->len;
	    RSTRING(str)->ptr = t_mm->addr;
	}
	if (RSTRING(repl)->len != plen) {
	    if (t_mm->frozen & MM_FIXED) {
		rb_raise(rb_eTypeError, "try to change the size of a fixed map");
	    }
	    memmove(RSTRING(str)->ptr + start + RSTRING(repl)->len,
		    RSTRING(str)->ptr + start + plen,
		    RSTRING(str)->len - start - plen);
	}
	memcpy(RSTRING(str)->ptr + start,
	       RSTRING(repl)->ptr, RSTRING(repl)->len);
	RSTRING(str)->len += RSTRING(repl)->len - plen;
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
	repl = rb_obj_as_string(argv[1]);
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
	start = mm_correct_backref();
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
	plen = END(0);
	if ((t_mm->real + RSTRING(val)->len - plen) > t_mm->len) {
	    mm_realloc(t_mm, RSTRING(str)->len + 2 * RSTRING(val)->len);
	    RSTRING(str)->ptr = t_mm->addr;
	}
	if (RSTRING(val)->len != plen) {
	    if (t_mm->frozen & MM_FIXED) {
		rb_raise(rb_eTypeError, "try to change the size of a fixed map");
	    }
	    memmove(RSTRING(str)->ptr + start + RSTRING(val)->len,
		    RSTRING(str)->ptr + start + plen,
		    RSTRING(str)->len - start - plen);
	}
	memcpy(RSTRING(str)->ptr + start,
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
	    if (TYPE(val) != T_STRING) val = rb_str_to_str(val);
	    mm_replace(t_mm, idx, 1, val);
	}
	return val;

      case T_REGEXP:
        {
	    VALUE args[2];
	    args[0] = indx;
	    args[1] = val;
	    mm_sub_bang(2, args, str);
	}
	return val;

      case T_STRING:
	beg = mm_index(1, &indx, str);
	if (beg != -1) {
	    if (TYPE(val) != T_STRING) val = rb_str_to_str(val);
	    mm_replace(str, beg, RSTRING(indx)->len, val);
	}
	return val;

      default:
	/* check if indx is Range */
	{
	    long beg, len;
	    if (rb_range_beg_len(indx, &beg, &len, t_mm->real, 2)) {
		if (TYPE(val) != T_STRING) val = rb_str_to_str(val);
		mm_replace(t_mm, beg, len, val);
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

	if (TYPE(argv[2]) != T_STRING) argv[2] = rb_str_to_str(argv[2]);
	beg = NUM2INT(argv[0]);
	len = NUM2INT(argv[1]);
	if (len < 0) rb_raise(rb_eIndexError, "negative length %d", len);
	if (beg < 0) {
	    beg += t_mm->real;
	}
	if (beg < 0 || t_mm->real < beg) {
	    if (beg < 0) {
		beg -= t_mm->real;
	    }
	    rb_raise(rb_eIndexError, "index %d out of string", beg);
	}
	if (beg + len > t_mm->real) {
	    len = t_mm->real - beg;
	}
	mm_replace(t_mm, beg, len, argv[2]);
	return argv[2];
    }
    if (argc != 2) {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)", argc);
    }
    return mm_aset(str, argv[0], argv[1]);
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
    str1 = mm_cat(str1, RSTRING(str2)->ptr, RSTRING(str2)->len);
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

    if (t_mm->real != (t - s) && (t_mm->frozen & MM_FIXED)) {
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
 
static VALUE
mm_cmp(a, b)
    VALUE a, b;
{
    int result;
    int recycle = 0;

    a = mm_str(a, MM_ORIGIN);
    if (TYPE(b) == T_DATA && rb_obj_is_kind_of(b, mm_cMap) == Qtrue) {
	recycle = 1;
	b = mm_str(b, MM_ORIGIN);
    }
    result = rb_str_cmp(a, b);
    rb_gc_force_recycle(a);
    if (recycle) rb_gc_force_recycle(b);
    return INT2FIX(result);
}

static VALUE
mm_equal(a, b)
    VALUE a, b;
{
    VALUE result;
    int recycle = 0;
    
    a = mm_str(a, MM_ORIGIN);
    if (TYPE(b) == T_DATA && rb_obj_is_kind_of(b, mm_cMap) == Qtrue) {
	recycle = 1;
	b = mm_str(b, MM_ORIGIN);
    }
    result = rb_funcall2(a, rb_intern("=="), 1, &b);
    rb_gc_force_recycle(a);
    if (recycle) rb_gc_force_recycle(b);
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
    if ((flag & MM_CHANGE) && (t_mm->frozen & MM_FIXED)) {
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
    return res;

}

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
mm_aref(argc, argv, obj)
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

void
Init_mmap()
{
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
    rb_define_const(mm_cMap, "MADV_NORMAL", INT2FIX(MADV_NORMAL));
    rb_define_const(mm_cMap, "MADV_RANDOM", INT2FIX(MADV_RANDOM));
    rb_define_const(mm_cMap, "MADV_SEQUENTIAL", INT2FIX(MADV_SEQUENTIAL));
    rb_define_const(mm_cMap, "MADV_WILLNEED", INT2FIX(MADV_WILLNEED));
    rb_define_const(mm_cMap, "MADV_DONTNEED", INT2FIX(MADV_DONTNEED));
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
    rb_include_module(mm_cMap, rb_mComparable);
    rb_include_module(mm_cMap, rb_mEnumerable);
    rb_define_singleton_method(mm_cMap, "new", mm_map, -1);
    rb_define_method(mm_cMap, "unmap", mm_unmap, 0);
    rb_define_method(mm_cMap, "munmap", mm_unmap, 0);
    rb_define_method(mm_cMap, "msync", mm_msync, -1);
    rb_define_method(mm_cMap, "sync", mm_msync, -1);
    rb_define_method(mm_cMap, "flush", mm_msync, -1);
    rb_define_method(mm_cMap, "mprotect", mm_mprotect, 1);
    rb_define_method(mm_cMap, "protect", mm_mprotect, 1);
    rb_define_method(mm_cMap, "madvise", mm_madvise, 1);
    rb_define_method(mm_cMap, "extend", mm_extend, 1);
    rb_define_method(mm_cMap, "freeze", mm_freeze, 1);
    rb_define_method(mm_cMap, "clone", mm_undefined, -1);
    rb_define_method(mm_cMap, "dup", mm_undefined, -1);
    rb_define_method(mm_cMap, "<=>", mm_cmp, 1);
    rb_define_method(mm_cMap, "==", mm_equal, 1);
    rb_define_method(mm_cMap, "===", mm_equal, 1);
    rb_define_method(mm_cMap, "eql?", mm_equal, 1);
    rb_define_method(mm_cMap, "hash", mm_hash, 0);
    rb_define_method(mm_cMap, "+", mm_undefined, -1);
    rb_define_method(mm_cMap, "*", mm_undefined, -1);
    rb_define_method(mm_cMap, "%", mm_undefined, -1);
    rb_define_method(mm_cMap, "[]", mm_aref, -1);
    rb_define_method(mm_cMap, "[]=", mm_aset_m, -1);
    rb_define_method(mm_cMap, "length", mm_size, 0);
    rb_define_method(mm_cMap, "size", mm_size, 0);
    rb_define_method(mm_cMap, "empty?", mm_empty, 0);
    rb_define_method(mm_cMap, "=~", mm_match, 1);
    rb_define_method(mm_cMap, "~", mm_undefined, -1);
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
    rb_define_method(mm_cMap, "to_s", mm_undefined, 0);
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

    rb_define_method(mm_cMap, "sub!", mm_sub_bang, -1);
    rb_define_method(mm_cMap, "gsub!", mm_gsub_bang, -1);
    rb_define_method(mm_cMap, "strip!", mm_strip_bang, -1);
    rb_define_method(mm_cMap, "chop!", mm_chop_bang, 0);
    rb_define_method(mm_cMap, "chomp!", mm_chomp_bang, -1);

    rb_define_method(mm_cMap, "tr", mm_undefined, -1);
    rb_define_method(mm_cMap, "tr_s", mm_undefined, -1);
    rb_define_method(mm_cMap, "delete", mm_undefined, -1);
    rb_define_method(mm_cMap, "squeeze", mm_undefined, -1);

    rb_define_method(mm_cMap, "tr!", mm_tr_bang, 2);
    rb_define_method(mm_cMap, "tr_s!", mm_tr_s_bang, 2);
    rb_define_method(mm_cMap, "delete!", mm_delete_bang, -1);
    rb_define_method(mm_cMap, "squeeze!", mm_squeeze_bang, -1);

    rb_define_method(mm_cMap, "each_line", mm_each_line, -1);
    rb_define_method(mm_cMap, "each", mm_each_line, -1);
    rb_define_method(mm_cMap, "each_byte", mm_each_byte, -1);

    rb_define_method(mm_cMap, "sum", mm_sum, -1);
}
