/*
 * chatclient.cpp
 * 客户端
 */
#include<unistd.h>
#include <stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "pub.h"
#define ERR_EXIT(m)\
	do {perror(m);\
	exit(EXIT_FAILURE);\
	} while(0)
char username[16]; //存储当前用户名
USER_LIST client_list; //维护当前在线用户列表
void do_someone_login(MESSAGE &msg)
{
	USER_INFO *user = (USER_INFO*) msg.body;
	struct in_addr temp;
	temp.s_addr = user->ip;
	printf("[%s] %s:%d has login in server!\n", user->username, inet_ntoa(temp),
			ntohs(user->port));
	client_list.push_back(*user);
}
void do_someone_logout(MESSAGE &msg)
{
	USER_LIST::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++)
	{
		if (strcmp(it->username,username) == 0)
			break;
	}
	if (it != client_list.end())
		client_list.erase(it);//双方都要更新维护的在线列表
	printf("user %s has logout from server!\n", it->username);
}
//由服务端触发，不需要写地址
void do_online_user(int sock)
{
	client_list.clear();
	int count;
	int ret = recvfrom(sock, &count, sizeof(int), 0, NULL, NULL);
	if (ret == -1)
		ERR_EXIT("S2C-do_online_user-recvfrom1");
	int n = ntohl(count);
	printf("there are %d users login in the server!\n", n);
	for (int i = 0; i < n; i++)
	{
		USER_INFO user;
		ret = recvfrom(sock, &user, sizeof(USER_INFO), 0, NULL, NULL);
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			ERR_EXIT("S2C-do_online_user-recvfrom2");
		}
		client_list.push_back(user);
		struct in_addr temp;
		temp.s_addr = user.ip;
		printf("[%d] user [%s] address %s:%d\n", i + 1, user.username,
				inet_ntoa(temp), ntohs(user.port));
	}
}
void do_chat(const MESSAGE &msg)
{
	CHAT_MSG *cm = (CHAT_MSG *) msg.body;
	printf("recv a message %s from user [%s]\n", cm->msg, cm->username);
}
bool sendmsgto(int sock, char* peername, char* msg)
{
	if (strcmp(peername, username) == 0)
	{
		printf("can not send to yourself!\n");
		return false;
	}
	USER_LIST::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++)
	{
		if (strcmp(it->username, peername) == 0)
			break;
	}
	//没找到用户
	if (it == client_list.end())
	{
		printf("user [%s] is not login in server!\n", peername);
		return false;
	}
	//找到用户
	MESSAGE message;
	memset(&message, 0, sizeof(message));
	message.cmd = htonl(C2C_CHAT);

	//封装聊天消息
	CHAT_MSG chat_msg;
	memset(&chat_msg, 0, sizeof(chat_msg));
	strcpy(chat_msg.username, username);
	strcpy(chat_msg.msg, msg);
	//把聊天消息作为消息对象发出去
	memcpy(message.body, &chat_msg, sizeof(chat_msg));

	struct sockaddr_in peeraddr;
	socklen_t peerlen = sizeof(peeraddr);
	memset(&peeraddr, 0, peerlen);
	peeraddr.sin_family = PF_INET;
	peeraddr.sin_port = it->port;
	peeraddr.sin_addr.s_addr = it->ip;

	struct in_addr temp;
	temp.s_addr = it->ip;
	printf("sending message from [%s] to [%s] %s:%d\n", username, peername,
			inet_ntoa(temp), ntohs(it->port));
	//发送点对点聊天信息
	if (sendto(sock, (const char*) &message, sizeof(message), 0,
			(struct sockaddr*) &peeraddr, peerlen) < 0)
		ERR_EXIT("C2C-msg-sendto");
	return true;
}
void parse_cmd(char* cmdline, int sock, struct sockaddr_in *seraddr)
{
	char cmd[10] = { 0 };
	char *p;
	p = strchr(cmdline, ' ');//找第一个空格
	if (p != NULL)
		*p = '\0'; //将空格替换为\0
	strcpy(cmd, cmdline); //strcpy遇\0停止
	if (strcmp(cmd, "exit") == 0)
	{
		//往服务器发送退出命令
		MESSAGE msg;
		memset(&msg, 0, sizeof(msg));
		msg.cmd = htonl(C2S_LOGOUT);
		strcpy(msg.body, username);
		if (sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr*) seraddr,
				sizeof(*seraddr)) < 0)
			ERR_EXIT("C2S-parse_cmd-sendto");
		printf("user %s has logout server\n", username);
		exit(0);
	} else if (strcmp(cmd, "send") == 0)
	{
		//目的客户端名
		char peername[16] = { 0 };
		char msg[100] = { 0 };
		while (*(++p) == ' ')
			;
		char *p2;
		p2 = strchr(p, ' ');
		if (p2 == NULL)
		{
			printf("Bad command\n");
			printf("\ncommands are:\n");
			printf("send [username] [message]\n");
			printf("list\n");
			printf("exit\n");
			printf("\n");
			return; //不用执行后面代码
		}
		*p2 = '\0';
		strcpy(peername, p);
		while (*(++p2) == ' ')
			;
		strcpy(msg, p2);

		sendmsgto(sock, peername, msg);

	} else if (strcmp(cmd, "list") == 0)
	{
		MESSAGE msg;
		memset(&msg, 0, sizeof(msg));
		msg.cmd = htonl(C2S_ONLINE_USE);
		strcpy(msg.body, username);
		if (sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr*) seraddr,
				sizeof(*seraddr)) < 0)
			ERR_EXIT("C2S-msg-sendto");
	} else
	{
		//输入错误命令
		printf("Bad command\n");
		printf("\ncommands are:\n");
		printf("send [username] [message]\n");
		printf("list\n");
		printf("exit\n");
		printf("\n");
	}
}
void chat_client(int sock)
{
	//初始化服务端
	struct sockaddr_in seraddr;
	memset(&seraddr, 0, sizeof(seraddr));
	socklen_t seraddr_len = sizeof(seraddr);
	seraddr.sin_family = PF_INET;
	seraddr.sin_port = htons(10000);
	seraddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	//对等方聊天时使用
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

	//维护接收服务端消息
	MESSAGE msg;
	int ret;
	while (1)
	{
		memset(username, 0, sizeof(username));
		printf("input your name:");
		fflush (stdout);
		//scanf("%s", username);
		if(fgets(username,sizeof(username),stdin) == NULL)
			ERR_EXIT("fgets");
		//解决多一个回车的问题
		int len  = strlen(username);
		username[len-1]='\0';
		//向服务器发送消息：登陆命令,用户名
		memset(&msg, 0, sizeof(msg));
		msg.cmd = htonl(C2S_LOGIN);
		strcpy(msg.body, username);
		ret = sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr*) &seraddr,
				seraddr_len);
		if (ret == -1)
		{
			ERR_EXIT("sendto-C2S_LOGIN");
		}
		//接收服务器应答
		memset(&msg, 0, sizeof(msg));
		ret = recvfrom(sock, &msg, sizeof(msg), 0, (struct sockaddr*) &seraddr,
				&seraddr_len);
		if (ret == -1)
			ERR_EXIT("C2S-recvfrom");
		int cmd = ntohl(msg.cmd);
		if (cmd == S2C_LOGIN_OK)
		{
			printf("user %s logined !\n", username);
			break;
		} else if (cmd == S2C_ALREADY_LOGINED)
		{
			printf("user %s is already logined! please enter another name !\n",
					username);
		}
	}

	//接收服务器发送的在线人数
	int count;
	ret = recvfrom(sock, &count, sizeof(int), 0, (struct sockaddr*) &seraddr,
			&seraddr_len);
	if (ret == -1)
		ERR_EXIT("S2C-count-recvfrom");
	int n = ntohl(count);
	printf("there are %d users logined in the server!\n", n);

	//接收服务器发送的在线用户
	for (int i = 0; i < n; i++)
	{
		USER_INFO user;
		ret = recvfrom(sock, &user, sizeof(USER_INFO), 0,
				(struct sockaddr*) &seraddr, &seraddr_len);
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			ERR_EXIT("S2C-onlineuser-recvfrom");
		}
		//将接收到的用户存入在线用户列表中
		client_list.push_back(user);
		struct in_addr temp;
		temp.s_addr = user.ip;
		printf("[%d] user %s address %s:%d\n", i + 1, user.username,
				inet_ntoa(temp), ntohs(user.port));
	}

	printf("\ncommands are:\n");
	printf("send [username] [message]\n");
	printf("list\n");
	printf("exit\n");
	printf("\n");

	//IO复用，因为客户端可能收到服务器的消息，也可能收到其他客户端消息,避免阻塞在标准输入处以致无法接受到消息
	fd_set rset;
	FD_ZERO(&rset);
	int nready, maxfd;
	int fd_stdin = fileno(stdin); //避免被重定向
	if (fd_stdin > sock)
		maxfd = fd_stdin;
	else
		maxfd = sock;
	while (1)
	{
		FD_SET(fd_stdin, &rset);
		FD_SET(sock, &rset);
		nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (nready == -1)
			ERR_EXIT("select");
		if (nready == 0)
			continue;
		if (FD_ISSET(sock, &rset)) //处理套接口接收到消息
		{
			peerlen = sizeof(peeraddr);
			memset(&msg, 0, sizeof(msg));
			ret = recvfrom(sock, &msg, sizeof(msg), 0,
					(struct sockaddr*) &peeraddr, &peerlen);
			if (ret == -1)
			{
				if (errno == EINTR)
					continue;
				ERR_EXIT("S2C-select-recvfrom");
			}
			int cmd = ntohl(msg.cmd);
			switch (cmd)
			{
				case S2C_SOMEONE_LOGIN:
					do_someone_login(msg);
					break;
				case S2C_SOMEONE_LOGOUT:
					do_someone_logout(msg);
					break;
				case S2C_ONLINE_USER: //从服务器给客户端发来消息
					do_online_user(sock);
					break;
				case C2C_CHAT:
					do_chat(msg);
					break;
				default:
					break;
			}
		}
		if (FD_ISSET(fd_stdin, &rset)) //处理控制台命令
		{
			char cmdline[100] = { 0 };
			if (fgets(cmdline, sizeof(cmdline), stdin) == NULL)
				break;//ctrl+C之类
			if (cmdline[0] == '\n')//回车
				continue;
			cmdline[strlen(cmdline) - 1] = '\0';
			//解析该行命令
			parse_cmd(cmdline, sock, &seraddr);
		}
	}
}
int main(void)
{
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		ERR_EXIT("socket");
	chat_client(sock);
	return 0;
}
