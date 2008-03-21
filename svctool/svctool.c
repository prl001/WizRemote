//svcTool Copyright 2008 Eric Fry efry@users.sourceforge.net
//Licensed under the GPLv2
//

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>

#define u8 unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define START_SEGMENT_MAGIC 0x1000BE90
#define SEGMENT_MAGIC 0x103BE90

#define SVC_FLAG_TV    0
#define SVC_FLAG_RADIO 1
#define SVC_FLAG_EMPTY 7

const char *svc_dat = "svc.dat";

typedef struct {
	u16 flag;
	u16 unk_2;
	u16 rf_offset;
	u16 unk_6;
	u16 svcid;
	unsigned char unk_a[0xa];
	u16 name_offset;
	u16 unk_16;
	u16 rfname_offset;
	unsigned char unk_1a[0x6];
	
	u16 lcn;
	u16 unk_22;
} CHInfo;

typedef struct {
	u16 unk1;
	u16 unk2;
	u32 rf;
	u16 onid;
	u16 tsid;
	unsigned char *name;
} RFInfo;

typedef struct {
	unsigned char unk_12[0x15];
	u16 num_channels;
	u16 num_deleted_channels;
	u16 num_total_channels;
	u32 svc_name_len;
	u32 rf_name_len;
	unsigned char *ch_names;
	unsigned char *rf_names;
	
	unsigned char unk_ch_hdr[0x4e];
	
	CHInfo *ch;
	
	unsigned char unk_rf_hdr[0x1e];

	u32 num_rfs;

	RFInfo *rf;
} SVC;


u32 read_bytes(FILE *fp, u32 n, unsigned char *buf)
{
	return fread(buf, 1, n, fp);
}

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

u32 write_bytes(FILE *fp, u32 n, unsigned char *buf)
{
	return fwrite(buf, 1, n, fp);
}

void write_u8(FILE *fp, u8 n)
{
	fwrite(&n, sizeof(u8), 1, fp);
}

void write_u16(FILE *fp, u16 n)
{
#ifdef BIG_ENDIAN
	unsigned char buf[2];
	buf[0] = n & 0xFF;
	buf[1] = n >> 8;
	fwrite(buf, 2, 1, fp);	
#else
	fwrite(&n, sizeof(u16), 1, fp);
#endif

	return;
}

void write_u32(FILE *fp, u32 n)
{
#ifdef BIG_ENDIAN
	unsigned char buf[4];
	buf[0] = n & 0xFF;
	buf[1] = (n >> 8) & 0xFF;
	buf[2] = (n >> 16) & 0xFF;
	buf[3] = (n >> 24) & 0xFF;
	fwrite(buf, 4, 1, fp);	
#else
	fwrite(&n, sizeof(u32), 1, fp);
#endif

	return;
}

SVC *read_svc(const char *filename)
{
	FILE *fp;
	SVC *svc;
	CHInfo *ch_ptr;
	RFInfo *rf_ptr;
	u32 i;

	fp = fopen(filename, "r");
	if(fp == NULL)
		return NULL;

	svc = malloc(sizeof(SVC));
	if(svc == NULL)
		return NULL;

	fseek(fp, 0x12, SEEK_SET);
	
	read_bytes(fp, 0x15, svc->unk_12);

	fseek(fp, 0x30, SEEK_SET);
	
	svc->num_channels = read_u16(fp);
	svc->num_deleted_channels = read_u16(fp);
	svc->num_total_channels = read_u16(fp);
	svc->svc_name_len = read_u32(fp);
	svc->rf_name_len = read_u32(fp);

	svc->ch_names = (unsigned char *)malloc(svc->svc_name_len);
	read_bytes(fp, svc->svc_name_len, svc->ch_names);
	
	svc->rf_names = (unsigned char *)malloc(svc->rf_name_len);
	read_bytes(fp, svc->rf_name_len, svc->rf_names);

	fseek(fp, 0xa, SEEK_CUR);
	read_bytes(fp, 0x4e, svc->unk_ch_hdr);
	
	svc->ch = malloc(sizeof(CHInfo) * svc->num_channels);
	
	for(ch_ptr=svc->ch,i=0;i < svc->num_channels;ch_ptr++,i++)
	{
		ch_ptr->flag = read_u16(fp);
		ch_ptr->unk_2 = read_u16(fp);
		ch_ptr->rf_offset = read_u16(fp);
		ch_ptr->unk_6 = read_u16(fp);
		ch_ptr->svcid = read_u16(fp);
		read_bytes(fp, 0xa, ch_ptr->unk_a);
		ch_ptr->name_offset = read_u16(fp);
		printf("offset = %d, %s\n",svc->ch[i].name_offset, &svc->ch_names[svc->ch[i].name_offset]);
		ch_ptr->unk_16 = read_u16(fp);
		ch_ptr->rfname_offset = read_u16(fp);
		read_bytes(fp, 0x6, ch_ptr->unk_1a);
	
		ch_ptr->lcn = read_u16(fp);
		ch_ptr->unk_22 = read_u16(fp);
	}

	fseek(fp, 0x6, SEEK_CUR); //move to rf_count
	svc->num_rfs = read_u32(fp);

	read_bytes(fp, 0x1e, svc->unk_rf_hdr);
	
	svc->rf = malloc(sizeof(RFInfo) * svc->num_rfs);

	for(rf_ptr=svc->rf,i=0;i < svc->num_rfs;rf_ptr++,i++)
	{
		rf_ptr->unk1 = read_u16(fp);
		rf_ptr->unk2 = read_u16(fp);
		rf_ptr->rf = read_u32(fp);
		rf_ptr->onid = read_u16(fp);
		rf_ptr->tsid = read_u16(fp);
	}

	fclose(fp);

	return svc;
}

int svc_find_ch(SVC *svc, u16 onid, u16 tsid, u16 svcid)
{
	RFInfo *rf_ptr;
	u32 i;
	for(i=0;i<svc->num_channels;i++)
	{
		rf_ptr = &svc->rf[svc->ch[i].rf_offset];
		if(rf_ptr->onid == onid && rf_ptr->tsid == tsid && svc->ch[i].svcid == svcid)
			return i;
	}
	
	return -1;
}

int svc_rename_ch(const char *filename, const char *outfilename, int onid, int tsid, int svcid, const char *new_name)
{
	SVC *svc;
	int ch_idx;
	int name_diff;
	CHInfo *ch;
	FILE *fp;
	unsigned char *name_ptr;
	unsigned char *names_tmp;
	unsigned char *name;
	
	int i;

	svc = read_svc(filename);
	if(svc == NULL)
		return 0;
	
	ch_idx = svc_find_ch(svc, onid, tsid, svcid);
	if(ch_idx == -1)
		return 0;
	
	ch = &svc->ch[ch_idx];
	
	name_diff = strlen(new_name) - strlen((char *)&svc->ch_names[ch->name_offset]);
	
	printf("Index %d, oldname = '%s' newname = '%s' diff = %d oldlen = %d, newlen = %d\n", ch_idx, &svc->ch_names[ch->name_offset], new_name,name_diff,svc->svc_name_len, svc->svc_name_len + name_diff);

	names_tmp = malloc(sizeof(unsigned char) * (svc->svc_name_len + name_diff));

	name_ptr = names_tmp;
	//adjust the name offsets.
	for(i=0; i < svc->num_channels; i++)
	{
		if(i==ch_idx)
			name = new_name;
		else
			name = &svc->ch_names[svc->ch[i].name_offset];

		svc->ch[i].name_offset = name_ptr - names_tmp;
		
		if(svc->ch[i].flag != SVC_FLAG_EMPTY)
		{
			strcpy((char *)name_ptr, name);
			name_ptr += strlen(name) + 1;
		}

	}

	free(svc->ch_names);

	svc->ch_names = names_tmp;
	svc->svc_name_len += name_diff;

	fp = fopen(outfilename, "w");
	
	write_u32(fp, START_SEGMENT_MAGIC);
	write_u32(fp, 3); //3 segments

	write_u32(fp, SEGMENT_MAGIC);
	write_u16(fp, 0); //first segment
	write_u32(fp, 1); //unknown
	write_bytes(fp, 0x15, svc->unk_12);
	write_u16(fp,0x400);
	write_u16(fp,svc->svc_name_len * 4);
	write_u16(fp,0x500);
	write_u16(fp,svc->rf_name_len * 4);
	write_u8(fp, 0);

	write_u16(fp,svc->num_channels);
	write_u16(fp,svc->num_deleted_channels);
	write_u16(fp,svc->num_total_channels);
	write_u32(fp,svc->svc_name_len);
	write_u32(fp,svc->rf_name_len);

	write_bytes(fp,svc->svc_name_len,svc->ch_names);
	write_bytes(fp,svc->rf_name_len,svc->rf_names);

	write_u32(fp, SEGMENT_MAGIC);
	write_u16(fp, 1); //second segment
	write_u32(fp, svc->num_channels);
	write_bytes(fp, 0x4e, svc->unk_ch_hdr);

	for(i=0;i < svc->num_channels;i++)
	{
		write_u16(fp,svc->ch[i].flag);
		write_u16(fp,svc->ch[i].unk_2);
		write_u16(fp,svc->ch[i].rf_offset);
		write_u16(fp,svc->ch[i].unk_6);
		write_u16(fp,svc->ch[i].svcid);
		write_bytes(fp, 0xa, svc->ch[i].unk_a);
		write_u16(fp,svc->ch[i].name_offset);
		write_u16(fp,svc->ch[i].unk_16);
		write_u16(fp,svc->ch[i].rfname_offset);
		write_bytes(fp, 0x6, svc->ch[i].unk_1a);

		write_u16(fp,svc->ch[i].lcn);
		write_u16(fp,svc->ch[i].unk_22);
	}

	write_u32(fp, SEGMENT_MAGIC);
	write_u16(fp, 2); //third segment
	write_u32(fp, svc->num_rfs);

	write_bytes(fp, 0x1e, svc->unk_rf_hdr);

	for(i=0;i < svc->num_rfs;i++)
	{
		write_u16(fp,svc->rf[i].unk1);
		write_u16(fp,svc->rf[i].unk2);
		write_u32(fp,svc->rf[i].rf);
		write_u16(fp,svc->rf[i].onid);
		write_u16(fp,svc->rf[i].tsid);
	}

	fclose(fp);

	return 1;
}

void print_usage(char *filename)
{
	printf("Usage: %s [filename]\n",filename);
	printf("If filename isn't specified then 'svc.dat' is used.\n");

	return;
}

int main(int argc, char *argv[])
{
	const char *filename = svc_dat;
	const char *newname;
	const char newfilename[] = "svc_new.dat";
	
	int onid,tsid,svcid;

	if(argc < 6)
	{
		print_usage(argv[0]);
		return 1;
	}

	filename = argv[1];
	onid = atoi(argv[2]);
	tsid = atoi(argv[3]);
	svcid = atoi(argv[4]);
	newname = argv[5];
	
	if(svc_rename_ch(filename, newfilename, onid, tsid, svcid, newname) == 0)
	{
		printf("Error: Opening '%s'\n", filename);
		print_usage(argv[0]);
		return 1;
	}

	return 0;
}
