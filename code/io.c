#include "io.h"

//-1 failure
//-2 no new line

ssize_t read_all_until_newline(int socket, char *buffer, size_t count) {
	size_t ans = 0;
	int goodo = 0;

	while (ans < count) {
		ssize_t it = read(socket, &buffer[ans], 1);
		if (it == 0)
			break;

		if (it == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		ans+=it;
		
		if (buffer[ans-1] == '\n') {
			goodo = 1;
			break;
		}
	}

	if (goodo)
		return (ssize_t) ans;

	return -2;
}

ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
	ssize_t ans = 0;

	while (1) {
		ssize_t it = read(socket, &buffer[ans], count-ans);
		if (it == 0)
			break;

		if (it == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		ans+=it;
		if ((size_t)ans == count) 
			break;
	}

	return ans;
}

ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
	ssize_t ans = 0;

	while (1) {
		ssize_t it = write(socket, &buffer[ans], count-ans);
		if (it == 0)
			break;

		if (it == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		ans+=it;
		if ((size_t)ans == count) 
			break;
	}

	return ans;
}

void write_all_noerr(int socket, const char *buffer, size_t count) {
	ssize_t result = write_all_to_socket(socket, buffer, count);

	if (result == -1) {
		fprintf(stderr, "Error writing to socket\n");
		exit(1);
	}

	if ((size_t)result != count) {
		fprintf(stderr, "Connection closed\n");
		exit(1);
	}
}

void read_all_noerr(int socket, char *buffer, size_t count) {
	ssize_t result = read_all_from_socket(socket, buffer, count);

	if (result == -1) {
		fprintf(stderr, "Error reading from socket\n");
		exit(1);
	}

	if ((size_t)result != count) {
		fprintf(stderr, "Connection closed\n");
		exit(1);
	}
}
