#include "io.h"
#include "setupServer.h"
#include <signal.h>

static int running = 1;
static int epfd;

typedef struct {
	int	cfd;
	int pfd[2];
} fds;

void close_connection(int fd) {
	printf("connection closed on fd %i\n", fd);

	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	shutdown(fd, SHUT_RDWR);
}

void close_server() {running = 0;}

void new_connection(int sock_fd) {
	int new_fd = accept(sock_fd, NULL, NULL);
	if (new_fd == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			perror("accept");
			return;
		}
	}
	//Add client to epoll
	struct epoll_event event;
	event.events = EPOLLIN | EPOLLOUT;
	fds *it = malloc(sizeof(fds));
	it->cfd = new_fd;
	it->pfd[0] = -1;
	it->pfd[1] = -1;
	event.data.ptr = it;

	epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &event);

	printf ("processed new connection with fd %i\n", new_fd);
}

void process_event(struct epoll_event ev) {

}

void setup_sig() {
	struct sigaction act;
	memset(&act, '\0', sizeof(act));
	act.sa_handler = close_server;
	
	int sig_res = sigaction(SIGINT, &act, NULL);

	if (sig_res == -1) {
		perror("sigaction");
		exit(1);
	}

	sig_res = sigaction(SIGPIPE, NULL, NULL);
}

int main(int argc, char **argv) {

	setup_sig();

	int sock_fd = setup_server(argv[1]);

	epfd = setup_epoll(sock_fd);

	struct epoll_event events[MAX_EVENTS];

	while (running) {

		int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
		for (int i=0; i<n; i++) {
			if (events[i].events & EPOLLERR) {
				fprintf(stderr, "Epoll error detected on client %i\n", events[i].data.fd);
				close_connection(events[i].data.fd);
			} else if (events[i].events & EPOLLRDHUP) {
				close_connection(events[i].data.fd);
			} else if (events[i].data.fd == sock_fd) {
				new_connection(sock_fd);
			} else {
				process_event(events[i]);
			}
		}
	}
}
