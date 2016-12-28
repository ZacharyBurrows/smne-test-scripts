/*
 * itron-dynament-tool - Itron Dynament Tool
 *
 * Used for testing Itron serial connection to Dynament Gas Sensor
 *
 * For questions/support: norman.mcentire@itron.com
 *
 * 2016 April - Updates
 * 2016 March - Original
*/

//to run: toolchain/bin/armv7l-timesys-linux-uclibcgnueabi-gcc MethaneLEL.c -o methaneLEL -lpthread


#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <string.h>
#include <signal.h>

#include <pthread.h>

//
// These are the command bytes that are sent
// to Dynament Gas Sensor. The are documented in the
// Dynament "Premer Sensor Communications Protocol",
// Issue 1.24, Document TDS0045
//

//
// 1.0 Communications Protocol
// - P2P (Point-to-Point)
// - Frame based

//
// 1.1. Buad Rates
// - Default is 38400 8N1
//
#define DEFAULT_BAUD_RATE B38400
//#define DEFAULT_BAUD_RATE B9600

//
// 1.2 Control Byte Constants
//

#define CBC_DLE	0x10 //Data Link Escape

#define CBC_RD 0x13 //Read
#define CBC_WR 0x15 // Write

#define CBC_ACK 0x16
#define CBC_NACK 0x19

#define CBC_DAT 0x1a //Single Data Frame

#define CBC_EOF 0x1f //End of Frame

#define CBC_WP1 0xe5 //Write Password 1
#define CBC_WP2 0xa2 //Write Password 2

//
// 1.3 Frame Structure
//
// Field: | DLE | CMD | PAYLOAD | DLE | EOF | CHECKSUM |
// Octect:|  1  |  1  |    N    |  1  |  1  |    2     |
//
// NOTE: Checksum is 16-bit sum of DLE through EOF
//
// NOTE: CHECKSUM = | CSUM-HI | CSUM-LO |
//

//
// 1.4 Variables
//
#define VAR_CONFIG_DATA 0
#define VAR_LIVE_DATA 1
#define VAR_ZERO_SENSOR1 2
#define VAR_SPAN_SENSOR1_R1 3
#define VAR_LIVE_DATA_SIMPLE 6
#define VAR_USER_DATA 11

//
// 1.5 Reading a Variable
//
// == SEND ==
// | DLE | RD | PAYLOAD (Variable ID) | DLE | EOF | CHSUM-HI | CHSUM-LO |
//
// == RECEIVE (Success) ==
// | DLE | DAT | DATA-LEN | DATA | DLE | EOF | CSUM-HI | CSUM |
//
// == RECEIVE (Failure) ==
// | DLE | NACK | REASON |
//
#define NACK_varNotReadable 1
#define NACK_varNotWritable 2
#define NACK_outOfRange 3
#define NACK_incorrectLength 4
#define NACK_unexpectedBytes 5
#define NACK_checksumFailed 6
#define NACK_incorrectVersion 7
#define NACK_busy 8
#define NACK_invalidData 9
#define NACK_invalidState 10
#define NACK_serialError 11
//No use of 12
#define NACK_deviceFault 13

//
// 1.5.1 Read Live Data
// == SEND ==
// DLE, RD, Variable ID, DLE, EOF, CSUM-HI, CSUM-LO
static char cmd_read_live_data[] = { 0xC0, 0x57, 0x0C, 0xC0};
// == RECEIVE ==
// DLE, DAT, Data Length, Data, DLE, EOF, CSUM-HI, CSUM-LO

//
// 1.5.2 Read Live Data Simple
// == SEND ==
// DLE, RD, Variable ID, DLE, EOF, CSUM-HI, CSUM-LO
static char cmd_read_live_data_simple[] = { 0x10, 0x13, 0x06, 0x10, 0x1f, 0x0, 0x58 };
// == RECEIVE ==
// DLE, DAT, Data Length, Data, DLE, EOF, CSUM-HI, CSUM-LO

// Read Config Data
// == SEND ==
// DLE, RD, Variable ID, DLE, EOF, CSUM-HI, CSUM-LO
static char cmd_read_config_data[] = { 0x10, 0x13, 0x00, 0x10, 0x1f, 0x0, 0x52 };
// == RECEIVE ==
// DLE, DAT, Data Length, Data, DLE, EOF, CSUM-HI, CSUM-LO

//
// NOTE: From 2.0 in Tech Ref:
// 	Byte = 8 bits
// 	Integer = 2 bytes
// 	Double = 4 bytes
// 	Long = 4 bytes

// 2.1 Config Data
typedef struct {
	unsigned short Version;
	unsigned char SensorType[8];
	unsigned short ModeBits;
	unsigned short SensorFsd;
	float ZeroOffset;
	float ZeroCalTemperature;
	float SpanCalTemperature;
	float DacZero;
	float DacFsd;
	float PosZeroSuppress;
	float NegZeroSuppress;
	float CalibrationGasValue;
	float SpanOffset;
	float El;
	float Power;
	unsigned char SerialNumber[10];
	float Rounding;
	unsigned short DacPowerUp;
	unsigned short BaudRate;
	unsigned short WarmUpTime;
} ConfigData;


// 2.5 Live Data
//
// TODO Handle all versions of live data
//      This is just version 4 live data!
//
typedef struct {
	unsigned short Version;		//Version 4 ONLY!
	unsigned short StatusFlags;	//
	float ReadingFloat;
	float Temperature;
	unsigned short Det1;
	unsigned short Ref;
	float Fa;
	unsigned long Uptime;
	unsigned short DetMin;
	unsigned short DetMax;
	unsigned short RefMin;
	unsigned short RefMax;
} LiveData;

//
// These are the command-line options
//
#define CMD_READ_CONFIG_DATA 1
#define CMD_READ_LIVE_DATA 2
#define CMD_READ_LIVE_DATA_SIMPLE 3

// Must enter command
static int cmd = -1;

static const struct option options[] = {

	{ "read-config-data", no_argument, NULL, CMD_READ_CONFIG_DATA },
	{ "read-live-data", no_argument, NULL, CMD_READ_LIVE_DATA },
	{ "read-live-data-simple", no_argument, NULL, CMD_READ_LIVE_DATA_SIMPLE },

	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "query-serial-info", no_argument, NULL, 'q' },
	{ "raw", no_argument, NULL, 'r' },

	{ "device", required_argument, NULL, 'd' },
	{ "baud", required_argument, NULL, 'b' },
	//{ "command", required_argument, NULL, 'c' },
	{ 0, 0, 0, 0 }
};

//
// Command-line settings
//

static int verbose;
static char *device = "/dev/ttyUSB0";
//static char *device = "/dev/ttyO5"; //This is P8 connector
static char *command;
static int query_serial_info;

static int raw_flag; //Show raw data instead of decoded data
static int sync_flag; //Only capture one sync session

static char *baud = "38400"; //Default
static speed_t speed = DEFAULT_BAUD_RATE; //B9600; //Default

//
// Terminial Settings
//
static int device_fd;

static struct termios savedTermios;
static struct termios newTermios;

// Flag to determine if we get response
static int got_response;

int dataArray[40];

//
// Convert baud input param to speed_t
//
speed_t
baud_to_speed() {
	if (strcmp(baud, "9600") == 0) {
		return B9600;
	}
	if (strcmp(baud, "19200") == 0) {
		return B19200;
	}
	if (strcmp(baud, "38400") == 0) {
		return B38400;
	}
	// Use as the default
	return B38400;
}

//
// Convert from speed_t to string
//
char *
speed_to_string(speed_t speed_in) {
	switch (speed_in) {
		case B9600: return "9600";
		case B19200: return "19200";
		case B38400: return "38400";
	}
	return "unknown";
}

//
// Display title
//
static void
displayTitle() {
	puts("Itron/Dynament Tool");
}

//
// Display version
//
static void
displayVersion() {
	puts("Version 1.1.0");
}

//
// Display usage
//
/*static void
show_cmds() {
	puts("Available options:");
	struct option *p = options;
	while (p->name) {
		puts(p->name);
		p++;	
	}
}*/

static void
displayUsage(char *cmd) {
	displayTitle();
	displayVersion();
	//show_cmds();
	printf("Usage Examples\n");

	printf("%s\n", cmd);

	printf("%s --read-live-data\n", cmd);
	printf("%s --read-live-data --raw\n", cmd);

	printf("%s --read-config-data\n", cmd);
	printf("%s --read-config-data --raw\n", cmd);

	printf("%s --device /dev/ttyUSB0 --read-live-data\n", cmd);
	printf("%s --device /dev/ttyUSB0 --read-live-data-simple\n", cmd);

	printf("%s --help\n", cmd);
	printf("%s --version\n", cmd);

	exit(1);	
}

//
// Display termios info
//
static void
displayTermiosInfo(const struct termios *p) {
	printf("ispeed: %s\n", speed_to_string(cfgetispeed(p)));
	printf("ospeed: %s\n", speed_to_string(cfgetospeed(p)));
	printf("c_iflag: 0x%lu\n", p->c_iflag);
	printf("c_oflag: 0x%lu\n", p->c_oflag);
	printf("c_cflag: 0x%lu\n", p->c_cflag);
	printf("c_lflag: 0x%lu\n", p->c_lflag);
}

static int rx_thread_ready;
static unsigned char buf[200];
static int buf_size;
static void *rx_thread_function(void *p) {
	if (verbose) {
		puts("Starting receive thread");
		fflush(stdout);
	}

	rx_thread_ready = 1;
	int temp = 0;
	int count = 0;
	int current_byte_count = 0;
	while (1) {
		//unsigned char buf[100];
		//fprintf(stderr,"read");
		int len = read(device_fd, &buf[buf_size], 1); //sizeof(buf));
	
		got_response = 1; //Let main thread know we got a response

		//fprintf(stderr,"done");
		if (len < 0) {
			printf("read error: %i\n", len);
			exit(1);
		}
		else {
			//fprintf(stderr, "0x%02x ", buf[buf_size]);
			if (raw_flag) {

				if(sync_flag){
					//printf("0x%02x ", buf[buf_size]);
					dataArray[current_byte_count] = buf[buf_size];
					current_byte_count++;
					if (current_byte_count%8 == 1 && current_byte_count != 1) {
						//printf("\nCurrent byte count: %i\n", current_byte_count);
						//printf("\n");
					}
				}

				//printf("0x%02x \n", temp);
				if(temp == 0x0f && buf[buf_size] == 0x0c && count < 2){
					sync_flag = !sync_flag;
					count++;
					//printf("\nFLAG SET!\n");
				}

				temp = buf[buf_size];

			}
		}
		buf_size++;
		//int i;
		//for (i = 0; i < len; i++) {
	//		fprintf(stderr, "0x%02x ", buf[i]);
		//}
	}
}

int
main(int argc, char *argv[])
{

	//
	// Parse the command-line options
	//

	// Default is the ping
	char *cmd_bytes = cmd_read_live_data;
	char cmd_size = sizeof(cmd_read_live_data);;

	int opt;
	int longindex = 0;

	while ((opt = getopt_long(argc, argv, "", options, &longindex)) != -1) {
		switch (opt) {
			case 'v': verbose = 1; break;
			case 'r': raw_flag = 1; break;
			case CMD_READ_LIVE_DATA: 
				cmd = opt;
				cmd_bytes = cmd_read_live_data;
				cmd_size = sizeof(cmd_read_live_data);
				break; 
			case CMD_READ_LIVE_DATA_SIMPLE:
				cmd = opt;
				cmd_bytes = cmd_read_live_data_simple;
				cmd_size = sizeof(cmd_read_live_data_simple);
				break;
			case CMD_READ_CONFIG_DATA:
				cmd = opt;
				cmd_bytes = cmd_read_config_data;
				cmd_size = sizeof(cmd_read_config_data);
				break;

			case 'd': device = optarg; break;
			case 'V': displayVersion(); return 0;

			//case 'c': command = optarg; break;

			case 'b': baud = optarg; break;
			case 'q': query_serial_info = 1; break;
			case 'h': 
			case '?':
			default: displayUsage(argv[0]);
		}
	}

	//
	// Default is to show usage info
	//
	if (cmd == -1) {
		displayUsage(argv[0]);
	}

	//
	// We need the device 
	//
	if (!device) {
		fprintf(stderr, "Please enter device, e.g. --device /dev/ttyUSB0\n");
		displayUsage(argv[0]);
	}

/**
	// If not doing a device query or ping we need the command
	if (!query_serial_info && !cmd_ping_flag) {
		if (!command) {
			fprintf(stderr, "Please enter command, e.g. --command on\n");
			displayUsage(argv[0]);
		}

		//
		// Make sure command is valid
		//
		if (strcmp(command, "on") == 0) {
			lightCommand = on;
			lightCommandSize = sizeof(on);
		} else if (strcmp(command, "off") == 0) {
			lightCommand = off;
			lightCommandSize = sizeof(off);
		}
		else {
			fprintf(stderr, "Command not recognized\n");
			displayUsage(argv[0]);	
		}
	}
**/

	//
	// Display verbose info
	//
	if (verbose) {
		displayTitle();
		displayVersion();
	}

	//
	// Open device
	//
	if (verbose) {
		printf("Opening %s\n", device);
	}
	device_fd = open(device, O_RDWR | O_NOCTTY);
	if (device_fd < 0) {
		fprintf(stderr, "Could not open %s\n", device);
		return (1);
	}

	//
	// Make sure the device is a tty
	//
	if (isatty(device_fd) == 0) {
		fprintf(stderr, "Device %s is not a tty\n", device);
		return (1);
	}

	if (verbose) {
		printf("Device %s opened\n", device);
		printf("Query attributes\n");
	}

	//
	// Save the current termios settings
	//
	int result = tcgetattr(device_fd, &savedTermios);
	if (result != 0) {
		fprintf(stderr, "tcgetattr failed\n");
		close(device_fd);
		exit(1);
	}
	if (query_serial_info || verbose) {
		printf("Existing termios for %s\n", device);
		displayTermiosInfo(&savedTermios);
		if (query_serial_info) {
			close(device_fd);
			exit(0);
		}
	}

	//
	// Setup the new termios settings
	//
	newTermios = savedTermios;

	//
	// Setup speed for both in and out
	//
	cfsetispeed(&newTermios, speed);
	cfsetospeed(&newTermios, speed);

	// 
	// Setup INPUT character handling
	//
	// - ignore break
	// - no CR to NL translation
	// - no NL to CR translation
	// - do not mark parity errors
	// - no input parity check
	// - do not strip off high bit
	// - no XON/XOFF flow control
	newTermios.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | 
				PARMRK | INPCK | ISTRIP | IXON | IXOFF);

	//newTermios.c_iflag &= ~IXOFF; //Disable XON/XOFF Flow control
	//newTermios.c_iflag &= ~IXON; //Disable XON/XOFF Flow control

	//
	// Setup OUTPUT character handling
	//
	// - no CR to NL translation
	// - no NL to CR translation
	// - no column 0 CR suppression
	// - no Ctrl-D suppression
	// - no fill characters
	// - no case mapping
	newTermios.c_oflag = 0;

	//
	// No Ling Processing
	//
	// - echo off
	// - echo newline off
	// - cannonical mode off
	// - extended input procesing off
	// - signal chars off
	newTermios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

	//
	// Turn off character processing
	// - clear current char size mask
	// - no parity checking
	// - no output processing
	// - force 8-bit 
	// - 1 stop bit	
	newTermios.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
	newTermios.c_cflag |= CS8; 

	//newTermios.c_cflag &= ~PARENB; //No Parity
	//newTermios.c_cflag &= ~CSTOPB; //1 stop bit
	//newTermios.c_cflag |= CLOCAL; //Ignore modem control lines	

	// One input characggter is enough to return from read
	// Inter-character timer is off
	newTermios.c_cc[VMIN] = 1;
	newTermios.c_cc[VTIME] = 0;

	//
	// Set input and output speed
	//
	//
	cfsetispeed(&newTermios, speed);
	cfsetospeed(&newTermios, speed);

	//
	// Set the new settings
	//
	result = tcsetattr(device_fd, TCSANOW, &newTermios);
	if (result != 0) {
		fprintf(stderr, "tcsetattr failed\n");
	}

	if (verbose) {
		printf("New termios for %s\n", device);
		displayTermiosInfo(&newTermios);
	}

	//
	// Startup thread to handle read from serial port
	//
	pthread_t rx_thread;
	pthread_create(&rx_thread, NULL, rx_thread_function, NULL);

	// Wait for rx_thread to startup
	while (rx_thread_ready == 0) {
		sleep(1);
	}

	//
	// Send the command
	//
		
	if (verbose) {
		puts("Sending command.");
		//TODO
		//int i;
		//for (i = 0; i < sizeof(len; i++) {
		//		fprintf(stderr, "0x%02x ", buf[i]);
		//}
		fflush(stdout);
	}

	int len = write(device_fd, cmd_bytes, cmd_size);
	if (verbose) {
		//printf("Sent %d remote command bytes\nResponse is", len);
		printf("Write %i of %i cmd bytes:\n", len, cmd_size);
	}

/**
	//
	// Restore the termios settings
	//
	if (verbose) {
		printf("Setting %s back to original settings\n", device);
	}
	result = tcsetattr(fd, TCSANOW, &savedTermios);
	if (result != 0) {
		fprintf(stderr, "tcsetattr failed\n");
	}
***/


	//
	// All commands send response
	//

	// Assume the receive thread will be done in 1 second
	sleep(1);

	// Check if we got a response
	if (!got_response) {
		printf("Timeout.\nNo response from device.\n");
		printf("Check power conections.\n");
		return 1;
	}

	//if (raw_flag) printf("\n");

	// Decode bytes if requested
	if (!raw_flag) {
		
		if (buf[0] == CBC_DLE) printf("DLE (0x%x)\n", CBC_DLE); else printf("???\n");
		if (buf[1] == CBC_DAT) printf("DAT (0x%x)\n", CBC_DAT); else printf("???\n");
		printf("Data Length: 0x%x\n", buf[2]);
	
		if ((cmd == CMD_READ_LIVE_DATA) || (cmd == CMD_READ_LIVE_DATA_SIMPLE)) {	
			LiveData *live_data = &buf[3];
			printf("Version: 0x%04x\n", live_data->Version);
			printf("StatusFlags: 0x%04x\n", live_data->StatusFlags);
			printf("Gas Reading: %f\n", live_data->ReadingFloat);

			if (cmd == CMD_READ_LIVE_DATA) {
				printf("Temperature: %f\n", live_data->Temperature);
				printf("Detector Signal: 0x%04x\n", live_data->Det1);
				printf("Reference Signal: 0x%04x\n", live_data->Ref);
				printf("Absorbance: %f\n", live_data->Fa);
				printf("Uptime: 0x%04x\n", live_data->Uptime);
				printf("DetMin: 0x%04x\n", live_data->DetMin);
				printf("DetMax: 0x%04x\n", live_data->DetMax);
				printf("RefMin: 0x%04x\n", live_data->RefMin);
				printf("RefMax: 0x%04x\n", live_data->RefMax);
				if (buf[32] == CBC_DLE) printf("DLE (0x%x)\n", CBC_DLE); else printf("???\n");
				if (buf[33] == CBC_EOF) printf("EOF (0x%x)\n", CBC_EOF); else printf("???\n");
			}
		}
		else if (cmd == CMD_READ_CONFIG_DATA) {
			ConfigData *config_data = &buf[3];
			printf("Version: 0x%04x\n", config_data->Version);
			printf("Sensor Type: %c%c%c%c%c%c%c%c\n",
				config_data->SensorType[0], config_data->SensorType[1],	
				config_data->SensorType[2], config_data->SensorType[3],	
				config_data->SensorType[4], config_data->SensorType[5],	
				config_data->SensorType[6], config_data->SensorType[7]);	
			printf("ModeBits: 0x%04x\n", config_data->ModeBits);
			printf("SensorFsd: 0x%04x\n", config_data->SensorFsd);
			printf("ZeroOffset: %f\n", config_data->ZeroOffset);
			printf("ZeroCalTemperature: %f\n", config_data->ZeroCalTemperature);
			printf("SpanCalTemperature: %f\n", config_data->SpanCalTemperature);
			printf("DacZero: %f\n", config_data->DacZero);
			printf("DacFsd: %f\n", config_data->DacFsd);
			printf("PosZeroSuppress: %f\n", config_data->PosZeroSuppress);
			printf("NegZeroSuppress: %f\n", config_data->NegZeroSuppress);
			printf("CalibrationGasValue: %f\n", config_data->CalibrationGasValue);
			printf("SpanOffset: %f\n", config_data->SpanOffset);
			printf("El: %f\n", config_data->El);
			printf("Power: %f\n", config_data->Power);
			printf("SerialNumber: %c%c%c%c%c%c%c%c%c%c\n",
				config_data->SerialNumber[0], config_data->SerialNumber[1],	
				config_data->SerialNumber[2], config_data->SerialNumber[3],	
				config_data->SerialNumber[4], config_data->SerialNumber[5],	
				config_data->SerialNumber[6], config_data->SerialNumber[7],	
				config_data->SerialNumber[8], config_data->SerialNumber[9]);
			printf("Rounding: %f\n", config_data->Rounding);
			printf("DacPowerUp: 0x%04x\n", config_data->DacPowerUp);
			printf("BaudRate: 0x%04x\n", config_data->BaudRate);
			printf("WarmUpTime: 0x%04x\n", config_data->WarmUpTime);
		}
	}
	
	// Wait for rx_thread to end
	//pthread_join(rx_thread, NULL);

	close(device_fd);

	if (verbose) {
		puts("All done");
	}

	float u = 3.0/32768.0;
	int Data1 = (128 *(int)(127 & dataArray[2])) + (127 & dataArray[3]); //units of cd
	int Data4 = (4*(128*(int)(127 & dataArray[8]) + (127 & dataArray[9])));
	int Data5 = (4*(128*(int)(127 & dataArray[10]) + (127 & dataArray[11])));
	int Data16 = 128*((127 & dataArray[32])+ (127 & dataArray[33]));

	float lightInt = Data1*u;
	float celsius = (((u*Data4)/3.0*2.7/2.0-0.5)/0.01);
	float humidity = ((Data5*u)*2.7/9-0.1515)/0.00636/(1.0546-0.00216*celsius);
	float LEL = Data16;

	//printf("Temperature(C): %f\nMethane(Percent LEL): %f\n", celsius, LEL);
	printf("%f\n", LEL);

	return 0;
}

