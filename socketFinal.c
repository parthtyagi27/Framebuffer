#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>


struct pixelData
{
	unsigned char x, y, r, g, b;
};
struct postData
{
	char name;
	unsigned char value;
};


unsigned char * trim (unsigned char * input)
{
	//TODO: Fix malloc over top size error!
	if (*(input) != ' ')
		return input;
	int i = 0;
	printf("Trim function: input = %s, length = %d\n", input, strlen(input));
	for (i; i < strlen(input); i++)
	{
		printf("Checking: %c\n", *(input + i));
		if (*(input + i) != ' ')
			break;
	}
	printf("i = %d\n", i);
	unsigned char *output = malloc(strlen(input) - i);
	printf("Starting string copy\n");
	for (int j = 0; j < strlen(input) - i; j++)
	{
		*(output + j) = *(input + i + j);
	}
	input = output;
	free(output);
	return input;
}

char processRequest(unsigned char buffer[], struct pixelData * pxl) 
{
	printf("Processing Request\n");
	unsigned char header[4];
	strncpy(header, buffer, 4);
	if (strcmp(header, "POST") != 0)
	{
		printf("ERROR: This server only processes POST requests\n");
		return -1;
	}
	//TODO: Finish splitting request at new line
	unsigned char *array[8];
	unsigned char * token;
	token = strtok(buffer, "\n");
	int index = 0;
	while (token != NULL) 
	{
		array[index] = (unsigned char *) malloc (strlen(token) * sizeof(unsigned char *));
		strncpy(array[index], token, strlen(token));
		index += 1;
		token = strtok(NULL, "\n");
	}
	token = NULL;
	unsigned char * request = array[index - 1];
	printf("%s\n", request);
	
	if (strncmp(request, "action=exit", 11) == 0)
	{
		return -27;
	}
	printf("Starting parse process\n");
	unsigned char *params[10];
	unsigned char *paramToken = strtok(request, "&");
	int paramCount = 0;
	while (paramToken != NULL)
	{
		params[paramCount] = (unsigned char *) malloc(strlen(paramToken) * sizeof(unsigned char *));
		strcpy(params[paramCount], paramToken);
		paramCount++;
		paramToken = strtok(NULL, "&");
	}

	struct postData parsedParameters[paramCount];
	int equalityIndex = 0;
	int tempValue = 0;
	for (int i = 0; i < paramCount; i++)
	{
		if (params[i] == NULL)
			break;
		//process each value
		int j = 0;
		printf("Parsing %s, with length = %d\n", params[i], strlen(params[i]));
		for (j = 0; j < strlen(params[i]); j++)	
		{
			if ((unsigned char)*(params[i] + j) == '=')
			{
				equalityIndex = j;
				break;
			}
		}
		tempValue = atoi(&(*(params[i] + equalityIndex + 1)));
		(parsedParameters+i)->name = (char) *(params[i]);
		(parsedParameters+i)->value = tempValue;
		equalityIndex = 0;
	}

	bool r = false;
	bool g = false;
	bool b = false;
	bool x = false;
	bool y = false;

	for (int i = 0; i < paramCount; i++) {
		if (parsedParameters[i].name == "r"[0]) {
			if (r) {
				return -2;
			}
			pxl->r = parsedParameters[i].value;
			r = true;
		}
		if (parsedParameters[i].name == "g"[0]) {
			if (g) {
				return -2;
			}
			pxl->g = parsedParameters[i].value;
			g = true;
		}
		if (parsedParameters[i].name == "b"[0]) {
			if (b) {
				return -2;
			}
			pxl->b = parsedParameters[i].value;
			b = true;
		}
		if (parsedParameters[i].name == "x"[0]) {
			if (x) {
				return -2;
			}
			pxl->x = parsedParameters[i].value;
			x = true;
		}
		if (parsedParameters[i].name == "y"[0]) {
			if (y) {
				return -2;
			}
			pxl->y = parsedParameters[i].value;
			y = true;
		}
	}

	for (int i = 0; i < paramCount; i++)
		free(params[i]);

	for (int i = 0; i < 8; i++)
		free(array[i]);

	if (x && y && r && g && b) {
		return 0;
	}

	return -1;
}

int main() 
{
	unsigned int requestCount = 0;
	int listenfd, connfd, n;
	struct sockaddr_in serverAdress;
	unsigned char  buff[4096 + 1];
	unsigned char recvLine[4096 + 1];

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		printf("ERROR: Failed to create socket\n");
		return -1;
	}

	bzero(&serverAdress, sizeof(serverAdress));
	serverAdress.sin_family = AF_INET;
	serverAdress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAdress.sin_port = htons(1800);

	if ((bind(listenfd, (struct sockaddr *) &serverAdress, sizeof(serverAdress))) < 0)
	{
		printf("ERROR: Failed to bind to the server\n");
		return -1;
	}

	if ((listen(listenfd, 10)) < 0)
	{
		printf("ERROR: Failed to listen for connections\n");
		return -1;
	}

	printf("Starting server\n");
	
	while (1)
	{
		struct sockaddr_in addr;
		socklen_t addr_len;

		printf("\nWaiting for connection\n");

		connfd = accept(listenfd, (struct sockaddr *) NULL, NULL);

		memset(recvLine, 0, 4096);

		if ((n = read(connfd, recvLine, 4096 - 1)) > 0)
			memcpy(buff, recvLine, sizeof(buff));
		if (n < 0) 
		{
			printf("ERROR: Read negative bytes\n");
			return -1;
		}
		struct pixelData query;
		char result = processRequest(recvLine, &query);
		requestCount++;
		if (result == -27) 
		{
			break;
		} else if (result == 0) 
		{
			printf("x:%d;y:%d;r:%d;g:%d;b:%d;\n",query.x,query.y,query.r,query.g,query.b);
		}
		printf("Processed Request: %d\n", requestCount);
		unsigned char response[] = "HTTP/1.1 200 OK \r\n The server recieved your request!\r\n";
		send(connfd, &response, sizeof(response), 0);
		close(connfd);
	}
	printf("Exiting server\n");
	close(listenfd);
}
