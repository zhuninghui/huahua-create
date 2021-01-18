#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "proto.h"
#include "string.h"

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char nickname[LENGTH_NAME] = {};

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

//************************************工具函数************************************
unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if (stat(path, &statbuff) < 0)
    {   
        return filesize;  
    }
    else
    {  
        filesize = statbuff.st_size;  
    }  
    return filesize; 
} 

void itoa(long i, char *string)
{
	long power, j;
	j = i;
	for (power = 1; j >= 10; j /= 10)
    {
		power *= 10;
    }
	for (; power > 0; power /= 10)
	{
		*string++ = '0' + i / power;
		i %= power;
	}
	*string = '\0';
}

void ls_print(char dirname[])
{
    DIR *dir_ptr;
    struct dirent *dir_entry = NULL;

    dir_ptr = opendir(dirname);
    while ((dir_entry = readdir(dir_ptr)) != NULL)
    {
        if(strcmp(dir_entry->d_name,".") == 0 || strcmp(dir_entry->d_name,"..") == 0)  
        {
            continue;
        }
        printf("%s ", dir_entry -> d_name);
    }
    printf("\n");
    closedir(dir_ptr);
} 
int if_true_filename(char filename[])
{
    int flag = 0;
    DIR *dir_ptr;
    dir_ptr = opendir(".");
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir_ptr)) != NULL)
    {
        if(strcmp(dir_entry->d_name,filename) == 0)
        {
            flag = 1;
            break;
        }
    }
    closedir(dir_ptr);
    return flag;
}

//************************************功能函数************************************
void view_ol_host_info(void)
{
    char command[LENGTH_CMD] = {};
    char recvOlHostInfo[LENGTH_SEND] = {};

    strncpy(command, "1", LENGTH_CMD);
    send(sockfd, command, LENGTH_CMD, 0);
    int receive = recv(sockfd, recvOlHostInfo, LENGTH_SEND, 0);
    if (receive > 0)
    {
        printf("\n在线主机信息：\n%s\n", recvOlHostInfo);
    }
    else
    {
        printf("receive error\n");
    }
}

void standby_mode(void)
{
    printf("进入待机模式\n");
    clock_t start, finish;
    char filename[LENGTH_NAME] = {};
    char sourcename[LENGTH_NAME] = {};

    recv(sockfd, filename, LENGTH_NAME, 0);
    recv(sockfd, sourcename, LENGTH_NAME, 0);
    printf("\n开始接受来自主机%s的文件%s\n", sourcename, filename);
    start = clock();
    FILE *fp = fopen(filename, "w+");
    if (fp == NULL)
    {
        printf("创建文件%s失败\n", filename);
        exit(1);
    }
    else
    {
        printf("创建文件%s成功\n", filename);
    }   
	char file_len[10] = {0};
	recv(sockfd, file_len, sizeof(file_len), 0);
	long file_size = atoi(file_len); 
	printf("文件大小 = %ld\n", file_size);
    char length_buffer[10] = {0};
    recv(sockfd, length_buffer, sizeof(length_buffer), 0);
    int LENGTH_BUFFER = atoi(length_buffer); 
    printf("传输文件块的字节数 = %d\n",LENGTH_BUFFER); 
    char data_buf[LENGTH_BUFFER];
	size_t fwtv, rrtv, len = 0;   
    double time;
	while(1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		rrtv = recv(sockfd, data_buf, sizeof(data_buf), 0);
		fwtv = fwrite(data_buf, 1, rrtv, fp);
        printf("[收到字节数: %ld，写入文件字节数：%ld]\n", strlen(data_buf) - 6, strlen(data_buf) - 6);
		fflush(fp);
		len += fwtv;
		if (len == file_size)
		{
			break;
		}
	}
    close(fp);
    finish = clock();
    time = (double)(finish - start) / CLOCKS_PER_SEC; 
    printf("文件发送到主机一共所用时间%f秒\n",time);
    printf("成功从主机%s接受文件%s\n\n", sourcename, filename);
}

void p2p_trans(void)
{
    char command[LENGTH_CMD] = {};
    char targetname[LENGTH_NAME] = {};
    char targetfilename[LENGTH_NAME] = {};

    strncpy(command, "3", LENGTH_CMD);
    send(sockfd, command, LENGTH_CMD, 0);

    printf("本地文件列表如下：\n");
    ls_print(".");
    printf("请输入点对点传输目标主机名：");
    scanf("%s", targetname);
    label:
    printf("请输入点对点传输文件名：");
    scanf("%s", targetfilename);
    if (if_true_filename(targetfilename))
    {
        printf("该文件在客户端存在，可以发送\n");
    }
    else 
    {
        printf("请重新输入\n");
        goto label;
    }

    printf("\n开始将文件%s转发至主机%s", targetfilename, targetname);

    FILE *fp = fopen(targetfilename, "r");
    if (fp == NULL)
    {
        printf("\n打开文件%s失败\n", targetfilename);
        exit(1);
    }
    else
    {
        printf("\n打开文件%s成功\n", targetfilename);
    }

    send(sockfd, targetname, LENGTH_NAME, 0);
    send(sockfd, targetfilename, LENGTH_NAME, 0);

	long file_size = get_file_size(targetfilename); 
	printf("文件大小 = %ld\n", file_size);
	char file_len[10] = {0};
	itoa(file_size, file_len);
	send(sockfd, file_len, sizeof(file_len), 0);

    
    int LENGTH_BUFFER;
    char choice;
    label3:
    printf("以下是可供选择的传输文件块的字节数\n");
    printf("A:128  B:256  C:1024  D:2048\n"); 
    printf("请输入选项，选择传输文件块的字节数：");
    scanf("%c", &choice);
    switch (choice)
    {
    case 'A':
    case 'a':
        LENGTH_BUFFER = 128;
        break;
    case 'B':
    case 'b':
        LENGTH_BUFFER = 256;
        break;
    case 'C':
    case 'c':
        LENGTH_BUFFER = 1024;
        break;
    case 'D':
    case 'd':
        LENGTH_BUFFER = 2048;
        break;
    default:
        printf("请重新输入\n");
        goto label3;
    }
    char length_buffer[10] = {0};
    itoa((long)LENGTH_BUFFER, length_buffer);
    send(sockfd, length_buffer, sizeof(length_buffer), 0);
	char data_buf[LENGTH_BUFFER];
	size_t frtv;
	while (1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		frtv = fread(data_buf, 1, LENGTH_BUFFER, fp);
		if (frtv == 0)
		{
			break;
		}
		send(sockfd, data_buf, frtv, 0);
        printf("[读取文件字节数: %ld，发送字节数：%ld]\n", strlen(data_buf) -6, strlen(data_buf) -6);
	}
    close(fp);
	printf("成功将文件%s传输至服务器\n", targetfilename);

    char message[LENGTH_MSG] = {};
    recv(sockfd, message, LENGTH_MSG, 0);
    if(strcmp(message, "success") == 0)
    {
    	printf("成功将文件%s传输至主机%s\n\n", targetfilename, targetname);
    }
}

void broadcast_trans(void)
{
    char command[LENGTH_CMD] = {};
    char targetfilename[LENGTH_NAME] = {};

    strncpy(command, "4", LENGTH_CMD);
    send(sockfd, command, LENGTH_CMD, 0);

    printf("本地文件列表如下：\n");
    ls_print(".");
    label2:
    printf("请输入广播传输文件名：");
    scanf("%s", targetfilename);
    if(if_true_filename(targetfilename))
    {
        printf("该文件在客户端存在，可以发送\n");
    }
    else 
    {
        printf("请重新输入\n");
        goto label2;
    }
    printf("\n开始将文件%s广播至所有在线主机\n", targetfilename);

    FILE *fp = fopen(targetfilename, "r");
    if (fp == NULL)
    {
        printf("\n打开文件%s失败\n", targetfilename);
        exit(1);
    }
    else
    {
        printf("\n打开文件%s成功\n", targetfilename);
    }

    send(sockfd, targetfilename, LENGTH_NAME, 0);

	long file_size = get_file_size(targetfilename); 
	printf("文件大小 = %ld\n", file_size);
	char file_len[10] = {0};
	itoa(file_size, file_len);
	send(sockfd, file_len, sizeof(file_len), 0);
    
    int LENGTH_BUFFER;
    char choice;
    label3:
    printf("以下是可供选择的传输文件块的字节数\n");
    printf("A:128  B:256  C:1024  D:2048\n"); 
    printf("请输入选项，选择传输文件块的字节数：");
    scanf("%c", &choice);
    switch (choice)
    {
    case 'A':
    case 'a':
        LENGTH_BUFFER = 128;
        break;
    case 'B':
    case 'b':
        LENGTH_BUFFER = 256;
        break;
    case 'C':
    case 'c':
        LENGTH_BUFFER = 1024;
        break;
    case 'D':
    case 'd':
        LENGTH_BUFFER = 2048;
        break;
    default:
        printf("请重新输入\n");
        goto label3;
    }
    if(if_true_filename(targetfilename))
    {
        printf("该文件在客户端存在，可以发送\n");
    }
    else 
    {
        printf("请重新输入\n");
        goto label2;
    }
    char length_buffer[10] = {0};
    itoa((long)LENGTH_BUFFER, length_buffer);
    send(sockfd, length_buffer, sizeof(length_buffer), 0);
	char data_buf[LENGTH_BUFFER];
	size_t frtv;
	while (1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		frtv = fread(data_buf, 1, LENGTH_BUFFER, fp);
		if (frtv == 0)
		{
			break;
		}
		send(sockfd, data_buf, frtv, 0);
        printf("[读取文件字节数: %ld，发送字节数：%ld]\n", strlen(data_buf) -6, strlen(data_buf) -6);
	}
    close(fp);
	printf("成功将文件%s传输至服务器\n", targetfilename);

    char message[LENGTH_MSG] = {};
    recv(sockfd, message, LENGTH_MSG, 0);
    if(strcmp(message, "success") == 0)
    {
    	printf("成功将文件%s广播至所有主机\n\n", targetfilename);
    }
}

void client_disconnect(void)
{
    char command[LENGTH_CMD] = {};

    strncpy(command, "0", LENGTH_CMD);
    send(sockfd, command, LENGTH_CMD, 0);
}

// 加载菜单界面
void menu(void)
{
    for(;;)
    {

        printf("********* 客户端 *********\n");
        printf("1、查看在线主机信息\n");
        printf("2、待机模式\n");
        printf("3、点对点传输\n");
        printf("4、广播传输\n");
        printf("0、退出\n");
        printf("**************************\n");

        switch (get_cmd('0', '4'))
        {
            case '1':
                view_ol_host_info();
                break;
            case '2':
                standby_mode();
                break;
            case '3':
                p2p_trans();
                break;
            case '4':
                broadcast_trans();
                break;
            case '0':
                client_disconnect();
                return;
        }
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, catch_ctrl_c_and_exit);

    // Naming
    printf("Please enter your name: ");
    if (fgets(nickname, LENGTH_NAME, stdin) != NULL) {
        str_trim_lf(nickname, LENGTH_NAME);
    }
    if (strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME-1) {
        printf("\nName must be more than one and less than thirty characters.\n");
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd == -1) {
        printf("Fail to create a socket.");
        exit(EXIT_FAILURE);
    }

    // Socket information
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    // server_info.sin_addr.s_addr = inet_addr("127.0.0.1");
    inet_pton(PF_INET, argv[1], &server_info.sin_addr);
    server_info.sin_port = htons(8888);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_info, s_addrlen);
    if (err == -1) {
        printf("Connection to Server error!\n");
        exit(EXIT_FAILURE);
    }
    
    // Names
    getsockname(sockfd, (struct sockaddr*) &client_info, (socklen_t*) &c_addrlen);
    getpeername(sockfd, (struct sockaddr*) &server_info, (socklen_t*) &s_addrlen);
    printf("Connect to Server: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    printf("You are: %s:%d\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

    send(sockfd, nickname, LENGTH_NAME, 0);

    menu();

    close(sockfd);
    return 0;
}