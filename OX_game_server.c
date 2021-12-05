#include<stdio.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<strings.h>
#include<arpa/inet.h>
#include<signal.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<string.h>
#include<sys/select.h>

typedef struct clifd{
    int fd;
    int id;
    char name[16];
    struct clifd *next;
}clifd;

// turn: (1, fir) , (0, sec)
// alive: (0, can create), (1, be used)
typedef struct game_room{
    int fir_id, sec_id;
    int fd[2];
    int turn, alive;
    char cb[3][3]; // checkerboard
}game_room;


#define BUFSIZE 4096
#define ACC_NUM 10
#define ROOM_NUM 5

game_room groom[ROOM_NUM];

void sig_chld(int);
void serve(int fd, int *id);


void init_database();
int login_check(char *buf);
void sv_login(int fd, int *id, char *buf);
int sv_logout(int id);
void sv_list(int fd);
// return: (-1, reject), (0, accept)
int sv_play(int fd, int id, char *buf);
// initial all of room
void init_room();
//create a room for user
void room_init(int tar_id, int src_id, int tar_fd, int src_fd);
int room_mk();
void room_cls();
int room_find(int fd);
void g_acc(int fd);
void g_rej(int fd);
void g_put(int fd, char *buf);
void disp_cb(int room_id, char *buf);
int judge(int r_id);

// account database which is for login.
char database[ACC_NUM][32];
char data_name[ACC_NUM][16];

//connect client fd
clifd *head = NULL, *rear = NULL;

int main(){
    // initail account data
    init_database();
    // initail game room
    init_room();

	int status;
    int listenfd, connfd;
    pid_t child_pid;
    socklen_t chilen;
    struct sockaddr_in cliaddr, servaddr;
    void sig_chld(int);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(12345);

    
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1} , sizeof(int)) < 0)
        perror("setsockopt");
    status = bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(status == -1){
        perror("Bind Error");
        exit(1);
    }

    fd_set rset;
    int maxfd;

    FD_ZERO(&rset);

    status = listen(listenfd, 128);
    if(status == -1)printf("error:LISTEN\n");
    
    signal(SIGCHLD, sig_chld);
    

    
    for(;;){
        FD_ZERO(&rset);
        FD_SET(listenfd, &rset);
        // add all of client fd to select.
        for(clifd *pos = head; pos != NULL; pos = pos->next){
            printf("fd %d add to select\n", pos->fd);
            FD_SET(pos->fd, &rset);
        }
        select(FD_SETSIZE, &rset, (fd_set *)0, (fd_set *)0, NULL);
        printf("select done\n");
        if(FD_ISSET(listenfd, &rset)){
            connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &chilen);
            if(head == NULL){
                head = malloc(sizeof(clifd));
                head->fd = connfd;
                head->next = NULL;
                head->id = -1;
                rear = head;
            }
            else{
                clifd *tmp = malloc(sizeof(clifd));
                rear->next = tmp;
                rear = rear->next;
                rear->id = -1;
                rear->fd = connfd;
                rear->next = NULL;
            }
        }
        else{
            for(clifd *pos = head; pos != NULL; pos = pos->next){
                if(FD_ISSET(pos->fd, &rset)){
                    printf("fd is %d\n", pos->fd);
                    serve(pos->fd, &(pos->id));
                    break;
                }
            }
        }
        
	}
	
	return 0;
}

// kill zombie child
void sig_chld(int sig_num){
    waitpid(-1, NULL, 0);
    return;
}

void serve(int fd, int *id){
    char buf[BUFSIZE];
    int ret;
    int this_acc;
    memset(buf, 0, BUFSIZE);
    read(fd, buf, BUFSIZE);
    printf("buf = %s\n", buf);
    if(*id == -1){
        sv_login(fd, id, buf);
    }
    else{
        if(strncmp(buf, "list", 4) == 0){
            sv_list(fd);
        }
        else if(strncmp(buf, "logout", 6) == 0){
            sv_logout(*id);
        }
        else if(strncmp(buf, "play", 4) == 0){
            sv_play(fd, *id, buf);
        }
        else if(strncmp(buf, "put", 3) == 0){
            g_put(fd, buf);
        }
        else if(strncmp(buf, "game_accept", 11) == 0){
            g_acc(fd);
        }
        else if(strncmp(buf, "game_reject", 11) == 0){
            g_rej(fd);
        }
        else{
            printf("Error serve: %s\n", buf);
        }
    }
}

int login_check(char *buf){
    for(int i = 0; i < ACC_NUM; i++){
        if(strcmp(buf, database[i]) == 0){
            return i;
        }
    }
    return -1;
}

void init_database(){
    strcpy(database[0], "apple:10520");
    strcpy(database[1], "banana:123456");
    strcpy(database[2], "cat:54acat");
    strcpy(database[3], "t:123");
    for(int i = 0; i < ACC_NUM; i++){
        memset(data_name[i], 0, 16);
    }
    strcpy(data_name[0], "apple");
    strcpy(data_name[1], "banana");
    strcpy(data_name[2], "cat");
    strcpy(data_name[3], "t");
}

int sv_logout(int id){
    clifd *pre = head;
    for(clifd *pos = head; pos != NULL; pre = pos, pos = pos->next){
        if(id == pos->id){
            printf("id:%d, fd:%d  logout\n", pos->id, pos->fd);
            if(pos == head){
                clifd *new_next = head->next;
                close(head->fd);
                free(head);
                head = new_next;
            }
            else{
                pre->next = pos->next;
                close(pos->fd);
                free(pos);
            }
            return 0;
        }
    }
    return -1;
}

void sv_list(int fd){
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    int idx = 0;
    for(clifd *pos = head; pos != NULL; pos = pos->next){
        for(int i = 0; i < 16; i++){
            if(database[pos->id][i] != ':'){
                buf[idx] = database[pos->id][i];
                idx++;
            }
            else{
                buf[idx] = '\n';
                idx++;
                break;
            }
        }
    }
    // printf("list buf = \n");
    // for(int i = 0; i < idx; i++)
    //     printf("%c", buf[i]);
    write(fd, buf, BUFSIZE);
}

int sv_play(int fd, int id, char *buf){
    char target[16], src[16];
    int tar_id = -1, src_id = id;
    int tar_fd = -1, src_fd = fd;
    memset(target, 0, 16);
    memset(src, 0, 16);
    strcpy(src, data_name[id]);
    strcpy(target, &buf[5]);
    for(int i = 0; i < 16; i++){
        if(target[i] == '\n'){
            target[i] = '\0';
        }
    }
    for(int i = 0; i < ACC_NUM; i++){
        if(strcmp(data_name[i], target) == 0){
            tar_id = i;
            break;
        }
    }
    if(tar_id == src_id || tar_id == -1){
        write(src_fd, "reject", 8);
        return -1;
    }
    // printf("tar_id = %d\n", tar_id);
    for(clifd *pos = head; head != NULL; pos = pos->next){
        if(pos->id == tar_id){
            tar_fd = pos->fd;
            write(tar_fd, "req ", 4);
            write(tar_fd, src, 16);
            room_init(tar_id, src_id, tar_fd, src_fd);
            return 0;
        }
    }
    //Error: can't find target
    write(src_fd, "reject", 8);
    return -1;
}

void sv_login(int fd, int *id, char *buf){
    int ret;
    if((ret = login_check(buf)) == -1){
        write(fd, "f", 2);
    }
    else{
        write(fd, "s", 2);
        *id = ret;
        char name[16];
        memset(name, 0, 16);
        strcpy(name, data_name[*id]);
        for(clifd *pos = head; pos != NULL; pos = pos->next){
            if(*id == pos->id){
                strncpy(pos->name, name, 16);
            }
        }
    } 
}

void init_room(){
    for(int i = 0; i < ROOM_NUM; i++){
        groom[i].alive = 0;
        for(int j = 0; j < 3; j++)
            for(int k = 0; k < 3; k++)
                groom[i].cb[j][k] = ' ';
        groom[i].turn = 0;
    }
}

void room_init(int tar_id, int src_id, int tar_fd, int src_fd){
    // printf("%d %d %d %d\n", src_fd, src_id, tar_fd, tar_id);
    int r_id = room_mk();
    if(r_id == -1){
        printf("ROOM error\n");
    }
    groom[r_id].fd[0] = src_fd;
    groom[r_id].fd[1] = tar_fd;
    groom[r_id].fir_id = src_id;
    groom[r_id].sec_id = tar_id;
    printf("Create a room.\n");
}

int room_mk(){
    for(int i = 0; i < ROOM_NUM; i++){
        if(groom[i].alive == 0){
            groom[i].alive = 1;
            return i;
        }
    }
    return -1;
}

void room_cls(int room_id){
    groom[room_id].alive = 0;
    groom[room_id].turn = 0;
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            groom[room_id].cb[i][j] = ' ';
    
}

int room_find(int fd){
    // printf("room_find fd = %d\n", fd);
    for(int i = 0; i < ROOM_NUM; i++){
        // printf("%d %d\n", groom[i].fd[0], groom[i].fd[1]);
        if(groom[i].fd[0] == fd || groom[i].fd[1] == fd){
            return i;
        }
    }
    return -1;
}

void g_rej(int fd){
    int ret = room_find(fd);
    if(ret == -1){
        printf("Error: room_find\n");
    }
    write(groom[ret].fd[0], "reject", 8);
    room_cls(ret);
}

void g_acc(int fd){
    int ret = room_find(fd);
    write(groom[ret].fd[0], "s", 4);
}

void g_put(int fd, char *buf){
    int pos1 = -1, pos2 = -1;
    pos1 = buf[4] -'0';
    pos2 = buf[6] -'0';
    int r_id = room_find(fd);
    if(pos1 > 2 || pos2 > 2 || pos1 < 0 || pos2 < 0){
        write(fd, "put_error\n", 16);
        disp_cb(r_id, buf);
        write(fd, buf, BUFSIZE);
        return;
    }
    if(groom[r_id].cb[pos1][pos2] != ' '){
        write(fd, "put_error\n", 16);
        disp_cb(r_id, buf);
        write(fd, buf, BUFSIZE);
        return;
    }
    char type = ' '; 
    if(groom[r_id].turn == 0){
        type = 'O';
    }
    else{
        type = 'X';
    }
    int turn = groom[r_id].turn;
    if(fd != groom[r_id].fd[turn]){
        write(fd, "It is not you turn!\n", 20);
        return; 
    }
    groom[r_id].cb[pos1][pos2] = type;
    disp_cb(r_id, buf);
    write(groom[r_id].fd[0], buf, BUFSIZE);
    write(groom[r_id].fd[1], buf, BUFSIZE);
    int ret = judge(r_id);
    if(ret > 0){ // game is over
        if (ret == 1){ // win-loss
            write(groom[r_id].fd[turn], "You win!\n", 9);
            write(groom[r_id].fd[!turn], "You lose!\n", 9);
        } 
        else if(ret == 2){ // tie
            write(groom[r_id].fd[!turn], "Tie!\n", 5);
            write(groom[r_id].fd[turn], "Tie!\n", 5);
        }
        room_cls(r_id);
        return;
    }
    // ret == 0, keep game
    write(groom[r_id].fd[!turn], "It is your turn.\n", 17);
    // printf("turn = %d %d\n", turn, !turn);
    groom[r_id].turn = !(groom[r_id].turn);
}

void disp_cb(int room_id, char *buf){
    int idx = 0;
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            buf[idx++] = groom[room_id].cb[i][j];
            if(j < 2)
                buf[idx++] = '|';
            else
                buf[idx++] = '\n';
        }
        if(i < 2){
            for(int k = 0; k < 5; k++, idx++){
                buf[idx] = '-';
            }
            buf[idx++] = '\n';
        }
    }
    buf[idx] = '\0';
    // printf("%s\n", buf);
}

int judge(int r_id){
    int tie = 1;
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            if(groom[r_id].cb[i][j] == ' '){
                tie = 0;
            }
        }
    }
    if(tie == 1){
        return 2;
    }
    for(int i = 0; i < 3; i++){
        if(groom[r_id].cb[i][0] == groom[r_id].cb[i][1] &&
        groom[r_id].cb[i][0] == groom[r_id].cb[i][2] &&
        groom[r_id].cb[i][0] != ' '){
            return 1;
        }
        if(groom[r_id].cb[0][i] == groom[r_id].cb[1][i] &&
        groom[r_id].cb[0][i] == groom[r_id].cb[2][i] &&
        groom[r_id].cb[0][i] != ' '){
            return 1;
        }
    }
    if(groom[r_id].cb[0][0] == groom[r_id].cb[1][1] &&
        groom[r_id].cb[0][0] == groom[r_id].cb[2][2] &&
        groom[r_id].cb[0][0] != ' '){
        return 1;
    }
    if(groom[r_id].cb[0][2] == groom[r_id].cb[1][1] &&
        groom[r_id].cb[0][2] == groom[r_id].cb[2][0] &&
        groom[r_id].cb[0][2] != ' '){
        return 1;
    }
    return 0;
}