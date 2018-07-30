#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <fstream>

#include "pciedev_io.h"
#include "newtimer_io.h"
#include "tamc200_io.h"

int			fd;

int main(int argc, char* argv[])
{
    int	 ch_in        = 0;
    char nod_name[15] = "";
    device_rw	            l_Read;
    device_ioctrl_data      io_RW;
    t_iptimer_ioctl_data    l_Ctrl;
    t_iptimer_ioctl_rw	    rw_Ctrl;
    t_iptimer_reg_data	    reg_Ctrl;
    int			    tmp_board;
    int			    tmp_element;
    int      	            tmp_data;
    int                     len = 0;
    int			    offset;
    int                     code = 0;
    int			    tmp_mode;
    u_int	            tmp_offset;
    int      	            tmp_barx;
    float                   tmp_fdata;
    int                     k = 0;
    int                     itemsize = 0;
    
    itemsize = sizeof(device_rw);
    printf("ITEMSIZE %i\n", itemsize);

    if(argc ==1)
        {
        printf("Input \"prog /dev/iptimerX\" \n");
        return 0;
        }

    strncpy(nod_name,argv[1],sizeof(nod_name));
    fd = open (nod_name, O_RDWR);
        if (fd < 0) {
        printf ("#CAN'T OPEN FILE \n");
        exit (1);
    }

    while (ch_in!=11){
	
        l_Ctrl.f_Irq_Mask	= 0;
        l_Ctrl.f_Irq_Sig	= 0;
        l_Ctrl.f_Status		= 0;

        rw_Ctrl.io_chn	= 0;
        rw_Ctrl.io_data = 0;
        rw_Ctrl.io_rw 	= 0;
        l_Read.offset_rw   = 0;
        l_Read.data_rw     = 0;
        l_Read.mode_rw     = 0;
        l_Read.barx_rw     = 0;

        printf("\n READ (1) or WRITE (0) or ADD_SIG (2)or REM_SIG (3)\n");
        printf("or GET_STS (4) or  SET_MASK (5) or SET_IRQ (6)\n");
        printf("or GET_DELAY (7) or  SET_DELAY (8) or GET_EVENT (9) or SET_EVENT (10)\n");
        printf("or GET_DAISY (13) or  SET_DAISY (14) or GET_TIME_BASE (15) or SET_TIME_BASE (16)\n");
        printf("or GET_PULSE (17) or  SET_PULSE (18) or GET_OUT (19) or SET_OUT (20)\n");
        printf("or GET_REGISTERS (23) or GET_LOC_REG (24) or GET_CLOCK_STS (25)\n");
        printf("or CHECK SVRS (26) or REM_SVRS (27)\n");
        printf("or GET_VARCLOCK (21) or SET_VARCLOCK (22) or REFRESH (12) or END (11) ?-\n");
        printf("or GET DRIVER VERSION (30) or GET FIRMWARE VERSION (31) or GET SLOT NUM (32) or END (11) ?-\n");
        scanf("%d",&ch_in);
        fflush(stdin);

        switch (ch_in){
            case 0 :
                printf ("\n INPUT  BARx (0,1,2,3...)  -");
                scanf ("%x",&tmp_barx);
                fflush(stdin);

                printf ("\n INPUT  MODE  (0-D8,1-D16,2-D32)  -");
                scanf ("%x",&tmp_mode);
                fflush(stdin);

                printf ("\n INPUT  ADDRESS (IN HEX)  -");
                scanf ("%x",&tmp_offset);
                fflush(stdin);

                printf ("\n INPUT DATA (IN HEX)  -");
                scanf ("%x",&tmp_data);
                fflush(stdin);

                l_Read.data_rw   = tmp_data;
                l_Read.offset_rw = tmp_offset;
                l_Read.mode_rw   = tmp_mode;
                l_Read.barx_rw   = tmp_barx;
                l_Read.size_rw   = 0;

                printf ("MODE - %X , OFFSET - %X, DATA - %X\n",  
                     l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);

                len = write (fd, &l_Read, sizeof(device_rw));
                if (len != itemsize ){
                        printf ("#CAN'T READ FILE return %i\n", len);
                }
                break;
            case 1 :
                printf ("\n INPUT  BARx (0,1,2,3)  -");
                scanf ("%x",&tmp_barx);
                fflush(stdin);
                printf ("\n INPUT  MODE  (0-D8,1-D16,2-D32)  -");
                scanf ("%x",&tmp_mode);
                fflush(stdin);
                printf ("\n INPUT OFFSET (IN HEX)  -");
                scanf ("%x",&tmp_offset);
                fflush(stdin);		
                l_Read.data_rw   = 0;
                l_Read.offset_rw = tmp_offset;
                l_Read.mode_rw = tmp_mode;
                l_Read.barx_rw   = tmp_barx;
                l_Read.size_rw   = 0;
                printf ("MODE - %X , OFFSET - %X, DATA - %X\n", 
                        l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);                
                len = read (fd, &l_Read, sizeof(device_rw));
                if (len != itemsize ){
		   printf ("#CAN'T READ FILE return %i\n", len);
		}
                printf ("READED : MODE - %X , OFFSET - %X, DATA - %X\n",l_Read.mode_rw, l_Read.offset_rw, l_Read.data_rw);
                break;
            case 30 :
                ioctl(fd, PCIEDEV_DRIVER_VERSION, &io_RW);
                tmp_fdata = (float)((float)io_RW.offset/10.0);
                tmp_fdata += (float)io_RW.data;
                printf ("DRIVER VERSION IS %f\n", tmp_fdata);
                break;
	    case 31 :
                ioctl(fd, PCIEDEV_FIRMWARE_VERSION, &io_RW);
                printf ("FIRMWARE VERSION IS - %X\n", io_RW.data);
		break;
            case 32 :
                ioctl(fd, PCIEDEV_PHYSICAL_SLOT, &io_RW);
                printf ("SLOT NUM IS - %X\n", io_RW.data);
		break;
            case 2 :
                offset = 0;

                printf ("\n INPUT SIG_MASK (IN HEX)  -");
                scanf ("%x",&offset);
                fflush(stdin);
                l_Ctrl.f_Status = 0;
                l_Ctrl.f_Irq_Mask = offset & 0x00FF;

                printf ("\n INPUT SIGNAL(0-SIGUSR1,1-SIGUSR2, 2-SIGURG)  -");
                scanf ("%x",&offset);
                fflush(stdin);
                if (offset == 0)
                        offset = SIG1;
                if (offset == 1)
                        offset = SIG2;
                if (offset == 2)
                        offset = URG;

                l_Ctrl.f_Irq_Sig = offset;
                printf ("\n INPUT STATUS (IN HEX)  -");
                scanf ("%x",&offset);
                fflush(stdin);
                l_Ctrl.f_Status = offset;
                code = ioctl (fd, IPTIMER_ADD_SERVER_SIGNAL, &l_Ctrl);
                if (code) 
                        {
                        printf ("#1 %d\n", code);
                        }
                printf ("f_Irq_Mask - %X , f_Irq_Sig - %X, f_Status - %X\n", 
                                            l_Ctrl.f_Irq_Mask, l_Ctrl.f_Irq_Sig, l_Ctrl.f_Status);

                break;
			case 7 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;

				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_DELAY, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("DELAY %X\n", rw_Ctrl.io_data);

				break;

			case 8 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n DELAY (HEX)  -");
				scanf ("%x",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;

				code = ioctl (fd, IPTIMER_DELAY, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;
				
			case 9 :
				printf ("\n CHANNEL (0-3)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;

				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_EVENT, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}
				printf (" CODE %i EVENT %X CHANNEL %X RW %X\n", 
				        code, rw_Ctrl.io_data, rw_Ctrl.io_chn, rw_Ctrl.io_rw);

				break;
			case 10 :
				printf ("\n CHANNEL (0-3)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n EVENT (HEX)  -");
				scanf ("%x",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;


				code = ioctl (fd, IPTIMER_EVENT, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}
                printf (" CODE %i EVENT %X CHANNEL %X RW %X\n", 
				        code, rw_Ctrl.io_data, rw_Ctrl.io_chn, rw_Ctrl.io_rw);

				break;
				
			case 12 :

				code = ioctl (fd, IPTIMER_REFRESH, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;
				
			case 13 :
				printf ("\n CHANNEL (0,1,4,5)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_DAISY_CHAIN, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("DAISY_CHAIN %X\n", rw_Ctrl.io_data);

				break;

			case 14 :
				printf ("\n CHANNEL (0,1,4,5)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n DAISY (0,1)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;

				code = ioctl (fd, IPTIMER_DAISY_CHAIN, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;
				
			case 15 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;

				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_TIME_BASE, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("EVENT %X\n", rw_Ctrl.io_data);

				break;

			case 16 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n TIME_BASE (0,1)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;

				code = ioctl (fd, IPTIMER_TIME_BASE, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;

			case 17 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;

				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_PULSE_LENGTH, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("PULSE_LENGTH %X\n", rw_Ctrl.io_data);

				break;

			case 18 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n PULSE_LENGTH (0,1)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;

				code = ioctl (fd, IPTIMER_PULSE_LENGTH, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;

			case 19 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;

				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_OUT, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("IPTIMER_OUT %X\n", rw_Ctrl.io_data);

				break;

			case 20 :
				printf ("\n CHANNEL (0-7)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_chn = offset;
				printf ("\n IPTIMER_OUT (0,1)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;


				code = ioctl (fd, IPTIMER_OUT, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;

            case 21 :
				rw_Ctrl.io_rw = 0;

				code = ioctl (fd, IPTIMER_VARCLOCK, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				printf ("IPTIMER_VARCLOCK %X\n", rw_Ctrl.io_data);

				break;

			case 22 :
				printf ("\n IPTIMER_VARCLOCK (0-3)  -");
				scanf ("%d",&offset);
				fflush(stdin);
				rw_Ctrl.io_data = offset;

				rw_Ctrl.io_rw = 1;

				code = ioctl (fd, IPTIMER_VARCLOCK, &rw_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}

				break;
				
			case 23 :
				code = ioctl (fd, IPTIMER_GET_REG, &reg_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}
				for (int k = 0; k < 24 ; k++){
					printf("\n REG - %X = %X", (k*2), reg_Ctrl.data [k]);
				}
				printf("\n");

				break; 
				
			case 24 :
				code = ioctl (fd, IPTIMER_GET_LOC_REG, &reg_Ctrl);
				if (code) 
					{
					printf ("#1 %d\n", code);
					}
				for (int k = 0; k < 24 ; k++){
					printf("\n REG - %X = %X", (k*2), reg_Ctrl.data [k]);
				}
				printf("\n");

				break;
				
			case 25 :
				rw_Ctrl.io_data = 0;
				rw_Ctrl.io_rw   = 0;

				code = ioctl (fd, IPTIMER_GET_CLCK_STS, &rw_Ctrl);
				if (code){
					printf ("#1 %d\n", code);
				}
				printf("\n CLOCK_STS - %d\n", rw_Ctrl.io_data);
				break;
				
			default:
				break;
		}
	}
	close(fd);
	return 0;
}

