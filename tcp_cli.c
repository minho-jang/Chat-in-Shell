#include	"unp.h"

typedef struct message {
	int 	code;
	char	buf[MAXLINE];
} Msg;

int
main(int argc, char **argv)
{
	int			sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 3)
		err_quit("client: need IP address and Port number");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

	str_cli(stdin, sockfd);		/* do it all */

	exit(0);
}

void
str_cli(FILE *fp, int sockfd)
{
	int		maxfdp1, stdineof;
	Msg		msg;
	fd_set		rset;
	int		n;
	char		whisper[7];

	strcpy(whisper, "/smsg ");
	stdineof = 0;
	FD_ZERO(&rset);
	for ( ; ; ) {
		bzero(&(msg.buf), sizeof(msg.buf));
		if (stdineof == 0)
			FD_SET(fileno(fp), &rset);
		FD_SET(sockfd, &rset);
		maxfdp1 = max(fileno(fp), sockfd) + 1;
		Select(maxfdp1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(sockfd, &rset)) {	/* from network */
			if ( (n = Read(sockfd, &msg, sizeof(msg))) == 0) {
				if (stdineof == 1)
					return;		/* normal termination */
				else
					err_quit("tcp_cli: server terminated prematurely");
			} else {
				Write(fileno(stdout), &(msg.buf), strlen(msg.buf));
			}
		}

		if (FD_ISSET(fileno(fp), &rset)) {  /* from me */
			n = Read(fileno(fp), &(msg.buf), sizeof(msg.buf));

			// quit
			if (! strcmp(msg.buf, "/quit\n")) {
				stdineof = 1;
				Shutdown(sockfd, SHUT_WR);	/* send FIN */
				FD_CLR(fileno(fp), &rset);
				continue;

			} 			
			//list
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
