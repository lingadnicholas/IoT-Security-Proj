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

#ifdef DUMMY
#define MRAA_GPIO_IN 0
#define MRAA_GPIO_EDGE_RISING 0
typedef int mraa_aio_context;
typedef int mraa_gpio_context;

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

//GPIO Dummy Functions
mraa_gpio_context mraa_gpio_init(int p) {
    return 1; // return fake device handler
}
void mraa_gpio_dir(mraa_gpio_context c, int d) {
    return; //empty input/out register
}
int mraa_gpio_read(mraa_gpio_context c) {
    return 0; //return fake button input 
}
void mraa_gpio_close(mraa_gpio_context c) {
    return; 
}
void mraa_gpio_isr(mraa_gpio_context c, int mode, void(*p)(void*), void*args) {
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
mraa_gpio_context button; 
mraa_aio_context sensor;

void closeall() {
    mraa_gpio_close(button);
    mraa_aio_close(sensor);
    close(fd);
    exit(0);
}

void shutdown() { //Printing time code from: https://www.techiedelight.com/print-current-date-and-time-in-c/
    struct tm* local;
    time_t rawtime;
    time(&rawtime);
    local = localtime(&rawtime);
    //Time format: hh:mm:ss
    printf("%02d:%02d:%02d SHUTDOWN\n", local->tm_hour, local->tm_min, local->tm_sec);
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
        shutdown();
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
            {"log", required_argument, 0, 'l'},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here */ 
		int option_index = 0;
		c = getopt_long(argc, argv, "p:s:", long_options, &option_index);
        
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
            case 'l': ;
                char *logfd = optarg;
                if ((fd = creat(logfd, 0666)) < 0)
                {
                    fprintf(stderr, "ERROR creating log file:%s\n", strerror(errno));
                }
                break;
			case '?':
				fprintf(stderr, 
				"Correct usage: './lab4b [OPTIONS]' options: --scale=[C/F] --period=# --log=[filename]\n"); 
				exit(1);
				break;
			default:
				exit(1);
		}
        
    }
    //Initialize the button and sensor.
   
    button = mraa_gpio_init(60); //TODO: IF NOT USABLE, USE GPIO_115 ADDR 73
    if (button == NULL) {
        fprintf(stderr, "ERROR button is NULL\n");
        exit(1);
    }
    //On button press, we will print a final sample, and shutdown
    //So button is IN
    mraa_gpio_dir(button, MRAA_GPIO_IN);
    
   
    sensor = mraa_aio_init(1); 
    if (sensor == NULL) {
        fprintf(stderr, "ERROR sensor is NULL\n");
        exit(1);

    }

    //Set up poll
    char buf[256];
    char com[256];
    struct pollfd pollInput;
    pollInput.fd = 0;
    pollInput.events = POLLIN | POLLHUP | POLLERR;
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &shutdown, NULL);
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
            printf("%02d:%02d:%02d %.1f\n", local->tm_hour, local->tm_min, local->tm_sec, temp);
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
            ret = read(0, buf, 256);
            if (ret < 0) {
                fprintf(stderr, "Read error:%s\n", strerror(errno));
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
            exit(1);
        }
        
    }
    closeall();

}
