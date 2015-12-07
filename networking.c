#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <memory.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


/*===============
 MACROS
 ===============*/
// for response/command codes
#define HELLO ((uint8_t) 0)
#define SET_STATION ((uint8_t) 1)

#define WELCOME ((uint8_t) 0)
#define ANNOUNCE ((uint8_t) 1)
#define INVALID ((uint8_t) 2)

//to get the max of two numbers
#define MAX(a, b) (a > b ? a : b)

// for maximum Buffer size
#define BUFSIZE 1024


/*======================
 PRIMARY FUNCTIONS
 =======================*/
// Send a "HELLO" message to the server through TCP
void send_hello(int tcp_socket, int udpport);

// Send a SET_STATION command to the server through TCP
void send_set_station(int tcp_socket, int station);

// Read from UDP socket and echo upto BUFSIZE-1 characters back to STDOUT
void read_and_echo(int udp_socket);

/*======================
 HELPER/SETUP FUNCTIONS
 =======================*/
// Set up and bind the UDP Socket
void init_udp_socket(struct addrinfo udp_hints, struct addrinfo *result, char *udpport, int udp_socket);

// Initialize Connect the TCP Port
void init_tcp_port(struct addrinfo tcp_hints, char *hostname, char *serverport, struct addrinfo *result, int tcp_socket);

// Handle input from the user
int handle_input(char *buf, int tcp_socket, int channels);

// Handle WELCOME message
int handle_welcome(int tcp_socket, int channels);

// Handle ANNOUNCE message
void handle_announce(int tcp_socket);

// Handle INVALID command
void handle_invalid_comm(int tcp_socket);

//-----------------------------------------------------------------------------------//
// This is where most of the logic comes into play and a majority of the functions are called
int main(int argc, char **argv) {
    if(argc != 4) {
        fprintf(stderr, "Usage: ./client <hostname> <serverport> <udpport>\n");
        exit(1);
    }
    
    //Set up two sockets
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    //Set up UDP socket
    struct addrinfo udp_hints;
    struct addrinfo *result = NULL;
    init_udp_socket(udp_hints, result, argv[3], udp_socket);
    
    //Set up TCP port
    struct addrinfo tcp_hints;
    init_tcp_port(tcp_hints, argv[1], argv[2], result, tcp_socket);
    
    //Set up a file descriptor set for the select() loop
    fd_set sockets;
    
    //Num_fds needs to be one more than the maximum file number
    int num_fds = MAX(tcp_socket,MAX(STDIN_FILENO, udp_socket)) +1;
    
    //Indicates whether the WELCOME response has been recieved yet
    int station_ready = 0;
    
    //Start the connection by sending a hello
    send_hello(tcp_socket, atoi(argv[3]));
    
    // station count
    int channels = 0;
    
    //The select() loop
    while(1) {
        //Set up the fd_set
        FD_ZERO(&sockets);
        FD_SET(tcp_socket, &sockets);
        FD_SET(udp_socket, &sockets);
        FD_SET(STDIN_FILENO, &sockets);
        // check select
        if(select(num_fds, &sockets, NULL, NULL, NULL) == -1) {
            perror("select"); //if error, break
            break;
        }
        //If TCP socket recieved something
        if(FD_ISSET(tcp_socket, &sockets)) {
            uint8_t reply_type = 0;
            if((read(tcp_socket, &reply_type, sizeof(uint8_t))) < 0) {
                perror("read");
                exit(1);
            }
            
            //if WELCOME
            if(reply_type == WELCOME) {
                //handle WELCOME
                channels = handle_welcome(tcp_socket, channels);
                fprintf(stderr, "There are %d stations (0-%d).\n", channels, channels-1);
                station_ready = 1;
                
                //if ANNOUNCE
            } else if(reply_type == ANNOUNCE) {
                //handle ANNOUNCE
                handle_announce(tcp_socket);
                
                //if we got an INVALID COMMAND message
            } else if(reply_type == INVALID) {
                // handle invalid command
                handle_invalid_comm(tcp_socket);
                //An invalid command message means the connection was closed, so break
                break;
            }
        }
        
        //if the UDP socket is ready (it will be nearly all the time)
        if(FD_ISSET(udp_socket, &sockets)) {
            //read and echo for each iteration
            read_and_echo(udp_socket);
        }
        
        //If the user entered something
        if(FD_ISSET(STDIN_FILENO, &sockets) && station_ready) {
            //load whatever the user typed into a buffer
            char buf[BUFSIZE];
            int bytes_read;
            if((bytes_read = read(STDIN_FILENO, buf, BUFSIZE)) < 0) {
                perror("read");
                exit(1);
            }
            // account for what the user input could be
            if(bytes_read == 0) break;   //control-D
            buf[bytes_read] = 0;   //null-terminate
            
            //then handle user input
            if(handle_input(buf, tcp_socket, channels)) break;
        }
    }
    
    //Close both the file descriptors before exiting
    close(tcp_socket);
    close(udp_socket);
    return 0;
}


/*
 Given the UDP hints, the results pointer, the UDP port, and the socket, this function
 sets up everything we need to bind the UDP and makes a connection
 
 Returns: nothing
 */
void init_udp_socket(struct addrinfo udp_hints, struct addrinfo *result, char *udpport, int udp_socket){
    udp_hints.ai_family = AF_UNSPEC;
    udp_hints.ai_socktype = SOCK_DGRAM;
    udp_hints.ai_flags = AI_PASSIVE;
    udp_hints.ai_protocol = 0;
    
    int err;
    
    if((err = getaddrinfo(NULL, udpport, &udp_hints, &result)) != 0) {
        perror("getaddrinfo");\
        printf("%d\n", err);
        exit(1);
    }
    
    struct addrinfo *r;
    
    for(r = result; r != NULL; r = r->ai_next) {
        if(bind(udp_socket, r->ai_addr, r->ai_addrlen) != -1) {
            break; //sucessful connection
        }
        freeaddrinfo(r);
    }
}



/*
 Given the TCP hints, the host name, the server port, the pointer to the results and the tcp socket, this function
 sets up and connects the TCP port.
 
 Returns: Nothing
 */
void init_tcp_port(struct addrinfo tcp_hints, char *hostname, char *serverport, struct addrinfo *result, int tcp_socket){
    
    tcp_hints.ai_family = AF_UNSPEC;
    tcp_hints.ai_socktype = SOCK_STREAM;
    tcp_hints.ai_flags = 0;
    tcp_hints.ai_protocol = 0;
    
    if(getaddrinfo(hostname, serverport, &tcp_hints, &result) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    
    struct addrinfo *r;
    //Loop through res of getaddrinfo
    for(r = result; r != NULL; r = r->ai_next) {
        if(connect(tcp_socket, r->ai_addr, r->ai_addrlen) != -1) {
            break; //sucessful connection
        }
        freeaddrinfo(r);
    }
}

/*
 Given the TCP socket and the UDP port, this function sends
 a "HELLO" message to server through TCP.
 A HELLO message contains 3 bytes.
 The first is a command indicator, and the second two specify a UDP port.
 
 Returns: nothing
 */
void send_hello(int tcp_socket, int udpport) {
    uint8_t command = HELLO;
    uint16_t port_n = htons(udpport); //The port number is assumed to be a host int
    
    // write the command
    if(write(tcp_socket, &command, sizeof(uint8_t)) < 0) {
        perror("write");
        exit(1);
    }
    //then write port number
    if(write(tcp_socket, &port_n, sizeof(uint16_t)) < 0) {
        perror("write");
        exit(1);
    }
}


/*
 Sends a SET_STATION command to the server through TCP for the selected station
 A Set Station command contains 3 bytes.
 The first indicates the command, and the second two specify the station.
 
 Returns: nothing
 */
void send_set_station(int tcp_socket, int station) {
    uint8_t command = SET_STATION;
    uint16_t station_n = htons(station);
    
    //write the command
    if(write(tcp_socket, &command, sizeof(uint8_t)) < 0) {
        perror("write");
        exit(1);
    }
    
    //then write the station number
    if(write(tcp_socket, &station_n, sizeof(uint16_t)) < 0) {
        perror("write");
        exit(1);
    }
}

/*
 Given the input stored in buf, the tcp_socket number and the numebr of channels, this functio properly handles input from the user, accounting for white space, whether the user wants to quit the program, and if the command is valid.
 
 Returns 1 if the program should exit, 0 otherwise
 */
int handle_input(char *buf, int tcp_socket, int channels) {
    //Ignore leading spaces by advancing the pointer
    int x = 0;
    while(isspace(buf[x]) && buf[x] != '\n'){
        x++;
        buf = &buf[x];
    }
    
    //If the line was just a newline character, do nothing.
    if(buf[0] == '\n' || buf[0] == '\0') return 0;
    
    //A valid command is a single sequence of non-space characters w/some quantity of whitespace and then a newline
    int y = 0;
    while(isalnum(buf[y])) y++; {
        //skip the word part and fill the rest with null characters
        while(buf[y] != '\n' && buf[y] != 0) {
            if(isalnum(buf[y])) {
                fprintf(stderr, "Invalid command.\n");
                return 0;
            }
            buf[y++] = 0;
        }
    }
    
    // 'q' or 'quit' will indicate that the program should quit (sent to server program)
    if(buf[0] == 'q' && (buf[1] == '\0' || buf[1] == '\n')) {
        return 1;
    } else if(buf[0] == 'q' && buf[1] == 'u' && buf[2] == 'i' && buf[3] == 't' && (buf[4] == '\0' || buf[4] == '\n')) {
        return 1;
        
        //Otherwise, assume it's an integer
    } else {
        int n = 0;
        int invalid = 0;
        //Increment invalid for every non-digit character. If it stays zero, the input was a valid integer.
        while(buf[n] != '\0' && buf[n] != '\n') {
            invalid += !isdigit(buf[n++]);
        }
        
        if(invalid || buf[0] == '\0') {
            fprintf(stderr, "Invalid command.\n");
            return 0;
        } else {
            //Make the buffer into integer and send command accordingly
            int station = atoi(buf);
            if(station < channels) {
                send_set_station(tcp_socket, station);
            } else  {
                //unless the station number was too high, then don't send it
                fprintf(stderr, "Invalid station.\n");
            }
        }
    }
    return 0;
}

/*
 Given the TCP socket and the number of channels, when the WELCOME type is received
 this function is called to notify of connection and retrieve the up to date count of
 the streaming channels.
 
 Returns: a properly converted count of the channels
 */
int handle_welcome(int tcp_socket, int channels){
    //indicate the station is ready
    fprintf(stderr, "Connected to server\n");
    
    //get the number of channels
    if(read(tcp_socket, &channels, sizeof(uint16_t)) < 0) {
        perror("read");
        exit(1);
    }
    
    //convert to a host short
    return ntohs(channels);
}

/*
 Given the TCP socket, this function is called when an ANNOUNCE message is received. This
 function prints the messages to standard out.
 
 Returns: nothing
 */
void handle_announce(int tcp_socket){
    //the first byte is its length
    uint8_t len;
    if(read(tcp_socket, &len, sizeof(uint8_t)) < 0) {
        perror("read");
        exit(1);
    }
    
    //buffer for the message to be read into
    char message[len];
    if(read(tcp_socket, message, len) < 0) {
        perror("read");
        exit(1);
    }
    //null-terminate it
    message[len] = 0;
    
    //and print it out to stderr
    fprintf(stderr, "Now playing: %s\n", message);
}

/*
 Given the TCP socket, this function is called upon when and Invalid command message is returned and so
 this prints out the message that is being sent in response to some invalid command.
 
 Returns: nothing
 */
void handle_invalid_comm(int tcp_socket){
    //read in the length of the message
    uint16_t len;
    if(read(tcp_socket, &len, sizeof(uint16_t)) < 0) {
        perror("read");
        exit(1);
    }
    //convert from network short,
    len = ntohs(len);
    //make a buffer,
    char message[len];
    
    //read the message into the buffer,
    if(read(tcp_socket, message, len) < 0) {
        perror("read");
        exit(1);
    }
    //null-terminate it,
    message[len] = 0;
    
    //and print it out to stderr
    fprintf(stderr, "%s\n", message);
}

/*
 Given the UDP Socket, this function reads from it and then writes what ever was streamed to
 the socket to STDOUT. Literally doing exactly what it's name implies.
 
 Returns: nothing
 */
void read_and_echo(int udp_socket) {
    char buf[4096];
    int bytes_read;
    if((bytes_read = read(udp_socket, buf, BUFSIZE)) < 0) {
        perror("read");
        exit(1);
    }
    write(STDOUT_FILENO, buf, bytes_read);
}
