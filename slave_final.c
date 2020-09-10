/**
 * This is i2c slave application implemented usig pigpio library. To ensure communication special type of master read/write commands are issued.
 * First message is write to the slave and it carries: register address, next action read from the slave (1) or write to the slave (0), lenght in bytes to read/write.
 * Second message performs action announced by the first message. If the action was write to the slave, special fleg first_read is set to 0 to ensure correct response
 * by the slave.
 * 
 * This slave device has 64 bytes of memory that is readable and writable from a master
 * 
 * To disable slave device, run this application with command line arguments
 * 
 * compile program using: gcc -Wall -pthread -o slave_final slave_final.c -lpigpio -lrt
 * 
 */
#include <pigpio.h>
#include <stdio.h>
#include <stdint.h>

#define RPI_SLAVE_I2C_ADDRESS 0x44
#define WHO_AM_I    0xff    /* This value is used to send slave i2c address */

void runSlave();
void closeSlave();
int getControlBits(int, char);

bsc_xfer_t xfer;            /* Struct to control data flow */
uint8_t slave_registers[64] = {0};
uint8_t next_command = 0;   /* Next command, read from slave (1), write to slave (0) */
uint8_t reg_adr = 0;        /* Register address */
uint8_t first_read = 1;     /* To ensure correct read */
uint8_t reg_length = 0;     /* Special */ 


int main(int argc, char *argv[]) {
    
    if (argc > 1) {
        closeSlave();
        return 0;
    }
    runSlave();
    return 0;
}

/**
 * Function that registers slave device on i2c bus and waits for communication
*/
void runSlave() {
    if(gpioInitialise() > 0) {
        
        printf("Initialized GPIOs\n");
        xfer.control = getControlBits(RPI_SLAVE_I2C_ADDRESS, 0);    /* Close previous slaves on this address */
        bscXfer(&xfer);
        
        xfer.control = getControlBits(RPI_SLAVE_I2C_ADDRESS, 1);
        int status = bscXfer(&xfer);                    /* After this, I2C slave is visible from master*/
    
        if (status >= 0) {
            printf("Slave opened\n");
            xfer.rxCnt = 0;                             /* Reset read count number */
            while(1) {
                
                bscXfer(&xfer);                         /* Refresh struct */
                if(xfer.rxCnt > 0) {                    /* Check if there are any data in rxBuffer */   
                    memset(xfer.txBuf,0,BSC_FIFO_SIZE); /* Reset tx buffer */
                    if(first_read > 0) {                /* From master, first command is write to the slave, next command can be read or write, this ensures correct behaveour */
                 
                        reg_adr = xfer.rxBuf[0];
                        next_command = xfer.rxBuf[1];     
                        reg_length = xfer.rxBuf[2];     
                        first_read = 0;                 /* Make sure that next write will write to registers */
                        if(next_command) {
                            printf("Copying data from slave to master...\n");
                            if(reg_adr == WHO_AM_I) {
                                xfer.txBuf[0] = RPI_SLAVE_I2C_ADDRESS;  /* Set address of this device*/
                                xfer.txCnt = 1;                         /* write lenght to txCnt */
                            }
                            else {
                                memcpy(xfer.txBuf,&slave_registers[reg_adr],reg_length);    /* Copy data from registers to tx buffer */
                                xfer.txCnt = reg_length;                                    /* write lenght to txCnt */
                                printf("Reg address: 0x%x , reg. length %d\n",reg_adr,reg_length);
                                for (int i =0; i < reg_length; i++)
                                    printf("[0x%x] value = 0x%x, tx value = 0x%x\n",reg_adr+i,slave_registers[reg_adr+i],xfer.txBuf[i]);
                            }
                            printf("\n");
                            first_read = 1;             /* Wait for next write command */
                        }
                    }else { // After register read, we are supposed to read from the register or write to the registers
                        printf("Writing to 0x%x register in lenght of %d bytes...:\n",reg_adr,reg_length);
                        memcpy(&slave_registers[reg_adr],xfer.rxBuf,reg_length);    /* write data to slave registers */
                        first_read = 1;                                             /* Wait for next write command */
                    }
                }
            }
        }else
        printf("Failed to open slave!!!\n");
    }
    else 
        printf("Unsucessful GPIO initialization!\n");
}
/**
 * Function that closes slave device on i2c bus
*/
void closeSlave() {
    if(gpioInitialise()>0){
        printf("Initialized GPIOs\n");

        xfer.control = getControlBits(RPI_SLAVE_I2C_ADDRESS, 0);
        bscXfer(&xfer);
        printf("Closed slave.\n");

        gpioTerminate();
        printf("Terminated GPIOs.\n");
    }
}

/**
 * Function that sets communication control bits.
*/
int getControlBits(int address /* max 127 */, char open) {
    /*
    Excerpt from http://abyz.me.uk/rpi/pigpio/cif.html#bscXfer regarding the control bits:

    22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
    a  a  a  a  a  a  a  -  -  IT HC TF IR RE TE BK EC ES PL PH I2 SP EN

    Bits 0-13 are copied unchanged to the BSC CR register. See pages 163-165 of the Broadcom 
    peripherals document for full details. 

    aaaaaaa defines the I2C slave address (only relevant in I2C mode)
    IT  invert transmit status flags
    HC  enable host control
    TF  enable test FIFO
    IR  invert receive status flags
    RE  enable receive
    TE  enable transmit
    BK  abort operation and clear FIFOs
    EC  send control register as first I2C byte
    ES  send status register as first I2C byte
    PL  set SPI polarity high
    PH  set SPI phase high
    I2  enable I2C mode
    SP  enable SPI mode
    EN  enable BSC peripheral
    */

    // Flags like this: 0b/*IT:*/0/*HC:*/0/*TF:*/0/*IR:*/0/*RE:*/0/*TE:*/0/*BK:*/0/*EC:*/0/*ES:*/0/*PL:*/0/*PH:*/0/*I2:*/0/*SP:*/0/*EN:*/0;

    int flags = 0;
    if(open)
        flags = /*RE:*/ (1 << 9) | /*TE:*/ (1 << 8) | /*I2:*/ (1 << 2) | /*EN:*/ (1 << 0);
    else // Close/Abort
        flags = /*BK:*/ (1 << 7) | /*I2:*/ (0 << 2) | /*EN:*/ (0 << 0);

    return (address << 16 /*= to the start of significant bits*/) | flags;
}
