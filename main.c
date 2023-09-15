/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "bearssl.h"

/*
 * Connect to the specified host and port. The connected socket is
 * returned, or -1 on error.
 */
static int
host_connect(const char *host, const char *port)
{
	struct addrinfo hints, *si, *p;
	int fd;
	int err;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	err = getaddrinfo(host, port, &hints, &si);
	if (err != 0) {
		fprintf(stderr, "ERROR: getaddrinfo(): %s\n",
			gai_strerror(err));
		return -1;
	}
	fd = -1;
	for (p = si; p != NULL; p = p->ai_next) {
		struct sockaddr *sa;
		void *addr;
		char tmp[INET6_ADDRSTRLEN + 50];

		sa = (struct sockaddr *)p->ai_addr;
		if (sa->sa_family == AF_INET) {
			addr = &((struct sockaddr_in *)sa)->sin_addr;
		} else if (sa->sa_family == AF_INET6) {
			addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
		} else {
			addr = NULL;
		}
		if (addr != NULL) {
			inet_ntop(p->ai_family, addr, tmp, sizeof tmp);
		} else {
			sprintf(tmp, "<unknown family: %d>",
				(int)sa->sa_family);
		}
		fprintf(stderr, "connecting to: %s\n", tmp);
		fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd < 0) {
			perror("socket()");
			continue;
		}
		if (connect(fd, p->ai_addr, p->ai_addrlen) < 0) {
			perror("connect()");
			close(fd);
			continue;
		}
		break;
	}
	if (p == NULL) {
		freeaddrinfo(si);
		fprintf(stderr, "ERROR: failed to connect\n");
		return -1;
	}
	freeaddrinfo(si);
	fprintf(stderr, "connected.\n");
	return fd;
}

/*
 * Low-level data read callback for the simplified SSL I/O API.
 */
static int
sock_read(void *ctx, unsigned char *buf, size_t len)
{
	for (;;) {
		ssize_t rlen;

		rlen = read(*(int *)ctx, buf, len);
		if (rlen <= 0) {
			if (rlen < 0 && errno == EINTR) {
				continue;
			}
			return -1;
		}
		return (int)rlen;
	}
}

/*
 * Low-level data write callback for the simplified SSL I/O API.
 */
static int
sock_write(void *ctx, const unsigned char *buf, size_t len)
{
	for (;;) {
		ssize_t wlen;

		wlen = write(*(int *)ctx, buf, len);
		if (wlen <= 0) {
			if (wlen < 0 && errno == EINTR) {
				continue;
			}
			return -1;
		}
		return (int)wlen;
	}
}

/*
 * The hardcoded trust anchors. These are the two DN + public key that
 * correspond to the self-signed certificates cert-root-rsa.pem and
 * cert-root-ec.pem.
 *
 * C code for hardcoded trust anchors can be generated with the "brssl"
 * command-line tool (with the "ta" command).
 */

#define TAs_NUM 2

static const unsigned char TA_DN0[] = {
    0x30, 0x61, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x0a,
    0x13, 0x0c, 0x44, 0x69, 0x67, 0x69, 0x43, 0x65, 0x72, 0x74, 0x20, 0x49,
    0x6e, 0x63, 0x31, 0x19, 0x30, 0x17, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13,
    0x10, 0x77, 0x77, 0x77, 0x2e, 0x64, 0x69, 0x67, 0x69, 0x63, 0x65, 0x72,
    0x74, 0x2e, 0x63, 0x6f, 0x6d, 0x31, 0x20, 0x30, 0x1e, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x13, 0x17, 0x44, 0x69, 0x67, 0x69, 0x43, 0x65, 0x72, 0x74,
    0x20, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x20, 0x52, 0x6f, 0x6f, 0x74,
    0x20, 0x43, 0x41,
};

static const unsigned char TA_RSA_N0[] = {
    0xe2, 0x3b, 0xe1, 0x11, 0x72, 0xde, 0xa8, 0xa4, 0xd3, 0xa3, 0x57, 0xaa,
    0x50, 0xa2, 0x8f, 0x0b, 0x77, 0x90, 0xc9, 0xa2, 0xa5, 0xee, 0x12, 0xce,
    0x96, 0x5b, 0x01, 0x09, 0x20, 0xcc, 0x01, 0x93, 0xa7, 0x4e, 0x30, 0xb7,
    0x53, 0xf7, 0x43, 0xc4, 0x69, 0x00, 0x57, 0x9d, 0xe2, 0x8d, 0x22, 0xdd,
    0x87, 0x06, 0x40, 0x00, 0x81, 0x09, 0xce, 0xce, 0x1b, 0x83, 0xbf, 0xdf,
    0xcd, 0x3b, 0x71, 0x46, 0xe2, 0xd6, 0x66, 0xc7, 0x05, 0xb3, 0x76, 0x27,
    0x16, 0x8f, 0x7b, 0x9e, 0x1e, 0x95, 0x7d, 0xee, 0xb7, 0x48, 0xa3, 0x08,
    0xda, 0xd6, 0xaf, 0x7a, 0x0c, 0x39, 0x06, 0x65, 0x7f, 0x4a, 0x5d, 0x1f,
    0xbc, 0x17, 0xf8, 0xab, 0xbe, 0xee, 0x28, 0xd7, 0x74, 0x7f, 0x7a, 0x78,
    0x99, 0x59, 0x85, 0x68, 0x6e, 0x5c, 0x23, 0x32, 0x4b, 0xbf, 0x4e, 0xc0,
    0xe8, 0x5a, 0x6d, 0xe3, 0x70, 0xbf, 0x77, 0x10, 0xbf, 0xfc, 0x01, 0xf6,
    0x85, 0xd9, 0xa8, 0x44, 0x10, 0x58, 0x32, 0xa9, 0x75, 0x18, 0xd5, 0xd1,
    0xa2, 0xbe, 0x47, 0xe2, 0x27, 0x6a, 0xf4, 0x9a, 0x33, 0xf8, 0x49, 0x08,
    0x60, 0x8b, 0xd4, 0x5f, 0xb4, 0x3a, 0x84, 0xbf, 0xa1, 0xaa, 0x4a, 0x4c,
    0x7d, 0x3e, 0xcf, 0x4f, 0x5f, 0x6c, 0x76, 0x5e, 0xa0, 0x4b, 0x37, 0x91,
    0x9e, 0xdc, 0x22, 0xe6, 0x6d, 0xce, 0x14, 0x1a, 0x8e, 0x6a, 0xcb, 0xfe,
    0xcd, 0xb3, 0x14, 0x64, 0x17, 0xc7, 0x5b, 0x29, 0x9e, 0x32, 0xbf, 0xf2,
    0xee, 0xfa, 0xd3, 0x0b, 0x42, 0xd4, 0xab, 0xb7, 0x41, 0x32, 0xda, 0x0c,
    0xd4, 0xef, 0xf8, 0x81, 0xd5, 0xbb, 0x8d, 0x58, 0x3f, 0xb5, 0x1b, 0xe8,
    0x49, 0x28, 0xa2, 0x70, 0xda, 0x31, 0x04, 0xdd, 0xf7, 0xb2, 0x16, 0xf2,
    0x4c, 0x0a, 0x4e, 0x07, 0xa8, 0xed, 0x4a, 0x3d, 0x5e, 0xb5, 0x7f, 0xa3,
    0x90, 0xc3, 0xaf, 0x27,
};

static const unsigned char TA_RSA_E0[] = {
    0x01, 0x00, 0x01,
};

static const unsigned char TA_DN1[] = {
    0x30, 0x5a, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x49, 0x45, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0a,
    0x13, 0x09, 0x42, 0x61, 0x6c, 0x74, 0x69, 0x6d, 0x6f, 0x72, 0x65, 0x31,
    0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x0a, 0x43, 0x79,
    0x62, 0x65, 0x72, 0x54, 0x72, 0x75, 0x73, 0x74, 0x31, 0x22, 0x30, 0x20,
    0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x19, 0x42, 0x61, 0x6c, 0x74, 0x69,
    0x6d, 0x6f, 0x72, 0x65, 0x20, 0x43, 0x79, 0x62, 0x65, 0x72, 0x54, 0x72,
    0x75, 0x73, 0x74, 0x20, 0x52, 0x6f, 0x6f, 0x74,
};

static const unsigned char TA_RSA_N1[] = {
    0xa3, 0x04, 0xbb, 0x22, 0xab, 0x98, 0x3d, 0x57, 0xe8, 0x26, 0x72, 0x9a,
    0xb5, 0x79, 0xd4, 0x29, 0xe2, 0xe1, 0xe8, 0x95, 0x80, 0xb1, 0xb0, 0xe3,
    0x5b, 0x8e, 0x2b, 0x29, 0x9a, 0x64, 0xdf, 0xa1, 0x5d, 0xed, 0xb0, 0x09,
    0x05, 0x6d, 0xdb, 0x28, 0x2e, 0xce, 0x62, 0xa2, 0x62, 0xfe, 0xb4, 0x88,
    0xda, 0x12, 0xeb, 0x38, 0xeb, 0x21, 0x9d, 0xc0, 0x41, 0x2b, 0x01, 0x52,
    0x7b, 0x88, 0x77, 0xd3, 0x1c, 0x8f, 0xc7, 0xba, 0xb9, 0x88, 0xb5, 0x6a,
    0x09, 0xe7, 0x73, 0xe8, 0x11, 0x40, 0xa7, 0xd1, 0xcc, 0xca, 0x62, 0x8d,
    0x2d, 0xe5, 0x8f, 0x0b, 0xa6, 0x50, 0xd2, 0xa8, 0x50, 0xc3, 0x28, 0xea,
    0xf5, 0xab, 0x25, 0x87, 0x8a, 0x9a, 0x96, 0x1c, 0xa9, 0x67, 0xb8, 0x3f,
    0x0c, 0xd5, 0xf7, 0xf9, 0x52, 0x13, 0x2f, 0xc2, 0x1b, 0xd5, 0x70, 0x70,
    0xf0, 0x8f, 0xc0, 0x12, 0xca, 0x06, 0xcb, 0x9a, 0xe1, 0xd9, 0xca, 0x33,
    0x7a, 0x77, 0xd6, 0xf8, 0xec, 0xb9, 0xf1, 0x68, 0x44, 0x42, 0x48, 0x13,
    0xd2, 0xc0, 0xc2, 0xa4, 0xae, 0x5e, 0x60, 0xfe, 0xb6, 0xa6, 0x05, 0xfc,
    0xb4, 0xdd, 0x07, 0x59, 0x02, 0xd4, 0x59, 0x18, 0x98, 0x63, 0xf5, 0xa5,
    0x63, 0xe0, 0x90, 0x0c, 0x7d, 0x5d, 0xb2, 0x06, 0x7a, 0xf3, 0x85, 0xea,
    0xeb, 0xd4, 0x03, 0xae, 0x5e, 0x84, 0x3e, 0x5f, 0xff, 0x15, 0xed, 0x69,
    0xbc, 0xf9, 0x39, 0x36, 0x72, 0x75, 0xcf, 0x77, 0x52, 0x4d, 0xf3, 0xc9,
    0x90, 0x2c, 0xb9, 0x3d, 0xe5, 0xc9, 0x23, 0x53, 0x3f, 0x1f, 0x24, 0x98,
    0x21, 0x5c, 0x07, 0x99, 0x29, 0xbd, 0xc6, 0x3a, 0xec, 0xe7, 0x6e, 0x86,
    0x3a, 0x6b, 0x97, 0x74, 0x63, 0x33, 0xbd, 0x68, 0x18, 0x31, 0xf0, 0x78,
    0x8d, 0x76, 0xbf, 0xfc, 0x9e, 0x8e, 0x5d, 0x2a, 0x86, 0xa7, 0x4d, 0x90,
    0xdc, 0x27, 0x1a, 0x39,
};

static const unsigned char TA_RSA_E1[] = {
    0x01, 0x00, 0x01,
};

static const br_x509_trust_anchor TAs[] = {
    {
        { (unsigned char *)TA_DN0, sizeof TA_DN0 },
        BR_X509_TA_CA,
        {
            BR_KEYTYPE_RSA,
            { .rsa = {
                (unsigned char *)TA_RSA_N0, sizeof TA_RSA_N0,
                (unsigned char *)TA_RSA_E0, sizeof TA_RSA_E0,
            } }
        }
    },
    {
        { (unsigned char *)TA_DN1, sizeof TA_DN1 },
        BR_X509_TA_CA,
        {
            BR_KEYTYPE_RSA,
            { .rsa = {
                (unsigned char *)TA_RSA_N1, sizeof TA_RSA_N1,
                (unsigned char *)TA_RSA_E1, sizeof TA_RSA_E1,
            } }
        }
    },
};

/*
 * Main program: this is a simple program that expects 2 or 3 arguments.
 * The first two arguments are a hostname and a port; the program will
 * open a SSL connection with that server and port. It will then send
 * a simple HTTP GET request, using the third argument as target path
 * ("/" is used as path if no third argument was provided). The HTTP
 * response, complete with header and contents, is received and written
 * on stdout.
 */
int
main(int argc, char *argv[])
{
	const char *host, *port, *path;
	int fd;
	br_ssl_client_context sc;
	br_x509_minimal_context xc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	br_sslio_context ioc;

	/*
	 * Parse command-line argument: host, port, and path. The path
	 * is optional; if absent, "/" is used.
	 */
	if (argc < 3 || argc > 4) {
		return EXIT_FAILURE;
	}
	host = argv[1];
	port = argv[2];
	if (argc == 4) {
		path = argv[3];
	} else {
		path = "/";
	}

	/*
	 * Ignore SIGPIPE to avoid crashing in case of abrupt socket close.
	 */
	signal(SIGPIPE, SIG_IGN);

	/*
	 * Open the socket to the target server.
	 */
	fd = host_connect(host, port);
	if (fd < 0) {
		return EXIT_FAILURE;
	}

	/*
	 * Initialise the client context:
	 * -- Use the "full" profile (all supported algorithms).
	 * -- The provided X.509 validation engine is initialised, with
	 *    the hardcoded trust anchor.
	 */
	br_ssl_client_init_full(&sc, &xc, TAs, TAs_NUM);

	/*
	 * Set the I/O buffer to the provided array. We allocated a
	 * buffer large enough for full-duplex behaviour with all
	 * allowed sizes of SSL records, hence we set the last argument
	 * to 1 (which means "split the buffer into separate input and
	 * output areas").
	 */
	br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);

	/*
	 * Reset the client context, for a new handshake. We provide the
	 * target host name: it will be used for the SNI extension. The
	 * last parameter is 0: we are not trying to resume a session.
	 */
	br_ssl_client_reset(&sc, host, 0);

	/*
	 * Initialise the simplified I/O wrapper context, to use our
	 * SSL client context, and the two callbacks for socket I/O.
	 */
	br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);

	/*
	 * Note that while the context has, at that point, already
	 * assembled the ClientHello to send, nothing happened on the
	 * network yet. Real I/O will occur only with the next call.
	 *
	 * We write our simple HTTP request. We could test each call
	 * for an error (-1), but this is not strictly necessary, since
	 * the error state "sticks": if the context fails for any reason
	 * (e.g. bad server certificate), then it will remain in failed
	 * state and all subsequent calls will return -1 as well.
	 */
	br_sslio_write_all(&ioc, "GET ", 4);
	br_sslio_write_all(&ioc, path, strlen(path));
	br_sslio_write_all(&ioc, " HTTP/1.0\r\nHost: ", 17);
	br_sslio_write_all(&ioc, host, strlen(host));
	br_sslio_write_all(&ioc, "\r\n\r\n", 4);

	/*
	 * SSL is a buffered protocol: we make sure that all our request
	 * bytes are sent onto the wire.
	 */
	br_sslio_flush(&ioc);

	/*
	 * Read the server's response. We use here a small 512-byte buffer,
	 * but most of the buffering occurs in the client context: the
	 * server will send full records (up to 16384 bytes worth of data
	 * each), and the client context buffers one full record at a time.
	 */
	for (;;) {
		int rlen;
		unsigned char tmp[512];

		rlen = br_sslio_read(&ioc, tmp, sizeof tmp);
		if (rlen < 0) {
			break;
		}
		fwrite(tmp, 1, rlen, stdout);
	}

	/*
	 * Close the socket.
	 */
	close(fd);

	/*
	 * Check whether we closed properly or not. If the engine is
	 * closed, then its error status allows to distinguish between
	 * a normal closure and a SSL error.
	 *
	 * If the engine is NOT closed, then this means that the
	 * underlying network socket was closed or failed in some way.
	 * Note that many Web servers out there do not properly close
	 * their SSL connections (they don't send a close_notify alert),
	 * which will be reported here as "socket closed without proper
	 * SSL termination".
	 */
	if (br_ssl_engine_current_state(&sc.eng) == BR_SSL_CLOSED) {
		int err;

		err = br_ssl_engine_last_error(&sc.eng);
		if (err == 0) {
			fprintf(stderr, "closed.\n");
			return EXIT_SUCCESS;
		} else {
			fprintf(stderr, "SSL error %d\n", err);
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr,
			"socket closed without proper SSL termination\n");
		return EXIT_FAILURE;
	}
}