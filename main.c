#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int socket_desc;
char *host;

const char *authToken;

const char* gateway_url = "GET / HTTP/1.1\r\n\r\n";

int main(int argc , char *argv[]) {
    struct sockaddr_in server;
	char server_reply[2000];

    host = argv[1];

    authToken = argv[2];

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

    if( send(socket_desc , gateway_url, strlen(gateway_url) , 0) < 0)
	{
		puts("Send failed");
	}

    while (1) {
    	if( recv(socket_desc, server_reply , 2000 , 0) < 0)
		{
			puts("recv failed");
		}
		puts("Reply received\n");
		puts(server_reply);
    }
}
