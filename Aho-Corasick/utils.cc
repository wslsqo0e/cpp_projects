#include "utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cctype>
#include <stdarg.h>
#include <errno.h>

char const *errname[] = {
  /*_00*/ "",              "EPERM",           "ENOENT",       "ESRCH",           "EINTR",
  /*_05*/ "EIO",           "ENXIO",           "E2BIG",        "ENOEXEC",         "EBADF",
  /*_10*/ "ECHILD",        "EAGAIN",          "ENOMEM",       "EACCES",          "EFAULT",
  /*_15*/ "ENOTBLK",       "EBUSY",           "EEXIST",       "EXDEV",           "ENODEV",
  /*_20*/ "ENOTDIR",       "EISDIR",          "EINVAL",       "ENFILE",          "EMFILE",
  /*_25*/ "ENOTTY",        "ETXTBSY",         "EFBIG",        "ENOSPC",          "ESPIPE",
  /*_30*/ "EROFS",         "EMLINK",          "EPIPE",        "EDOM",            "ERANGE",
#if   defined(__FreeBSD__)
  /*_35*/ "EDEADLK",       "EINPROGRESS",     "EALREADY",     "ENOTSOCK",        "EDESTADDRREQ",
  /*_40*/ "EMSGSIZE",      "EPROTOTYPE",      "ENOPROTOOPT",  "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
  /*_45*/ "EOPNOTSUPP",    "EPFNOSUPPORT",    "EAFNOSUPPORT", "EADDRINUSE",      "EADDRNOTAVAIL",
  /*_50*/ "ENETDOWN",      "ENETUNREACH",     "ENETRESET",    "ECONNABORTED",    "ECONNRESET",
  /*_55*/ "ENOBUFS",       "EISCONN",         "ENOTCONN",     "ESHUTDOWN",       "ETOOMANYREFS",
  /*_60*/ "ETIMEDOUT",     "ECONNREFUSED",    "ELOOP",        "ENAMETOOLONG",    "EHOSTDOWN",
  /*_65*/ "EHOSTUNREACH",  "ENOTEMPTY",       "EPROCLIM",     "EUSERS",          "EDQUOT",
  /*_70*/ "ESTALE",        "EREMOTE",         "EBADRPC",      "ERPCMISMATCH",    "EPROGUNAVAIL",
  /*_75*/ "EPROGMISMATCH", "EPROCUNAVAIL",    "ENOLCK",       "ENOSYS",          "EFTYPE",
  /*_80*/ "EAUTH",         "ENEEDAUTH",       "EIDRM",        "ENOMSG",          "EOVERFLOW",
  /*_85*/ "ECANCELED",     "EILSEQ",          "ENOATTR",      "EDOOFUS",         "EBADMSG",
  /*_90*/ "EMULTIHOP",     "ENOLINK",         "EPROTO"
#elif defined(__linux__)
  /*_35*/ "EDEADLK",       "ENAMETOOLONG",    "ENOLCK",       "ENOSYS",          "ENOTEMPTY",
  /*_40*/ "ELOOP",         "E041",            "ENOMSG",       "EIDRM",           "ECHRNG",
  /*_45*/ "EL2NSYNC",      "EL3HLT",          "EL3RST",       "ELNRNG",          "EUNATCH",
  /*_50*/ "ENOCSI",        "EL2HLT",          "EBADE",        "EBADR",           "EXFULL",
  /*_55*/ "ENOANO",        "EBADRQC",         "EBADSLT",      "E058",            "EBFONT",
  /*_60*/ "ENOSTR",        "ENODATA",         "ETIME",        "ENOSR",           "ENONET",
  /*_65*/ "ENOPKG",        "EREMOTE",         "ENOLINK",      "EADV",            "ESRMNT",
  /*_70*/ "ECOMM",         "EPROTO",          "EMULTIHOP",    "EDOTDOT",         "EBADMSG",
  /*_75*/ "EOVERFLOW",     "ENOTUNIQ",        "EBADFD",       "EREMCHG",         "ELIBACC",
  /*_80*/ "ELIBBAD",       "ELIBSCN",         "ELIBMAX",      "ELIBEXEC",        "EILSEQ",
  /*_85*/ "ERESTART",      "ESTRPIPE",        "EUSERS",       "ENOTSOCK",        "EDESTADDRREQ",
  /*_90*/ "EMSGSIZE",      "EPROTOTYPE",      "ENOPROTOOPT",  "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
  /*_95*/ "EOPNOTSUPP",    "EPFNOSUPPORT",    "EAFNOSUPPORT", "EADDRINUSE",      "EADDRNOTAVAIL",
  /*100*/ "ENETDOWN",      "ENETUNREACH",     "ENETRESET",    "ECONNABORTED",    "ECONNRESET",
  /*105*/ "ENOBUFS",       "EISCONN",         "ENOTCONN",     "ESHUTDOWN",       "ETOOMANYREFS",
  /*110*/ "ETIMEDOUT",     "ECONNREFUSED",    "EHOSTDOWN",    "EHOSTUNREACH",    "EALREADY",
  /*115*/ "EINPROGRESS",   "ESTALE",          "EUCLEAN",      "ENOTNAM",         "ENAVAIL",
  /*120*/ "EISNAM",        "EREMOTEIO",       "EDQUOT",       "ENOMEDIUM",       "EMEDIUMTYPE",
  /*125*/ "ECANCELED",     "ENOKEY",          "EKEYEXPIRED",  "EKEYREVOKED",     "EKEYREJECTED",
  /*130*/ "EOWNERDEAD",    "ENOTRECOVERABLE", "ERFKILL"
#endif
};

MEMBUF read_file(const char *filename)
{
  MEMBUF      ret = NILBUF;
  int         fd = filename && *filename && strcmp(filename, "-") ? open(filename, O_RDONLY) : 0;
  struct stat s;

  if (fd < 0 || fstat(fd, &s)) {
    if (fd >= 0) close(fd);
    buffree(ret);
    return NILBUF;
  }

  ret = membuf(s.st_size);
  int nbytes = read(fd, ret.ptr, ret.len);
  if (nbytes < 0) {
    if (fd >= 0) close(fd);
    buffree(ret);
    return NILBUF;
  }
  ret.len = nbytes;

  close(fd);
  ret.ptr[ret.len] = 0;
  return  ret;
}

MEMBUF chomp(MEMBUF buf) {   // split according space character
  if (buf.ptr)
    while (buf.len > 0 && isspace(buf.ptr[buf.len - 1])) {
      buf.ptr[--buf.len] = 0;
    }

  return  buf;
}

void die(char const *fmt, ...)
{
  va_list	vargs;
  va_start(vargs, fmt);
  vfprintf(stderr, fmt, vargs);
  va_end(vargs);
  if (fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, " %s %s", errname[errno], strerror(errno));
  putc('\n', stderr);
  _exit(1);
}

MEMREF* refsplit(char *text, char sep, int *pcount)
{
  char	*cp;
  int		i, nstrs = 0;
  MEMREF      *strv = NULL;

  if (*text) {
    for (cp = text, nstrs = 1; (cp = strchr(cp, sep)); ++cp)
      ++nstrs;

    strv = static_cast<MEMREF*>(malloc(nstrs * sizeof(MEMREF)));

    for (i = 0, cp = text; (cp = const_cast<char*>(strchr(strv[i].ptr = cp, sep))); ++i, ++cp) {
      strv[i].len = cp - strv[i].ptr;
      *cp = 0;
    }

    strv[i].len = strlen(strv[i].ptr);
  }
  if (pcount)
    *pcount = nstrs;
  return    strv;
}

double tick(void)
{
  struct timeval t;
  gettimeofday(&t, 0);
  return t.tv_sec + 1E-6 * t.tv_usec;
}
