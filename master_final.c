/**
 * This is C application for master I2C device. It should be run with command line arguments, as follows:
 * first argument:  0 -> read from slave, 1 -> write to slave, 2 -> get slave device address
 * second argument: slave register address in range from 0 to 64; Non existing if first argument is 2
 * third argument:  lenght of registers read from slave or written to slave; Non existing if first argument is 2
 */ 
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SLAVEID			0x44    /* ID of a slave device */
#define WHO_AM_I        0xff    /* ID of a register for disabling slave device */
#define READ_OFFSET     17      /* offset for correct reading txBuffer in bsc_xfer */

int32_t write_registers(int32_t, uint8_t, uint8_t, uint8_t* );
void read_registers(int32_t, uint8_t, uint8_t, uint8_t*);
uint8_t rx_buffer[512];         /* size 512 is the same size as buffers in xfer struct, this is required to have the same offset after read from the slave */
uint8_t tx_buffer[512] = {0};
	
int32_t main(int argc, char *argv[])
{
	int32_t fd;
    uint8_t addr;
    uint8_t len;
    int8_t action;

    // Check if number of command line arguments is correct
    if (argc < 2 || argc > 4) {
        printf("Error! Wrong number of command line arguments for application %s.\n",argv[0]);
        return -1;
    }

	// Try to open I2C device
	fd = open("/dev/i2c-1", O_RDWR);

	// Check for any errors
	if (fd < 0) {
		printf("Error while trying to open i2c device.\n");
        return -1;
	}

    action = atoi(argv[1]);

    switch (action) {
        case 0:
            addr = atoi(argv[2]);   /* slave register address */
            len = atoi(argv[3]);    /* lenght of registers read from slave */
            if (addr < 0 || addr > 63) {
                printf("Error! Wrong address span. Correct address span is from 0 to 64.\n");
                close(fd);
                return -1;
            }
            if (addr + len > 63) {
                printf("Warning! Lenght selected for reading is to big. %d elements will be read from registers from address 0x%x\n",64-addr,addr);
            }
            read_registers(fd, addr, len, rx_buffer);
            sleep(1);
            break;
        case 1:
            addr = atoi(argv[2]);   /* slave register address */
            len = atoi(argv[3]);    /* lenght of registers read from slave */
            if (addr < 0 || addr > 63) {
                printf("Error! Wrong address span. Correct address span is from 0 to 64.\n");
                close(fd);
                return -1;
            }
            if (addr + len > 63) {
                printf("Warning! Lenght selected for writing is to big. %d elements will be written to registers from address 0x%x\n",64-addr,addr);
            }
            printf("Enter hex data for writing to slave...\n");
            for (uint8_t i; i < len; i++) {
                scanf("%x",&tx_buffer[i]);
            }
            write_registers(fd, addr, len, tx_buffer);
            sleep(1);            
            break;
        case 2:
            read_registers(fd, WHO_AM_I,1,rx_buffer);
            sleep(1);
            break;
        default:
            printf("Error! Wrong second command line argument.\nCorrect command line arguments are: 0 -> reading from slave 1 -> writing to slave, 2 -> disable slave device\n");
            close(fd);
            return -1;
    }

	close(fd);
	
	return 0;
}

int32_t write_registers(int32_t fd, uint8_t reg_adr, uint8_t len, uint8_t* tx_buffer ) {

    uint8_t msg_status[] = {reg_adr,0,len}; /* array containing register addres, read/write from/to i2c slave flag, lenght of data to read/write from i2c slave */
    
    struct i2c_msg iomsgs[] = {
		[0] = {
			.addr = SLAVEID,	/* slave address */
			.flags = 0,		    /* write access */
			.buf = msg_status,	/* register address */
			.len = 3            /* lenght of control array msg_status */
		},
		[1] = {
			.addr = SLAVEID,	/* slave address */
			.flags = 0,		    /* write access */
			.buf = tx_buffer,	/* write register */
			.len = len          /* lenght of a message */
		}
	};

    struct i2c_rdwr_ioctl_data msgset[] = {
		[0] = {
        .msgs = iomsgs,	
		.nmsgs = 2
        }			
	};

    return ioctl(fd, I2C_RDWR, &msgset);

}

void read_registers(int32_t fd, uint8_t reg_adr, uint8_t len, uint8_t* rx_buffer) {

    uint8_t msg_status[] = {reg_adr,1,len}; /* array containing register addres, read/write from/to i2c slave flag, lenght of data to read/write from i2c slave */

    struct i2c_msg iomsgs[] = {
		[0] = {
			.addr = SLAVEID,	        /* slave address */
			.flags = 0,		            /* write access */
			.buf = msg_status,	        /* buffer containing message controls */
			.len = 3                    /* lenght of control array msg_status */
		},
		[1] = {
			.addr = SLAVEID,	        /* slave address */
			.flags = I2C_M_RD,		    /* read access */
			.buf = rx_buffer,	        /* read buffer */
			.len = len + READ_OFFSET    /* lenght of a message + offset of 17 bytes */
		}
	};
	
    struct i2c_rdwr_ioctl_data msgset[] = {
		[0] = {
        .msgs = iomsgs,	
		.nmsgs = 2
        }			
	};

    ioctl(fd, I2C_RDWR, &msgset);
    printf("Bytes read:\n",len);
    printf("[addr] val\n");
    for(int i = 0; i < len; i++) {
        printf("[0x%x]  0x%x\n",reg_adr + i,rx_buffer[i + READ_OFFSET]);
    }
	printf("\n");

}