#include "processHeader.h"

int process_request_line(status *stat) {
	char buff [1024];
	ssize_t res = read_all_until_newline(stat->cfd, buff, 1024);

	if (res < 1) {
		fprintf(stderr, "Invalid request\n");
		return 1;
	}

	buff[res-1] = '\0';

	char *token = strtok(buff, " ");

	if (token == NULL) {
		fprintf(stderr, "Invalid request\n");
		return 1;
	}

	if (!strcmp(token, "GET")) {
		stat->m = GET;
	} else if (!strcmp(token, "POST")) {
		stat->m = POST;
	} else {
		fprintf(stderr, "Invalid method\n");	
		return 1;
	}

	token = strtok(NULL, " ");

	if (token == NULL) {
		fprintf(stderr, "Invalid request\n");
		return 1;
	}

	stat->filename = malloc (strlen(token)+1);
	strcpy(stat->filename, token);

	token = strtok(NULL, " ");

	if (token == NULL) {
		fprintf(stderr, "Invalid request\n");
		return 1;
	}

	return 0;
}

headerFields *process_header_fields(status *stat) {
	char buff [1024];
	while (1) {
		ssize_t res = read_all_until_newline(stat->cfd, buff, 1024);

		if (res <= 1) 
			break;

		buff[res-1] = '\0';	

		fprintf(stderr, "%s\n", buff);
	}

	return NULL;
}

void process_header(status *stat) {

	process_request_line(stat);
	stat->fields = process_header_fields(stat);

	return;
}