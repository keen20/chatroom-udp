/*
 * chatserver.cpp
 * 服务端
 */
#include <stdlib.h>
#include<stdio.h>
#include <errno.h>
#include <string.h>
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
USER_LIST client_list; //维护一个全局变量表示在线用户列表
void do_login(int sock, const MESSAGE &msg, struct sockaddr_in* clientaddr)
{
	//如果是登陆，那么message里面的cmd为C2S_LOGIN，body为用户名
	socklen_t clientlen = sizeof(*clientaddr);
	USER_INFO user_info;//刚登陆的用户
	MESSAGE reply_msg;
	ssize_t ret;
	//查找用户
	USER_LIST::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++)
	{
		if (strcmp(it->username, msg.body) == 0)
			break;
	}

	//没找到该用户
	if (it == client_list.end())
	{
		memset(&user_info, 0, sizeof(user_info));
		strcpy(user_info.username, msg.body);
		user_info.ip = clientaddr->sin_addr.s_addr;//按照网络字节序存储
		user_info.port = clientaddr->sin_port;
		printf("user %s logined ! address %s:%d\n", user_info.username,
				inet_ntoa(clientaddr->sin_addr), htons(user_info.port));
		client_list.push_back(user_info);

		//给客户端登陆成功应答
		memset(&reply_msg, 0, sizeof(reply_msg));
		reply_msg.cmd = htonl(S2C_LOGIN_OK);
		ret = sendto(sock, &reply_msg, sizeof(reply_msg), 0,
				(struct sockaddr *) clientaddr, clientlen);
		//ret = sendto(sock, "aa", 4, 0,(struct sockaddr *) clientaddr, clientlen);
		if (-1 == ret)
		{
			ERR_EXIT("sendto-S2C_LOGIN_OK");
		}
		//发送在线人数
		int count = htonl((int) client_list.size());
		ret = sendto(sock, &count, sizeof(int), 0,
				(struct sockaddr*) clientaddr, clientlen);
		if (ret == -1)
			ERR_EXIT("S2C-sendto-usercount");
		if((int) client_list.size() == 1)
		{
			printf("only user [%s] address %s:%d online now !\n",
					user_info.username, inet_ntoa(clientaddr->sin_addr),
					htons(user_info.port));
		}
		else
		{
			printf("sending user list to user [%s] address %s:%d\n",
				user_info.username, inet_ntoa(clientaddr->sin_addr),
				htons(user_info.port));
		}
		//发送在线用户列表
		for (it = client_list.begin(); it != client_list.end(); it++)
		{
			ret = sendto(sock, &*it, sizeof(USER_INFO), 0,
					(struct sockaddr*) clientaddr, clientlen);
			if (ret == -1)
			{
				if (errno == EINTR)
					continue;
				ERR_EXIT("init-S2C-sendto-userlist");
			}
		}

		//向其他用户发送有新用户登陆通知
		for (it = client_list.begin(); it != client_list.end(); it++)
		{
			//排除该用户
			if (strcmp(it->username, msg.body) == 0)
				continue;
			struct sockaddr_in peeraddr;
			socklen_t peerlen = sizeof(peeraddr);
			memset(&peeraddr, 0, peerlen);
			peeraddr.sin_family = PF_INET;
			peeraddr.sin_port = it->port;
			peeraddr.sin_addr.s_addr = it->ip;
			memset(&reply_msg, 0, sizeof(reply_msg));
			reply_msg.cmd = htonl(S2C_SOMEONE_LOGIN);
			memcpy(reply_msg.body, &user_info, sizeof(user_info));
			ret = sendto(sock, &reply_msg, sizeof(reply_msg), 0,
					(struct sockaddr*) &peeraddr, peerlen);
			if (ret == -1)
			{
				if (errno == EINTR)
					continue;
				ERR_EXIT("sendto-S2C_SOMEONE_LOGIN");
			}
		}

	} else //该用户已经存在
	{
		printf("user %s is already logined!\n", msg.body);
		memset(&reply_msg, 0, sizeof(reply_msg));
		reply_msg.cmd = htonl(S2C_ALREADY_LOGINED);
		int ret = sendto(sock, &reply_msg, sizeof(reply_msg), 0,
				(struct sockaddr*) clientaddr, clientlen);
		if (ret == -1)
			ERR_EXIT("sendto-S2C_ALREADY_LOGINED");
	}
}

void do_logout(int sock, const MESSAGE& msg, struct sockaddr_in *clientaddr)
{
	MESSAGE reply_msg;
	memset(&reply_msg, 0, sizeof(reply_msg));
	reply_msg.cmd = htonl(S2C_SOMEONE_LOGOUT);
	int ret = sendto(sock, &reply_msg, sizeof(reply_msg), 0,
			(struct sockaddr*) clientaddr, sizeof(*clientaddr));
	if (ret == -1)
		ERR_EXIT("sendto-S2C_SOMEONE_LOGOUT");
	USER_LIST::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++)
	{
		if (strcmp(it->username, msg.body) == 0)
			break;
	}
	if (it != client_list.end())
		client_list.erase(it);
	printf("user %s has logout from server!\n", it->username);
}
void do_sendlist(int sock, const MESSAGE& msg, struct sockaddr_in *clientaddr)
{
	MESSAGE reply_msg;
	memset(&reply_msg, 0, sizeof(reply_msg));
	reply_msg.cmd = htonl(S2C_ONLINE_USER);

	int ret = sendto(sock, &reply_msg, sizeof(reply_msg), 0,
			(struct sockaddr*) clientaddr, sizeof(*clientaddr));
	if (ret == -1)
		ERR_EXIT("sendto-S2C_ONLINE_USER1");
	USER_LIST::iterator it;

	//发送在线人数
	int count = htonl((int) client_list.size());
	ret = sendto(sock, &count, sizeof(int), 0, (struct sockaddr*) clientaddr,
			sizeof(*clientaddr));
	if (ret == -1)
		ERR_EXIT("sendto-S2C_ONLINE_USER2");
	printf("sending user list to user %s address %s:%d\n", msg.body,
			inet_ntoa(clientaddr->sin_addr), htons(clientaddr->sin_port));

	//发送在线用户列表
	for (it = client_list.begin(); it != client_list.end(); it++)
	{
		ret = sendto(sock, &*it, sizeof(USER_INFO), 0,
				(struct sockaddr*) clientaddr, sizeof(*clientaddr));
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			ERR_EXIT("S2C-sendto-userlist");
		}
	}
}

void chat_ser(int sock)
{
	struct sockaddr_in clientaddr;
	socklen_t clientlen;
	ssize_t ret;
	MESSAGE msg;
	while (1)
	{
		memset(&msg, 0, sizeof(msg));
		clientlen = sizeof(clientaddr);
		//消息是以结构体的形式接收
		ret = recvfrom(sock, &msg, sizeof(msg), 0,
				(struct sockaddr *) &clientaddr, &clientlen);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			ERR_EXIT("recvfrom");
		}
		int cmd = ntohl(msg.cmd); //发送过来的是网络字节序
		switch (cmd)
		{
			case C2S_LOGIN:
				do_login(sock, msg, &clientaddr);
				break;
			case C2S_LOGOUT:
				do_logout(sock, msg, &clientaddr);
				break;
			case C2S_ONLINE_USE:
				do_sendlist(sock, msg, &clientaddr);
				break;
		}
	}
}
int main(void)
{
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		ERR_EXIT("socket");
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(10000);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0)
		ERR_EXIT("bind");
	chat_ser(sock);
	return 0;
}

