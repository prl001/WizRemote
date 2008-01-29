//wizRemote Copyright 2008 Eric Fry efry@users.sourceforge.net
//Licensed under the GPLv2
//

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include "ezxml.h"

#define debug(x) printf(x)
#define MAX_LINE 512
#define BOOK_PATH "/tmp/config/book.xml"

const char book_xml_header[] = "<?xml version=\"1.0\" encoding=\"iso-8859-1\" standalone=\"yes\" ?>\n<BeyonWiz>\n    <!-- Timer list for BeyonWiz -->\n    <TimerList version=\"5\">\n";
const char book_xml_footer[] = "    </TimerList>\n</BeyonWiz>\n";

typedef struct {
	char *filename;
	unsigned int startmjd;
	unsigned int nextmjd;
	unsigned int start;
	unsigned int duration;
	unsigned char repeat;
	unsigned char play;
	unsigned char lock;
	unsigned int onid;
	unsigned int tsid;
	unsigned int svcid;
} Timer;

typedef struct {
	int n_timers;
	Timer *timers;
} Book;

int soc = -1;
int cli = -1;

void cmd_list(int socket);
void cmd_add(int socket);
void cmd_del(int socket);
void cmd_shutdown(int socket);
void cmd_quit(int socket);

typedef struct {
	void(*handler)(int);
	char cmd[9];
} CmdHandler;

const CmdHandler cmdHandlerTbl[] = 
	{
		{cmd_list,"list"},
		{cmd_add, "add"},
		{cmd_del, "delete"},
		{cmd_shutdown, "shutdown"},
		{cmd_quit, "quit"},		
		{NULL,""}
	};


void socket_cleanup()
{
	if(cli != -1)
		close(cli);

	if(soc != -1)
		close(soc);
	debug("Closing sockets.\n");
}

void sigterm_handler(int s)
{
	socket_cleanup();
	exit(0);
}

Book *book_load_file(const char *path)
{
	Book *book;
	ezxml_t beyonwiz_xml, timerlist_xml, timer_xml;
	Timer *timer;
	int i;
	const char *fname;

	book = (Book *)malloc(sizeof(Book));
 
	book->n_timers = 0;
	book->timers = NULL;

	beyonwiz_xml = ezxml_parse_file(path);
	timerlist_xml = ezxml_child(beyonwiz_xml, "TimerList");
 
	if(timerlist_xml)
	{
		for(timer_xml = ezxml_child(timerlist_xml, "Timer"); timer_xml; timer_xml = timer_xml->next)
		{
			book->n_timers++;
		}

		printf("number of timers = %d\n", book->n_timers);

		timer = (Timer *)malloc(sizeof(Timer) * book->n_timers);
		book->timers = timer;

		for(i=0,timer_xml = ezxml_child(timerlist_xml, "Timer"); timer_xml; timer_xml = timer_xml->next,i++)
		{
			fname = ezxml_attr(timer_xml, "fname");
			timer->filename = (char *)malloc(strlen(fname) + 1);
			strcpy(timer->filename, fname);
			
			timer->startmjd = atoi(ezxml_attr(timer_xml, "startMjd"));
			timer->nextmjd = atoi(ezxml_attr(timer_xml, "nextMjd"));
			timer->start = atoi(ezxml_attr(timer_xml, "start"));
			timer->duration = atoi(ezxml_attr(timer_xml, "duration"));
			timer->repeat = atoi(ezxml_attr(timer_xml, "repeat"));
			timer->play = atoi(ezxml_attr(timer_xml, "play"));
			timer->lock = atoi(ezxml_attr(timer_xml, "lock"));
			timer->onid = atoi(ezxml_attr(timer_xml, "onId"));
			timer->tsid = atoi(ezxml_attr(timer_xml, "tsId"));
			timer->svcid = atoi(ezxml_attr(timer_xml, "svcId"));

			printf("filename = %s\n", timer->filename);
			printf("startMdj = %d\n", timer->startmjd);
			timer++;
		}

	}
	ezxml_free(beyonwiz_xml);
	return book;
}

void book_close(Book *book)
{
	int i;
	Timer *timer;
	if(book->n_timers > 0)
	{
		//free filenames
		for(i=0,timer=book->timers;i<book->n_timers;i++,timer++)
		{
			free(timer->filename);
		}

		//free the rest of the timer data
		free(book->timers);
	}

	free(book);
}

Timer *read_timer(int socket)
{
	Timer *timer;
	char line[MAX_LINE];
	int i,len;
	char *ptr;

	if(read_line(socket, line) == 0)
		return NULL;

	timer = (Timer *)malloc(sizeof(Timer));
	if(timer == NULL)
		return NULL;

	len = strlen(line);
	ptr = line;
	for(i=0;i<len;i++)
	{
		if(line[i] == ':')
			line[i] = '\0';
	}

	timer->filename = (char *)malloc(strlen(ptr) + 1);
	if(timer->filename == NULL)
	{
		free(timer);
		return NULL;
	}

	strcpy(timer->filename, ptr);

	ptr += strlen(ptr)+1;
	timer->startmjd = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->nextmjd = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->start = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->duration = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->repeat = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->play = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->lock = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->onid = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->tsid = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->svcid = ptr[0] == '\0' ? 0 : atoi(ptr);

	return timer;
}

void reboot_wiz()
{
	socket_cleanup();
	execl("/bin/sh", "sh", "reboot.sh", NULL);
	exit(0);
}

void cmd_list(int socket)
{
	char line[MAX_LINE];
	Book *book = book_load_file(BOOK_PATH);
	Timer *timer;
	int i;

	sprintf(line, "%d\n", book->n_timers);
	write(socket, line, strlen(line));

	for(i=0,timer=book->timers;i<book->n_timers;i++,timer++)
	{
		sprintf(line,"%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			timer->filename, timer->startmjd, timer->nextmjd, timer->start, timer->duration, 
			timer->repeat, timer->play, timer->lock, timer->onid, timer->tsid, timer->svcid);
		write(socket,line,strlen(line));
	}

	return;
}

void cmd_add(int socket)
{
	char line[MAX_LINE];
	Book *book = book_load_file(BOOK_PATH);
	Timer *timer, *new_timer;
	int i;
	FILE *fp;

	new_timer = read_timer(socket);
	if(new_timer == NULL)
	{
		book_close(book);
		return;
	}

	fp = fopen(BOOK_PATH, "w");

	fwrite(book_xml_header, 1, sizeof(book_xml_header)-1,fp);
	printf("%s", book_xml_header);

	for(i=0,timer=book->timers;i<book->n_timers;i++,timer++)
	{
		sprintf(line,"        <Timer fname=\"%s\" startMjd=\"%d\" nextMjd=\"%d\" start=\"%d\" duration=\"%d\" repeat=\"%d\" play=\"%d\" lock=\"%d\" onId=\"%d\" tsId=\"%d\" svcId=\"%d\" />\n",
			timer->filename, timer->startmjd, timer->nextmjd, timer->start, timer->duration, 
			timer->repeat, timer->play, timer->lock, timer->onid, timer->tsid, timer->svcid);
		fwrite(line, 1, strlen(line),fp);

		printf("%s",line);
	}

	timer = new_timer;

	sprintf(line,"        <Timer fname=\"%s\" startMjd=\"%d\" nextMjd=\"%d\" start=\"%d\" duration=\"%d\" repeat=\"%d\" play=\"%d\" lock=\"%d\" onId=\"%d\" tsId=\"%d\" svcId=\"%d\" />\n",
		timer->filename, timer->startmjd, timer->nextmjd, timer->start, timer->duration, 
		timer->repeat, timer->play, timer->lock, timer->onid, timer->tsid, timer->svcid);
	fwrite(line, 1, strlen(line),fp);

	printf("%s",line);
	free(new_timer->filename);
	free(new_timer);

	fwrite(book_xml_footer, 1, sizeof(book_xml_footer)-1, fp);
	printf("%s", book_xml_footer); 
	fclose(fp);

	reboot_wiz();

	return;
}

void cmd_del(int socket)
{
	return;
}

void cmd_shutdown(int socket)
{
	return;
}

void cmd_quit(int socket)
{
	socket_cleanup();
	exit(0);
}

int read_line(int socket, char *data)
{
	int i;

	for(i=0;i<MAX_LINE;i++)
	{
		read(socket, &data[i], 1);
		if(data[i] == '\n')
		{
			data[i] = '\0';
			if(i >= 1 && data[i-1] == '\r')
				data[i-1] = '\0';
			return 1;
		}
	}

	return 0;
}

void process_command(int socket)
{
	char data[MAX_LINE];
	const CmdHandler *cmd;

	read_line(socket, data);

	for(cmd = cmdHandlerTbl;cmd->handler != NULL;cmd++)
	{
		if(strcmp(cmd->cmd,data)==0)
		{
			printf("Command '%s' received.\n", cmd->cmd);
			cmd->handler(socket);
			return;
		}
	}

	printf("Unknown command '%s' received!\n", data);
	return;
}

main()
{
	Book *book;
	int soc_len;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( 30464 );
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);

	soc = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

	signal(SIGTERM,sigterm_handler);

	bind( soc, (struct sockaddr *)&serv_addr, sizeof(serv_addr) );
	listen(soc, 1);

	soc_len = sizeof(cli_addr);
	for(;;)
	{
		cli = accept(soc, (struct sockaddr *)&cli_addr, &soc_len );
		process_command(cli);

		write(cli,"ok\n", 3);
		close(cli);
		cli = -1;
	}
}
