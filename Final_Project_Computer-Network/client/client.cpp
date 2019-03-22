// {{{ include
#include <stdio.h>
#include <termio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pwd.h>
// }}}
// {{{ variables and funcs
char friends[10000];
int pid;
char server_addr[] = "140.112.30.32";
char server_port[] = "3490";
int server_fd;
struct addrinfo hints, *res;
int sockfd;
char Send[100];
char Recv[100];
void login(void);
void my_sig(int);
void process(void);
struct {
	char username[20];
}myinfo;
void clear() {
	for (int i = 0; i < 100; i++) {
		fprintf(stderr, "\n");
	}
}
void myrecv() {
	memset(Recv, 0 , sizeof Recv);
	if (recv(server_fd, Recv, sizeof Recv, 0) == 0) {
		fprintf(stderr, "Server down T_T\n");
		exit(1);
	}
}
void mysend(int x) {
	memset(Send, 0, sizeof Send);
	sprintf(Send, "%d",x);
	send(server_fd, Send, strlen(Send), 0);
}
void init_signal() {
	struct sigaction act;
	act.sa_handler = my_sig;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NODEFER;
	if (sigaction(SIGINT, &act, NULL) == -1) {
		fprintf(stderr, "SIGINT err\n");
	}	
}
int getch() {
    struct termios oldtc, newtc;
    int ch;
    tcgetattr(STDIN_FILENO, &oldtc);
    newtc = oldtc;
    newtc.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newtc);
    ch=getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
    return ch;
}
void main_page() {
	clear();
	fprintf(stderr, "(C) chat!\n");
	fprintf(stderr, "(V) view friend list\n");
	fprintf(stderr, "(A) add friend\n");
	fprintf(stderr, "(W) who is online\n");
	fprintf(stderr, "(L) log out\n");
	fprintf(stderr, "(F) file\n");
	fprintf(stderr, "-----------------------------\n");
	fprintf(stderr, ">> ");
}
void get_now_time(char now[]) {
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep); 
	sprintf(now, "[%d/%d %d:%d] %s: ",p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, myinfo.username); 
}
// }}}
// {{{ main()
int main() {
	// {{{ pre-process
	init_signal();
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((getaddrinfo(server_addr, server_port, &hints, &res)) == -1) {
		fprintf(stderr, "getaddrinfo error\n");
		exit(1);
	}
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "socket error\n");
		exit(1);
	}
	if ((connect(sockfd, res->ai_addr, res->ai_addrlen)) == -1) {
		fprintf(stderr, "connect error\n");
		exit(1);
	}
	server_fd = sockfd;
	clear();
	// }}}
	login();
	return 0;
}
// }}}
// {{{ login()
void login() {
	fprintf(stderr, "---Welcome---\n");
	fprintf(stderr, " New user? type \"/signup\"\n");
	fprintf(stderr, " Have account? type your username!\n");
	fprintf(stderr, " Press ^C to end the program...\n");
	fprintf(stderr, "--------------------------\n");
	while(1) {
		fprintf(stderr, ">> ");
		memset(Send, 0 , sizeof(Send));
		scanf("%s",Send);
		if (strcmp(Send, "/signup") == 0) {
			// {{{ signup
			send(server_fd, Send, strlen(Send), 0);
			myrecv();
			if (Recv[0] != '0') {
				fprintf(stderr, "This should not happen...\n");
				exit(1);
			}
			fprintf(stderr, "Create your username:\n>> ");
			while (1) {
				scanf("%s",Send);
				if (!isalpha(Send[0])) {
					fprintf(stderr, "Invalid User name! (should begin with english letter)\n>> ");
					continue;
				}
				send(server_fd, Send, strlen(Send), 0);
				myrecv();
				if (Recv[0] == '-' && Recv[1] == '1') {
					fprintf(stderr, "User name already used, try a new one!\n");
					fprintf(stderr, ">> ");
				} else if (Recv[0] == '0') {
					strcpy(myinfo.username, Send);
					fprintf(stderr, "Vaild name!\n");
					fprintf(stderr, "Type in your password:\n>> ");
					break;
				}
			}
			char c;
			int cnt = 0;
			getchar();
			while ((c = getch())) {
				if (c == '\r' || c == '\n') break;
				if (c == 8 || c == 127) {
					if (cnt == 0) continue;
					cnt--;
					fprintf(stderr, "\b \b");
				} else {
					Send[cnt++] = c;
					fprintf(stderr, "*");
				}
			}
			puts("");
			Send[cnt++] = 0;
			send(server_fd, Send, strlen(Send), 0);
			myrecv();
			if (Recv[0] != '0') {
				fprintf(stderr, "This should not happened QQ\n");
				exit(1);
			}
			process();
			return;
			// }}}
		} else {
			send(server_fd, Send, strlen(Send), 0);
			myrecv();
			if (Recv[0] == '-' && Recv[1] == '1') {
				fprintf(stderr ,"user not found...\n");
				fprintf(stderr ,"type \"/signup\" or re-login\n");
				continue;
			} else if (Recv[0] == '0') {
				strcpy(myinfo.username, Send);
				fprintf(stderr, "Password:\n>> ");
			} else if (Recv[0] == '-' && Recv[1] == '3') {
				fprintf(stderr ,"This user is current online...\n");
				fprintf(stderr ,"type \"/signup\" or re-login\n");
				continue;
			}
			getchar();
			while (1) {
				char c;
				int cnt = 0;
				while ((c = getch())) {
					if (c == '\r' || c == '\n') break;
					if (c == 8 || c == 127) {
						if (cnt == -1) continue;
						cnt--;
						fprintf(stderr, "\b \b");
					} else {
						Send[cnt++] = c;
						fprintf(stderr, "*");
					}
				}
				puts("");
				Send[cnt++] = 0;
				send(server_fd, Send, strlen(Send), 0);
				myrecv();
				if (Recv[0] == '-' && Recv[1] == '1') {
					fprintf(stderr, "Wrong Password! Try again\n");
					fprintf(stderr, ">> ");
				} else if (Recv[0] == '0') {
					process();
					return;
				}
			}
		}
	}
}
// }}}
// {{{ chat()
void chat() {
	clear();
	fprintf(stderr, "who you want to chat with ;)\n");
	int cnt = 0;
	for (int i = 0; ; i++) {
		if (friends[i] == '\0') break;
		if (friends[i] == '\n') cnt++;
		fprintf(stderr, "%c", friends[i]);
		if (cnt == 10) break;
	}
	fprintf(stderr, "or ...(type /b to main page)\n>> ");
	char chat_name[20] = {};
	mysend(2);
	while (1) {
		scanf("%s",chat_name);
		if (strcmp(chat_name, "/b") == 0){
			mysend(9);
			return;
		}
		sprintf(Send, "%s", chat_name);
		send(server_fd, Send, strlen(Send), 0);
		myrecv();
		if (Recv[0] == '-' && Recv[1] == '1') {
			fprintf(stderr, "User not found... try another!\n>> ");
			continue;
		} else if (Recv[0] == '0'){
			fprintf(stderr, "start chat!\n");
			break;
		}
	}
	getchar();
	char msg[1000];
	fprintf(stderr,">> ");
	while (1) {
		struct timeval tv;
		fd_set readfds;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);
		FD_SET(STDIN_FILENO, &readfds);
		select(server_fd+1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(server_fd, &readfds)) {
			myrecv();
			fprintf(stderr, "%s",Recv);
		}
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			memset(msg, 0, sizeof msg);
			fgets(msg, 1000, stdin);
			if (strcmp(msg, "/b\n") == 0) {
				mysend(-127);
				break;
			}
			char snd[1000]; get_now_time(snd);
			strcat(snd, msg);
			strcat(snd, ">> ");
			int x = send(server_fd, snd,strlen(snd), 0);
			fprintf(stderr, ">> ");
		}
	}
}
// }}}
// {{{ send_file()
void send_file(){
	char buf_file_path[10][100], buf_send[100], who_to_send[100];
	int cnt = 0, chkfd;
	struct passwd *pw = getpwuid(getuid());
	char *homedir = pw->pw_dir;
	fprintf(stderr, "Who do you want to send ?\n");
	fprintf(stderr, "or ...(type /b to file page)\n>> ");
	while(1) {
		scanf("%s", who_to_send);
		if (strcmp(who_to_send, "/b") == 0) {
			mysend(9);
			return;
		} else if(strcmp(who_to_send, myinfo.username) == 0) {
			fprintf(stderr, "Why send to yourself ?\n>> ");
			continue;
		}
		sprintf(Send, "%s", who_to_send);
		send(server_fd, Send, strlen(Send), 0);
		myrecv();
		if (Recv[0] == '-' && Recv[1] == '1') {
			fprintf(stderr, "User not found... try another!\n>> ");
			continue;
		} else if (Recv[0] == '0') break;
	}
	fprintf(stderr, "Please enter file name & pathname.\n");
	fprintf(stderr, "Example: ~/abc.txt\n>> ");
	char homebuf[64], cur_buf[64];
	while(1) {
		scanf("%s", buf_file_path[cnt]);
		if (strcmp(buf_file_path[cnt], "/done") == 0) break;
		if(buf_file_path[cnt][0] == '~'){
			strcpy(homebuf, homedir);
			strcpy(cur_buf, buf_file_path[cnt]+1);
			strcat(homebuf, cur_buf);
			strcpy(buf_file_path[cnt], homebuf);
		}
		//fprintf(stderr, "debug %s\n", buf_file_path[cnt]);
		if (access(buf_file_path[cnt], 0) != 0) {
			fprintf(stderr, "File does not exist...\n");
		} else{
			//close(chkfd);
			cnt++;
		}
		if(cnt < 10){
			fprintf(stderr, "Anymore to send ? (You can send %d more files)\n", 10 - cnt);
			fprintf(stderr, "or ...(type /done to start sending)\n>> ");
		} else break;
	}
	int sendfile_fd, path_length, read_buf[100];
	char *slash;
	for(int i = 0; i < cnt; i++){
		sendfile_fd = open(buf_file_path[i], O_RDONLY);
		slash = strrchr(buf_file_path[i], '/');
		if (slash == NULL) {
			strcpy(Send, buf_file_path[i]);
		} else strcpy(Send, slash+1);
		fprintf(stderr, "transmitting %s...\n\n", Send);
		send(server_fd, Send, strlen(Send), 0);
		myrecv();
		if(Recv[0] == '-' && Recv[1] == '1') {
			fprintf(stderr, "This should not happen...\n");
			exit(1);
		}
		ssize_t n;
		while((n = read(sendfile_fd, read_buf, 80)) > 0) {
			send(server_fd, read_buf, n, 0);
			myrecv();
			if(Recv[0] == '-' && Recv[1] == '1') {
				fprintf(stderr, "Transmission error\n");
				exit(1);
			}
		}
		close(sendfile_fd);
		send(server_fd, "endoffile-----", strlen("endoffile-----"), 0);
		myrecv();
	}
	mysend(-2);
	fprintf(stderr, "Successfully sent!\n");
	return;
}
// }}}
// {{{ recv_file()
void recv_file(char *name, char *homename){
	char pathbuf[200], checkans[64];
	sprintf(pathbuf, "%s/%s", homename, name);
	if(access(pathbuf, 0) == 0){
		fprintf(stderr, "This filename has already exist...\n");
		fprintf(stderr, "Are you sure to replace it ?\n(yes then type y, otherwise type n)\n>> ");
		scanf("%s", checkans);
		if(checkans[0] != 'y'){
			mysend(-1);
			myrecv();
			return;
		}
	}
	mysend(0);
	int recvfile_fd = open(pathbuf, O_CREAT | O_TRUNC | O_RDWR, 0777);
	while(1){
		myrecv();
		if(strcmp(Recv, "endoffile-----") == 0) break;
		write(recvfile_fd, Recv, strlen(Recv));
		mysend(0);
	}
	return;
}
// }}}
void process() {
	fprintf(stderr, "Hi %s\n", myinfo.username);
	for (int i = 0; i < 5; i++) {
		fprintf(stderr, ".");
		usleep(300000);
	}
	recv(server_fd, friends, sizeof friends, 0);
	if (friends[0] == '-' && friends[1] == '1') memset(friends, 0, sizeof friends);
	char cmd[10];
	struct passwd *pw = getpwuid(getuid());
	char *homedir = pw->pw_dir;
	int filenum;
	while (1) {
		main_page();
		scanf("%s", cmd);
		if (strcmp(cmd, "C") == 0) {
			chat();
		} else if (strcmp(cmd, "V") == 0) {
			clear();
			fprintf(stderr, "%s", friends);
			fprintf(stderr, "type enter to continue...\n");
			getchar();
			getchar();
		} else if (strcmp(cmd, "A") == 0) {
			clear();
			fprintf(stderr, "add some friend of you!:\n>> ");
			memset(Send, 0, sizeof(Send));
			char sup[100] = {};
			scanf("%s",sup);
			if (strcmp(sup, myinfo.username) == 0) {
				fprintf(stderr, "why add yourself :/\n");
				fprintf(stderr, "type enter to continue...\n");
				getchar(); getchar();
				continue;
			}
			mysend(3);
			myrecv();
			if (Recv[0] != '0') {
				fprintf(stderr, "This should not happened...\n");
				exit(1);
			}
			send(server_fd, sup, strlen(sup), 0);
			myrecv();
			if (Recv[0] == '-' && Recv[1] == '1') {
				fprintf(stderr, "User not found...\n");
				fprintf(stderr, "type enter to continue...\n");
				getchar(); getchar();
			} else if (Recv[0] == '-' && Recv[1] == '2') {
				fprintf(stderr, "%s is already your friend!\n", sup);
				fprintf(stderr, "type enter to continue...\n");
				getchar(); getchar();
			} else {
				fprintf(stderr, "%s is now your friend ;)\n", sup);	
				fprintf(stderr, "type enter to continue...\n");
				memset(friends, 0, sizeof friends);
				recv(server_fd, friends, sizeof friends, 0);
				getchar(); getchar();
			}
		} else if (strcmp(cmd, "L") == 0) {
			raise(SIGINT);	
		} else if (strcmp(cmd, "W") == 0) {
			mysend(4);
			myrecv();
			clear();
			fprintf(stderr, "%s...is online!\n", Recv);
			fprintf(stderr, "type enter to continue...\n");
			getchar(); getchar();
		} else if (strcmp(cmd, "F") == 0) {
			while(1){
				clear();
				fprintf(stderr, "(R) recieve file\n");
				fprintf(stderr, "(S) send file\n");
				fprintf(stderr, "or ...(type /b to main page)\n>> ");
				scanf("%s", cmd);
				if (strcmp(cmd, "/b") == 0) break;
				else if (strcmp(cmd, "R") == 0) {
					clear();
					mysend(6);
					myrecv();
					if (Recv[0] == '-' && Recv[1] == '1'){
						fprintf(stderr, "No files coming.\n");
					} else {
						filenum = atoi(Recv);
						fprintf(stderr, "There are %d files haven't been received\n", filenum);
						for(int i = 1; i <= filenum; i++){
							mysend(i);
							myrecv();
							fprintf(stderr, "download %s to home directory?\n(yes then type y, otherwise type n)\n>> ", Recv);
							scanf("%s", cmd);
							if(cmd[0] == 'y'){
								recv_file(Recv, homedir);
							} else {
								mysend(-1);
								myrecv();
							}
						}
					}
				} else if (strcmp(cmd, "S") == 0){
					clear();
					mysend(5);
					send_file();
				}
				fprintf(stderr, "type enter to continue...\n");
				getchar(); getchar();
			}
		}
	}
}



// {{{ mysig
void my_sig(int signo) {
	if (signo == SIGINT) {
		fprintf(stderr, "\nbye bye!\n");
		exit(0);
	}
}
// }}}
