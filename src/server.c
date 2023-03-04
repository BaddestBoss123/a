#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	(void)bind(sockfd, (struct sockaddr*)&(struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(8080)
	}, sizeof(struct sockaddr_in));

	for (;;) {
		char buffer[1024];

		struct sockaddr_in client;
		int n = recvfrom(sockfd, (char*)buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr*)&client, &(socklen_t){ sizeof(struct sockaddr_in) });
		buffer[n] = '\0';

		char addrstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client.sin_addr), addrstr, INET_ADDRSTRLEN);

        printf("Received %d bytes from %s:%d: %s\n", n, addrstr, ntohs(client.sin_port), buffer);

		sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
	}

	return 0;
}