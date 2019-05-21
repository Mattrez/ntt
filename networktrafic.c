#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

typedef unsigned int u32;
typedef unsigned long u64;
typedef struct dirent dirent;

#define U64LEN 20
#define MAXINTERFACES 10

const char* GCompressedLetter = "BKMGP";
const char* GInterfacePath = "/sys/class/net/";

typedef struct
{
	u32 Bytes;
	u32 Order;
} compressed_bytes;

typedef enum
{
	TRANSMIT,
	RECIVE
} ByteType;

static u32
StringLen(const char *String)
{
	u32 Result = 0;
	
	while(String[Result] != '\0' &&
			String[Result] != '\n')
	{
		++Result;
	}

	return Result;
}

static u64
StringToU64(const char *String)
{
	u64 Result = 0;
	u64 TenPower = 1;
	u32 StringLenght = StringLen(String);

	for(u32 i = StringLenght; i > 0; i--)
	{
		u64 Num = String[i - 1] - '0';
		Result += TenPower * Num;
		TenPower = TenPower * 10;
	}

	return Result;
}

static void
GetInterfaces(dirent *Interfaces)
{
	int DirDesc = 0;
	DirDesc = open(GInterfacePath, O_RDONLY);

	DIR *Dir;
	Dir = fdopendir(DirDesc);

	if(!Dir)
	{
		printf("Cannot open directory -> %s\n", GInterfacePath);
		return;
	}

	dirent *ent;
	for(u32 i = 0; i < 2; i++)
	{
		ent = readdir(Dir);
	} // Truncating "." and ".." directories

	for(u32 i = 0; i < MAXINTERFACES; i++)
	{
		ent = readdir(Dir);
		if(ent == 0)
		{
			return;
		}

		memcpy(Interfaces++, ent, sizeof(dirent));
	}

	close(DirDesc);
	closedir(Dir);
}

static void
GetWorkingInterfaces(dirent *Interfaces)
{
	GetInterfaces(Interfaces);

	for(u32 i = 0; i < MAXINTERFACES; i++)
	{
		if(Interfaces->d_ino == 0)
		{
			continue;
		}

		char FilePath[64] = { 0 };
		strcpy(FilePath, GInterfacePath);
		strcat(FilePath, Interfaces->d_name);
		strcat(FilePath, "/operstate");
		int OperStateFD = open(FilePath, O_RDONLY);
		
		char Buffer[16] = { 0 };
		read(OperStateFD, Buffer, 16);

		close(OperStateFD);

		if(strncmp(Buffer, "up", 2) != 0)
		{
			memmove(Interfaces, Interfaces + 1,
					  (MAXINTERFACES - i - 1) * sizeof(dirent));
			continue;
		} // if Buffer is not equal exactly to "up"

		++Interfaces;
	}
}

static u64
GetBytes(ByteType Type)
{
	dirent Interfaces[MAXINTERFACES] = { 0 };
	GetWorkingInterfaces(Interfaces);
	dirent *Interface = Interfaces;

	u64 Result = 0;
	for(u32 i = 0; i < MAXINTERFACES; i++)
	{
		FILE *File = 0;
		char NumberBuffer[U64LEN + 1] = { 0 };

		if(Interface->d_ino == 0)
		{
			break;
		} // if the file id is 0, quit

		char FilePath[64] = { 0 };
		if(Type == TRANSMIT)
		{
			strcpy(FilePath, GInterfacePath);
			strcat(FilePath, Interface->d_name);
			strcat(FilePath, "/statistics/tx_bytes");

			File = fopen(FilePath, "r");
		}
		else if(Type == RECIVE)
		{
			strcpy(FilePath, GInterfacePath);
			strcat(FilePath, Interface->d_name);
			strcat(FilePath, "/statistics/rx_bytes");

			File = fopen(FilePath, "r");
		}

		fread(NumberBuffer, U64LEN + 1, 1, File);
		fclose(File);

		NumberBuffer[StringLen(NumberBuffer)] = '\0';
		Result += StringToU64(NumberBuffer);

		++Interface;
	}

	return Result;
}

static void
SaveOldBytes(ByteType Type, u64 ByteValue)
{
	FILE *File = 0;
	char Buffer[U64LEN + 1];
	sprintf(Buffer, "%lu\n", ByteValue);

	if(Type == TRANSMIT)
	{
		const char *FilePath = "/tmp/old_tx";
		File = fopen(FilePath, "w");
	}
	else if(Type == RECIVE)
	{
		const char *FilePath = "/tmp/old_rx";
		File = fopen(FilePath, "w");
	}

	fwrite(Buffer, StringLen(Buffer), 1, File);
	fclose(File);
}

static u64
ReadOldBytes(ByteType Type)
{
	u64 Result = 0;
	FILE *File = 0;
	char Buffer[U64LEN + 1];

	if(Type == TRANSMIT)
	{
		const char *FilePath = "/tmp/old_tx";
		File = fopen(FilePath, "r");
	}
	else if(Type == RECIVE)
	{
		const char *FilePath = "/tmp/old_rx";
		File = fopen(FilePath, "r");
	}

	if(!File)
	{
		return GetBytes(Type);
	} // if there is no old bytes file, return old bytes as current bytes

	fread(Buffer, U64LEN + 1, 1, File);
	fclose(File);

	Result = StringToU64(Buffer);

	return Result;
}

static compressed_bytes
CompressBytes(u64 Bytes)
{
	compressed_bytes Result = { 0 };

	while(Bytes >= 1024)
	{
		Bytes = Bytes / 1024;
		++Result.Order;
	}

	Result.Bytes = Bytes;

	return Result;
}

static void
TrunkcateBytes(compressed_bytes *CompresedBytes)
{
	if(CompresedBytes->Order == 0)
	{
		CompresedBytes->Order = 1;
		CompresedBytes->Bytes = 0;
	}
}

int main(void)
{
	u64 RXBytes = GetBytes(RECIVE);
	u64 OldRX = ReadOldBytes(RECIVE);

	u64 DiffRecv = RXBytes - OldRX;

	OldRX = RXBytes;
	SaveOldBytes(RECIVE, OldRX);

	compressed_bytes CompressedRX = CompressBytes(DiffRecv);
	TrunkcateBytes(&CompressedRX);

	u64 TXBytes = GetBytes(TRANSMIT);
	u64 OldTX = ReadOldBytes(TRANSMIT);

	u64 DiffTran = TXBytes - OldTX;

	OldTX = TXBytes;
	SaveOldBytes(TRANSMIT, OldTX);

	compressed_bytes CompressedTX = CompressBytes(DiffTran);
	TrunkcateBytes(&CompressedTX);

	printf("%u%cB/s %u%cB/s\n",
			 CompressedRX.Bytes,
			 GCompressedLetter[CompressedRX.Order],
			 CompressedTX.Bytes,
			 GCompressedLetter[CompressedTX.Order]);

	return 0;
}
