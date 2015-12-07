# CS033-Networking Lab
// Personal implementation of networking lab that as of Fall '15, is a new introduction to CS033.  

OVERVIEW:
For this assignment, I implemented a client for an internet radio station server. The radio station server (provided), henceforth dubbed the music server, maintains several radio stations, each of which loop a single song continuously. After starting, the music server streams information about a station and the mp3 encoding of the song being played on that station to each client that connects to it.

A music client communicates with the radio station server using two ports and two different protocols.
One port communicates using TCP, handling control data for the server. 
The other port communicates using UDP, receiving song data from the server.

THE SERVER (provided by TA staff):
The source code for the music server is found in server_src.  Compile the main.c file to get an executable.  The current Makefile just has the executable being called main.o.  You may call it whatever you want, but be sure to have the server running before you run the client. 
Run the server with the following arguments:
./main <port> <file 1 [file 2 [file 3...]]]>

The files will each correspond to a single station on which the file will be streamed until the server is stopped.

THE CLIENT:
The client manages input and output from the two ports passed to it, as well as from stdin, using a select() event loop.
To compile the file, just type make into the command line within the directory containing the networking.c file. 
You will then have a client.o executable. This executable takes three arguments:

./client <hostname> <serverport> <udpport>

a. hostname is the name of the machine that is running the music server.If you are running the
server on the same machine as you are running the client, you can use localhost as your host
name. 
b.serverport is the ports used to connect to the server 
c. udpport is the port used by the server to send my client data
Choose any ports greater than 1023 (as many of the lower numbered ones are reserved.  Also, serverport should match the port given to the server)

INTERACTING WITH THE SERVER:
This will take two terminal windows.
Step 1. Run the music server with at least 1 station in one of your terminals (see the Files directory for a sample of potential files.  Some are text and some are mp3 files)
Step 2. Run the client executable with proper hostname and ports on the other terminal.  
Step 3. Once you are connected, you wil see a message detailing how many stations there are.  Feel free to select any within range. 
        If you select a station out of range, nothing will happen. 
Step 4. To quit the client, just hit ctrl-d
Step 5. To quit the server, enter either 'q' or 'quit' or ctrl-d

You can give the server a single mp3 file, a single text file or a directory of these type of files. 
If you give the server a text file, the contents should be seen in the client window being streamed to STDOUT. 
You can also try piping the output of your music client to an music playing program. My computer has mpg123 so this is what I would type in if I wanted to actually hear a specific station:
./client hostname serverport udpport | mpg123 -


