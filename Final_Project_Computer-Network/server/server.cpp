// {{{ include
#include <stdio.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// }}}
// {{{ variables
void login(void);
char Recv[100] = {};
char Send[100] = {};
int user_fd, my_fd;
void process(void);
int status;
int sockfd;
int BACKLOG = 100;
struct addrinfo hints;
struct addrinfo* servinfo;
struct timeval tv;
fd_set readfds;
char PORT[] = "3490";
socklen_t addr_size;
struct sockaddr_storage their_addr;
struct Client{
	char username[20];
	char password[20];
	int fd;
}client;
void myrecv() {
	memset(Recv, 0, sizeof Recv);
	if (recv(client.fd, Recv, sizeof Recv, 0) == 0) raise(SIGINT);
	return;
}
void mysend(int x) {
	sprintf(Send,"%d", x);
	send(client.fd, Send, strlen(Send), 0);
}
// }}}
// {{{ prevenzt zombie
void my_sig(int);
void init_signal() {
	struct sigaction act;
	act.sa_handler = my_sig;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NODEFER;
	if (sigaction(SIGCHLD, &act, NULL) == -1) {
		fprintf(stderr, "SIGCHLD err\n");
	}	
	if (sigaction(SIGINT, &act, NULL) == -1) {
		fprintf(stderr, "SIGINT err\n");
	}	
}
// }}} 
// {{{ main

int main() {
	fprintf(stderr, "server up...\n");
// {{{ pre-process
	init_signal();
	user_fd = openat(AT_FDCWD, "User", O_RDONLY);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status)); 
		exit(1);
	}
	if ((sockfd = socket(servinfo->ai_family,
					servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
		fprintf(stderr, "socket error\n");
		exit(1);
	}
	if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {

		fprintf(stderr, "bind error %s\n", strerror(errno));
		exit(1);
	}

	listen(sockfd, BACKLOG);
// }}}
	fprintf(stderr, "ready to accept...\n");
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		select(sockfd+1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(sockfd, &readfds)) { // new client
			if(fork() == 0) {
				addr_size = sizeof(their_addr);
				client.fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
				login();
				exit(0); // signal to prevent zombie
			}
		}
	}
	freeaddrinfo(servinfo); 
}

// }}}
// {{{ login()
char chk_login[100] = {};
void login() {
	int dir_fd;
	chdir("User");	
	myrecv();
again:
	if (strcmp(Recv, "/signup") == 0) { 
		// {{{ sign up
		mysend(0);
		while (1) {
			myrecv();
			if (openat(AT_FDCWD, Recv, O_RDONLY) > 0) { // already used
				mysend(-1);
			} else {
				strcpy(client.username, Recv);
				mysend(0);
				break;
			}
		}
		myrecv();
		strcpy(client.password, Recv);	
		mkdir(client.username, 0700);	
		if ((dir_fd=openat(AT_FDCWD, client.username,O_RDONLY)) < 0){
			fprintf(stderr, "open fail\n");
			exit(1);
		}
		int pwfd = openat(dir_fd, "pwd", O_CREAT | O_RDWR | O_TRUNC,0700);
		write(pwfd, client.password, strlen(client.password));
		close(pwfd);
		chdir(client.username);
		mkdir("file", 0700);
		chdir("../");
		mysend(0);
		process();
		return;
		// }}}
	} else { // get username
		// {{{ login loop
		while (1) {
			strcat(chk_login, "_l");
			strcat(chk_login, Recv);
			if ((dir_fd = openat(AT_FDCWD, Recv, O_RDONLY)) < 0) {
				mysend(-1);
				myrecv();
				goto again;
			} else { // require password
				if (openat(AT_FDCWD, chk_login, O_CREAT|O_EXCL) < 0) { // someone login
					mysend(-3);
					myrecv();
					goto again;
				}
				strcpy(client.username, Recv);
				mysend(0);
				myrecv();
				int pwfd = openat(dir_fd, "pwd", O_RDONLY);
				if (pwfd < 0) {
					fprintf(stderr, "pwfd open error\n");
					exit(1);
				}
				char password[20] = {}; read(pwfd, password, 20);
				while (1) {
					if (strcmp(password, Recv) == 0) { // correct
						mysend(0);
						strcpy(client.password, Recv);
						break;
					} else {
						mysend(-1);
						myrecv();
					}
				}
				close(pwfd);
				process();
				return;
			}
		}
		// }}}
	}
}

// }}}
// {{{ set_friend()
void set_friend() {
	int fd = openat(my_fd, "friend", O_CREAT | O_EXCL | O_RDONLY, 0600);
	if (fd < 0) {
		fd = openat(my_fd, "friend", O_RDONLY);
	}
	char rin[10000];
	if (read(fd, rin, 10000) < 0) {
		fprintf(stderr, "set_friend read fail\n");
		exit(1);
	}
	if (strlen(rin) == 0) {
		sprintf(rin, "-1");
	}
	send(client.fd, rin , strlen(rin), 0);
	close(fd);
	return;
}
// }}}
// {{{ chat()
void chat() {
	char chat_name[20] = {};
	char chat_name_log[25] = {};
	char my_name[20] = {};
	strcpy(my_name, client.username);
	int chat_fd;
	// chk user exist
	while (1) {
		memset(chat_name, 0 , sizeof chat_name);
		memset(chat_name_log, 0 , sizeof chat_name_log);
		recv(client.fd, chat_name, 20, 0);
		strcpy(chat_name_log, chat_name);
		strcat(chat_name_log, "_log");
		if (chat_name[0] == '9') {
			return;
		}
		if ((chat_fd = openat(user_fd, chat_name, O_RDONLY)) < 0) {
			mysend(-1);
			continue;
		} else {
			mysend(0);
			break;
		}
	}
	// start chat
	if ((chat_fd = openat(user_fd, chat_name, O_RDONLY)) < 0) {
		fprintf(stderr, "openat chat_name error");
		exit(1);
	}
	int mytxt, read_out, chattxt, chattxt_log;
	if ((mytxt = openat(chat_fd, my_name, O_SYNC | O_CREAT | O_EXCL|O_RDWR|O_APPEND, 0700) ) < 0) {
		mytxt = openat(chat_fd, my_name, O_RDWR | O_SYNC | O_APPEND);
	}
	if ((chattxt = openat(my_fd, chat_name,O_SYNC | O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0700) ) < 0) {
		chattxt = openat(my_fd, chat_name, O_RDWR | O_SYNC | O_APPEND);
	}
	if ((chattxt_log = openat(my_fd, chat_name_log,O_SYNC| O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0700) ) < 0) {
		chattxt_log = openat(my_fd, chat_name_log, O_RDWR | O_SYNC | O_APPEND);
	}
	if ((read_out = openat(my_fd, chat_name, O_RDONLY)) < 0) {
		fprintf(stderr, "This should not happened..\n");
		exit(0);
	}
	char msg[10000] = {};
	char ofs[20] = {};
	int offset = 0;
	if (read(chattxt_log, ofs, 20))
		sscanf(ofs, "%d", &offset);
	lseek(read_out, offset, SEEK_SET);
	while (1) {
		tv.tv_sec = 0;  
		tv.tv_usec = 100000;
		FD_ZERO(&readfds);
  		FD_SET(client.fd, &readfds);
		select(client.fd+1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(client.fd, &readfds)) { // new recv
			memset(Recv, 0, sizeof(Recv));
			if (!recv(client.fd, Recv, sizeof Recv, 0)) {
				offset = lseek(read_out, 0, SEEK_CUR);
				ftruncate(chattxt_log, 0);
				char rec[20] = {}; sprintf(rec, "%d", offset);
				write(chattxt_log, rec, strlen(rec));
				raise(SIGINT);
			} 
			if (Recv[0] == '-' && Recv[1] == '1' && Recv[2] == '2' && Recv[3] == '7') {
				offset = lseek(read_out, 0, SEEK_CUR);
				ftruncate(chattxt_log, 0);
				char rec[20] = {}; sprintf(rec, "%d", offset);
				write(chattxt_log, rec, strlen(rec));
				fprintf(stderr, "out offset: %d\n",offset); 
				break;
			}
			fprintf(stderr, "%s: %s",client.username,Recv);
			write(mytxt, Recv, strlen(Recv));
		}
		memset(msg, 0 ,sizeof msg);
		read(read_out, msg, sizeof msg);
		if (strlen(msg)) {
			send(client.fd, msg, strlen(msg), 0);
		}
	}
	close(mytxt), close(read_out), close(chattxt), close(chattxt_log);
}
// }}}
// {{{ send_file()
void send_file(){
	char send_to[64];
	int sendfile_fd, chk_fd;
	while(1){
		memset(send_to, 0 , sizeof send_to);
		recv(client.fd, send_to, 20, 0);
		if (send_to[0] == '9') return;
		if ((chk_fd = openat(user_fd, send_to, O_RDONLY)) < 0){
			mysend(-1);
			continue;
		}
		else {
			mysend(0);
			break;
		}
	}
	chdir("../"); chdir(send_to); chdir("file");
	char file_name[64], real_name[64], file_buf[105];
	while(1){
		//memset(file_name, '\0', sizeof file_name);
		myrecv();
		if (Recv[0] == '-' && Recv[1] == '2') break;
		mysend(0);
		sprintf(real_name, "%s-%s", Recv, client.username);
		sendfile_fd = open(real_name, O_RDWR | O_CREAT | O_TRUNC, 0777);
		while(1){
			//memset(file_buf, '\0', sizeof file_buf);
			myrecv();
			if (strcmp(Recv, "endoffile-----") == 0) break;
			write(sendfile_fd, Recv, strlen(Recv));
			mysend(0);
		}
		mysend(0);
		close(sendfile_fd);
	}
	chdir("../../"); chdir(client.username);
	return;
}
// }}}
// {{{ recv_file()
void recv_file(){
	DIR *dirp;
	struct dirent *direntp;
	char name_buf[100][100];
	int cnt = 0;
	if((dirp = opendir("file")) == NULL){
		fprintf(stderr, "no filename named file.\n");
		exit(1);
	}
	while((direntp = readdir(dirp)) != NULL){
		if(strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0) continue;
		else{
			strcpy(name_buf[cnt], direntp->d_name);
			cnt++;
		}
	}
	if(cnt == 0){
		mysend(-1);
		return;
	}
	mysend(cnt);
	chdir("file");
	int sendfile_fd, databuf[100];
	ssize_t n;
	for(int i = 0; i < cnt; i++){
		myrecv();
		send(client.fd, name_buf[i], strlen(name_buf[i]), 0);
		myrecv(); // send data?
		if(Recv[0] == '-' && Recv[1] == '1'){
			mysend(-1);
			unlink(name_buf[i]);
			continue;
		}
		sendfile_fd = open(name_buf[i], O_RDONLY);
		while((n = read(sendfile_fd, databuf, 80)) > 0){
			//fprintf(stderr, "aaa%s\n", databuf);
			send(client.fd, databuf, n, 0);
			myrecv();
		}
		send(client.fd, "endoffile-----", strlen("endoffile-----"), 0);
		close(sendfile_fd);
		unlink(name_buf[i]);
	}
	chdir("..");
	return;
}
// }}}
// {{{ process()
char login_file[100] = {}; 
void process() {
	fprintf(stderr, "%s login!\n", client.username);
	my_fd = openat(AT_FDCWD, client.username, O_RDONLY);
	chdir(client.username);
	strcat(login_file, "_l");
	strcat(login_file, client.username);
	if (openat(user_fd, login_file, O_CREAT) < 0) {
		fprintf(stderr, "open login_file error\n");
		exit(1);
	}
	// cwd is client.name
	set_friend();
	int friend_fd = openat(my_fd, "friend",O_RDWR | O_APPEND);
	while (1) { // recv cmds
		myrecv();
		if (Recv[0] == '2') { // start chat
			chat();		
		} else if (Recv[0] == '3') { // add friend
			mysend(0);
			myrecv();
			if (openat(user_fd, Recv, O_RDONLY) < 0) {
				mysend(-1);
			} else {
				char f[100] = {};
				strcat(f, "f_");
				strcat(f, Recv);
				if (openat(my_fd, f, O_CREAT | O_EXCL | O_RDONLY) < 0) {
					mysend(-2);
				} else {
					mysend(0);
					strcat(Recv, "\n");
					write(friend_fd, "-", 1);
					write(friend_fd, Recv, strlen(Recv));
					set_friend();
				}
			}
		} else if (Recv[0] == '4') { // who is online
			char snd[1000] = {};
			DIR* dp = opendir("..");
			struct dirent* rd;
			while ((rd = readdir(dp)) != NULL) {
				if (rd->d_name[0] == '_' && rd->d_name[1] == 'l') {
					strcat(snd, &rd->d_name[2]);
					strcat(snd, "\n");
				}
			}
			send(client.fd, snd, strlen(snd), 0);
		 } else if (Recv[0] == '5') { // send file
			send_file();
		 } else if (Recv[0] == '6') { // recv file
			recv_file();
		 }
	}

}
// }}}
void my_sig(int signo) {
	if (signo == SIGCHLD) {
		wait(NULL);
	}
	if (signo == SIGINT) {
		unlinkat(user_fd, login_file, 0);
		unlinkat(user_fd, chk_login, 0);
		fprintf(stderr, "%s logout\n", client.username);
		exit(0);
	}
}
