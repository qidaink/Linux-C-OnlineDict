/** =====================================================
 * Copyright © hk. 2022-2022. All rights reserved.
 * File name: cilentDict.c
 * Author   : fanhua
 * Description: 在线词典client客户端程序——TCP
 * ======================================================
 */
/* 头文件 */
#include <stdio.h>     /* perror scanf printf */
#include <stdlib.h>    /* exit   atoi  system */
#include <unistd.h>    /* sleep */

#include <sys/types.h> /* socket           connect send */
#include <sys/socket.h>/* socket inet_addr connect send */
#include <netinet/in.h>/* inet_addr */
#include <arpa/inet.h> /* inet_addr inet_pton htonl*/

#include <string.h>    /* bzero strncasecmp strlen */

#define R 1   /* user - register */
#define U 2   /* user - update   */
#define D 3   /* user - delect   */
#define S 4   /* user - show     */
#define L 5   /* user - login    */
#define Q 6   /* user - query    */
#define H 7   /* user - history  */

/* 定义通信双方的信息结构体 */
#define N 32
typedef struct
{
	int flag;/* 标记是否为root用户 */
	int type;
	char name[N];
	char data[256];
} MSG;

int clientMainMenu(void);     /* 客户端主菜单 */
/* 客户端连接服务器 */
int clientConnectMenu(void);  /* 客户端连接服务器菜单界面 */
int clientConnectServer(void);/* 连接服务器 */
int establishConnect(char *serverIP, int serverPort, char *clientIP, int clientPort);/* 与服务器建立TCP连接,由clientConnectServer调用 */
/* 账户管理 */
int clientRegister(int accept_fd, MSG *msg);     /* 客户端用户注册请求 */
/* 用户功能与数据请求 */
int clientFuncMenu(void);                   /* 客户端功能菜单 */
int clientLogin(int accept_fd, MSG * msg);  /* 客户端用户登录请求 */
int clientQuery(int accept_fd, MSG * msg);  /* 客户端用户单词查询请求 */
int clientHistory(int accept_fd, MSG * msg);/* 客户端历史记录查询请求 */
int clientAccountMenu(void);/* 账户管理界面 */
int clientAccountManage(int accept_fd, MSG * msg);/* 账户管理功能 */
int clientUpdate(int accept_fd, MSG *msg);        /* 修改用户密码请求 */
int clientDelete(int accept_fd, MSG * msg);       /* 用户账户删除 */
int clientShow(int accept_fd, MSG * msg);         /* 客户端显示所有用户信息 */

int main(int argc, char *argv[])
{
	clientAccountMenu();
	int num = 0; /* 接收用户选择序号的变量 */
	int socket_fd = -1;/* socket 套接字 */
	MSG msg;
	/* 1.连接服务器 */
	socket_fd = clientConnectServer();
	while(1)
	{
		clientMainMenu(); /* 打印主菜单 */
		printf("please choose:");
		if(scanf("%d%*[^\n]", &num) < 0 )
		{
			perror("scanf");
			exit(-1);
		}
		switch(num)
		{
		case 1:/* 账户注册功能 */
			clientRegister(socket_fd, &msg);
			break;
		case 2:/* 用户登录 */
			clientLogin(socket_fd, &msg);
			break;
		case 3:/* 退出进程 */
			send(socket_fd, "quit", sizeof("quit"),0);
			close(socket_fd);
			exit(0);
			break;
		default:
			printf("Invalid data cmd!\n");
			break;
		}
	}

	return 0;
}
/*=================================================*/
/**
 * @Function: clientMainMenu
 * @Description: 客户端主菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int clientMainMenu(void)
{
	printf("\n\033[1m------------------- client main menu ----------------------\033[0m\n");
	printf("| \033[1;33m1.register          2.login              3.quit\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}
/*=================================================*/
/**
 * @Function: clientConnectMenu
 * @Description: 客户端连接服务器菜单界面
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int clientConnectMenu(void)
{
	printf("\n\033[1m---------------- Connection Description -------------------\033[0m\n");
	printf("|\033[1;32m First, We should enter the IP and port of the server.\033[0m\n");
	printf("|\033[1;32m Second, We should enter the IP and port of the client.\033[0m\n");
	printf("|\n");
	printf("|\033[1;33m serverIP: IP address bound to the server.\033[0m\n");
	printf("|\033[1;33m serverPort: >5000.\033[0m\n");
	printf("|\033[1;33m clientIP: IP address of the local NIC(or 127.0.0.1).\033[0m\n");
	printf("|\033[1;33m clientPort: >5000 and != serverPort.\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}

/**
 * @Function: clientConnectServer
 * @Description: 客户端连接服务器
 * @param   : none
 * @return  : 返回一个整数
 *            ret,连接成功,并返回创建的socket套接字;
 *            -1,连接失败
 */
int clientConnectServer(void)
{
	char confirm;
	int ret = -1;
	char serverIP[16] = {0}; /* 服务器端IP地址 */
	char clientIP[16] = {0}; /* 客户端IP地址 */
	int serverPort = -1; /* 服务器端口 */
	int clientPort = -1; /* 客户端端口 */
	/* 1.打印提示信息 */
	clientConnectMenu();
	printf("Whether to use the default settings(Y or N):");
	ret = scanf("%c%*[^\n]", &confirm);/* 获取输入，并清除多余字符 */
	if(ret < 0)
	{
		perror("scanf");
		return -1;
	}
	if(confirm == 'Y' || confirm == 'y')
	{
		strncpy(serverIP, "192.168.10.101", 16);
		serverPort = 5001;
		strncpy(clientIP, "127.0.0.1", 16);
		clientPort = 5002;
	}
	else
	{
		/* 2.输入服务器端IP地址和端口——用于连接服务器 */
		printf("serverIP serverPort: ");
		ret = scanf("%s %d%*[^\n]", serverIP, &serverPort);
		if(ret < 0)
		{
			perror("scanf");
			return -1;
		}
		/* 3.输入客户端要绑定的IP和端口——用于设置自身的IP和端口号 */
		printf("clientIP clientPort: ");
		ret = scanf("%s %d%*[^\n]", clientIP, &clientPort);
		if(ret < 0)
		{
			perror("scanf");
			return -1;
		}
	}
	printf("server:\033[1;32;40m%s %d\033[0m\n", serverIP, serverPort);
	printf("client:\033[1;32;40m%s %d\033[0m\n", clientIP, clientPort);
	/* 3.建立TCP连接 */
	printf("\033[1;33mconnecting to the server ...!\033[0m\n");
	ret = establishConnect(serverIP, serverPort, clientIP, clientPort);
	if(ret > 0)
		printf("\033[1;32mconnecting to the server successfully!\033[0m\n");
	else
		printf("\033[1;31m[error]Failed to connect to server!\033[0m\n");
	return ret;
}

/**
 * @Function: establishConnect
 * @Description: 与服务器建立TCP连接
 * @param serverIP   : 服务器IP
 * @param serverPort : 服务器端口
 * @param clientIP   : 客户端IP
 * @param clientPort : 客户端端口
 * @return  : 返回一个整数
 *            socket_fd,连接成功,并返回创建的socket套接字;
 *            -1,连接失败
 */
int establishConnect(char *serverIP, int serverPort, char *clientIP, int clientPort)
{
	/* 1.创建套接字，得到套接字描述符 */
	int socket_fd = -1; /* 接收服务器端socket描述符 */
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror ("socket");
		exit(-1);
	}
	/* 2.网络属性设置 */
	/* 2.1允许绑定地址快速重用 */
	int b_reuse = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof (int));
	/* 3.客户端绑定固定的 IP 和 端口号 (是可选的)*/
	/* 3.1填充结构体 */
	struct sockaddr_in sinClient;
	bzero(&sinClient, sizeof (sinClient)); /* 将内存块（字符串）的前n个字节清零 */
	sinClient.sin_family = AF_INET;   /* 协议族 */
	sinClient.sin_port = htons(clientPort); /* 网络字节序的端口号 */
	if(inet_pton(AF_INET, clientIP, (void *)&sinClient.sin_addr) != 1)/* IP地址 */
	{
		perror ("inet_pton");
		return -1;
	}
	/* 3.2绑定 */
	if(bind(socket_fd, (struct sockaddr *)&sinClient, sizeof(sinClient)) < 0)
	{
		perror("bind");
		return -1;
	}
	/* 4.连接服务器 */
	/* 4.1填充struct sockaddr_in结构体变量 */
	struct sockaddr_in sin;
	bzero (&sin, sizeof(sin)); /* 将内存块（字符串）的前n个字节清零 */
	sin.sin_family = AF_INET;   /* 协议族 */
	sin.sin_port = htons(serverPort); /* 网络字节序的端口号 */
	/* 客户端的需要与系统网卡的IP一致 */
	if(inet_pton(AF_INET, serverIP, (void *)&sin.sin_addr) != 1)/* IP地址 */
	{
		perror("inet_pton");
		return -1;
	}
	/* 4.2连接 */
	if(connect(socket_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("connect");
		return -1;
	}
	printf("client(\033[1;32m%s:%d\033[0m) staring...OK!\n", clientIP, clientPort);
	return socket_fd;
}

/*=================================================*/
// 客户端账户管理功能相关请求

/**
 * @Function: clientRegister
 * @Description: 客户端用户注册请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,用户名已存在;
 *            1,注册成功;
 *            -1,注册失败
 */
int clientRegister(int accept_fd, MSG * msg)
{
	msg->type = R; /* 设置数据类型 R=1 */
	/* 1.输入用户名 */
	printf("Please enter user name:");
	if( scanf("%s%*[^\n]", msg->name) < 0)
	{
		perror("scanf");
		return -1;
	}
	/* 输入用户密码 */
	printf("Please enter user passwd:");
	if( scanf("%s%*[^\n]", msg->data) < 0)
	{
		perror("scanf");
		return -1;
	}
	/* 判断是否为root用户 */
	if(strncmp(msg->name, "root", 4) == 0)
		msg->flag = 1;
	else
		msg->flag = 0;
	/* 向服务器发送注册用户信息的数据包 */
	if( send(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器回复消息，无论如何服务器一定要回复，否则会在这里一直阻塞 */
	if(recv(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("recv");
		return -1;
	}
	/* 打印服务器回复消息 */
	if(!strncmp(msg->data, "OK", 2))
		printf("\033[1;35m[server reply]:\033[0m\033[1;32m%s\033[0m\n", msg->data);
	else
		printf("\033[1;35m[server reply]:\033[0m\033[1;31m%s\033[0m\n", msg->data);
	return 0;
}

/*=================================================*/
//用户登录及各项功能
/**
 * @Function: clientQueryMenu
 * @Description: 客户端功能菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 */
int clientFuncMenu(void)
{
	printf("\n\033[1m------------------ client function menu -------------------\033[0m\n");
	printf("| \033[1;33m1.query          2.history\033[0m\n");
	printf("| \033[1;33m3.account        4.quit\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}

/**
 * @Function: clientLogin
 * @Description: 客户端用户登录请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 指向一个客户端发给服务器请求的数据所存放结构体变量
 * @return  : 返回一个整数
 *            0,登录失败;
 *            1,登陆成功;
 *            -1,函数出现错误
 */
int clientLogin(int accept_fd, MSG * msg)
{
	int num = 0;
	msg->type = L; /* 设置数据类型 */
	/* 输入用户名 */
	printf("Please enter user name:");
	if( scanf("%s%*[^\n]", msg->name) < 0)
	{
		perror("scanf");
		return -1;
	}
	/* 输入用户密码 */
	printf("Please enter passwd:");
	if( scanf("%s%*[^\n]", msg->data) < 0)
	{
		perror("scanf");
		return -1;
	}
	/* 判断是否为root用户 */
	if(strncmp(msg->name, "root", 4) == 0)
		msg->flag = 1;
	else
		msg->flag = 0;
	/* 向服务器发送登录用户信息的数据包 */
	if( send(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器回复消息 */
	if(recv(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("recv");
		return -1;
	}
	/* 判断服务器返回消息，确定是否登陆成功 */
	if(strncmp(msg->data, "OK", 2) == 0)
	{
		printf("\033[1;32m[ OK  ]Login successfully!\033[0m\n");
		printf("\033[1;35m[server reply]:\033[0m\033[1;32m%s\033[0m\n", msg->data);
	}
	else 
	{
		printf("\033[1;31m[error]Login failed!\033[0m\n");
		printf("\033[1;35m[server reply]:\033[0m\033[1;31m%s\033[0m\n", msg->data);
		return -1;
	}
	while(1)
	{
		clientFuncMenu();
		printf("please choose:");
		if(scanf("%d%*[^\n]", &num) < 0 )
		{
			perror("scanf");
			exit(-1);
		}
		switch(num)
		{
		case 1:
			clientQuery(accept_fd, msg);
			break;
		case 2:
			clientHistory(accept_fd, msg);
			break;
		case 3:
			clientAccountManage(accept_fd, msg);
			break;
		case 4:/* 退出本级菜单 */
			send(accept_fd, "quit", sizeof("quit"),0);
			return 0;
			break;
		default:
			printf("Invalid data cmd!\n");
			break;
		}
	}
	return 0;
}
/**
 * @Function: clientQuery
 * @Description: 客户端用户单词查询请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 指向一个客户端发给服务器请求的数据所存放结构体变量
 * @return  : 返回一个整数
 *            0,登录失败;
 *            -1,函数出错;
 */
int clientQuery(int accept_fd, MSG * msg)
{
	msg->type = Q; /* 设置数据类型 */
	while(1)
	{
		printf("Please enter a word:");
		if( scanf("%s%*[^\n]", msg->data) < 0)
		{
			perror("scanf");
			return -1;
		}

		/* 输入#号，结束单词查询 */
		if(strncmp(msg->data, "#", 1) == 0)
			break;

		/* 将要查询的单词发送给服务器 */
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror("send");
			return -1;
		}

		/* 等待接受服务器，传递回来的单词的注释信息 */
		if( recv(accept_fd, msg,sizeof(MSG), 0) < 0)
		{
			perror("recv");
			return -1;
		}
		printf("\n\033[1m------------------- query result ----------------------\033[0m\n");
		printf("\033[1;32m%s\033[0m\n", msg->data);
		printf("\033[1m-----------------------------------------------------------\033[0m\n");
	
	}
		
	return 0;
}

/**
 * @Function: clientHistory
 * @Description: 客户端历史记录查询请求
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 指向一个客户端发给服务器请求的数据所存放结构体变量
 * @return  : 返回一个整数
 *            0,登录失败;
 *            -1,函数出错;
 */
int clientHistory(int accept_fd, MSG * msg)
{
	msg->type = H; /* 设置数据类型 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器传来的历史记录信息 */
	printf("\n\033[1m------------------------ history  -------------------------\033[0m\n");
	while(1)
	{
		if( recv(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror("recv");
			return -1;
		}

		if(msg->data[0] == '\0')
			break;
		
		printf("\033[1;32m%s\033[0m\n", msg->data);	
	}
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
		
	return 0;
}

/**
 * @Function: clientAccountMenu
 * @Description: 客户端用户登录后账户管理功能
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,打印成功;
 */
int clientAccountMenu(void)
{
	printf("\n\033[1m------------------ client account menu --------------------\033[0m\n");
	printf("| \033[1;33m1.update          2.delete(root)\033[0m\n");
	printf("| \033[1;33m3.show(root)      4.quit\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}

/**
 * @Function: clientAccountManage
 * @Description: 客户端账户管理功能
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,成功;
 *            -1,失败;
 */
int clientAccountManage(int accept_fd, MSG * msg)
{
	int num = -1;
	while(1)
	{
		clientAccountMenu(); /* 打印账户管理菜单 */
		printf("please choose:");
		if(scanf("%d%*[^\n]", &num) < 0 )
		{
			perror("scanf");
			exit(-1);
		}
		switch(num)
		{
		case 1:/* 账户密码修改功能 */
			clientUpdate(accept_fd, msg);
			break;
		case 2:/* 账户删除功能 */
			clientDelete(accept_fd, msg);
			break;
		case 3:/* 账户显示功能 */
			clientShow(accept_fd, msg);
			break;
		case 4:/* 退出账户管理功能 */
			return 0;/* 提前退出账户管理功能 */
			break;
		default:
			printf("Invalid data cmd!\n");
			break;
		}
	}
}
/**
 * @Function: clientUpdate
 * @Description: 客户端用户密码修改(root用户可修改所有密码，其他用户只能修改自己的)
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,修改成功;
 *            -1,修改失败
 */
int clientUpdate(int accept_fd, MSG * msg)
{
	char temp_name[32];
	/* 登录后，用户名一直保存在msg->name中 */
	msg->type = U;                 /* 设置数据类型 U=2 */
	strcpy(temp_name, msg->name);  /* 保存之前的用户名(主要是防止root修改其他用户密码时，msg中name的切换) */
	/* 区分root用户和普通用户 */
	if(msg->flag == 1)/* root 用户	*/
	{
		printf("\033[1;33mThe login user is root and can change the passwords of other users!\033[0m\n");
		/* 1.输入用户名 */
		printf("Please enter user name:");
		if( scanf("%s%*[^\n]", msg->name) < 0)
		{
			perror("scanf");
			return -1;
		}
		/* 输入用户新的密码 */
		printf("Please enter user new passwd:");
		if( scanf("%s%*[^\n]", msg->data) < 0)
		{
			perror("scanf");
			return -1;
		}
	}
	else
	{
		printf("\033[1;33mThe login user is a common user and can only change its own password\033[0m\n");
		/* 输入用户新的密码 */
		printf("Please enter user new passwd:");
		if( scanf("%s%*[^\n]", msg->data) < 0)
		{
			perror("scanf");
			return -1;
		}
	}
	
	/* 向服务器发送注册用户信息的数据包 */
	if( send(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器回复消息，无论如何服务器一定要回复，否则会在这里一直阻塞 */
	if(recv(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("recv");
		return -1;
	}
	/* 恢复原来的用户名 */
	strcpy(msg->name, temp_name);
	/* 打印服务器回复消息 */
	if(!strncmp(msg->data, "OK", 2))
		printf("\033[1;35m[server reply]:\033[0m\033[1;32m%s\033[0m\n", msg->data);
	else
		printf("\033[1;35m[server reply]:\033[0m\033[1;31m%s\033[0m\n", msg->data);
	return 0;
}


/**
 * @Function: clientDelete
 * @Description: 客户端用户账户删除(root用户才可可删除其他用户)
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败
 */
int clientDelete(int accept_fd, MSG * msg)
{
	char temp_name[32];
	/* 登录后，用户名一直保存在msg->name中 */
	msg->type = D;                 /* 设置数据类型 D=3 */
	strcpy(temp_name, msg->name);  /* 保存之前的用户名(主要是防止root修改其他用户密码时，msg中name的切换) */
	/* 区分root用户和普通用户 */
	if(msg->flag == 1)/* root 用户	*/
	{
		printf("\033[1;33mThe login user is root and can delete other users!\033[0m\n");
		/* 1.输入用户名 */
		printf("Please enter user name:");
		if( scanf("%s%*[^\n]", msg->name) < 0)
		{
			perror("scanf");
			return -1;
		}
	}
	else
	{
		printf("\033[1;31m[error]The login user has no permission to delete other users.\033[0m\n");
		return -1;
	}
	
	/* 向服务器发送删除用户信息的数据包 */
	if( send(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器回复消息，无论如何服务器一定要回复，否则会在这里一直阻塞 */
	if(recv(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("recv");
		return -1;
	}
	/* 恢复原来的用户名 */
	strcpy(msg->name, temp_name);
	/* 打印服务器回复消息 */
	if(!strncmp(msg->data, "OK", 2))
		printf("\033[1;35m[server reply]:\033[0m\033[1;32m%s\033[0m\n", msg->data);
	else
		printf("\033[1;35m[server reply]:\033[0m\033[1;31m%s\033[0m\n", msg->data);
	return 0;
}

/**
 * @Function: clientShow
 * @Description: 客户端用户账户显示(root用户才可显示所有用户)
 * @param accept_fd: 客户端的socket套接字
 * @param msg      : 服务器与客户端通信的数据结构体指针变量
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败
 */
int clientShow(int accept_fd, MSG * msg)
{
	char temp_name[32];
	/* 登录后，用户名一直保存在msg->name中 */
	msg->type = S;                 /* 设置数据类型 D=3 */
	strcpy(temp_name, msg->name);  /* 保存之前的用户名(主要是防止root修改其他用户密码时，msg中name的切换) */
	/* 区分root用户和普通用户 */
	if(msg->flag == 1)/* root 用户	*/
	{
		printf("\033[1;33mThe login user is root and can show other users!\033[0m\n");
	}
	else
	{
		printf("\033[1;31m[error]The login user has no permission to show other users.\033[0m\n");
		return -1;
	}
	
	/* 向服务器发送删除用户信息的数据包 */
	if( send(accept_fd, msg, sizeof(MSG),0) < 0)
	{
		perror("send");
		return -1;
	}
	/* 接收服务器传来的用户信息 */
	printf("\n\033[1m----------------------- user info  ------------------------\033[0m\n");
	while(1)
	{
		if( recv(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror("recv");
			return -1;
		}

		if(msg->data[0] == '\0')
			break;
		
		printf("\033[1;32m%s\033[0m\n", msg->data);	
	}
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	
	return 0;
}