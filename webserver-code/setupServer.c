#include "setupServer.h"

//Setups server socket and returns fd
//exits on failure

int setup_server(char *port) {
	printf("Setting up server\n");
	//Make socket

	int sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (sock_fd == -1) {
		perror(NULL);
		exit(1);
	}

	//Set sockopt

	int sock_opt = 1;

	int setsockopt_result = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &sock_opt, sizeof(sock_opt));

	if (setsockopt_result == -1) {
		perror(NULL);
		exit(1);
	}	

	//Get address info

	struct addrinfo hints, *info;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int addrinfo_result = getaddrinfo("127.0.0.1", port, &hints, &info);

	if (addrinfo_result) {
		fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(addrinfo_result));
		exit(1);
	}

	//Bind

	int bind_result = bind(sock_fd, info->ai_addr, info->ai_addrlen);

	if (bind_result == -1) {
		perror(NULL);
		exit(1);
	}

	//Listen

	int listen_result = listen(sock_fd, MAX_EVENTS);

	if (listen_result == -1) {
		perror(NULL);
		exit(1);
	}

	freeaddrinfo(info);

	printf("Server setup on port %s\n", port);

	return sock_fd;
}

int setup_epoll(int sock_fd) {
	//epoll create
	int epfd = epoll_create(1);

	if (epfd == -1) {
		perror(NULL);
		exit(1);
	}

	//Add listening socket to epoll
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = sock_fd;

	epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &event);

	return epfd;
}