/* SQLite's file layer for vo (spec 026 research R5): the only way SQLite reaches files. SQLite is built
 * with SQLITE_OS_OTHER, so this file provides sqlite3_os_init and registers the VFS "vo".
 *
 * Every file SQLite opens (a database, its journal, an attached database, a VACUUM INTO target) is
 * checked against the running grant with rt_path_allowed and opened with rt_open_file (spec 013: from
 * the scope directory, one component at a time, no links). Nothing here raises: a refused path makes the
 * SQLite call fail and is left in sqlite_vfs_denied for the caller (src/sqlite_rt.c), which raises R0010
 * once SQLite has returned, so no runtime error ever unwinds through SQLite's stack.
 * Temporary files never happen (SQLITE_TEMP_STORE=3); the clock fails (research R6); randomness is a
 * fixed pattern; locks are open-file-description locks (per descriptor, so two connections of one
 * program to one file keep their own locks, and they still conflict with other processes' locks). */
#include "vo.h"
#include <sqlite3.h>

#define O_RDWR_ 2
#define O_CREAT_ 0x40
#define F_OFD_GETLK 36
#define F_OFD_SETLK 37
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define AT_EMPTY_PATH 0x1000
#define STATX_SIZE_ 0x200
#define STATX_SIZE_OFF_ 40
#define PENDING_BYTE_ 0x40000000L /* SQLite's lock bytes (os.h) */
#define RESERVED_BYTE_ (PENDING_BYTE_ + 1)
#define SHARED_FIRST_ (PENDING_BYTE_ + 2)
#define SHARED_SIZE_ 510

/* The path and effect of the last refused open, for the caller to report (R0010). */
Str *sqlite_vfs_denied;
long sqlite_vfs_denied_bit;

typedef struct {
    sqlite3_file base;
    int fd;
    int lock; /* SQLITE_LOCK_NONE .. SQLITE_LOCK_EXCLUSIVE */
} VoFile;

/* struct flock on Linux x86-64, ARM64 and RISC-V 64 */
typedef struct {
    short type, whence;
    long start, len;
    int pid, pad;
} Flock;

static int ofd_lock(int fd, int type, long start, long len)
{
    Flock fl = {(short)type, 0, start, len, 0, 0};
    return vo_syscall3(SYS_fcntl, fd, F_OFD_SETLK, (long)&fl) < 0 ? -1 : 0;
}

static int vo_close(sqlite3_file *f)
{
    VoFile *v = (VoFile *)f;
    if (v->fd >= 0) sys_close(v->fd);
    v->fd = -1;
    return SQLITE_OK;
}

static int vo_read(sqlite3_file *f, void *buf, int amt, sqlite3_int64 off)
{
    VoFile *v = (VoFile *)f;
    long got = 0;
    while (got < amt) {
        long r = vo_syscall4(SYS_pread64, v->fd, (long)buf + got, amt - got, off + got);
        if (r < 0) return SQLITE_IOERR_READ;
        if (r == 0) break;
        got += r;
    }
    if (got < amt) { /* SQLite expects the rest zeroed on a short read */
        char *b = buf;
        for (long i = got; i < amt; i++) b[i] = 0;
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int vo_write(sqlite3_file *f, const void *buf, int amt, sqlite3_int64 off)
{
    VoFile *v = (VoFile *)f;
    long done = 0;
    while (done < amt) {
        long r = vo_syscall4(SYS_pwrite64, v->fd, (long)buf + done, amt - done, off + done);
        if (r <= 0) return r == -28 ? SQLITE_FULL : SQLITE_IOERR_WRITE; /* ENOSPC */
        done += r;
    }
    return SQLITE_OK;
}

static int vo_truncate(sqlite3_file *f, sqlite3_int64 size)
{
    return vo_syscall2(SYS_ftruncate, ((VoFile *)f)->fd, size) < 0 ? SQLITE_IOERR_TRUNCATE : SQLITE_OK;
}

static int vo_sync(sqlite3_file *f, int flags)
{
    (void)flags;
    return vo_syscall1(SYS_fsync, ((VoFile *)f)->fd) < 0 ? SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int vo_file_size(sqlite3_file *f, sqlite3_int64 *size)
{
    long stx[32]; /* struct statx, 256 bytes */
    if (vo_syscall5(SYS_statx, ((VoFile *)f)->fd, (long)"", AT_EMPTY_PATH, STATX_SIZE_, (long)stx) < 0)
        return SQLITE_IOERR_FSTAT;
    *size = *(long *)((char *)stx + STATX_SIZE_OFF_);
    return SQLITE_OK;
}

/* SQLite's locking protocol over the lock bytes, as its Unix VFS does it (os_unix.c unixLock). */
static int vo_lock(sqlite3_file *f, int level)
{
    VoFile *v = (VoFile *)f;
    if (v->lock >= level) return SQLITE_OK;
    if (level == SQLITE_LOCK_SHARED) {
        if (ofd_lock(v->fd, F_RDLCK, PENDING_BYTE_, 1)) return SQLITE_BUSY; /* no writer is entering */
        int r = ofd_lock(v->fd, F_RDLCK, SHARED_FIRST_, SHARED_SIZE_);
        ofd_lock(v->fd, F_UNLCK, PENDING_BYTE_, 1);
        if (r) return SQLITE_BUSY;
    } else if (level == SQLITE_LOCK_RESERVED) {
        if (ofd_lock(v->fd, F_WRLCK, RESERVED_BYTE_, 1)) return SQLITE_BUSY;
    } else { /* PENDING, then EXCLUSIVE */
        if (v->lock < SQLITE_LOCK_PENDING && ofd_lock(v->fd, F_WRLCK, PENDING_BYTE_, 1)) return SQLITE_BUSY;
        v->lock = SQLITE_LOCK_PENDING;
        if (level == SQLITE_LOCK_EXCLUSIVE && ofd_lock(v->fd, F_WRLCK, SHARED_FIRST_, SHARED_SIZE_)) return SQLITE_BUSY;
    }
    v->lock = level;
    return SQLITE_OK;
}

static int vo_unlock(sqlite3_file *f, int level)
{
    VoFile *v = (VoFile *)f;
    if (v->lock <= level) return SQLITE_OK;
    if (level == SQLITE_LOCK_SHARED) {
        if (v->lock >= SQLITE_LOCK_EXCLUSIVE && ofd_lock(v->fd, F_RDLCK, SHARED_FIRST_, SHARED_SIZE_))
            return SQLITE_IOERR_RDLOCK;
        ofd_lock(v->fd, F_UNLCK, PENDING_BYTE_, 2); /* PENDING and RESERVED */
    } else {
        ofd_lock(v->fd, F_UNLCK, 0, 0); /* everything */
    }
    v->lock = level;
    return SQLITE_OK;
}

static int vo_check_reserved(sqlite3_file *f, int *out)
{
    VoFile *v = (VoFile *)f;
    *out = 0;
    if (v->lock >= SQLITE_LOCK_RESERVED) {
        *out = 1;
        return SQLITE_OK;
    }
    Flock fl = {F_WRLCK, 0, RESERVED_BYTE_, 1, 0, 0};
    if (vo_syscall3(SYS_fcntl, v->fd, F_OFD_GETLK, (long)&fl) < 0) return SQLITE_IOERR_CHECKRESERVEDLOCK;
    *out = fl.type != F_UNLCK;
    return SQLITE_OK;
}

static int vo_file_control(sqlite3_file *f, int op, void *arg)
{
    (void)f;
    (void)op;
    (void)arg;
    return SQLITE_NOTFOUND;
}

static int vo_sector_size(sqlite3_file *f)
{
    (void)f;
    return 4096;
}

static int vo_device(sqlite3_file *f)
{
    (void)f;
    return 0;
}

static const sqlite3_io_methods vo_io = {
    1,           vo_close,      vo_read,       vo_write,          vo_truncate, vo_sync, vo_file_size,
    vo_lock,     vo_unlock,     vo_check_reserved, vo_file_control, vo_sector_size, vo_device,
    0,           0,             0,             0,                 0,           0,
};

/* name as a Str, or 0 for no name (a temporary file) */
static Str *path_of(const char *name)
{
    if (!name) return 0;
    long n = 0;
    while (name[n]) n++;
    return str_new((char *)name, n);
}

static int refuse(long bit, Str *path)
{
    if (!sqlite_vfs_denied) { /* the first refusal is the one reported */
        sqlite_vfs_denied = path;
        sqlite_vfs_denied_bit = bit;
    }
    return SQLITE_AUTH;
}

static int vo_open(sqlite3_vfs *vfs, sqlite3_filename name, sqlite3_file *f, int flags, int *out_flags)
{
    (void)vfs;
    VoFile *v = (VoFile *)f;
    v->base.pMethods = 0;
    v->fd = -1;
    v->lock = SQLITE_LOCK_NONE;
    Str *path = path_of(name);
    if (!path) return SQLITE_CANTOPEN; /* never: temporary storage is in memory */
    GC_ROOT(&path);
    int write = (flags & SQLITE_OPEN_READWRITE) != 0;
    if (!rt_path_allowed(EF_FREAD, path)) {
        GC_UNROOT(1);
        return refuse(EF_FREAD, path);
    }
    if (write && !rt_path_allowed(EF_FWRITE, path)) {
        GC_UNROOT(1);
        return refuse(EF_FWRITE, path);
    }
    long oflags = write ? O_RDWR_ : O_RDONLY;
    if (flags & SQLITE_OPEN_CREATE) oflags |= O_CREAT_;
    long fd = rt_open_file(write ? EF_FWRITE : EF_FREAD, path, oflags);
    GC_UNROOT(1);
    if (fd < 0) return SQLITE_CANTOPEN;
    v->fd = (int)fd;
    v->base.pMethods = &vo_io;
    if (out_flags) *out_flags = flags;
    return SQLITE_OK;
}

static int vo_delete(sqlite3_vfs *vfs, const char *name, int sync)
{
    (void)vfs;
    (void)sync;
    Str *path = path_of(name), *base = 0;
    GC_ROOT(&path);
    GC_ROOT(&base);
    int r = SQLITE_OK;
    if (!rt_path_allowed(EF_FWRITE, path)) {
        r = refuse(EF_FWRITE, path);
    } else {
        long dir = rt_open_parent(EF_FWRITE, path, &base, 0);
        if (dir < 0 || vo_syscall3(SYS_unlinkat, dir, (long)base->data, 0) < 0) r = SQLITE_IOERR_DELETE;
        if (dir >= 0) sys_close((int)dir);
    }
    GC_UNROOT(2);
    return r;
}

static int vo_access(sqlite3_vfs *vfs, const char *name, int flags, int *out)
{
    (void)vfs;
    (void)flags;
    Str *path = path_of(name), *base = 0;
    GC_ROOT(&path);
    GC_ROOT(&base);
    *out = 0;
    if (rt_path_allowed(EF_FREAD, path)) {
        long stx[32];
        long dir = rt_open_parent(EF_FREAD, path, &base, 0);
        if (dir >= 0) {
            *out = vo_syscall5(SYS_statx, dir, (long)base->data, 0x100 /* no follow */, STATX_SIZE_, (long)stx) == 0;
            sys_close((int)dir);
        }
    }
    GC_UNROOT(2);
    return SQLITE_OK;
}

static int vo_full_path(sqlite3_vfs *vfs, const char *name, int n, char *out)
{
    (void)vfs;
    int i = 0;
    for (; name[i] && i < n - 1; i++) out[i] = name[i];
    out[i] = 0;
    return name[i] ? SQLITE_CANTOPEN : SQLITE_OK;
}

static int vo_randomness(sqlite3_vfs *vfs, int n, char *out)
{
    (void)vfs;
    for (int i = 0; i < n; i++) out[i] = (char)(i * 131 + 7); /* fixed: no hidden randomness */
    return n;
}

static int vo_sleep(sqlite3_vfs *vfs, int us)
{
    (void)vfs;
    long ts[2] = {us / 1000000, (us % 1000000) * 1000L};
    vo_syscall5(SYS_ppoll, 0, 0, (long)ts, 0, 8);
    return us;
}

static int vo_time(sqlite3_vfs *vfs, double *out)
{
    (void)vfs;
    (void)out;
    return SQLITE_ERROR; /* SQL may not read the clock (research R6) */
}

static int vo_time64(sqlite3_vfs *vfs, sqlite3_int64 *out)
{
    (void)vfs;
    (void)out;
    return SQLITE_ERROR;
}

static int vo_last_error(sqlite3_vfs *vfs, int n, char *out)
{
    (void)vfs;
    (void)n;
    (void)out;
    return 0;
}

static sqlite3_vfs vo_vfs = {
    2, (int)sizeof(VoFile), 1024, 0, "vo", 0, vo_open, vo_delete, vo_access, vo_full_path,
    0, 0, 0, 0, vo_randomness, vo_sleep, vo_time, vo_last_error, vo_time64, 0, 0, 0,
};

int sqlite3_os_init(void)
{
    return sqlite3_vfs_register(&vo_vfs, 1);
}

int sqlite3_os_end(void)
{
    return SQLITE_OK;
}
