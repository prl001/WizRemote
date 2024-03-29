//wizRemote Copyright 2008 Eric Fry efry@users.sourceforge.net
//Licensed under the GPLv2
//

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<poll.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/uio.h>
#include<sys/errno.h>
#include<sys/socket.h>
#ifdef HAS_SYS_MOUNT_H
#include<sys/param.h>
#include<sys/mount.h>
#else //ucLinux
#include<sys/statfs.h>
#endif

#include<netinet/in.h>
#include "ezxml.h"

#include "aes.c"
#include "md5.c"

extern int errno;

#define u8 unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define debug(x) printf(x)
#define TOKEN_SIZE 16 //128 bits
#define MAX_LINE 512
#define WIZREMOTE_PORT 30464
#define BOOK_PATH "/tmp/config/book.xml"
#define HDD_PATH "/tmp/mnt/idehdd"
#define SVC_PATH "/tmp/config/svc.dat"

const char book_xml_header[] = "<?xml version=\"1.0\" encoding=\"iso-8859-1\" standalone=\"yes\" ?>\n<BeyonWiz>\n    <!-- Timer list for BeyonWiz -->\n";
const char book_xml_timerlist_start[] = "    <TimerList version=\"5\">\n";
const char book_xml_timerlist_empty[] = "    <TimerList version=\"5\" />\n";
const char book_xml_timerlist_end[]   = "    </TimerList>\n";

const char book_xml_footer[] = "</BeyonWiz>\n";

const unsigned char token_xor_key[TOKEN_SIZE] = { 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0xae, 0x57, 0x05e, 0x29, 0xfd, 0x87, 0x10, 0xe6 };

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

typedef struct {
	//u32 rf;
	u16 onid;
	u16 tsid;
} RFInfo;

int soc = -1;
int cli = -1;

void cmd_statfs(int socket);
void cmd_chlist(int socket);
void cmd_list(int socket);
void cmd_add(int socket);
void cmd_del(int socket);
void cmd_shutdown(int socket);
void cmd_reboot(int socket);
void cmd_quit(int socket);

typedef struct {
	void(*handler)(int);
	char cmd[9];
} CmdHandler;

const CmdHandler cmdHandlerTbl[] = 
	{
		{cmd_statfs,"statfs"},
		{cmd_chlist,"chlist"},
		{cmd_list,"list"},
		{cmd_add, "add"},
		{cmd_del, "delete"},
		{cmd_shutdown, "shutdown"},
		{cmd_reboot, "reboot"},
		{cmd_quit, "quit"},
		{NULL,""}
	};

u16 read_u16(FILE *fp)
{
	u16 i;

#ifdef BIG_ENDIAN
	unsigned char c;
	fread(&c, 1, 1, fp);
	i = c;
	fread(&c, 1, 1, fp);
	i += (c << 8);
#else
	fread(&i, sizeof(u16), 1, fp);
#endif

	return i;
}

u32 read_u32(FILE *fp)
{
	u32 i;

#ifdef BIG_ENDIAN
	unsigned char c;
	fread(&c, 1, 1, fp);
	i = c;
	fread(&c, 1, 1, fp);
	i += (c << 8);
	fread(&c, 1, 1, fp);
	i += (c << 16);
	fread(&c, 1, 1, fp);
	i += (c << 24);
#else
	fread(&i, sizeof(u32), 1, fp);
#endif

	return i;
}


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
	
	//FIXME check to make sure we are reading a version 5 timerlist.
	
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

			printf("timer fname = %s, startMjd = %d, start = %d\n", timer->filename, timer->startmjd, timer->start);
			timer++;
		}

	}
	ezxml_free(beyonwiz_xml);
	return book;
}

void book_free(Book *book)
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

//Split a line into substrings by converting ':' chars into '\0'
//Return the number of arguments
int line_split_args(char *line)
{
	int i, len, arg_count = 0;

	len = strlen(line);
	for(i=0;i<=len;i++)
	{
		if(line[i] == '\0')
		{
			arg_count++;
		}

		if(line[i] == ':')
		{
			arg_count++;
			line[i] = '\0';
		}
	}

	return arg_count;
}

//Read a timer line from the socket and create a Timer record.
Timer *read_timer(int socket)
{
	Timer *timer;
	char line[MAX_LINE];
	int arg_count=0;
	char *ptr;

	if(read_line(socket, line) == 0)
		return NULL;

	timer = (Timer *)malloc(sizeof(Timer));
	if(timer == NULL)
		return NULL;

	timer->filename = (char *)malloc(strlen(line) + 1);
	if(timer->filename == NULL)
	{
		free(timer);
		return NULL;
	}
	
	strcpy(timer->filename, line);
	
	if(read_line(socket, line) == 0)
	{
		free(timer->filename);
		free(timer);
		return NULL;
	}
	
	arg_count = line_split_args(line);
	if(arg_count != 10)
		return NULL;


	//Load arguments into Timer struct
	ptr=line;
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

Timer *read_info_timer(int socket)
{
	Timer *timer;
	char line[MAX_LINE];
	int arg_count;
	char *ptr;

	if(read_line(socket, line) == 0)
		return NULL;

	arg_count = line_split_args(line);
	if(arg_count != 5)
		return NULL;

	timer = (Timer *)malloc(sizeof(Timer));
	if(timer == NULL)
		return NULL;

	ptr = line;
	timer->startmjd = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->start = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->onid = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->tsid = ptr[0] == '\0' ? 0 : atoi(ptr);

	ptr += strlen(ptr)+1;
	timer->svcid = ptr[0] == '\0' ? 0 : atoi(ptr);

	return timer;
}

//match two Timers on start date, time and channel
int match_timers(Timer *t1, Timer *t2)
{
	if(t1->startmjd == t2->startmjd && t1->start == t2->start &&
		t1->onid == t2->onid && t1->tsid == t2->tsid && t1->svcid == t2->svcid)
		return 1;

	return 0;
}

//Save the book.xml file to flash and reboot the machine.
void reboot_wiz()
{
	socket_cleanup();
	execl("/bin/sh", "sh", "reboot.sh", NULL);
	exit(0);
}

void cmd_statfs(int socket)
{
	char line[80];
	
	struct statfs s;
	
	if(statfs(HDD_PATH, &s) == 0)
	{
		sprintf(line, "%d,%d,%d\n", s.f_bsize, s.f_blocks, s.f_bavail);
	}
	else
	{
		strcpy(line, "0,0,0\n");	
	}

	write(socket,line,strlen(line));
	
	return;
}

void cmd_chlist(int socket)
{
	char line[80];
	FILE *fp;
	int n,i,svc_name_len, rf_name_len;
	unsigned char *names;
	u16 svcid;
	u16 name_offset;
	u16 rf_offset;
	u32 svc_offset;
	RFInfo *rf, *rf_ptr;
	u16 rf_count;
	
	fp = fopen(SVC_PATH, "r");
	if(fp == NULL)
		return;

	fseek(fp, 0x30, SEEK_SET);
	//read number of channels
	n = read_u16(fp);
	fseek(fp, 0x4, SEEK_CUR);
	svc_name_len = read_u32(fp);
	rf_name_len = read_u32(fp);

	names = (unsigned char *)malloc(svc_name_len);
	
	fseek(fp, 0x3e, SEEK_SET);
	
	//read in name data
	fread(names,svc_name_len, 1, fp);
	
	//move to start of svcid segment
	fseek(fp, rf_name_len + 0x58, SEEK_CUR);
	
	//save the offset for later
	svc_offset = ftell(fp);
	
	//skip the svcid segment
	fseek(fp, 0x24 * n, SEEK_CUR);

	fseek(fp, 0x6, SEEK_CUR); //move to rf_count
	rf_count = read_u16(fp);
	
	rf = malloc(sizeof(RFInfo) * rf_count);
	
	rf_ptr = rf;
	
	//move to first rf entry
	fseek(fp, 0x20, SEEK_CUR);
	
	for(i=0;i < rf_count && !feof(fp);i++)
	{
		fseek(fp, 0x8, SEEK_CUR); //skip rf data + Mhz
		rf_ptr->onid = read_u16(fp);
		rf_ptr->tsid = read_u16(fp);
		
		rf_ptr++;
	}

	fseek(fp, svc_offset, SEEK_SET);
	
	//send the number of channels
	sprintf(line, "%d\n", n);
	printf("%s",line);
	write(socket,line,strlen(line));
	
	for(i=0;i<n;i++)
	{
		fseek(fp, 0x4, SEEK_CUR);
		rf_offset = read_u16(fp); //rf offset
		fseek(fp, 0x2, SEEK_CUR); //skip unknown
		svcid = read_u16(fp); //svcid
		fseek(fp, 0xa, SEEK_CUR); //skip to name offset data
		
		name_offset = read_u16(fp);
		
		sprintf(line, "%d,%d,%d:%s\n",rf[rf_offset].onid, rf[rf_offset].tsid, svcid, &names[name_offset]);
		printf("%s",line);
		write(socket,line,strlen(line));
		
		fseek(fp, 0xe, SEEK_CUR); //skip to end of this record.

	}

	free(rf);
	free(names);
	fclose(fp);

	return;
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
		sprintf(line,"%s\n%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			timer->filename, timer->startmjd, timer->nextmjd, timer->start, timer->duration, 
			timer->repeat, timer->play, timer->lock, timer->onid, timer->tsid, timer->svcid);
		write(socket,line,strlen(line));
	}

	book_free(book);

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
		book_free(book);
		return;
	}

	fp = fopen(BOOK_PATH, "w");

	fwrite(book_xml_header, 1, sizeof(book_xml_header)-1,fp);
	printf("%s", book_xml_header);

	fwrite(book_xml_timerlist_start, 1, sizeof(book_xml_timerlist_start)-1,fp);
	printf("%s", book_xml_timerlist_start);

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

	fwrite(book_xml_timerlist_end, 1, sizeof(book_xml_timerlist_end)-1,fp);
	printf("%s", book_xml_timerlist_end);
		
	fwrite(book_xml_footer, 1, sizeof(book_xml_footer)-1, fp);
	printf("%s", book_xml_footer); 
	fclose(fp);

	book_free(book);

	return;
}

void cmd_del(int socket)
{
	char line[MAX_LINE];
	Book *book = book_load_file(BOOK_PATH);
	Timer *timer, *del_timer;
	int i;
	FILE *fp;

	del_timer = read_info_timer(socket);
	if(del_timer == NULL)
	{
		printf("Failed to read info timer\n");
		book_free(book);
		return;
	}

	fp = fopen(BOOK_PATH, "w");

	fwrite(book_xml_header, 1, sizeof(book_xml_header)-1,fp);
	printf("%s", book_xml_header);

	timer=book->timers;

	if(book->n_timers == 0 || (book->n_timers == 1 && match_timers(timer,del_timer)))
	{
			fwrite(book_xml_timerlist_empty, 1, sizeof(book_xml_timerlist_empty)-1,fp);
			printf("%s", book_xml_timerlist_empty);
	}
	else
	{
		fwrite(book_xml_timerlist_start, 1, sizeof(book_xml_timerlist_start)-1,fp);
		printf("%s", book_xml_timerlist_start);
	
		for(i=0;i<book->n_timers;i++,timer++)
		{
			//don't write the timer that we are deleting
			if(match_timers(timer,del_timer))
				continue;
			
			sprintf(line,"        <Timer fname=\"%s\" startMjd=\"%d\" nextMjd=\"%d\" start=\"%d\" duration=\"%d\" repeat=\"%d\" play=\"%d\" lock=\"%d\" onId=\"%d\" tsId=\"%d\" svcId=\"%d\" />\n",
				timer->filename, timer->startmjd, timer->nextmjd, timer->start, timer->duration, 
				timer->repeat, timer->play, timer->lock, timer->onid, timer->tsid, timer->svcid);
			fwrite(line, 1, strlen(line),fp);

			printf("%s",line);
		}

		fwrite(book_xml_timerlist_end, 1, sizeof(book_xml_timerlist_end)-1,fp);
		printf("%s", book_xml_timerlist_end);
	}
	

	fwrite(book_xml_footer, 1, sizeof(book_xml_footer)-1, fp);
	printf("%s", book_xml_footer); 
	fclose(fp);

	free(del_timer);
	
	book_free(book);

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

void cmd_reboot(int socket)
{
	reboot_wiz();
}

int read_line(int socket, char *data)
{
	int i, nfds, ret;
	struct pollfd pfd[1];

	pfd[0].fd = socket;
	pfd[0].events = POLLIN;

	for(i=0;i<MAX_LINE;)
	{
		ret = read(socket, &data[i], 1);
		if(ret < 1)
		{
			if(ret == -1 && errno == EAGAIN) //we've run out of data, poll again.
			{
				nfds = poll(pfd, 1, 20 * 1000);
				if(nfds == 0)
				{
					printf("read_line(): Socket timeout!\n");
					return 0;
				}
				if(nfds == -1)
				{
					printf("read_line(): poll error!\n");
					return 0;
				}

				continue;
			}
			else //ret = 0 socket closed ret = -1 && !EAGAIN read error
				return 0;
		}
		if(data[i] == '\n')
		{
			data[i] = '\0';
			if(i >= 1 && data[i-1] == '\r')
				data[i-1] = '\0';
			return 1;
		}
		//printf("data[%d] = %c\n",i,data[i]);
		i++;
	}

	return 0;
}

int read_nbytes(int socket, unsigned char *data, int nbytes)
{
	int i, nfds, bytes_read;
	struct pollfd pfd[1];

	pfd[0].fd = socket;
	pfd[0].events = POLLIN;

	for(i=0;i<nbytes;)
	{
		bytes_read = read(socket, &data[i], nbytes - i);

		if(bytes_read < 1)
		{
			if(bytes_read == -1 && errno == EAGAIN) //we've run out of data, poll again.
			{
				nfds = poll(pfd, 1, 20 * 1000);
				if(nfds == 0)
				{
					printf("read_nbytes(): Socket timeout!\n");
					return 0;
				}
				if(nfds == -1)
				{
					printf("read_nbytes(): poll error!\n");
					return 0;
				}
				
				continue;
			}
			return 0;
		}
		i += bytes_read;
	}

	return 1;
}

int aes_load_key(unsigned char *key)
{
	FILE *fp;
	char passphrase[80];
	int i;
	char chr;
	struct cvs_MD5Context ctx;

	fp = fopen("wizremote.key", "r");
	if(fp == NULL)
	{
		return 0;
	}

	for(i=0;i<80;i++)
	{
		if(fread(&chr, 1, 1, fp) < 1)
			break;

		if(chr == '\r' || chr == '\n')
			break;
 
		passphrase[i] = chr;
	}

	fclose(fp);

	cvs_MD5Init(&ctx);
	cvs_MD5Update(&ctx, passphrase, i);
	cvs_MD5Final(key, &ctx);

	printf("AES key loaded. ");

  for(i=0;i < 16;i++)
		printf("%x", key[i]);

	printf("\n");

	return 1;
}

int aes_handshake(int socket, unsigned char *key)
{
	unsigned char expkey[4 * 4 * (10 + 1)];

	unsigned char token[TOKEN_SIZE];
	unsigned char enc[TOKEN_SIZE];
	unsigned char response[TOKEN_SIZE];
	int i;

	ExpandKey(key,expkey);

	for(i=0;i<TOKEN_SIZE;i++)
	{
		token[i] = rand()%255;
	}

	Encrypt(token, expkey, enc);

	//send token
	write(socket, enc, TOKEN_SIZE);

	//read response
	if(read_nbytes(socket, enc, TOKEN_SIZE) == 0)
		return 0;

	Decrypt(enc, expkey, response);

	//check response
	for(i=0;i<TOKEN_SIZE;i++)
	{
		if((response[i] ^ token_xor_key[i]) != token[i])
			return 0; 
	}

	return 1;
}

int process_command(int socket)
{
	char data[MAX_LINE];
	const CmdHandler *cmd;

	if(!read_line(socket, data))
		return 0;

	for(cmd = cmdHandlerTbl;cmd->handler != NULL;cmd++)
	{
		if(strcmp(cmd->cmd,data)==0)
		{
			printf("Command '%s' received.\n\n", cmd->cmd);
			cmd->handler(socket);
			return 1;
		}
	}

	printf("Unknown command '%s' received!\n\n", data);
	return 0;
}

int main()
{

	int soc_len;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	unsigned char aes_key[16]; //128 bit md5 hash of passphrase
	int aes_flag = 0;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( WIZREMOTE_PORT );
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);

	soc = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

	signal(SIGTERM,sigterm_handler);

	bind( soc, (struct sockaddr *)&serv_addr, sizeof(serv_addr) );
	listen(soc, 1);
	
	printf("WizRemote starting on port %d\n", WIZREMOTE_PORT);
	
	aes_flag = aes_load_key(aes_key);

	soc_len = sizeof(cli_addr);
	for(;;)
	{
		cli = accept(soc, (struct sockaddr *)&cli_addr, &soc_len );

		fcntl(cli, F_SETFL, O_NONBLOCK); //set socket non-blocking

		if(aes_flag == 0 || aes_handshake(cli, aes_key) == 1)
		{
			for(;process_command(cli);)
				write(cli,"ok\n", 3);
		}

		close(cli);
		cli = -1;
	}
}
