/* NAME: NICHOLAS LINGAD
 * EMAIL: lingadnicholas@g.ucla.edu
 * ID: 605284477
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>

/*Following code is from the disc 1B slides!*/
#ifdef DUMMY
typedef int mraa_aio_context;

//AIO Dummy Declarations
mraa_aio_context mraa_aio_init(int p) {
    return 1; //return fake device handler 
}
int mraa_aio_read(mraa_aio_context c) {
    return 650; //return fake temperature value 
}
void mraa_aio_close(mraa_aio_context c) {
    return;
}


#else
#include <mraa.h>
#include <mraa/aio.h>
#endif

#define B 4275 // thermistor value
#define R0 100000.0 // nominal base value 
int FAHRENHEIT = 1;
int PERIOD = 1;
int fd = 1; 
int stop = 0;
mraa_aio_context sensor;
char* host = NULL;
char* id = NULL;
int port = -1;
struct sockaddr_in serv_addr; 
int sockfd;
SSL* ssl; 

void secure_write(char* output) {
    if (SSL_write(ssl, output, strlen(output)) < 0) {
        fprintf(stderr, "Error with SSL write\n");
        exit(2);
    }
}
//Code from 1b slides
void ssl_setup() {
    OpenSSL_add_all_algorithms();
    if (SSL_library_init() < 0) {
        fprintf(stderr, "Error initializing SSL library\n");
        exit(2);
    }
    SSL_load_error_strings();
    SSL_CTX *newContext = SSL_CTX_new(TLSv1_client_method());

    //Attach SSL to a socket: 
    ssl = SSL_new(newContext);
    if (ssl == NULL) {
        fprintf(stderr, "SSL_new error\n");
        exit(2);
    }
    if (SSL_set_fd(ssl, sockfd) < 0) {
        fprintf(stderr, "Unable to SSL_set_fd\n");
        exit(2);
    }
    if (SSL_connect(ssl) < 0) {
        fprintf(stderr, "Unable to SSL_connect\n");
        exit(2);
    }

    //Immediately send (and log) an ID terminated with newline
    char output[20]; 
    sprintf(output, "ID=%s\n", id);
    secure_write(output);
    dprintf(fd, "ID=%s\n", id);
}
//Code from 1b slides
void client_connect() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error with socket\n");
        exit(2);
    }
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Error gethostbyname\n");
        exit(2);
    }
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        fprintf(stderr, "ERROR server rejected:%s\n", strerror(errno));
        exit(2);
    }
    ssl_setup();

}
void closeall() {
    mraa_aio_close(sensor);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
    exit(0);
}

void shutdownProgram() { //Printing time code from: https://www.techiedelight.com/print-current-date-and-time-in-c/
    struct tm* local;
    time_t rawtime;
    time(&rawtime);
    local = localtime(&rawtime);
    //Time format: hh:mm:ss
    char output[50];
    sprintf(output, "%02d:%02d:%02d SHUTDOWN\n", local->tm_hour, local->tm_min, local->tm_sec);
    secure_write(output);
    if (fd != 1){
        dprintf(fd, "%02d:%02d:%02d SHUTDOWN\n", local->tm_hour, local->tm_min, local->tm_sec);
    }
    closeall(); 
}

float convertTemp(unsigned int reading) { //Code from disc 1B slides
    float R = 1023.0/((float) reading) - 1.0;
    R = R0*R;
    //Temperature in celcius
    float C = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
    if (!FAHRENHEIT)
        return C;
    else //Temperature in fahrenheit
        return (C * 9)/5 + 32; 
}

void doCommand(char* command) {
    //strcmp and strncmp are your friends
    //If logging is enabled, print to log
    if (fd != 1) {
        dprintf(fd, "%s\n", command);
    }

    if (strcmp(command, "SCALE=F") == 0) 
    {
        FAHRENHEIT = 1; 
    }
    else if (strcmp(command, "SCALE=C") == 0)
    {
        FAHRENHEIT = 0;
    }
    else if (strncmp(command, "PERIOD=", strlen("PERIOD=")) == 0)
    {
        int tempVal = atoi(command + strlen("PERIOD=")); 
        if (tempVal <= 0){
            //invalid
        }
        else {
            PERIOD = tempVal; 
        }
    }
    else if (strcmp(command, "START") == 0) 
    {
        stop = 0;
    }
    else if (strcmp(command, "STOP") == 0)
    {
        stop = 1; 
    }
    else if (strncmp(command, "LOG", strlen("LOG")) == 0) 
    {
        //Do nothing, for now 
    }
    else if (strcmp(command, "OFF") == 0) 
    {
        shutdownProgram();
    }
}


int main(int argc, char* argv[]) {
	int c;
	while (1)
	{
		static struct option long_options[] = 
		{
			{"period", required_argument, 0, 'p'},
			{"scale", required_argument, 0, 's'},
            {"id", required_argument, 0, 'i'},
            {"host", required_argument, 0, 'h'},
            {"log", required_argument, 0, 'l'},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here */ 
		int option_index = 0;
		c = getopt_long(argc, argv, "p:s:i:h:l:", long_options, &option_index);
        
		/* Detect the end of options. */
		if (c == -1)
			break; 

		switch (c) 
		{
			case 'p': ;
		    	int tempVal;
				if ((tempVal = atoi(optarg)) == 0 || tempVal < 1) {
					fprintf(stderr, "Argument for --period should be a positive integer\n");
					exit(1);
                }
                PERIOD = tempVal; 
				break;
			case 's': ;
				if (*optarg != 'C' && *optarg != 'F') {
				fprintf(stderr, "Argument for --scale should be C or F\n");
				exit(1);
				}
                if (*optarg == 'C')
                    FAHRENHEIT = 0;
				break;
            case 'i': ;
                if (strlen(optarg) != 9) {
                    fprintf(stderr, "--id length should be 9 characters.\n");
                    exit(1);
                }
                id = optarg; 
                break;
            case 'h': ;
                host = optarg; 
                break;
            case 'l': ;
                char *logfd = optarg;
                if ((fd = creat(logfd, 0666)) < 0)
                {
                    fprintf(stderr, "ERROR creating log file:%s\n", strerror(errno));
                }
                break;
			case '?':
				fprintf(stderr, 
				"Correct usage: './lab4b [OPTIONS] [PORT NUMBER]' options: --scale=[C/F] --period=# MANDATORY: --log=[filename] --id=[9 digit number] --host=[name or address]\n"); 
				exit(1);
				break;
			default:
				exit(1);
		}
        
    }

    //optind: https://linux.die.net/man/3/optind
    if (optind < argc) {
		int tempVal1;
		if ((tempVal1 = atoi(argv[optind])) == 0 || tempVal1 < 0) {
			fprintf(stderr, "Portnum should not be negative\n");
			exit(1);
        }
        port = tempVal1;
    }
    if (fd == 1 || host == NULL || port == -1 || id == NULL ) {
        fprintf(stderr, "Options --log=[filename] --id=[9 digit number] --host=[name or address] portnum are MANDATORY\n");
        exit(1);
    }
    client_connect(); //Connect to server
    //Initialize sensor.
   
    sensor = mraa_aio_init(1); 
    if (sensor == NULL) {
        fprintf(stderr, "ERROR sensor is NULL\n");
        exit(1);

    }

    //Set up poll
    char buf[256];
    char com[256];
    struct pollfd pollInput;
    pollInput.fd = sockfd;
    pollInput.events = POLLIN | POLLHUP | POLLERR;
    time_t lastLogTime;
    int first = 1;
    int index = 0; //Used bc we don't always read in all at once.
    while (1) {
        /* If it is time to report temperature and !stop 
         * read from temperature sensor (mraa read), convert and report
         */
        //Compare current time with last log time, if greater than period (in seconds), then log! 
        //difftime code: https://www.tutorialspoint.com/c_standard_library/c_function_difftime.htm
        time_t curTime;
        time(&curTime); 
        if ((first == 1 && !stop) || ((int)difftime(curTime, lastLogTime) >= PERIOD && !stop))
        {
            first = 0;  //I KNOW THIS IS INEFFICIENT IM SORRY SDKFJGHJKSFGHKJS
            lastLogTime = curTime; 
            struct tm* local;
            time_t rawtime;
            time(&rawtime);
            local = localtime(&rawtime);
            float temp = convertTemp(mraa_aio_read(sensor));
            char output[50];
            sprintf(output, "%02d:%02d:%02d %.1f\n", local->tm_hour, local->tm_min, local->tm_sec, temp);
            secure_write(output);
            if (fd != 1) {
                dprintf(fd, "%02d:%02d:%02d %.1f\n", local->tm_hour, local->tm_min, local->tm_sec, temp);
            }
        }

        //Read command from stdin until encountering \n
        
        int ret;
        if ((ret = poll(&pollInput, 1, 0)) < 0){
            fprintf(stderr, "Error with poll:%s\n", strerror(errno)); 
            exit(1);
        }
    
        if (pollInput.revents && POLLIN) {
            //Read input from STDIN
            ret = SSL_read(ssl, buf, 256);
            if (ret < 0) {
                fprintf(stderr, "Read error\n");
                exit(1);
            }
            int i;
            for (i = 0; i < ret; i++)
            {
                if (buf[i] == '\n')
                {
                    com[index] = '\0'; //Set to null byte (so we dont need to clear everyyy time)
                    index = 0; 
                    doCommand(com);
                }
                else {
                    com[index] = buf[i];
                    index++;
                }
            }
        }

        if (pollInput.revents & (POLLHUP | POLLERR)) {
            fprintf(stderr, "Error with poll of stdin\n");
            exit(2);
        }
        
    }
    closeall();

}
