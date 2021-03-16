#include	"unp.h"
#define UCLIENTMAX 100

struct udp_client {
	int			key;
	struct sockaddr_in	ucliaddr;
};

int 			udpfd, client[FD_SETSIZE];
int 			maxi, udp_count;
struct udp_client	udp_cli[UCLIENTMAX];
char			addr_str[30];

typedef struct message {
	int 	code;
	char	buf[MAXLINE];
} Msg;

char* addr_to_name(struct sockaddr_in addr);
char* addr_tcpfd(int fd);
Msg make_chat(Msg msg, char* name);
void broadcast(Msg msg, int tcp_fd, void* udp_addr);
char* user_list(char* buf, int tcp_fd, void* udp_addr);
void whisper(Msg smsg, int tcp_fd, void* udp_addr);

int
main(int argc, char **argv)
{
	Msg			msg;
	int			connfd, listenfd, sockfd, nready, maxfd, i; 
	fd_set			rset, allset;
	ssize_t			n;
	socklen_t 		len;
	const int		on = 1;
	struct sockaddr_in	cliaddr, servaddr;
	char*			name;

	if (argc != 2)
		err_quit("server: need Port number");

		/* create listening TCP socket */
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(atoi(argv[1]));

	Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

	printf("[server address is %s:%d]\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

	Listen(listenfd, LISTENQ);

		/* create UDP socket */
	udpfd = Socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(atoi(argv[1]));

	Bind(udpfd, (SA *) &servaddr, sizeof(servaddr));
	
	// initialize
	name = malloc(sizeof(char) * 25);
	udp_count = 0;
	for (i = 0; i < UCLIENTMAX; i++) {
		bzero(&udp_cli[i], sizeof(udp_cli[i]));
		udp_cli[i].key = -1;
	}
	maxi = -1;	
	for (i = 0; i < FD_SETSIZE; i++)
		client[i] = -1;
	FD_ZERO(&rset);
	FD_ZERO(&allset);

	maxfd = max(listenfd, udpfd);
	FD_SET(listenfd, &allset);
	FD_SET(udpfd, &allset);
	
	for ( ; ; ) {
		bzero(&(msg.buf), sizeof(msg.buf));
		rset = allset;
		nready = Select(maxfd + 1, &rset, NULL, NULL, NULL); 

		if (FD_ISSET(listenfd, &rset)) {

			// tcp connect
			len = sizeof(cliaddr);
			connfd = Accept(listenfd, (SA *) &cliaddr, &len);
			strcpy(name, addr_to_name(cliaddr));

			for (i = 0; i < FD_SETSIZE; i++) {
				if(client[i] < 0) {
					client[i] = connfd;
					break;
				}
			}
			if ( i == FD_SETSIZE )
				err_quit("too many clients");

			FD_SET(connfd, &allset);
			if (connfd > maxfd)
				maxfd = connfd;

			if ( i > maxi )
				maxi = i;

			printf("connected from [%s] (TCP)\n", name);
			snprintf(msg.buf, sizeof(msg.buf), "[%s is connected.]\n", name);
			// advertise everyone client connection
			broadcast(msg, connfd, NULL);


			if (--nready <= 0)
				continue;
		}
		
		if (FD_ISSET(udpfd, &rset)) {			
			len = sizeof(cliaddr);
			n = Recvfrom(udpfd, &msg, sizeof(msg), 0, (SA *) &cliaddr, &len);
			sprintf(name, "%s", addr_to_name(cliaddr));

			// udp connect
			if (msg.code == 1) {
				// who is empty	
				for (i = 0; i < UCLIENTMAX; i++) {
					if (udp_cli[i].key < 0)
						break;
				}
				if (i == UCLIENTMAX)
					err_quit("too many udp user.");
				udp_cli[i].ucliaddr = cliaddr;
				udp_cli[i].key = 0;
				udp_count ++;   

				printf("connected from [%s] (UDP)\n", name);

				snprintf(msg.buf, sizeof(msg.buf), "[%s is connected.]\n", name);
				// advertise everyone client connection
				broadcast(msg, -1, &cliaddr);
			}

			// udp exit
			else if (msg.code == 2) {
				// who is exit
				for (i = 0; i < UCLIENTMAX; i++) {
					if ( bcmp( &(udp_cli[i].ucliaddr), &cliaddr, len) == 0) {
						bzero(&udp_cli[i], sizeof(udp_cli[i]));
						udp_cli[i].key = -1;
						printf("leaved at [%s] (UDP)\n", name);
						snprintf(msg.buf, sizeof(msg.buf), "[%s exits.]\n", name);
						// advertise everyone client exit
						broadcast(msg, -1, &cliaddr);
						break;
					}				
				}
				
				udp_count--;
			}

			// udp list
			else if (msg.code == 3) {
				bzero(&(msg.buf), sizeof(msg.buf));
				strcpy(msg.buf, user_list(msg.buf, -1, &cliaddr));
				
				Sendto(udpfd, &msg, sizeof(msg), 0, (SA *)&cliaddr, len);
			}
			
			// udp whisper
			else if (msg.code == 4)
				whisper(msg, -1, &cliaddr);

			// udp chat
			else if (msg.code == 0) {
				msg = make_chat(msg, name);
				broadcast(msg, -1, &cliaddr);
			}
		}

		// tcp control
		for (i = 0; i <= maxi; i++) {
			if ( (sockfd = client[i]) < 0)
				continue;
			if (FD_ISSET(sockfd, &rset)) {
				n = Read(sockfd, &msg, sizeof(msg));
				strcpy(name, addr_tcpfd(sockfd));
				
				// tcp exit. receives FIN
				if (n == 0) {
					snprintf(msg.buf, sizeof(msg.buf), "[%s exits.]\n", name);
					// advertise everyone client exit
					broadcast(msg, sockfd, NULL);

					Close(sockfd);
					FD_CLR(sockfd, &allset);
					client[i] = -1;
					printf("leaved at [%s] (TCP)\n", name);

					break;
				}
				
				// connect msg code
				else if (msg.code == 1)
					break;		

				// exit msg code
				else if (msg.code == 2) 
					break;		

				// tcp list
				else if (msg.code == 3) {
					bzero(&(msg.buf), sizeof(msg.buf));
					strcpy(msg.buf, user_list(msg.buf, sockfd, NULL));
					Writen(sockfd, &msg, sizeof(msg));
				} 
				
				// tcp whisper
				else if (msg.code == 4)
					whisper(msg, sockfd, NULL);

				// tcp chat
				else if (msg.code == 0) {
					msg = make_chat(msg, name);
					broadcast(msg, sockfd, NULL);
				}

				if (--nready <= 0)
					break;
			}
		}
	}
	
	free(name);
	return 0;
}


// make sentence like "[ ip : port ] ...content..."
Msg make_chat(Msg msg, char* name) {
	char	tmpstr[MAXLINE];

	strcpy(tmpstr, msg.buf);
	bzero(&msg, sizeof(msg));
	sprintf(msg.buf, "[%s] ", name);	
	strcat(msg.buf, tmpstr);

	return msg;
}

// broadcast the message except myself
void broadcast(Msg msg, int fd, void* udp_addr) {
	int 		i;
	socklen_t	len;

	// to TCP client
	for (i = 0; i <= maxi; i++) {
		if (client[i] < 0  || client[i] == fd)
			continue;
		else
			Writen(client[i], &msg, sizeof(msg));
	}

	// to UDP client
	for (i = 0; i < UCLIENTMAX; i++) {
		if (udp_cli[i].key < 0)
			continue;
	
		if ( udp_addr != NULL && bcmp(&(udp_cli[i].ucliaddr), udp_addr, sizeof(udp_cli[i].ucliaddr)) == 0) {
			continue;
		}
		
		else {
			len = sizeof(udp_cli[i].ucliaddr);
			Sendto(udpfd, &msg, sizeof(msg), 0, (SA*)&(udp_cli[i].ucliaddr), len);
		}
	}
}


// address to string like "255.255.255.255:9999"
char* addr_to_name(struct sockaddr_in addr) {
	sprintf(addr_str, "%s:%d", inet_ntoa(addr.sin_addr), addr.sin_port);
	return addr_str;
}


// print chatting user list at buffer
char* user_list(char* buf, int tcp_fd, void* udp_addr) {
	int	i;
	char*	list_name = malloc(sizeof(char) * 30);;

	strcat(buf, "======== client list ========\n");
					
	// tcp users
	for (i = 0; i <= maxi; i++) {
		if (client[i] < 0)
			continue;
		else {
			if (client[i] == tcp_fd)
				sprintf(list_name, "[%s] me\n", addr_tcpfd(client[i]));
			else
				sprintf(list_name, "[%s]\n", addr_tcpfd(client[i]));
			strcat(buf, list_name);
		}
	}

	// udp users
	for (i = 0; i < UCLIENTMAX; i++) {
		if (udp_cli[i].key < 0)
			continue;

		if (udp_addr != NULL && bcmp(&(udp_cli[i].ucliaddr), udp_addr, sizeof(udp_cli[i].ucliaddr)) == 0)
			sprintf(list_name, "[%s] me\n", addr_to_name(udp_cli[i].ucliaddr));
		else
			sprintf(list_name, "[%s]\n", addr_to_name(udp_cli[i].ucliaddr));

		strcat(buf, list_name);
	}

	strcat(buf, "=============================\n");


	free(list_name);
	return buf;
}


// whisper. can talk to only one
void whisper(Msg smsg, int tcp_fd, void* udp_addr) {
	int 	i;
	char* 	des_name = malloc(sizeof(char) * 25);
	char	from_name[25];
	char 	addr[20];
	char	tmpstr[MAXLINE];

	// cut string
	char * ptr = strtok(smsg.buf, " ");	// "/smsg"
	strcpy(addr, strtok(NULL, " "));	// ip and port
	strcpy(tmpstr, strtok(NULL, ""));	// content
	
	// find who send from
	for (i = 0; i <= maxi; i++)
		if (client[i] == tcp_fd) {
			strcpy(from_name, addr_tcpfd(client[i]));
			break;
		}

	for (i = 0; i < UCLIENTMAX; i++) {
		if (udp_cli[i].key < 0)
			continue;
	
		if ( udp_addr != NULL && bcmp(&(udp_cli[i].ucliaddr), udp_addr, sizeof(udp_cli[i].ucliaddr)) == 0) {
			strcpy(from_name, addr_to_name(udp_cli[i].ucliaddr));
			break;
		}
	}

	// make smsg
	bzero(&(smsg), sizeof(smsg));
	sprintf(smsg.buf, "[smsg from %s] ", from_name);
	strcat(smsg.buf, tmpstr);

	// find destination. if tcp...
	for (i = 0; i <= maxi; i++) {
		if (client[i] < 0)
			continue;
		else {
			strcpy(des_name, addr_tcpfd(client[i]));
			if ( (strcmp(des_name, addr)) == 0) {
				Writen(client[i], &smsg, sizeof(smsg));
				break;
			}	
		}
	}

	// find destination. if udp...
	for (i = 0; i < UCLIENTMAX; i++) {
		if (udp_cli[i].key < 0)
			continue;

		strcpy(des_name, addr_to_name(udp_cli[i].ucliaddr));
		if (strcmp(des_name, addr) == 0) {
			Sendto(udpfd, &smsg, sizeof(smsg), 0, (SA*)&(udp_cli[i].ucliaddr), sizeof(udp_cli[i].ucliaddr));
			break;
		}
	}
	
	free(des_name);
}

// address of tcp file description
char* addr_tcpfd(int fd) {
	struct sockaddr_in	tmp;
	socklen_t 		len = sizeof(tmp);
	
	getpeername(fd, (SA *)&tmp, &len); 
	return addr_to_name(tmp);
}
