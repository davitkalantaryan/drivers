
#define		__BUFFER_SIZE__	32768

#ifdef _WIN64
	#ifndef WIN32
		#define WIN32
	#endif
#endif


#include <time.h>


#define MSEC2(finish, start)	( (double)( (finish).tv_usec - (start).tv_usec )*0.001 + \
							(double)( (finish).tv_sec - (start).tv_sec ) * 1000. )

#include <stdlib.h>

#include "../../aallfiles/sources/consolefunctions1.h"

#ifdef WIN32
#include <WINDOWS.H>
#define		_SLEEPM_	Sleep
#else
#include <unistd.h>
#define		_SLEEPM_(x)	usleep(1000*(x))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern int System3(const char* Execute, char* Buffer, int BufLen, char* ErrBuff, int ErrBufLen,int* pRead);

#ifdef WIN32
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#else/* #ifdef WIN32 */
#include <sys/time.h>
#endif/* #ifdef WIN32 */

#ifdef __cplusplus
}
#endif


int main()
{

	const COLORTP cRedColor = RED;
	COLORTP cDefColor;
	int nRead, nPrevRead, nRet;
	char vcBuffer[__BUFFER_SIZE__];
	timeval tv1,tv2;
	bool bRedText(false);
	double lfTimeLeft;

	ConsTextColor1(stdout,NULL,&cDefColor);

	nRet = System3("dmesg",vcBuffer,__BUFFER_SIZE__,NULL,0,&nPrevRead);

	if(!nRet)
	{
		nRet = system("clear");
		printf("Read = %d,  BufSize = %d\n",nPrevRead,__BUFFER_SIZE__);
		printf("%s\n", vcBuffer);
	}

	while(1)
	{
		//nRet = system("clear");
		//nRet += system("dmesg");

		nRet = System3("dmesg",vcBuffer,__BUFFER_SIZE__,NULL,0,&nRead);

		//printf("nRead = %d\n",nRead);

		if(nRet)continue;

		if(nRead<nPrevRead)
		{
			nRet = system("clear");
			ConsTextColor1(stdout,&cDefColor,NULL);
			printf("%s\n",vcBuffer);
			nPrevRead = nRead;
		}
		else if(nRead>nPrevRead)
		{
			ConsTextColor1(stdout,&cRedColor,NULL);
			printf("%s\n",vcBuffer+nPrevRead);
			gettimeofday(&tv1,NULL);
			bRedText = true;
			nPrevRead = nRead;
		}


		if(bRedText)
		{
			gettimeofday(&tv2,NULL);
			lfTimeLeft = MSEC2(tv2,tv1);

			if(lfTimeLeft>150000.)
			{
				ConsTextColor1(stdout,&cDefColor,NULL);
				nRet = system("clear");
				printf("Read = %d,  BufSize = %d\n",nRead,__BUFFER_SIZE__);
				printf("%s\n", vcBuffer);

				bRedText = false;
			}

		}

		_SLEEPM_(200);
	}

	return 0;
}
