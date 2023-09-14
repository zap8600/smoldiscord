#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/test.h>
#include <errno.h>

int socket_desc;
char *host;

WOLFSSL_CTX* ctx;
WOLFSSL* ssl;
WOLFSSL_METHOD* method;

const char *authToken;

char message[200];

int main(int argc , char *argv[]) {
    struct sockaddr_in server;
    char server_reply[2000];

    host = argv[1];

    authToken = argv[2];

    snprintf(message, sizeof(message), "GET /api/v10/users/@me HTTP/1.1\r\nHost: discord.com\r\nAuthorization: Bot %s\r\n\r\n", authToken);

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }

    server.sin_addr.s_addr = inet_addr(host);
    server.sin_family = AF_INET;
    server.sin_port = htons( 80 );

    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("connect error");
        return 1;
    }

    puts("Connected\n");

    wolfSSL_Init();
    method = wolfTLSv1_2_client_method();

    if ( (ctx = wolfSSL_CTX_new(method)) == NULL) {
        err_sys("wolfSSL_CTX_new error");
    }

    if ( (ssl = wolfSSL_new(ctx)) == NULL) {
        err_sys("wolfSSL_new error");
    }

    if (wolfSSL_CTX_load_verify_locations(ctx, "certs/ca-cert.pem", 0) != SSL_SUCCESS) {
        err_sys("Error loading certs/ca-cert.pem");
    }

    wolfSSL_set_fd(ssl, socket_desc);
    if(wolfSSL_connect(ssl) != SSL_SUCCESS) {
		err_sys("Error connecting.")
	}

    wolfSSL_write(ssl, message, strlen(message));

	while (1) {
		wolfSSL_read(ssl, server_reply, 2000);
        puts("Reply received\n");
        puts(server_reply);
	}

    //close(socket_desc);
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
}
