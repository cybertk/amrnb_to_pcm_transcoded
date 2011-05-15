/*
 *   Copyright 2011, Kyan He <kyan.ql.he@gmail.com>
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
 *   Modified: 
 *      Kyan He <kyan.ql.he@gmail.com> @ Sun May 15 20:56:23 CST 2011
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>


#include "log.h"
#include "util.h"
#include "gsmamr_dec.h"

#define PORT                38858
#define MAX_PACKET_SIZE     1024
#define PCM_FRAME_SIZE      160

void *g_decoder;

void amr_decode(int16_t *samples, void *payload, int length)
{
    unsigned char *bytes = (unsigned char *)payload;
    //enum Frame_Type_3GPP type;

    //type = (Frame_Type_3GPP)(byte[0] >> 3);

    length -= 2;

    if (AMRDecode(g_decoder, 7, bytes, samples, MIME_IETF) != length) {
        E("decode error");
    }
}

void echo(int fd) {

    int sz;
    char buf[MAX_PACKET_SIZE];
    struct sockaddr_in from;
    int from_len;
    struct timeval now;
    int16_t samples[PCM_FRAME_SIZE];

    sz = recvfrom(fd, buf,
            MAX_PACKET_SIZE, 0,
            (struct sockaddr *)&from, &from_len);

    // get current timestamp
    gettimeofday(&now, 0);

    if (sz == 32) {

        // decode amr-nb to pcm 16
        amr_decode(samples, buf, sz);

        D("[%d.%d] forward %d bytes data", now.tv_sec, now.tv_usec, sz);

        sendto(fd, samples, PCM_FRAME_SIZE, 0, (struct sockaddr *)&from, from_len);
    } else if (sz > 0) {
        D("[%d.%d] ignore malformed packet", now.tv_sec, now.tv_usec);
    }
}

int main(int argc, char* argv[])
{
    int s;
    fd_set rfds;

    // create a udp server socket
    s = local_datagram(PORT);

    if (s < 0 || GSMInitDecode(&g_decoder, "RTP")) {
        E("Init error, %s", strerror(errno));
    }

    // make socket non-blocking
    fcntl(s, F_SETFL, O_NONBLOCK);

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);

        if (select(s + 1, &rfds, 0, 0, 0) > 0) {
            if (FD_ISSET(s, &rfds))
                echo(s);
        }
    }
}


