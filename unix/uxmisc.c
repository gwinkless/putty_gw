/*
 * PuTTY miscellaneous Unix stuff
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>

#include "putty.h"

unsigned long getticks(void)
{
    /*
     * We want to use milliseconds rather than the microseconds or
     * nanoseconds given by the underlying clock functions, because we
     * need a decent number of them to fit into a 32-bit word so it
     * can be used for keepalives.
     */
#if defined HAVE_CLOCK_GETTIME && defined HAVE_DECL_CLOCK_MONOTONIC
    {
        /* Use CLOCK_MONOTONIC if available, so as to be unconfused if
         * the system clock changes. */
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
            return ts.tv_sec * TICKSPERSEC +
                ts.tv_nsec / (1000000000 / TICKSPERSEC);
    }
#endif
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * TICKSPERSEC + tv.tv_usec / (1000000 / TICKSPERSEC);
    }
}

Filename *filename_from_str(const char *str)
{
    Filename *ret = snew(Filename);
    ret->path = dupstr(str);
    return ret;
}

Filename *filename_copy(const Filename *fn)
{
    return filename_from_str(fn->path);
}

const char *filename_to_str(const Filename *fn)
{
    return fn->path;
}

int filename_equal(const Filename *f1, const Filename *f2)
{
    return !strcmp(f1->path, f2->path);
}

int filename_is_null(const Filename *fn)
{
    return !fn->path[0];
}

void filename_free(Filename *fn)
{
    sfree(fn->path);
    sfree(fn);
}

int filename_serialise(const Filename *f, void *vdata)
{
    char *data = (char *)vdata;
    int len = strlen(f->path) + 1;     /* include trailing NUL */
    if (data) {
        strcpy(data, f->path);
    }
    return len;
}
Filename *filename_deserialise(void *vdata, int maxsize, int *used)
{
    char *data = (char *)vdata;
    char *end;
    end = memchr(data, '\0', maxsize);
    if (!end)
        return NULL;
    end++;
    *used = end - data;
    return filename_from_str(data);
}

char filename_char_sanitise(char c)
{
    if (c == '/')
        return '.';
    return c;
}

#ifdef DEBUG
static FILE *debug_fp = NULL;

void dputs(const char *buf)
{
    if (!debug_fp) {
	debug_fp = fopen("debug.log", "w");
    }

    if (write(1, buf, strlen(buf)) < 0) {} /* 'error check' to placate gcc */

    fputs(buf, debug_fp);
    fflush(debug_fp);
}
#endif

char *get_username(void)
{
    struct passwd *p;
    uid_t uid = getuid();
    char *user, *ret = NULL;

    /*
     * First, find who we think we are using getlogin. If this
     * agrees with our uid, we'll go along with it. This should
     * allow sharing of uids between several login names whilst
     * coping correctly with people who have su'ed.
     */
    user = getlogin();
    setpwent();
    if (user)
	p = getpwnam(user);
    else
	p = NULL;
    if (p && p->pw_uid == uid) {
	/*
	 * The result of getlogin() really does correspond to
	 * our uid. Fine.
	 */
	ret = user;
    } else {
	/*
	 * If that didn't work, for whatever reason, we'll do
	 * the simpler version: look up our uid in the password
	 * file and map it straight to a name.
	 */
	p = getpwuid(uid);
	if (!p)
	    return NULL;
	ret = p->pw_name;
    }
    endpwent();

    return dupstr(ret);
}

/*
 * Display the fingerprints of the PGP Master Keys to the user.
 * (This is here rather than in uxcons because it's appropriate even for
 * Unix GUI apps.)
 */
void pgp_fingerprints(void)
{
    fputs("These are the fingerprints of the PuTTY PGP Master Keys. They can\n"
	  "be used to establish a trust path from this executable to another\n"
	  "one. See the manual for more information.\n"
	  "(Note: these fingerprints have nothing to do with SSH!)\n"
	  "\n"
	  "PuTTY Master Key as of 2015 (RSA, 4096-bit):\n"
	  "  " PGP_MASTER_KEY_FP "\n\n"
	  "Original PuTTY Master Key (RSA, 1024-bit):\n"
	  "  " PGP_RSA_MASTER_KEY_FP "\n"
	  "Original PuTTY Master Key (DSA, 1024-bit):\n"
	  "  " PGP_DSA_MASTER_KEY_FP "\n", stdout);
}

/*
 * Set and clear fcntl options on a file descriptor. We don't
 * realistically expect any of these operations to fail (the most
 * plausible error condition is EBADF, but we always believe ourselves
 * to be passing a valid fd so even that's an assertion-fail sort of
 * response), so we don't make any effort to return sensible error
 * codes to the caller - we just log to standard error and die
 * unceremoniously. However, nonblock and no_nonblock do return the
 * previous state of O_NONBLOCK.
 */
void cloexec(int fd) {
    int fdflags;

    fdflags = fcntl(fd, F_GETFD);
    if (fdflags < 0) {
        fprintf(stderr, "%d: fcntl(F_GETFD): %s\n", fd, strerror(errno));
        exit(1);
    }
    if (fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
        fprintf(stderr, "%d: fcntl(F_SETFD): %s\n", fd, strerror(errno));
        exit(1);
    }
}
void noncloexec(int fd) {
    int fdflags;

    fdflags = fcntl(fd, F_GETFD);
    if (fdflags < 0) {
        fprintf(stderr, "%d: fcntl(F_GETFD): %s\n", fd, strerror(errno));
        exit(1);
    }
    if (fcntl(fd, F_SETFD, fdflags & ~FD_CLOEXEC) < 0) {
        fprintf(stderr, "%d: fcntl(F_SETFD): %s\n", fd, strerror(errno));
        exit(1);
    }
}
int nonblock(int fd) {
    int fdflags;

    fdflags = fcntl(fd, F_GETFL);
    if (fdflags < 0) {
        fprintf(stderr, "%d: fcntl(F_GETFL): %s\n", fd, strerror(errno));
        exit(1);
    }
    if (fcntl(fd, F_SETFL, fdflags | O_NONBLOCK) < 0) {
        fprintf(stderr, "%d: fcntl(F_SETFL): %s\n", fd, strerror(errno));
        exit(1);
    }

    return fdflags & O_NONBLOCK;
}
int no_nonblock(int fd) {
    int fdflags;

    fdflags = fcntl(fd, F_GETFL);
    if (fdflags < 0) {
        fprintf(stderr, "%d: fcntl(F_GETFL): %s\n", fd, strerror(errno));
        exit(1);
    }
    if (fcntl(fd, F_SETFL, fdflags & ~O_NONBLOCK) < 0) {
        fprintf(stderr, "%d: fcntl(F_SETFL): %s\n", fd, strerror(errno));
        exit(1);
    }

    return fdflags & O_NONBLOCK;
}

FILE *f_open(const Filename *filename, char const *mode, int is_private)
{
    if (!is_private) {
	return fopen(filename->path, mode);
    } else {
	int fd;
	assert(mode[0] == 'w');	       /* is_private is meaningless for read,
					  and tricky for append */
	fd = open(filename->path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
	    return NULL;
	return fdopen(fd, mode);
    }
}

FontSpec *fontspec_new(const char *name)
{
    FontSpec *f = snew(FontSpec);
    f->name = dupstr(name);
    return f;
}
FontSpec *fontspec_copy(const FontSpec *f)
{
    return fontspec_new(f->name);
}
void fontspec_free(FontSpec *f)
{
    sfree(f->name);
    sfree(f);
}
int fontspec_serialise(FontSpec *f, void *data)
{
    int len = strlen(f->name);
    if (data)
        strcpy(data, f->name);
    return len + 1;                    /* include trailing NUL */
}
FontSpec *fontspec_deserialise(void *vdata, int maxsize, int *used)
{
    char *data = (char *)vdata;
    char *end = memchr(data, '\0', maxsize);
    if (!end)
        return NULL;
    *used = end - data + 1;
    return fontspec_new(data);
}

char *make_dir_and_check_ours(const char *dirname)
{
    struct stat st;

    /*
     * Create the directory. We might have created it before, so
     * EEXIST is an OK error; but anything else is doom.
     */
    if (mkdir(dirname, 0700) < 0 && errno != EEXIST)
        return dupprintf("%s: mkdir: %s", dirname, strerror(errno));

    /*
     * Now check that that directory is _owned by us_ and not writable
     * by anybody else. This protects us against somebody else
     * previously having created the directory in a way that's
     * writable to us, and thus manipulating us into creating the
     * actual socket in a directory they can see so that they can
     * connect to it and use our authenticated SSH sessions.
     */
    if (stat(dirname, &st) < 0)
        return dupprintf("%s: stat: %s", dirname, strerror(errno));
    if (st.st_uid != getuid())
        return dupprintf("%s: directory owned by uid %d, not by us",
                         dirname, st.st_uid);
    if ((st.st_mode & 077) != 0)
        return dupprintf("%s: directory has overgenerous permissions %03o"
                         " (expected 700)", dirname, st.st_mode & 0777);

    return NULL;
}

char *make_dir_path(const char *path, mode_t mode)
{
    int pos = 0;
    char *prefix;
    while (1) {
        pos += strcspn(path + pos, "/");

        if (pos > 0) {
            prefix = dupprintf("%.*s", pos, path);

            if (mkdir(prefix, mode) < 0 && errno != EEXIST) {
                char *ret = dupprintf("%s: mkdir: %s",
                                      prefix, strerror(errno));
                sfree(prefix);
                return ret;
            }

            sfree(prefix);
        }

        if (!path[pos])
            return NULL;
        pos += strspn(path + pos, "/");
    }
}

char *mkdir_path(Filename *fn) {
    char *ret=NULL;
    char *pos;
    if ((pos=strrchr(fn->path, '/')) != NULL) { /* get the path only */
        char *folderpath=dupprintf("%.*s", pos - fn->path, fn->path);
        if (access(folderpath, F_OK)==0) {
            /* if path already exists, tell caller we're not creating anything */
            ret=dupstr("");
        } else {
            /* go try to create the path then */
            ret=make_dir_path(folderpath, 0777);
        }
        sfree(folderpath);
    } else {
        ret=dupstr(""); /* tell parent we didn't create a path */
    }
    return ret;
}

char *expand_envstrings(char *str) {

/* we just expand ~, ${envname} or $envname. No other expansion is performed. */

    
    char *dest=NULL, *varname, *tmps;
    int i, j, skipnext=0, pass, elen;
/* first pass gets length, second does the actual copy */    
    for (pass=0; pass < 2; pass++) {
        for (i=0, j=0; str[i]; i++) {
            if (str[i]=='\\' && (str[i+1]=='$' || str[i+1]=='~')) {
                skipnext=1;
            } else if (skipnext==0 && str[i]=='$') {
                /* get the env var that this corresponds to */
                if (str[i+1]=='{' && ((tmps=strchr(&str[i+2], '}'))!=NULL)) {
/* I'm going to assume you're not an idiot and we won't have invalid chars inside
   the ${}. That may be erroneous, but the worst that will happen is we get no 
   answer from getenv() */                    
                    elen=tmps-&str[i+2];
                    varname=dupprintf("%.*s", elen, &str[i+2]);
                    i+=elen+2;
                } else if (isalpha(str[i+1]) || str[i+1]=='_') {
                    elen=1;
                    while (isalnum(str[i+elen+1]) || str[i+elen+1]=='_') elen++;
                    varname=dupprintf("%.*s", elen, &str[i+1]);
                    i+=elen;
                } else {
                    elen=-1;
                }
                if (elen > 0) {
                    if ((tmps=getenv(varname))!=NULL) {
                        j+=(pass==1) ? sprintf(&dest[j], "%s", tmps) : strlen(tmps);
                    }
                    sfree(varname);
                }
            } else if (skipnext==0 && str[i]=='~') {
                if ((tmps=getenv("HOME"))!=NULL) {
                    j+=(pass==1) ? sprintf(&dest[j], "%s", tmps) : strlen(tmps);
                }
            } else {
                if (pass) dest[j]=str[i];
                j++;
                skipnext=0;
            }
        }
        if (pass==0) dest=smalloc(j+1);
    }
    dest[j]='\0';
    sfree(str);

    return dest;
}

Filename *ConvertV70LogFileToV71(Filename *fp) {
/* in v71 we started allowing environment variable expansion in logfilenames, 
   so you can do eg $HOME/puttylogs/&H&Y&M&D&T.log; unfortunately because
   $ is a valid filename character, and on v70 we would have simply written
   the string as-was, we have to convert any string loaded from a pre-v71 conf
   and insert backslashes before $. This support function does that. */
    int i, j;
    char *newpath;
    for (j=0,i=0; fp->path[i]; i++) if (fp->path[i]=='$' || fp->path[i]=='~') j++;
/* i now contains the string length, j contains the number of $ signs */
    if (j) {
        newpath=smalloc(j+i+1);
        for (i=0,j=0; fp->path[i]; i++) {
            if (fp->path[i] == '$' || fp->path[i]=='~') {
                newpath[i+j] = '\\';
                j++;
            }
            newpath[i+j] = fp->path[i];
        }
        newpath[i+j+1]='\0';
        sfree(fp->path);
        fp->path = newpath;
    }
    return fp;
}
