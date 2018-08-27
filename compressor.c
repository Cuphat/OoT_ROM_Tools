#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "yaz0.c"
#include "crc.c"

#define UINTSIZE 0x1000000
#define COMPSIZE 0x2000000
#define DCMPSIZE 0x4000000
#define byteSwap(x, y) asm("bswap %%eax" : "=a"(x) : "a"(y))

/* Structs */
typedef struct
{
	uint32_t startV;
	uint32_t   endV;
	uint32_t startP;
	uint32_t   endP;
}
table_t;

typedef struct 
{
	uint32_t num;
	uint8_t* src;
	uint8_t* dst;
	int src_size;
	int dst_size;
	table_t  tab;
}
args_t;

typedef struct
{
	table_t table;
	uint8_t* data;
	uint8_t  comp;
	uint32_t size;
}
output_t;

typedef struct
{
	uint32_t fileCount;
	uint32_t* ref_size;
	uint32_t* src_size;
	uint8_t**      ref;
	uint8_t**      src;
}
archive_t;

/* Functions */
uint32_t findTable(uint8_t*);
void getTableEnt(table_t*, uint32_t*, uint32_t);
void* thread_func(void*);
void errorCheck(int, char**);
void makeArchive(char*, char*);
int32_t getNumCores();
int32_t getNext();

/* Globals */
uint8_t* inROM;
uint8_t* outROM;
uint8_t* refTab;
char* name;
pthread_mutex_t filelock;
pthread_mutex_t countlock;
int32_t numFiles, nextFile, arcCount;
uint32_t* fileTab;
archive_t* archive;
output_t* out;

int main(int argc, char** argv)
{
	FILE* file;
	int32_t tabStart, tabSize, tabCount;
	volatile int32_t prev, prev_comp;
	int32_t i, size, numCores, tempSize;
	pthread_t* threads;
	table_t tab;

	errorCheck(argc, argv);
	printf("Starting compression.\n");
	fflush(stdout);

	/* Open input, read into inROM */
	file = fopen(argv[1], "rb");
	inROM = calloc(DCMPSIZE, sizeof(uint8_t));
	fread(inROM, DCMPSIZE, 1, file);
	fclose(file);

	/* Read archive if it exists*/
	file = fopen("ARCHIVE.bin", "rb");
	if(file != NULL)
	{
		/* Get number of files */
		printf("Loading Archive.\n");
		fflush(stdout);
		archive = malloc(sizeof(archive_t));
		fread(&(archive->fileCount), sizeof(uint32_t), 1, file);

		/* Allocate space for files and sizes */
		archive->ref_size = malloc(sizeof(uint32_t) * archive->fileCount);
		archive->src_size = malloc(sizeof(uint32_t) * archive->fileCount);
		archive->ref = malloc(sizeof(uint8_t*) * archive->fileCount);
		archive->src = malloc(sizeof(uint8_t*) * archive->fileCount);

		/* Read in file size and then file data */
		for(i = 0; i < archive->fileCount; i++)
		{
			/* Decompressed "Reference" file */
			fread(&tempSize, sizeof(uint32_t), 1, file);
			archive->ref[i] = malloc(tempSize);
			archive->ref_size[i] = tempSize;
			fread(archive->ref[i], 1, tempSize, file);

			/* Compressed "Source" file */
			fread(&tempSize, sizeof(uint32_t), 1, file);
			archive->src[i] = malloc(tempSize);
			archive->src_size[i] = tempSize;
			fread(archive->src[i], 1, tempSize, file);
		}
		fclose(file);
	}
	else
	{
		printf("No archive found, this could take a while.\n");
		fflush(stdout);
		archive = NULL;
	}

	/* Find the file table and relevant info */
	tabStart = findTable(inROM);
	fileTab = (uint32_t*)(inROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;

	/* Read in compression reference */
	file = fopen("table.txt", "r");
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	refTab = malloc(size);
	for(i = 0; i < size; i++)
		refTab[i] = (fgetc(file) == '1') ? 1 : 0;
	fclose(file);

	/* Initialise some stuff */
	out = malloc(sizeof(output_t) * tabCount);
	pthread_mutex_init(&filelock, NULL);
	pthread_mutex_init(&countlock, NULL);
	numFiles = tabCount - 3;
	nextFile = 3;
	arcCount = 0;

	/* Get CPU core count */
	numCores = getNumCores() - 1;
	threads = malloc(sizeof(pthread_t) * numCores);
	printf("Detected %d cores\n", (numCores + 1));
	fflush(stdout);

	/* Create all the threads */
	for(i = 0; i < numCores; i++)
		pthread_create(&threads[i], NULL, thread_func, NULL);

	/* Wait for all of the threads to finish */
	for(i = 0; i < numCores; i++)
		pthread_join(threads[i], NULL);

	/* Setup for copying to outROM */
	printf("Files compressed, writing new ROM.\n");
	fflush(stdout);
	outROM = calloc(COMPSIZE, sizeof(uint8_t));
	memcpy(outROM, inROM, tabStart + tabSize);
	prev = tabStart + tabSize;
	prev_comp = refTab[2];
	tabStart += 0x20;

	/* Free some stuff */
	pthread_mutex_destroy(&filelock);
	pthread_mutex_destroy(&countlock);
	if(archive != NULL)
	{
		free(archive->ref);
		free(archive->src);
		free(archive->ref_size);
		free(archive->src_size);
		free(archive);
	}
	free(threads);
	free(refTab);
	free(inROM);

	/* Copy to outROM loop */
	for(i = 3; i < tabCount; i++)
	{
		tab = out[i].table;
		size = out[i].size;
		tabStart += 0x10;

		/* Finish table and copy to outROM */
		if(tab.startV != tab.endV)
		{
			tab.startP = prev;
			if(out[i].comp)
				tab.endP = tab.startP + size;

			memcpy(outROM + tab.startP, out[i].data, size);
			byteSwap(tab.startV, tab.startV);
			byteSwap(tab.endV, tab.endV);
			byteSwap(tab.startP, tab.startP);
			byteSwap(tab.endP, tab.endP);
			memcpy(outROM + tabStart, &tab, sizeof(table_t));
		}

		/* Setup for next iteration */
		prev += size;
		prev_comp = out[i].comp;

		free(out[i].data);
	}
	free(out);

	/* Make and fill the output ROM */
	file = fopen(name, "wb");
	fwrite(outROM, COMPSIZE, 1, file);
	fclose(file);
	free(outROM);

	/* Fix the CRC using some kind of magic or something */
	fix_crc(name);

	/* Make the archive if needed */
	if(archive == NULL)
	{
		printf("Creating archive.\n");
		fflush(stdout);
		makeArchive(argv[1], name);
	}

	printf("Compression complete.\n");
	fflush(stdout);
	free(name);
	return(0);
}

uint32_t findTable(uint8_t* argROM)
{
	uint32_t i, temp;
	uint32_t* tempROM;

	tempROM = (uint32_t*)argROM;

	for(i = 0; i+4 < UINTSIZE; i += 4)
	{
		/* This marks the begining of the filetable */
		byteSwap(temp, tempROM[i]);
		if(temp == 0x7A656C64)
		{
			byteSwap(temp, tempROM[i+1]);
			if(temp == 0x61407372)
			{
				byteSwap(temp, tempROM[i+2]);
				if((temp & 0xFF000000) == 0x64000000)
				{
					/* Find first entry in file table */
					i += 8;
					byteSwap(temp, tempROM[i]);
					while(temp != 0x00001060)
					{
						i += 4;
						byteSwap(temp, tempROM[i]);
					}
					return((i-4) * sizeof(uint32_t));
				}
			}
		}
	}

	fprintf(stderr, "Error: Couldn't find file table\n");
	exit(1);
}

void getTableEnt(table_t* tab, uint32_t* files, uint32_t i)
{
	byteSwap(tab->startV, files[i*4]);
	byteSwap(tab->endV,   files[(i*4)+1]);
	byteSwap(tab->startP, files[(i*4)+2]);
	byteSwap(tab->endP,   files[(i*4)+3]);
}

void* thread_func(void* null)
{
	args_t* a;
	table_t t;
	int32_t next, i, nextArchive;

	while((next = getNext()) != -1)
	{
		a = malloc(sizeof(args_t));
		a->num = next;
		getTableEnt(&(a->tab), fileTab, next);
		t = a->tab;
		i = a->num;

		/* Setup the src*/
		a->src_size = t.endV - t.startV;
		a->src = inROM + t.startV;

		/* If needed, compress and fix size */
		/* Otherwise, just copy src into outROM */
		if(refTab[i])
		{
			pthread_mutex_lock(&countlock);
			nextArchive = arcCount++;
			pthread_mutex_unlock(&countlock);

			/* If the uncompressed version is the same as vanilla, just copy/paste it */
			/* Otherwise, compress it manually */
			if((archive != NULL) && (memcmp(a->src, archive->ref[nextArchive], archive->ref_size[nextArchive]) == 0))
			{
				out[i].comp = 1;
				a->src_size = archive->src_size[nextArchive];
				out[i].data = malloc(a->src_size);
				memcpy(out[i].data, archive->src[nextArchive], a->src_size);
				free(archive->ref[nextArchive]);
				free(archive->src[nextArchive]);
			}
			else
			{
				a->dst_size = a->src_size + 0x160;
				a->dst = calloc(a->dst_size, sizeof(uint8_t));
				yaz0_encode(a->src, a->src_size, a->dst, &(a->dst_size));
				a->src_size = ((a->dst_size + 31) & -16);
				out[i].comp = 1;
				out[i].data = malloc(a->src_size);
				memcpy(out[i].data, a->dst, a->src_size);
				free(a->dst);
				if(archive != NULL)
				{
					free(archive->ref[nextArchive]);
					free(archive->src[nextArchive]);
				}
			}
		}
		else
		{
			out[i].comp = 0;
			out[i].data = malloc(a->src_size);
			memcpy(out[i].data, a->src, a->src_size);
		}

		/* Set up the table entry and size */
		out[i].table = t;
		out[i].size = a->src_size;
		free(a);
	}

	return(NULL);
}

void makeArchive(char* fileOne, char* fileTwo)
{
	table_t tab;
	uint32_t tabSize, tabCount, tabStart, i, fileCount;
	uint32_t fileSize;
	archive_t archive;
	uint8_t* tempROM;
	FILE* file;

	/* Open and read the input files */
	file = fopen(fileOne, "rb");
	inROM = calloc(DCMPSIZE, sizeof(uint8_t));
	fread(inROM, 1, DCMPSIZE, file);
	fclose(file);

	file = fopen(fileTwo, "rb");
	tempROM = calloc(COMPSIZE, sizeof(uint8_t));
	fread(tempROM, 1, COMPSIZE, file);
	fclose(file);

	/* Find some info on the DMAtable */
	tabStart = findTable(tempROM);
	fileTab = (uint32_t*)(tempROM + tabStart);
	getTableEnt(&tab, fileTab, 2);
	tabSize = tab.endV - tab.startV;
	tabCount = tabSize / 16;
	archive.fileCount = 0;
	fileCount = 0;

	/* Find the number of compressed files in the ROM */
	for(i = 3; i <= tabCount; i++)
	{
		getTableEnt(&tab, fileTab, i);

		if(tab.endP != 0)
			fileCount++;
	}

	/* Open output file */
	file = fopen("ARCHIVE.bin", "wb");
	fwrite(&fileCount, sizeof(uint32_t), 1, file);

	/* Write the archive data */
	for(i = 3; i <= tabCount; i++)
	{
		getTableEnt(&tab, fileTab, i);

		if(tab.endP != 0)
		{
			/* Write the size and data for the decompressed portion */
			fileSize = tab.endV - tab.startV;
			fwrite(&fileSize, sizeof(uint32_t), 1, file);
			fwrite(inROM + tab.startV, 1, fileSize, file);

			/* Write the size and data for the compressed portion */
			fileSize = tab.endP - tab.startP;
			fwrite(&fileSize, sizeof(uint32_t), 1, file);
			fwrite((tempROM + tab.startP), 1, fileSize, file);
		}
	}

	fclose(file);
	free(tempROM);
	free(inROM);
}

int32_t getNumCores()
{
	/* Windows */
	#ifdef _WIN32

		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return(info.dwNumberOfProcessors);

	/* Mac */
	#elif MACOS

		int nm[2];
		size_t len;
		uint32_t count;

		len = 4;
		nm[0] = CTL_HW;
		nm[1] = HW_AVAILCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);

		if (count < 1)
		{
			nm[1] = HW_NCPU;
			sysctl(nm, 2, &count, &len, NULL, 0);
			if (count < 1)
				count = 1;
		}
		return(count);

	/* Linux */
	#else

		return(sysconf(_SC_NPROCESSORS_ONLN));

	#endif
}

int32_t getNext()
{
	int32_t file, temp;

	/* Counter mutex */
	pthread_mutex_lock(&filelock);
	file = nextFile++;
	pthread_mutex_unlock(&filelock);

	/* Progress tracker */
	if((file % 150) == 0)
	{
		temp = numFiles - file;
		printf("%#4d files remaining\n", (temp + 2));
		fflush(stdout);
	}

	/* Base case */
	if (file > numFiles)
		file = -1;

	return(file);
}

void errorCheck(int argc, char** argv)
{
	int i;
	FILE* file;

	/* Check for arguments */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: Compress [Input ROM]\n");
		exit(1);
	}

	/* Check that input ROM exists */
	file = fopen(argv[1], "rb");
	if(file == NULL)
	{
		perror(argv[1]);
		fclose(file);
		exit(1);
	}

	/* Check that input ROM is correct size */
	fseek(file, 0, SEEK_END);
	i = ftell(file);
	fclose(file);
	if(i != DCMPSIZE)
	{
		fprintf(stderr, "Warning: Invalid input ROM size\n");
		exit(1);
	}

    /* Check that table.txt exists */
    file = fopen("table.txt", "r");
    if(file == NULL)
    {
        perror("table.txt");
        fclose(file);
        exit(1);
    }

	/* Check that output ROM is writeable */
	i = strlen(argv[1]) + 5;
	name = malloc(i);
	strcpy(name, argv[1]);
	for(i; i > 0; i--)
	{
		if(name[i] == '.')
		{
			name[i] = '\0';
			break;
		}
	}
	strcat(name, "-comp.z64");
	file = fopen(name, "wb");
	if(file == NULL)
	{
	    perror(name);
	    fclose(file);
	    free(name);
	    exit(1);
	}
	fclose(file);
}