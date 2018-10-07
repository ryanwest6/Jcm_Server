/*
 * This executable runs a server that takes requests via sockets to run
 * other programs.
 *
 * Author: Ryan West
 */

#ifndef JCM_SERVER
#define JCM_SERVER

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <signal.h>
#include <cstdio> //for redirecting buffer stream

#include <stdarg.h> //for printf redirection and va_arg

#include <pthread.h>
#include <vector>
#include <sstream>  // for istringstream
#include <iostream>  // for cout

#include <string.h>
#include "CppUtils.h"

#define DEFAULT_PORT "3490"  //the default port to connect to
#define BACKLOG 10     //max number of connections at once
#define MAXDATASIZE 1024 //max number of bytes we can send at oncee

#define LOG_FILE "jcm.log"


//Packet headers that define the packet type
#define PACKET_TYPE_TEXT 0x1
#define PACKET_TYPE_BINARY 0x2

class  JCMServer {

public:

   JCMServer();
   ~JCMServer();

   //Starts the JCM Server.
   int start();

   //These need to be public so the static stub functions can access them.
   //Better if they were private.
   //Runs the sender thread, which sends whatever data is placed in sendDataPtr.
   void * senderThread();
   //Runs the listener thread, which receives commands from the client, runs
   //them, and puts the result in sendDataPtr (along with header type/length).
   void * listenerThread();

   //FUNCTIONS
private:

   void sendIfReady();

   //prints to both the console screen AND the log file
   void print(const char * fmt, ...);

   //top method that interprets all commands,
   void interpretCommand(string command);
   //interprets all commands associated with reading a register or frame
   void interpretReadCommand(vector<string> c);

   //interprets all commands associated with reading the XADC values
   void interpretXadcCommand(vector<string> c);

   //interprets all commands associated with reading frames
   void readInFramesFromDevice(vector<string> c);

   //interprets all commands associated with reading from the bscan
   //Read is true if reading and false if writing
   void interpretBscanCommand(vector<string> c, bool read);

   //interprets all commands associated with writing a register or frame
   void interpretWriteCommand(vector<string> c);

   //interprets all operation commands
   void interpretOperationCommand(vector<string> c);

   //interprets option commands such as changing jtag to high-Z
   void interpretOptionsCommand(vector<string> c);

   //interprets fault injection commands
   void interpretInjectFaultCommand(vector<string> c);

   //interprets scrubbing commands
   void interpretScrubCommand(vector<string> c);

   //parses a command string into a vector, separated by white space
   vector<string> parseByWhiteSpace(string command);

   //converts a string to an int of base 10 or 16
   int getInt(string s, int base);

   //gets the internet address
   void * get_in_addr(struct sockaddr *sa);

   //sends data to the buffer
   void sendToBuf(void *data, int len, char header = PACKET_TYPE_BINARY);

   //sends a string to the buffer
   void sendStrToBuf(const char* s);

private:
   //DATA MEMBERS

   //Pointer to raw data to send. The listener thread performs the client's
   //request on the fpga and then loads the result data into this. The sender
   //thread then sends and clears its contents.
   void * sendDataPtr;
   //Length of the data to send
   u32 sendDataLen;
   //Type of data to send (text or binary)
   u32 sendDataHeader[2];

   //address of the client connected to the jcm
   char clientAddr[INET6_ADDRSTRLEN];
   //File descriptor of the new socket connection
   int new_fd;
   int port; //the port to use

   //if it can process a command (false until all already processed data
   //has been sent)
   bool readyToProcessCommand;
   //if the sender is ready to send more data
   bool readyToSendData;
   // command interpreter sets this if "exit" sent/socket closed
   bool exitThread;

   //the last command received from someone
   string lastCommand;

   //Option variables
   //if jtagToHighZ is true
   bool jtagHZ = false;

   //Utility functions object
   CppUtils * util;
   //Connection to the fpga and all functions
   XilinxTopLibrary * xTopLib;

   //the log file that is to be written to
   FILE * logFilePtr;
   //the name of the log file
   const char* fileName = LOG_FILE;

   //Server string responses

   const char* err0Str = "Unknown command";
   const char* err1Str = "Invalid command";

   const char* readErr0Str = "Unknown read command";
   const char* readErr1Str = "Specify which register to read";
   const char* readErr2Str = "Invalid arguments. "
       	"Type \"read help\" for details.";

   const char* writeErr0Str = "Unknown write command";
   const char* writeErr1Str = "Specify which register to write";
   const char* writeErr2Str = "Invalid arguments. "
      "Type \"write help\" for details.";

   const char* sendErr0Str = "Unknown operation";
   const char* sendErr1Str = "Specify which operation";

   const char* sendHelpStr = "Operations: injectfault (normal (no correction),"
      "random, multiframe), scrub (NYI)";

   const char* optErr0Str = "Unknown option";
   const char* optErr1Str = "Specify which option to change";

   const char* glutmaskErr = "Argument must be 0 or 1";

   const char* invalidArgsStr = "Invalid arguments.";

   const char* faultErr0Str = "Unknown fault operation (use 'op injectfault "
      " normal ...')";

   const char* helpString = "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue"
        "Connected to the jcm server. Commands:\n"
   	 "setup: \t\tsets up the connected FPGA; must be run first\n"
   	 "configure: \tconfigures the FPGA with the bit file specified in AutoConfig.txt\n"
   	 "readback: \tretrieves a golden readback copy from the FPGA\n"
   	 "scrub -type: \tperforms scrubbing on the FPGA and outputs results.\n"
   	 "\tTypes: -c (continuous), -b (blind), -h (hybrid)\n"
   	 "fault: \t\tbegin injecting faults\n"
   	 "read [reg]: \treads the specified register. Type \"read help\".\n"
   	 "write [reg]: \twrites the specified register. Type \"write help\".\n"
   	 "options [o]: \tchange various device options. Type \"options help\".\n"
   	 "echo 'message': repeats back message for testing\n"
   	 "exit: \t\tend this session\n"
   	 "? or help: \tshow this dialogue";

   const char* helpReadString = "The read (r) command reads the value of a "
   	"register on the "
   	"fpga. Supported registers:\nfar\nctrl0\ncrc\ncrchw\ncrcsw\ncrclive\n"
   	"status\ncor1\ncmd\nxadc ([reg]) displays a list of several "
   	"temperatures and voltages. A specific register value may be retrieved "
   	"by appending one of the following to this command:\n\tcurtemp\n\tvccint\n\t"
   	"vccaux\n\tvoltage.\n"
   	"frame [address] (-n [number of frames])\n"
   	"bscan [bscan # (1-4)] [# words to read]";

   const char* helpWriteString = "The write (w) command writes the value of a "
   	"register on the fpga. Supported registers:\nfar\t[value to write]\n"
   	"cor1\t[value to write]\nbscan\t[bscan # "
   	"(1-4)] [# of words to write] [value to write]\n"
      "glutmask [0/1] Sets or clears the glut mask";

   const char* helpOptionsString = "Supported options:\n"
   	"jtagtohighz [on/off]:\tenables or disables this\n"
   	"activedevice [#]:\tsets the active device index\n"
   	"view:\t\tdisplays current settings";

   //sending this string to the client indicates success, but the client does not
   //print it to the screen
   const char* genericSuccessReponse = ".";

};

#endif
