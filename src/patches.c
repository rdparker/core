/* 
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.

*/

/*********************************************************/
/* patches.c                                             */
/*                                                       */
/* Contains any fixes which need to be made because of   */
/* lack of OS support on a given platform                */
/* These are conditionally compiled, pending extensions  */
/* or developments in the OS concerned.                  */
/*********************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static char *cf_format_strtimestamp(struct tm *tm, char *buf);

/*********************************************************/

/* We assume that s is at least MAX_FILENAME large.
 * MapName() is thread-safe, but the argument is modified. */

#ifdef NT
# if defined(__MINGW32__)

char *MapNameCopy(const char *s)
{
    char *str = xstrdup(s);

    char *c = str;
    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }

    return str;
}

char *MapName(char *s)
{
    char *c = s;

    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }
    return s;
}

# elif defined(__CYGWIN__)

char *MapNameCopy(const char *s)
{
    Writer *w = StringWriter();

    /* c:\a\b -> /cygdrive/c\a\b */
    if (s[0] && isalpha(s[0]) && s[1] == ':')
    {
        WriterWriteF(w, "/cygdrive/%c", s[0]);
        s += 2;
    }

    for (; *s; s++)
    {
        /* a//b//c -> a/b/c */
        /* a\\b\\c -> a\b\c */
        if (IsFileSep(*s) && IsFileSep(*(s + 1)))
        {
            continue;
        }

        /* a\b\c -> a/b/c */
        WriterWriteChar(w, *s == '\\' ? '/' : *s);
    }

    return StringWriterClose(w);
}

char *MapName(char *s)
{
    char *ret = MapNameCopy(s);

    if (strlcpy(s, ret, MAX_FILENAME) >= MAX_FILENAME)
    {
        FatalError("Expanded path (%s) is longer than MAX_FILENAME ("
                   TOSTRING(MAX_FILENAME) ") characters",
                   ret);
    }

    return s;
}

# else/* !__MINGW32__ && !__CYGWIN__ */
#  error Unknown NT-based compilation environment
# endif/* __MINGW32__ || __CYGWIN__ */
#else /* !NT */

char *MapName(char *s)
{
    return s;
}

char *MapNameCopy(const char *s)
{
    return xstrdup(s);
}

#endif /* NT */

/*********************************************************/

char *MapNameForward(char *s)
/* Like MapName(), but maps all slashes to forward */
{
    while ((s = strchr(s, '\\')))
    {
        *s = '/';
    }
    return s;
}

/*********************************************************/

#ifndef HAVE_GETNETGRENT

# if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

#  ifndef const
#   define const
#  endif
# endif

/*********************************************************/

int setnetgrent(netgroup)
     const char *netgroup;

{
    return 0;
}

/**********************************************************/

int getnetgrent(a, b, c)
     char **a, **b, **c;

{
    *a = NULL;
    *b = NULL;
    *c = NULL;
    return 0;
}

/***********************************************************/

void endnetgrent()
{
}

#endif

#ifndef HAVE_UNAME

# if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

#  ifndef const
#   define const
#  endif
# endif

/***********************************************************/
/* UNAME is missing on some weird OSes                     */
/***********************************************************/

int uname(struct utsname *sys)
# ifdef MINGW
{
    return NovaWin_uname(sys);
}

# else                          /* NOT MINGW */
{
    char buffer[CF_BUFSIZE], *sp;

    if (gethostname(buffer, CF_BUFSIZE) == -1)
    {
        perror("gethostname");
        exit(1);
    }

    strcpy(sys->nodename, buffer);

    if (strcmp(buffer, AUTOCONF_HOSTNAME) != 0)
    {
        CfOut(cf_verbose, "", "This binary was complied on a different host (%s).\n", AUTOCONF_HOSTNAME);
        CfOut(cf_verbose, "", "This host does not have uname, so I can't tell if it is the exact same OS\n");
    }

    strcpy(sys->sysname, AUTOCONF_SYSNAME);
    strcpy(sys->release, "cfengine-had-to-guess");
    strcpy(sys->machine, "missing-uname(2)");
    strcpy(sys->version, "unknown");

    /* Extract a version number if possible */

    for (sp = sys->sysname; *sp != '\0'; sp++)
    {
        if (isdigit(*sp))
        {
            strcpy(sys->release, sp);
            strcpy(sys->version, sp);
            *sp = '\0';
            break;
        }
    }

    return (0);
}

# endif/* NOT MINGW */

#endif /* NOT HAVE_UNAME */

/***********************************************************/
/* putenv() missing on old BSD systems                     */
/***********************************************************/

#ifndef HAVE_PUTENV

int putenv(char *s)
{
    CfOut(cf_verbose, "", "(This system does not have putenv: cannot update CFALLCLASSES\n");
    return 0;
}

#endif

/***********************************************************/
/* seteuid/gid() missing on some on posix systems          */
/***********************************************************/

#ifndef HAVE_SETEUID

# if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

#  ifndef const
#   define const
#  endif
# endif

int seteuid(uid_t uid)
{
# ifdef HAVE_SETREUID
    return setreuid(-1, uid);
# else
    CfOut(cf_verbose, "", "(This system does not have setreuid (patches.c)\n");
    return -1;
# endif
}

#endif

/***********************************************************/

#ifndef HAVE_SETEGID

int setegid(gid_t gid)
{
# ifdef HAVE_SETREGID
    return setregid(-1, gid);
# else
    CfOut(cf_verbose, "", "(This system does not have setregid (patches.c)\n");
    return -1;
# endif
}

#endif

/*******************************************************************/

int IsPrivileged()
{
#ifdef NT
    return true;
#else
    return (getuid() == 0);
#endif
}

/*******************************************************************/

char *cf_ctime(const time_t *timep)
{
    static char buf[26];

    return cf_strtimestamp_local(*timep, buf);
}

/*
 * This function converts passed time_t value to string timestamp used
 * throughout the system. By sheer coincidence this timestamp has the same
 * format as ctime(3) output on most systems (but NT differs in definition of
 * ctime format, so those are not identical there).
 *
 * Buffer passed should be at least 26 bytes long (including the trailing zero).
 *
 * Please use this function instead of (non-portable and deprecated) ctime_r or
 * (non-threadsafe) cf_ctime or ctime.
 */

/*******************************************************************/

char *cf_strtimestamp_local(const time_t time, char *buf)
{
    struct tm tm;

    if (localtime_r(&time, &tm) == NULL)
    {
        CfOut(cf_verbose, "localtime_r", "Unable to parse passed timestamp");
        return NULL;
    }

    return cf_format_strtimestamp(&tm, buf);
}

/*******************************************************************/

char *cf_strtimestamp_utc(const time_t time, char *buf)
{
    struct tm tm;

    if (gmtime_r(&time, &tm) == NULL)
    {
        CfOut(cf_verbose, "gmtime_r", "Unable to parse passed timestamp");
        return NULL;
    }

    return cf_format_strtimestamp(&tm, buf);
}

/*******************************************************************/

static char *cf_format_strtimestamp(struct tm *tm, char *buf)
{
    /* Security checks */
    if (tm->tm_year < -2899 || tm->tm_year > 8099)
    {
        CfOut(cf_error, "", "Unable to format timestamp: passed year is out of range: %d", tm->tm_year + 1900);
        return NULL;
    }

/* There is no easy way to replicate ctime output by using strftime */

    if (snprintf(buf, 26, "%3.3s %3.3s %2d %02d:%02d:%02d %04d",
                 DAY_TEXT[tm->tm_wday ? (tm->tm_wday - 1) : 6], MONTH_TEXT[tm->tm_mon],
                 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900) >= 26)
    {
        CfOut(cf_error, "", "Unable to format timestamp: passed values are out of range");
        return NULL;
    }

    return buf;
}

/*******************************************************************/

int cf_closesocket(int sd)
{
    int res;

#ifdef MINGW
    res = closesocket(sd);
#else
    res = close(sd);
#endif

    if (res != 0)
    {
        CfOut(cf_error, "cf_closesocket", "!! Could not close socket");
    }

    return res;
}

/*******************************************************************/

int cf_mkdir(const char *path, mode_t mode)
{
#ifdef MINGW
    return NovaWin_mkdir(path, mode);
#else
    return mkdir(path, mode);
#endif
}

/*******************************************************************/

int cf_chmod(const char *path, mode_t mode)
{
#ifdef MINGW
    return NovaWin_chmod(path, mode);
#else
    return chmod(path, mode);
#endif
}

/*******************************************************************/

int cf_rename(const char *oldpath, const char *newpath)
{
#ifdef MINGW
    return NovaWin_rename(oldpath, newpath);
#else
    return rename(oldpath, newpath);
#endif
}

/*******************************************************************/

void OpenNetwork()
{
#ifdef MINGW
    NovaWin_OpenNetwork();
#else
/* no network init on Unix */
#endif
}

/*******************************************************************/

void CloseNetwork()
{
#ifdef MINGW
    NovaWin_CloseNetwork();
#else
/* no network close on Unix */
#endif
}

/*******************************************************************/

void CloseWmi()
{
#ifdef MINGW
    NovaWin_WmiDeInitialize();
#else
/* no WMI on Unix */
#endif
}

/*******************************************************************/

#ifdef MINGW                    // FIXME: Timeouts ignored on windows for now...
unsigned int alarm(unsigned int seconds)
{
    return 0;
}
#endif /* MINGW */

/*******************************************************************/

/*******************************************************************/

int LinkOrCopy(const char *from, const char *to, int sym)
/**
 *  Creates symlink to file on platforms supporting it, copies on
 *  others.
 **/
{

#ifdef MINGW                    // only copy on Windows for now

    if (!CopyFile(from, to, TRUE))
    {
        return false;
    }

#else /* NOT MINGW */

    if (sym)
    {
        if (symlink(from, to) == -1)
        {
            return false;
        }
    }
    else                        // hardlink
    {
        if (link(from, to) == -1)
        {
            return false;
        }
    }

#endif /* NOT MINGW */

    return true;
}

#if !defined(__MINGW32__)

int ExclusiveLockFile(int fd)
{
    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
    };

    while (fcntl(fd, F_SETLKW, &lock) == -1)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    return 0;
}

int ExclusiveUnlockFile(int fd)
{
    return close(fd);
}

#endif


#if defined(__MINGW32__)

/* On Windows, gmtime and localtime buffers are thread-specific,
   so using them in place of the reentrant versions is safe      */

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    struct tm *result_gmtime = gmtime(timep);
    
    if(result_gmtime)
    {
        *result = *result_gmtime;
    }
    
    return result_gmtime;
}


struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    struct tm *result_localtime = localtime(timep);
    
    if(result_localtime)
    {
        *result = *result_localtime;
    }
    
    return result_localtime;
}

#endif  /* __MINGW32__ */
