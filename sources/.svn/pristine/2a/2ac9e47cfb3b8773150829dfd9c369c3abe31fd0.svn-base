
#include <stdio.h>

#include <sys/stat.h> 
#include <fcntl.h>


char	g_vcBuffer[200];


int main()
{
	FILE* fpFile;

	fpFile = fopen("/dev/test_drv0","r");
	//open("/dev/test_drv0",O_RDONLY);

	printf( "fpFile = %ld\n",(long int)fpFile);

	if( !fpFile )
		return 1;

	fread(g_vcBuffer,1,199,fpFile );

	printf( "g_vcBuffer is \"%s\"\n",g_vcBuffer);

	fclose(fpFile);

	//open("/dev/test_drv0",O_RDONLY);

	return 0;
}
