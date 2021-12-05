#include<stdio.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<strings.h>
#include<arpa/inet.h>
#include<string.h>

#define BUFSIZE 4096


// return: (-1: error), (>-1: fd number)
int con_server();


// Input: (fd: server fd)
// 1. get password and account.
// 2. check
// return: (-1: error), (0: success)
int login(int fd);

int cmd_handler(char *cmd, int fd);
void cmd_help();
int cmd_logout(int servfd); 
void cmd_list(int fd);
int cmd_play(int fd, char *target);
void play_handler(int fd);
void play_req(int fd, char *buf);
void play_put(int fd, char *buf);

int main(){
	int servfd;
	int ret = 0;
	servfd = con_server();
	ret = login(servfd);
	while(-1 == ret){
		printf("\n!!---------------!!\n");
		printf("Error account or password\n");
		printf("Please try agin...\n");
		printf("!!---------------!!\n\n");
		ret = login(servfd);
		printf("ret = %d\n", ret);
	}

	char buf[BUFSIZE];
	
	fd_set rset;


	printf("You could input cmd \"help\"\n");
	printf("Input cmd:\n");
	while(1){
		FD_ZERO(&rset);
		FD_SET(0, &rset); // stdin
		FD_SET(servfd, &rset);
		select(FD_SETSIZE, &rset, (fd_set *)0, (fd_set *)0, NULL);
		if(FD_ISSET(0, &rset)){
			memset(buf, 0, BUFSIZE);
			read(0, buf, BUFSIZE);
			ret = cmd_handler(buf, servfd);
			if(ret == -1){
				break;
			}
		}
		else if(FD_ISSET(servfd, &rset)){
			play_handler(servfd);
		}
	}

	close(servfd);
	return 0;
}





int con_server(){
	printf("Connect to server...\n");
	int domain = AF_INET;
	int type = SOCK_STREAM;
	int protocol = 0;
	int fd = socket(domain, type, protocol);
	
	int ret;
	struct sockaddr_in servaddr;
	int addrlen = sizeof(servaddr);
	if(fd == -1){
		perror("Socket");
		exit(1);
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(12345);

	ret = connect(fd, (struct sockaddr *)&servaddr, addrlen);
	if(ret == -1){
		perror("Connect");
		exit(1);
	}
	printf("Connect success\n");
	return fd;
}


int login(int fd){
	char buf[32];
	memset(buf, 0, 32);
	char acnt[16];
	char password[16];

	printf("Please enter your account name:\n");
	scanf("%s", acnt);
	printf("Please enter your account password:\n");
	scanf("%s", password);
	sprintf(buf, "%s:%s", acnt, password);
	write(fd, buf, sizeof(buf));
	read(fd, buf, sizeof(buf));
	printf("login success\n");
	if('f' == buf[0]){
		return -1;
	}
	else if('s' == buf[0]){
		return 0;
	}
}


int cmd_handler(char *cmd, int fd){
	if(strncmp(cmd, "help", 4) == 0){
		cmd_help();
	}
	else if(strncmp(cmd, "logout", 6) == 0){
		cmd_logout(fd);
		return -1;
	}
	else if(strncmp(cmd, "list", 4) == 0){
		cmd_list(fd);
	}
	else if(strncmp(cmd, "play", 4) == 0){
		int ret = cmd_play(fd, &cmd[5]);
		return ret;
	}
	else if(strncmp(cmd, "put", 3) == 0){
		play_put(fd, cmd);
	}
	else{
		printf("Error cmd!\n");
	}
	return 0;
}

void cmd_help(){
	printf("\n--------------------------------\n");
	printf("help: print all of cmd\n");
	printf("list: print all of online users\n");
	printf("logout: logout and exit\n");
	printf("play [name]: play with \"name\"\n");
	printf("put #-#: (#, 0~2), ex: put 0-1\n");
	printf("--------------------------------\n");
}

int cmd_logout(int servfd){
	printf("logout...\n");
	write(servfd, "logout", BUFSIZE);
	printf("Success\n");
	return 0;
}

void cmd_list(int fd){
	write(fd, "list", BUFSIZE);
	char buf[BUFSIZ];
	read(fd, buf, BUFSIZ);
	printf("Online users:\n");
	for(int i = 0; i < BUFSIZE; i++){
		if(buf[i] == '\0'){
			break;
		}
		printf("%c", buf[i]);
	}
}

int cmd_play(int fd, char *target){
	char buf[BUFSIZE];
	char tar_name[16];
	memset(tar_name, 0, 16);
	for(int i = 0; i < 16; i++){
		if(target[i] != 0){
			tar_name[i] = target[i];
		}
	}
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "play %s", tar_name);
	for(int i = 0; i < 16; i++){
		buf[5+i] = tar_name[i];
	}
	write(fd, buf, BUFSIZE);
	printf("Send play request...\n");
	read(fd, buf, BUFSIZE);
	if(buf[0] == 's'){
		printf("The invitation be accept.\n");
		printf("You play first:\n");
		printf(" | | \n-----\n | | \n-----\n | | \n");
		return 1;
	}
	else{
		printf("The invitation be reject.\n");
		printf("Please try it again.\n");
		return 0;
	}
}

void play_handler(int fd){
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	read(fd, buf, BUFSIZE);
	if(strncmp(buf, "req", 3) == 0){
		play_req(fd, buf);
	}
	else{
		write(1, buf, BUFSIZE);
	}
}

void play_req(int fd, char *buf){
	char res[8];
	printf("%s want play with you.\n", &buf[4]);
	printf("yes\\no:");
	scanf("%s", res);
	if(res[0] == 'y'){
		write(fd, "game_accept", 16);
	}
	else{
		write(fd, "game_reject", 16);
	}
}

void play_put(int fd, char *buf){
	write(fd, buf, BUFSIZE);
	read(fd, buf, BUFSIZE);
	printf("%s", buf);
}