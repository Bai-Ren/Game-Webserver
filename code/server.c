/**
 * Networking
 * CS 241 - Spring 2017
 */
#include "common.h"
#include "format.h"
#include "dictionary.h"
#include "vector.h"
#include <stdbool.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>

#define MAX_EVENTS 16

int running;
dictionary *m;
vector *v;
int epfd;

typedef struct {
	char	buffer [1024]; 
	char	filename [1024];
	verb	current_request;
	size_t	size;
	size_t	completed;
	int		file_fd;
} status;

void *string_copy_constructor(void *elem) {
	char *str = elem;
	return strdup(str);
}

void string_destructor(void *elem) { free(elem); }

void *string_default_constructor(void) {
	// A single null byte
	return calloc(1, sizeof(char));
}

void *status_default_constructor() {
	status *ret = malloc(sizeof(status));
	ret->current_request = NONE;
	return ret;
}

void *status_copy_constructor (void *elem) {
	status *it = (status *) elem;
	status *ret = malloc (sizeof(status));
	if (elem != NULL) {
		for (size_t i=0; i<1024; i++)
			ret->buffer[i] = it->buffer[i];
		ret->current_request = it->current_request;
		ret->size = it->size;
		ret->completed = it->completed;
	}
	return ret;
}

void status_destructor(void *elem) {free(elem);}

void setup_dictionary() {
	m = dictionary_create(NULL, int_compare, int_copy_constructor, int_destructor, status_copy_constructor, status_destructor);
	v = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
}

void close_connection(int fd) {
	printf("connection closed on fd %i\n", fd);

	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	dictionary_remove(m, &fd);

	shutdown(fd, SHUT_RDWR);
}

void close_server() {running = 0;}

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

int setup_server(char *port) {
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

	running = 1;

	freeaddrinfo(info);

	return sock_fd;
}

int setup_epoll(int sock_fd) {
	//epoll create
	epfd = epoll_create(1);

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
	event.data.fd = new_fd;

	epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &event);

	//Add client to dictionary
	status stat;
	stat.current_request = NONE;
	dictionary_set(m, &new_fd, &stat);

	printf ("processed new connection with fd %i\n", new_fd);
}

void send_error(int fd, const char *error) {
	printf("error on fd %i\n%s\n", fd, error);
	write_all_to_socket(fd, "ERROR\n", 6);
	write_all_to_socket(fd, error, strlen(error));
}

void send_ok(int fd) {
	printf("ok on fd %i\n", fd);
	write_all_to_socket(fd, "OK\n", 3);
}

void remove_file(char *filename) {
	remove(filename);

	size_t size = vector_size(v);

	for(size_t i=0; size; i++) {
		if (!strcmp(vector_get(v,i), filename)) {
			vector_erase(v,i);
			break;
		}
	}
}

void process_header(int fd, status *stat) {
	char *buff = stat->buffer;
	ssize_t read, tot = 0;

	read = read_all_from_socket(fd, buff, 4);
	tot += read;
	if (read < 4) {
		send_error(fd, err_bad_request);
		close_connection(fd);
		return;
	}

	buff[4] = '\0';

// CASE PUT

	if (!strcmp(buff, "PUT ")) {
		read = read_all_until_newline(fd, stat->filename, 1019-sizeof(size_t));
		if (read < 0) {
			send_error(fd, err_bad_request);
			close_connection(fd);
			return;
		}
		stat->filename[read-1] = '\0';		

		stat->current_request = PUT;
		read = read_all_from_socket(fd, (char *) &stat->size, sizeof(size_t));
		if (read < (ssize_t) sizeof(size_t)) {
			send_error(fd, err_bad_request);
			close_connection(fd);
			return;
		}
		stat->completed = 0;

		stat->file_fd = open(stat->filename, O_WRONLY | O_CREAT, 0666);

		if (stat->file_fd == -1) {
			perror("open");
			close_connection(fd);
			return;
		}

		int in = 0;

		VEC_FOR_EACH(v, if (!strcmp(elem, stat->filename)) {in = 1;} )

		if (!in)
			vector_push_back(v, stat->filename);

		printf("processed PUT with filename %s and size %lu from fd %i\n", stat->filename, stat->size, fd);

		return;
	} 

// CASE GET

	else if (!strcmp(buff, "GET ")) {
		read = read_all_until_newline(fd, stat->filename, 1019-sizeof(size_t));
		if (read < 0) {
			send_error(fd, err_bad_request);
			close_connection(fd);
			return;
		}

		stat->filename[read-1] = '\0';

		int in = 0;

		VEC_FOR_EACH(v, if (!strcmp(elem, stat->filename)) {in = 1;} )

		if (!in) {
			send_error(fd, err_no_such_file);
			close_connection(fd);
			return;			
		}

		stat->current_request = GET;

		stat->completed = 0;

		stat->file_fd = open(stat->filename, O_RDONLY);

		if (stat->file_fd == -1) {
			perror("open");
			send_error(fd, err_no_such_file);
			close_connection(fd);
			return;
		}

		struct stat sbuf;
		memset(&sbuf, 0, sizeof(sbuf));
		fstat(stat->file_fd, &sbuf);

		stat->size = (size_t) sbuf.st_size;		

		send_ok(fd);
		ssize_t wts = write_all_to_socket(fd, (char *) &stat->size, sizeof(size_t));
		if (wts < (ssize_t) sizeof(size_t)) {
			printf("Error writing size of file to client %i\n", fd);
			close_connection(fd);
			return;
		}

		printf("processed GET with filename %s from fd %i\n", stat->filename, fd);		

		return;
	}

	read = read_all_from_socket(fd, &buff[4], 1);
	tot += read;
	if (read < 1) {
		send_error(fd, err_bad_request);
		close_connection(fd);
		return;
	}	
	buff[5] = '\0';

// CASE LIST

	if (!strcmp(buff, "LIST\n")) {
		send_ok(fd);

		stat->current_request = LIST;
		stat->completed = 0;
		stat->size = vector_size(v);

		size_t size = 0;

		if (!vector_empty(v)) {
			VEC_FOR_EACH(v, size += strlen(elem);)
			size += vector_size(v) - 1;
		}

		ssize_t wts = write_all_to_socket(fd, (char *) &size, sizeof(size_t));
		if (wts < (ssize_t) sizeof(size_t)) {
			printf("Error writing size of file to client %i\n", fd);
			close_connection(fd);
			return;
		}		

		printf("processed LIST from fd %i\n", fd);

		if (size == 0)
			close_connection(fd);
 
		return;
	}

	read = read_all_from_socket(fd, &buff[5], 2);
	tot += read;
	if (read < 2) {
		send_error(fd, err_bad_request);
		close_connection(fd);
		return;
	}
	buff[7] = '\0';

// CASE DELETE

	if (!strcmp(buff, "DELETE ")) {
		read = read_all_until_newline(fd, stat->filename, 1019-sizeof(size_t));
		if (read < 0) {
			send_error(fd, err_bad_request);
			close_connection(fd);
			return;
		}

		stat->filename[read-1] = '\0';

		int in = 0;

		VEC_FOR_EACH(v, if (!strcmp(elem, stat->filename)) {in = 1;} )

		if (!in) {
			send_error(fd, err_no_such_file);
			close_connection(fd);
			return;			
		}

		remove_file(stat->filename);
		send_ok(fd);
		close_connection(fd);

		printf("processed DELETE with filename %s from fd %i\n", stat->filename, fd);

		return;
	}

// CASE INVALID

	else {
		send_error(fd, err_bad_request);
		close_connection(fd);
		return;
	}
}

void process_PUT(int fd, status *stat) {
	char *buff = stat->buffer;

	ssize_t it = read(fd, buff, 1024);
	
	if (it == 0) {
		printf("client %i closed connection\n", fd);
		close_connection(fd);
		remove_file(stat->filename);
		return;
	}

	if (it == -1) {
		if (errno == EINTR)
				return;
		else {
			perror("read");
			return;
		}	
	}

	write(stat->file_fd, buff, it);

	stat->completed += it;

	if (stat->completed > stat->size) {
		send_error(fd, err_bad_file_size);
		close_connection(fd);
		remove_file(stat->filename);
		return;
	}

	if (stat->completed == stat->size) {
		while (1) {
			it = read(fd, buff, 1);
			if (it != -1)
				break;
			if (it == -1 && errno != EINTR)
				break;
		}
		if (it == 1) {
			send_error(fd, err_bad_file_size);
			close_connection(fd);
			remove_file(stat->filename);
			return;
		}

		send_ok(fd);
		close_connection(fd);
		return;
	}
}

void process_GET(int fd, status *stat) {
	char *buff = stat->buffer;

	ssize_t it = read(stat->file_fd, buff, 1024);

	if (it == 0)
		return;

	if (it == -1) {
		if (errno == EINTR)
				return;
		else {
			perror("read");
			return;
		}	
	}

	ssize_t it2 = write_all_to_socket(fd, buff, it);

	if (it2 == -1) {
		close_connection(fd);
		return;
	}

	stat->completed += it;

	if (stat->completed >= stat->size) {
		close_connection(fd);
		return;
	}
}

void process_DELETE(int fd, status *stat) {
	printf("process_DELETE was called which is an ERROR\n");
}

void process_LIST(int fd, status *stat) {
	char *str = vector_get(v, stat->completed++);
	write_all_to_socket(fd, str, strlen(str));

	if (stat->completed == stat->size) {
		close_connection(fd);
	} else {
		write_all_to_socket(fd, "\n", 1);
	}
}

void process_event(struct epoll_event ev) {
	int fd = ev.data.fd;
	status *stat = dictionary_get(m, &fd);

	switch(stat->current_request) {
		case NONE:
			process_header(fd, stat);
			break;
		case GET:
			process_GET(fd, stat);
			break;
		case PUT:
			process_PUT(fd, stat);
			break;
		case DELETE:
			process_DELETE(fd, stat);
			break;
		case LIST:
			process_LIST(fd, stat);
			break;
		default:
			print_error_message("invalid verb passed into execute_verb\n");
			exit(1);
	}

}

int main(int argc, char **argv) {
	char server_dir[14];
	strcpy(server_dir, "storageXXXXXX");

	char *sd = mkdtemp(server_dir);

	if (sd == NULL) {
		printf("Error making tempdir\n");
		exit(1);
	}

	print_temp_directory(server_dir);

	chdir(server_dir);

	setup_sig();
	setup_dictionary();

	int sock_fd = setup_server(argv[1]);

	int epfd = setup_epoll(sock_fd);

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

	VEC_FOR_EACH(v, remove(elem);)

	chdir("..");
	rmdir(server_dir);
	dictionary_destroy(m);
	vector_destroy(v);
}
