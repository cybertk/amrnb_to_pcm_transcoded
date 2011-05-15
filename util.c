/*
 *   Copyright 2009, Kyan He <kyan.ql.he@gmail.com>
 *
 *   -- util.c --
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *   Modified: 
 *      Kyan He <kyan.ql.he@gmail.com> @ Mon Feb 28 17:44:33 CST 2011
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>


#include "log.h"
#include "list.h"

int local_datagram(int port)
{
    struct sockaddr_in sa;
    int opt = 1;
    int s;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        return -1;
    }

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        close(s);
        return -1;
    }

    return s;
}

int local_socket(int port)
{
    struct sockaddr_in sa;
    int opt = 1;
    int s;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        close(s);
        return -1;
    }

    if(listen(s, 5) < 0) {
        close(s);
        return -1;
    }

    return s;
}

int remote_socket(const char *host, int port)
{
    struct sockaddr_in sa;
    struct hostent *hp;
    int s;

    if(!(hp = gethostbyname(host))){
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_port = htons(port);
    sa.sin_family = hp->h_addrtype;
    memcpy((void*) &sa.sin_addr, (void*) hp->h_addr, hp->h_length);

    if((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    if(connect(s, (struct sockaddr*) &sa, sizeof(sa)) != 0){
        close(s);
        return -1;
    }

    return s;
}


static int log_fd = -1;
/* Inital log level before init.rc is parsed and this this is reset. */
//static int log_level = LOG_DEFAULT_LEVEL;
static int log_level = 0;


void log_set_level(int level) {
    log_level = level;
}

#define LOG_BUF_MAX 512

/*
 * create_socket - creates a Unix domain socket in ANDROID_SOCKET_DIR
 * ("/dev/socket") as dictated in init.rc. This socket is inherited by the
 * daemon. We communicate the file descriptor's value via the environment
 * variable ANDROID_SOCKET_ENV_PREFIX<name> ("ANDROID_SOCKET_foo").
 */
int create_socket(const char *name, int type, mode_t perm, uid_t uid, gid_t gid)
{
    struct sockaddr_un addr;
    int fd, ret;

    fd = socket(PF_UNIX, type, 0);
    if (fd < 0) {
        E("Failed to open socket '%s': %s\n", name, strerror(errno));
        return -1;
    }

    memset(&addr, 0 , sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/%s",
             name);

    ret = unlink(addr.sun_path);
    if (ret != 0 && errno != ENOENT) {
        E("Failed to unlink old socket '%s': %s\n", name, strerror(errno));
        goto out_close;
    }

    ret = bind(fd, (struct sockaddr *) &addr, sizeof (addr));
    if (ret) {
        E("Failed to bind socket '%s': %s\n", name, strerror(errno));
        goto out_unlink;
    }

    chown(addr.sun_path, uid, gid);
    chmod(addr.sun_path, perm);

    D("Created socket '%s' with mode '%o', user '%d', group '%d'\n",
         addr.sun_path, perm, uid, gid);

    return fd;

out_unlink:
    unlink(addr.sun_path);
out_close:
    close(fd);
    return -1;
}

/* reads a file, making sure it is terminated with \n \0 */
void *read_file(const char *fn, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;

    data = 0;
    fd = open(fn, O_RDONLY);
    if(fd < 0) return 0;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz + 2);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);
    data[sz] = '\n';
    data[sz+1] = 0;
    if(_sz) *_sz = sz;
    return data;

oops:
    close(fd);
    if(data != 0) free(data);
    return 0;
}

void list_init(struct listnode *node)
{
    node->next = node;
    node->prev = node;
}

void list_add_tail(struct listnode *head, struct listnode *item)
{
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
}

void list_remove(struct listnode *item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
}

#define MAX_MTD_PARTITIONS 16

/*
 * gettime() - returns the time in seconds of the system's monotonic clock or
 * zero on error.
 */
time_t gettime(void)
{
    struct timespec ts;
    int ret;

 //   ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret < 0) {
        E("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        return 0;
    }

    return ts.tv_sec;
}

int mkdir_recursive(const char *pathname, mode_t mode)
{
    char buf[128];
    const char *slash;
    const char *p = pathname;
    int width;
    int ret;
    struct stat info;

    while ((slash = strchr(p, '/')) != NULL) {
        width = slash - pathname;
        p = slash + 1;
        if (width < 0)
            break;
        if (width == 0)
            continue;
        if ((unsigned int)width > sizeof(buf) - 1) {
            E("path too long for mkdir_recursive\n");
            return -1;
        }
        memcpy(buf, pathname, width);
        buf[width] = 0;
        if (stat(buf, &info) != 0) {
            ret = mkdir(buf, mode);
            if (ret && errno != EEXIST)
                return ret;
        }
    }
    ret = mkdir(pathname, mode);
    if (ret && errno != EEXIST)
        return ret;
    return 0;
}

void sanitize(char *s)
{
    if (!s)
        return;
    while (isalnum(*s))
        s++;
    *s = 0;
}
void make_link(const char *oldpath, const char *newpath)
{
    int ret;
    char buf[256];
    char *slash;
    int width;

    slash = strrchr(newpath, '/');
    if (!slash)
        return;
    width = slash - newpath;
    if (width <= 0 || width > (int)sizeof(buf) - 1)
        return;
    memcpy(buf, newpath, width);
    buf[width] = 0;
    ret = mkdir_recursive(buf, 0755);
    if (ret)
        E("Failed to create directory %s: %s (%d)\n", buf, strerror(errno), errno);

    ret = symlink(oldpath, newpath);
    if (ret && errno != EEXIST)
        E("Failed to symlink %s to %s: %s (%d)\n", oldpath, newpath, strerror(errno), errno);
}

void remove_link(const char *oldpath, const char *newpath)
{
    char path[256];
    ssize_t ret;
    ret = readlink(newpath, path, sizeof(path) - 1);
    if (ret <= 0)
        return;
    path[ret] = 0;
    if (!strcmp(path, oldpath))
        unlink(newpath);
}

int wait_for_file(const char *filename, int timeout)
{
    struct stat info;
    time_t timeout_time = gettime() + timeout;
    int ret = -1;

    while (gettime() < timeout_time && ((ret = stat(filename, &info)) < 0))
        usleep(10000);

    return ret;
}

void open_devnull_stdio(void)
{
    int fd;
    static const char *name = "/dev/__null__";
    if (mknod(name, S_IFCHR | 0600, (1 << 8) | 3) == 0) {
        fd = open(name, O_RDWR);
        unlink(name);
        if (fd >= 0) {
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) {
                close(fd);
            }
            return;
        }
    }

    exit(1);
}
