#include	"unp.h"

typedef struct message {
	int	code;
	char	buf[MAXLINE];
} Msg;

int
main(int argc, char **argv)
{
	int			sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 3)
		err_quit("client: need IP address and Port number");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));	
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);	
	dg_cli(stdin, sockfd, (SA *) &servaddr, sizeof(servaddr));

	exit(0);
}


void
dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen)
{
	int		n, stdineof, maxfdp1;
	fd_set		rset;
	Msg		msg;
	char		whisper[7];

	Connect (sockfd, (SA *) pservaddr, servlen);
	msg.code = 1;
	Write(sockfd, &msg, sizeof(msg));
	
	strcpy(whisper, "/smsg ");
	stdineof = 0;
	FD_ZERO(&rset);
	for ( ; ; ) {
		bzero(&(msg.buf), MAXLINE);

		if (stdineof == 0)
			FD_SET(fileno(fp), &rset);
		FD_SET(sockfd, &rset);
		maxfdp1 = max(fileno(fp), sockfd) + 1;
		Select(maxfdp1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(sockfd, &rset)) {	/* from network */
			n = Read(sockfd, &msg, sizeof(msg));
			Write(fileno(stdout), msg.buf, strlen(msg.buf));
		}

		if (FD_ISSET(fileno(fp), &rset)) {  /* from me */
			n = Read(fileno(fp), &(msg.buf), sizeof(msg.buf));

			// quit			
			if (! strcmp(msg.buf, "/quit\n")) { 	
				stdineof = 1;
				msg.code = 2;
				Writen(sockfd, &msg, sizeof(msg));
				FD_CLR(fileno(fp), &rset);
				return;
			} 
			
			// list
			else if (! strcmp(msg.buf, "/list\n")) {	
				msg.code = 3;
				Writen(sockfd, &msg, sizeof(msg));
			}
			
			// whisper
			else if (bcmp( &(msg.buf), &whisper, 6) == 0) {
				msg.code = 4;
				Writen(sockfd, &msg, sizeof(msg));
			}
		
			// chat
			else {
				msg.code = 0;
				Writen(sockfd, &msg, sizeof(msg));
			}
		}
	}
}
