/*
  util.c:

Copyright (C) 2001 Tomo.M (author).
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "srelay.h"

#ifdef LOCAL_FAC
# define LOCALFAC  LOCAL_FAC
#else
# define LOCALFAC  0
#endif
#ifdef SYSLOG_FAC
#  define SYSLOGFAC (SYSLOG_FAC|LOCALFAC)
#else
#  define SYSLOGFAC (LOG_DAEMON|LOCALFAC)
#endif

int forcesyslog = 0;

void msg_out(int severity, const char *fmt, ...)
{
  va_list ap;
  int priority;

  switch (severity) {
  case crit:
    priority = SYSLOGFAC|LOG_ERR;
    break;
  case warn:
    priority = SYSLOGFAC|LOG_WARNING;
    break;
  case norm:
  default:
    priority = SYSLOGFAC|LOG_INFO;
    break;
  }

  va_start(ap, fmt);
  if (!forcesyslog && isatty(fileno(stderr))) {
    vfprintf(stderr, fmt, ap);
  } else {
    vsyslog(priority, fmt, ap);
  }
  va_end(ap);
}

/*
  set_blocking:
          i/o mode to descriptor
 */
void set_blocking(int s)
{
  int flags;

  flags = fcntl(s, F_GETFL);
  if (flags >= 0) {
    fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
  }
}

#define ITIMER_REAL     0
int settimer(int sec)
{
  struct itimerval it, ot;

  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = sec;
  it.it_value.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &it, &ot) == 0) {
    return(ot.it_value.tv_sec);
  } else {
    return(-1);
  }
}

/**  SIGALRM handling **/
void timeout(int signo)
{
  return;
}

void reapchild(int signo)
{
  int olderrno = errno;
  pid_t pid;
  int status, count;

  count=0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (count++ > 1000) {
      /* reapchild: waitpid loop */
      break;
    }
  }
  if (cur_child > 0) {
    cur_child--;
  } else {
    cur_child = 0;
  }
  errno = olderrno;
}

void cleanup(int signo)
{
  /* unlink PID file */
  if (pidfile != NULL) {
    seteuid(0);
    unlink(pidfile);
    seteuid(PROCUID);
  }
  msg_out(norm, "sig TERM received. exitting...");
  exit(0);
}

sigfunc_t setsignal(int sig, sigfunc_t handler)
{
  struct sigaction n, o;

  memset(&n, 0, sizeof n);
  n.sa_handler = handler;

  if (sig != SIGALRM) {
    n.sa_flags = SA_RESTART;
  }

  if (sigaction(sig, &n, &o) < 0)
    return SIG_ERR;
  return o.sa_handler;
}

int blocksignal(int sig)
{
  sigset_t sset, oset;

  sigemptyset(&sset);
  sigaddset(&sset, sig);
  if (sigprocmask(SIG_BLOCK, &sset, &oset) < 0)
    return -1;
  else
    return sigismember(&oset, sig);
}

int releasesignal(int sig)
{
  sigset_t sset, oset;

  sigemptyset(&sset);
  sigaddset(&sset, sig);
  if (sigprocmask(SIG_UNBLOCK, &sset, &oset) < 0)
    return -1;
  else
    return sigismember(&oset, sig);
}

#ifndef HAVE_INET_PTON
int inet_pton(int af, char *src, void *dst)
{
#ifdef HAVE_INET_ADDR
  in_addr_t addr;

  if (src == NULL || dst == NULL)
    return(0);

  if (af != PF_INET)
    return(0);

  if ((addr = inet_addr(src)) != -1) {
    memcpy(dst, &addr, sizeof(in_addr_t));
    return(1);
  }
#endif  
  return(0);
}
#endif
