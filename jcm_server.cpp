/*
 * This executable runs a server that takes requests via sockets ipv4 to run
 * other programs.
 *
 * Author: Ryan West
 */
#include "XilinxTopLibrary.h"
#include "jcm_server.h"

// /* Catch Signal Handler functio */
// static void signal_callback_handler(int signum){
// 		signal(SIGPIPE, signal_callback_handler);
//         print("Caught signal SIGPIPE %d\n",signum);
// }

int main(int argc, char *argv[]) {

	CppUtils * utl = new CppUtils();

	//Parse Command Line Options
	for(int i = 1; i < argc; i++){
		if(utl->compare(argv[i], "-info") == 0 ||
				utl->compare(argv[i], "-h") == 0 ||
				utl->compare(argv[i], "-help") == 0 ||
				utl->compare(argv[i], "--help") == 0){
			printf("This program runs a server that runs a variety of basic JCM"
			       "functions, which is controlled by a separate client.\n");
			return 0;
		}
	}

	delete utl;
	JCMServer * server = new JCMServer();
	server->start();
	delete server;
	printf("Exited JCMServer->start()\n");
	return 0;
}

JCMServer::JCMServer(){
		util = new CppUtils();
		xTopLib = new XilinxTopLibrary("config_files/zedboard_rev_d_config.txt", "config_files/zedboard_rev_d_config.txt");
}

JCMServer::~JCMServer(){
	delete util;
	delete xTopLib;
}

void JCMServer::print(const char* fmt, ...) {
	int ret;
	va_list vargs;
	va_start(vargs, fmt);
	//printf like normal
	ret = vprintf(fmt, vargs);
	if (ret <= 0)
		fprintf(stderr, "error: print() return value is %d\n", ret);
	//also print to file
	ret = vfprintf(logFilePtr, fmt, vargs);
	if (ret <= 0)
		fprintf(stderr, "error: print() return value is %d\n", ret);
	va_end(vargs);
}

// get sockaddr sa, IPv4 or IPv6:
void * JCMServer::get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//takes a command and parses it by white space into a vector.
vector<string> JCMServer::parseByWhiteSpace(string command){

  stringstream iss(command);
  vector<string> v;
  string tmp;

  while (iss >> tmp)
    v.push_back(tmp);

	return v;
}

//Returns a string of the values from the device (frames)
void JCMServer::readInFramesFromDevice(vector<string> c) {
	if (c.size() <= 2){
		sendStrToBuf(readErr2Str);
		return;
	}

	u32 beginFrameAddress;
	int numFrames = 1;

	try {
		//read specific # of frames if command: "read frame ADDR [# frames]"
		if (c.size() == 4) {
			//convert string to base 10 int
			numFrames = stoi(c[3], nullptr, 10);
		} //if no number specified, default to reading 1 frame
		beginFrameAddress = stoi(c[2], nullptr, 16);
	}
	catch (const invalid_argument& ia) {
		sendStrToBuf(readErr2Str);
		return;
	}
	catch (const out_of_range& oor) {
		sendStrToBuf("Address out of range");
	}

	int numWordsPerFrame = xTopLib->getWordsPerFrame();
	int numBytesPerFrame = numWordsPerFrame * 32; //4 char word has 8*4 bytes.

	if (numFrames < 1 || beginFrameAddress < 0) {
		sendStrToBuf(readErr2Str);
		return;
	}

	// clear glut mask to see if that changes things
	xTopLib->clearGlutMaskBit(jtagHZ);
	static u32 * frames = xTopLib->readFrames(beginFrameAddress, numFrames, jtagHZ);
		xTopLib->clearGlutMaskBit(jtagHZ);
	static u32 * frame2 = xTopLib->readFrames(beginFrameAddress, numFrames, jtagHZ);

	//integrity test PASSED
	for (int i = 0; i < 1; i++) {
			xTopLib->clearGlutMaskBit(jtagHZ);
		frames = xTopLib->readFrames(beginFrameAddress, numFrames, jtagHZ);
			xTopLib->clearGlutMaskBit(jtagHZ);
		frame2 = xTopLib->readFrames(beginFrameAddress, numFrames, jtagHZ);

		for (u32 j = 0; j < numFrames * numWordsPerFrame; j++) {
			if (frames[j] != frame2[j]) {
				printf("At index [%d], 1: %d, 2: %d\n", j, frames[j], frame2[j]);
				print("Error different\n");
				//break;
			}
		}
		print(".");
		if (i % 10 == 0)
			fflush(stdout);
	}



	sendToBuf(frames, sizeof(u32) * numFrames * numBytesPerFrame);
}

//Converts a string to an int
int JCMServer::getInt(string s, int base) {
	int i;

	if (base != 10 && base != 16)
		print("Programmer cannot call getInt without 10 or 16\n");

	//make sure this is only numeric
	for (int j = 0; j < s.length(); j++)
		if ((base == 10 && !(s[j] >= '0' && s[j] <= '9')) ||
		   (base == 16 && !isxdigit(s[j]))) {
			sendStrToBuf(writeErr2Str);
			throw invalid_argument("NAN");
		}

	try {
		i = stoi(s, nullptr, base);
	}
	catch (const invalid_argument& ia) {
		print("SHOULD\n");
		sendStrToBuf(writeErr2Str);
		throw std::invalid_argument("NAN");
	}
	catch (const out_of_range& ia) {
		sendStrToBuf(writeErr2Str);
		throw std::invalid_argument("NAN");
	}
	return i;
}

//handles reading and writing of the bscan.
void JCMServer::interpretBscanCommand(vector<string> c, bool read) {
	if (read) {
		if (c.size() != 4) {
			sendStrToBuf(readErr2Str);
			return;
		}
	}
	//if not reading, we are writing instead and it needs 3 additional args
	else if (c.size() != 5) {
		sendStrToBuf(readErr2Str);
		return;
	}

	int bscanNumber, bscanNumBytes, bscanNumWords;
	u32 * regValue;
	try {
			//convert strings to base 10 int
			bscanNumber = stoi(c[2], nullptr, 10);
			bscanNumBytes = stoi(c[3], nullptr, 10); //used in reading
			bscanNumWords = stoi(c[3], nullptr, 10); //used in writing

	//		if (!read) //register to write to if writing
		//		*regValue = (u32*) stoi(c[4], nullptr, 16);
	}
	catch (const invalid_argument& ia) {
		sendStrToBuf(readErr2Str);
		return;
	}

	if (bscanNumber < 1 || bscanNumber > 4) {
		sendStrToBuf("Invalid Bscan number (must be 1-4)");
		return;
	}

	if (read) {
		u32 * bScanResult = xTopLib->readBscan(bscanNumber,
					bscanNumBytes * 32, jtagHZ);

		sendToBuf(bScanResult, bscanNumBytes);
		//stringstream ss;
		//ss << "Bscan " << bscanNumber << ":";
		//for (int i = 0; i < bscanNumBytes; i++)
		//	ss << "\n" << std::hex << setw(8) << setfill('0') << bScanResult[i];
		//sprint(sv->sharedBuf, ss.str().c_str());
	}
	//for writing
	else {
	//	sv->xTopLib->writeBscan(bscanNumber, bscanNumWords, regValue, sv->jtagHZ);
		sendStrToBuf("Bscan write not implemented");
	}
}

void JCMServer::interpretXadcCommand(vector<string> c) {
	static float f;

	//if command is just "read xadc", print a summary of all
	if (c.size() <= 2) {
		string tmp;
		char buffer[50];

		float temperature = xTopLib->readXadcTemp(JCM_XILINX_READ_TEMP);
		sprintf(buffer, "xadc temp = %.1f C\n", temperature);
		tmp += buffer;
		float maxtemperature = xTopLib->readXadcTemp(JCM_XILINX_READ_MAX_TEMP);
		sprintf(buffer, "xadc temp Max = %.1f C\n", maxtemperature);
		tmp += buffer;
		float voltage = xTopLib->readXadcVoltage(JCM_XILINX_READ_VCCINT);
		sprintf(buffer, "xadc Vccint = %.1f mV\n", voltage);
		tmp += buffer;
		float maxvoltage = xTopLib->readXadcVoltage(JCM_XILINX_READ_MAX_VCCINT);
		sprintf(buffer, "xadc Vccint Max = %.1f mV\n", maxvoltage);
		tmp += buffer;
		voltage = xTopLib->readXadcVoltage(JCM_XILINX_READ_VCCAUX);
		sprintf(buffer, "xadc Vccaux = %.1f mV\n", voltage);
		tmp += buffer;
		maxvoltage = xTopLib->readXadcVoltage(JCM_XILINX_READ_MAX_VCCAUX);
		sprintf(buffer, "xadc Vccaux Max = %.1f mV", maxvoltage);
		tmp += buffer;

		sendStrToBuf(tmp.c_str());
	}
	else if (c[2] == "curtemp")
		sendToBuf(&(f = xTopLib->readXadcCurTemp()), sizeof(float));
	else if (c[2] == "vccint") {
		sendToBuf(&(f = xTopLib->readXadcVccInt()), sizeof(float));
	}
	else if (c[2] == "vccaux") {
		sendToBuf(&(f = xTopLib->readXadcVccAux()), sizeof(float));
	}
	//these require a u32 "readCommand"... not sure what that iss
	// else if (c[2] == "temp") {
	// 	sprintf(sv->sharedBuf, "xadc Temperature: %.1f C",
	// 					sv->xTopLib->readXadcTemp());
	// }
	// else if (c[2] == "voltage") {
	// 	sprintf(sv->sharedBuf, "xadc Voltage: %.1f mV",
	// 					sv->xTopLib->readXadcVoltage());
	// }
	else {
		sendStrToBuf("Unkown XADC register");
	}
}

void JCMServer::sendToBuf(void *data, int len, char header){
	sendDataPtr = data;
	//printf("\tsendToBuf: void* data: %d\n\t", (int*)sendDataPtr);
	sendDataLen = len;
	sendDataHeader[0] = header;
	//length of response (in sendDataHeader[1]) is determined in sender thread
}

void JCMServer::sendStrToBuf(const char* s) {
	sendToBuf((void *) s, strlen(s), PACKET_TYPE_TEXT);
}

void JCMServer::interpretReadCommand(vector<string> c) {
		//these static vars are necessary because without them, the void pointer
		//sendDataPtr would point to a temporary value returned from a Xilinx
		//Top Library function, and when the value is deleted, the pointer would
		//point to nothing. Thus, persistent static vars are required to keep the
		//data until the next command.
	   static u32 v;
		static int k;
		if (c.size() < 2)
			sendStrToBuf(readErr1Str);
		else if (c[1] == "help" || c[1] == "?")
			sendStrToBuf(helpReadString);
		else if(c[1] == "far")
			sendToBuf(&(v = xTopLib->readFar(jtagHZ)), sizeof(u32));
		else if(c[1] == "idcode")
			sendToBuf(&(v = xTopLib->readIdCode(jtagHZ)), sizeof(u32));
		else if(c[1] == "ctrl0")
			sendToBuf(&(v = xTopLib->readCtrl0()), sizeof(u32));
		else if(c[1] == "crc")
			sendToBuf(&(v = xTopLib->readCrc(jtagHZ)), sizeof(u32));
		else if(c[1] == "crchw")
			sendToBuf(&(v = xTopLib->readCrcHw(jtagHZ)), sizeof(u32));
		else if(c[1] == "crcsw")
			sendToBuf(&(v = xTopLib->readCrcSw(jtagHZ)), sizeof(u32));
		else if(c[1] == "crclive")
			sendToBuf(&(v = xTopLib->readCrcLive()), sizeof(u32));
		else if(c[1] == "status")
			sendToBuf(&(v = xTopLib->readStatus(jtagHZ)), sizeof(u32));
		else if(c[1] == "cor1")
			sendToBuf(&(v = xTopLib->readCor1(jtagHZ)), sizeof(u32));
		else if(c[1] == "cmd")
			sendToBuf(&(v = xTopLib->readCmd(jtagHZ)), sizeof(u32));
		else if (c[1] == "numlogicframes") {
			sendToBuf(&(k = xTopLib->getNumLogicFrames()), sizeof(k));
			printf("num logic frames: %d\n", k);
		}
		else if (c[1] == "numbramframes") {
			sendToBuf(&(k = xTopLib->getNumBramFrames()), sizeof(k));
			printf("num bram frames: %d\n", k);
		}
		else if (c[1] == "numtotalframes") {
			sendToBuf(&(k = xTopLib->getTotalFrames()), sizeof(k));
			printf("num total frames: %d\n", k);
		}
		else if (c[1] == "wordsperframe") {
			sendToBuf(&(k = xTopLib->getWordsPerFrame()), sizeof(k));
			printf("num words per frames: %d\n", k);
		}
		else if(c[1] == "xadc")
			interpretXadcCommand(c);
		else if(c[1] == "frame")
			readInFramesFromDevice(c);
		else if (c[1] == "bscan")
			interpretBscanCommand(c, true);
		else if (c[1] == "fradlist"){
			static u32 * fradArray = xTopLib->getFrameAddressArray();
			sendToBuf(fradArray, sizeof(u32));
		}
		else if (c[1] == "hwversion")
			{ /*in order to print hardware version, function must be changed*/ }
		else
			sendStrToBuf(readErr0Str);
}

void JCMServer::interpretOperationCommand(vector<string> c) {
	if (c.size() < 2)
		sendStrToBuf(sendErr1Str);
	else if (c[1] == "help" || c[1] == "?")
		sendStrToBuf(sendHelpStr);
	else if (c[1] == "capture") {
		xTopLib->issueCapture(jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[1] == "prog") {

		u32 WBStarAddr = 0;

		try {
			//If no address provided, default to zero
			if (c.size() >= 3)
				WBStarAddr = getInt(c[2], 16);
		}
		catch (invalid_argument& ia) {
			print("issueProg parseint failed.\n");
			return;
		}

		xTopLib->issueProg(WBStarAddr, jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[1] == "injectfault" || c[1] == "i")
		interpretInjectFaultCommand(c);
	else if (c[1] == "scrub")
		interpretScrubCommand(c);
	else
		sendStrToBuf(sendErr0Str);
}

void JCMServer::interpretScrubCommand(vector<string> c) {
	if (c.size() < 3)
		sendStrToBuf("Specify scrub option");
	else if (c[2] == "blind") {
		//IT WILL PROBABLY STALL HERE
		xTopLib->blindScrub(false, true, jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[2] == "readback") {
		sendStrToBuf("Not yet implemented");
	}
	else if (c[2] == "hybrid") {
		sendStrToBuf("Not yet implemented");
	}
	else
		sendStrToBuf("Unknown scrub command");
}

void JCMServer::interpretInjectFaultCommand(vector<string> c) {

	if (c.size() < 3) {
		sendStrToBuf(invalidArgsStr);
		return;
	}

	//type of fault (if not standard) is specified after 'injectfault'
	if (c[2] == "spartan")
		sendStrToBuf("Not yet implemented (maybe never)");
	//Syntax: "op injectfault multiframe [frame address] [command reg value]"
	else if (c[2] == "multiframe") {
		if (c.size() < 5) {
			sendStrToBuf(invalidArgsStr);
			return;
		}
		u32 frad = 0, commandReg = 0;
		try {
			frad = getInt(c[3], 16);
			commandReg = getInt(c[4], 16);
		}
		catch (invalid_argument& ia) {
			print("multiframe injectfault parseint failed.\n");
			return;
		}

		xTopLib->injectMultiFrameFault(frad, commandReg, jtagHZ);
		//function returns void, so no way to determine success.
		sendStrToBuf(genericSuccessReponse);
	}
	//Syntax: "op injectfault random [faultInjectionSize] (repairFault)"
	else if (c[2] == "random" || c[2] == "r") {
		if (c.size() < 4) {
			sendStrToBuf("usage: op injectfault random [# bits to inject]");
			return;
		}
		int faultInjectionSize = 1;
		bool repairFault = false;
		try { faultInjectionSize = getInt(c[3], 10); }
		catch (invalid_argument& ia) {
			print("random injectfault parseint failed.\n");
			return;
		}
		if (c.size() == 5 && c[4] == "repairfault")
			repairFault = true;

		bool success = xTopLib->injectRandomFault(faultInjectionSize, true,
			repairFault, false, jtagHZ);
		if (success)
			sendStrToBuf("random fault injection succeeded");
		else
			sendStrToBuf("random fault injection failed");
	}
	//Syntax: "op injectfault normal [frameAddress] [wordNum] [bitNum] [numBits]"
	else if (c[2] == "normal" || c[2] == "n") {
		if (c.size() != 7) {
			sendStrToBuf("usage: op injectfault normal [address] [word]"
				"[bit] [# bits to inject]");
			return;
		}
		u32 frameAddress = 0;
		int wordNum = 0, bitNum = 0, numBits = 1;
		try {
			frameAddress = getInt(c[3], 16);
			wordNum = getInt(c[4], 10);
			bitNum = getInt(c[5], 10);
			numBits = getInt(c[6], 10);
		}
		catch (invalid_argument& ia) {
			print("normal injectfault parseint failed.\n");
			return;
		}

		bool success = xTopLib->injectFault(frameAddress, wordNum, bitNum,
			numBits, false, true, jtagHZ);
		if (success)
			sendStrToBuf("normal fault injection succeeded");
		else
			sendStrToBuf("normal fault injection failed");
	}
	else
		sendStrToBuf(faultErr0Str);
}

void JCMServer::interpretWriteCommand(vector<string> c) {
	if (c.size() < 2)
		sendStrToBuf(writeErr1Str);
	else if (c[1] == "help" || c[1] == "?")
		sendStrToBuf(helpWriteString);
	else if (c.size() < 3)
		sendStrToBuf(writeErr2Str);
	else if (c[1] == "bscan")
		//interpretBscanCommand(sv, c, sv->jtagHZ);
		sendStrToBuf(writeErr0Str); //Not yet implemented
	else if (c[1] == "far") {
		u32 farVal;
		try { farVal = getInt(c[2], 16); }
		catch (invalid_argument& ia) {
			print("w FAR parse int failed.\n");
			return;
		}
		xTopLib->writeFar(farVal, jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[1] == "cor1") {
		u32 vall;
		try { vall = getInt(c[2], 16); }
		catch (invalid_argument& ia) {
			print("w COR parse int failed.\n");
			return;
		}
		xTopLib->writeCor1(vall, jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[1] == "crcsw") {
		u32 vall;
		try { vall = getInt(c[2], 16); }
		catch (invalid_argument& ia) {
			print("w CrcSw parse int failed.\n");
			return;
		}
		xTopLib->writeCrcSw(vall, jtagHZ);
		sendStrToBuf(genericSuccessReponse);
	}
	else if (c[1] == "glutmask") {
		if (c[2] != "0" && c[2] != "1"  && c[2] != "set" && c[2] != "clear")
			sendStrToBuf(glutmaskErr);
		else {
			bool setMask = false;
			if (c[2] == "1" || c[2] == "set")
				setMask = true;

			if (setMask) {
				xTopLib->setGlutMaskBit(jtagHZ);
				sendStrToBuf("Glut mask bit SET");
			}
			else {
				xTopLib->clearGlutMaskBit(jtagHZ);
				sendStrToBuf("Glut mask bit CLEARED");
			}
		}
	}
	else
		sendStrToBuf(writeErr0Str);
}

void JCMServer::interpretOptionsCommand(vector<string> c) {
	if (c.size() < 2)
		sendStrToBuf(optErr1Str);
	else if (c[1] == "?" || c[1] == "help")
		sendStrToBuf(helpOptionsString);
	else if (c[1] == "view") {
		//Possible issue: s is deleted when function ends so bad outupt
		//(pointer to garbage)
		static string s;
		if (jtagHZ)
			s += "Jtag to High-Z:\tON";
		else
			s += "Jtag to High-Z:\tOFF";
		sendStrToBuf(s.c_str());
	}
	else if (c[1] == "jtagtohighz") {
		if (c.size() < 3 || !(c[2] == "on" || c[2] == "off"))
			sendStrToBuf(invalidArgsStr);
		else {
			if (c[2] == "on") {
				jtagHZ = true;
				sendStrToBuf("Jtag to High-Z ON");
			}
			else {
				jtagHZ = false;
				sendStrToBuf("Jtag to High-Z OFF");
			}
		}
	}
	else if (c[1] == "activedevice") {
		if (c.size() < 3) {
			sendStrToBuf(invalidArgsStr);
			return;
		}
		for (int i = 0; i < c[2].length(); i++)
			if (!isdigit(c[2][i])) {
				sendStrToBuf(invalidArgsStr);
				return;
			}
		xTopLib->setActiveDeviceIndex(getInt(c[2], 10));
		//sprintf(sv->sharedBuf, "Active device index set to %s", c[2].c_str());
		sendStrToBuf("Active device index set");
	}
	else
		sendStrToBuf(optErr0Str);
}

//takes a string sent by the client and interprets it, carrying out instructions.f
void JCMServer::interpretCommand(string command){

	vector<string> c = parseByWhiteSpace(command);

	if (c.size() < 1) {
		sendStrToBuf("interpretCommand: command vector was null or 0 size\n");
		return;
	}

	//NOTE: Do NOT include a trailing \n at the end of replies; this is handled by
	// the client

	//This is the only case where nothing is written to the buffer. The previous
	//data is resent (in the case of a send error)
	if (command == "resend")
		return;
	else if (c[0] == "?" || c[0] == "help")
		sendStrToBuf(helpString);
	else if(c[0] == "setup")
		sendStrToBuf("Not yet implemented");
	else if(c[0] == "configure"){
		string alternateBitFile = "";
		//possible solution to redirect stdout to a buffer then send it to user:
		//Issue: must be POSIX-compliant (doesn't work so probably isn't)
		//
		//char buffer[MAXDATASIZE];
		//auto fp = fmemopen(buffer, MAXDATASIZE, "w");
		//if (!fp) {
		//	print("Configure: Error opening alternate stream for stdout\n");
		//}
		//auto old = stdout; //save old stdout stream
		//stdout = fp; //reassign stdout to the new buffer stream (redirects output)
		bool success = xTopLib->getDevice()->configureDevice(alternateBitFile);
		//fclose(fp); //close the temporary stream
		//stdout = old; //restore stdout
		//print("Buffer:%s\n", buffer);
		if (success)
			sendStrToBuf("Finished full configuration");
		else
			sendStrToBuf("Full configuration failed");
	}
	else if(c[0] == "readback"){
		//NOTE: this are default values from jcm_full_readback, and should perhaps
		//be constants somewhere!
		bool readBram = false, clearGlutMask = true, issueCapture = false;

		// Perform readback (path should be constant!)
		xTopLib->readFullDevice("/tmp/readBack.data", readBram, clearGlutMask,
				issueCapture, jtagHZ);
		//NOTE XilinxUtils->readFullDevice returns a pointer to data; even if the
		//fpga is off, the jcm_full_readback.elf will not throw an error. The server
		//doesn't either since this should be handled in that function.
		sendStrToBuf("Readback complete(?)");
	}
	else if(c[0] == "scrub")  //this will need to be changed to support -b -c -h
		sendStrToBuf("Not yet implemented");
	else if(c[0] == "fault")
		sendStrToBuf("Not yet implemented");
	// all read commands are handled by another function
	else if(c[0] == "read" || c[0] == "r")
		interpretReadCommand(c);
	else if(c[0] == "write" || c[0] == "w")
		interpretWriteCommand(c);
	else if(c[0] == "op") //op for operation
		interpretOperationCommand(c);
	//the echo command returns all text after "echo " in the command
	else if(c[0] == "echo"){
		//I still need to enable this for new functionality
		static string echoResponse;     //ASK why this can't be done in one line.
		echoResponse = command.substr(5);
		if (command.size() >= 6)
			sendStrToBuf(echoResponse.c_str());
		else
		   sendStrToBuf(err0Str);
	}
	else if (c[0] == "options" || c[0] == "option" || c[0] == "o")
		interpretOptionsCommand(c);
	else if(c[0] == "exit")
		exitThread = true;//Don't respond on exit or else seg fault (writing from)
	else
		sendStrToBuf(err0Str);
		//sprintf(sv->sharedBuf, "Unknown command");
}


//The sender and listener static stub functions are needed because the pthread
//library requires a normal pointer to a function as the new thread's starting
//point. A pointer to a member of an object will not work. These simply calll
//the correct object method. Possible change by not using Pthread library.
static void * senderThreadStaticStub(void *s) {
	((JCMServer*) s)->senderThread();
}
static void * listenerThreadStaticStub(void *s) {
	((JCMServer*) s)->listenerThread();
}

//other child process does the talking.
void * JCMServer::listenerThread(){
  while (!exitThread){
    char command[MAXDATASIZE];
	 memset(command, 0, sizeof command);
    int numbytes = 0;

    //receiving endF
    if ((numbytes = recv(new_fd, command, MAXDATASIZE-1, 0)) == -1){
      perror("recv");
		break;
    }
	 //if the other person shut down (recv returns a 0 for this)
	 if (numbytes == 0) {
		 exitThread = true;
		 break;
	 }

    command[numbytes] = '\0';
    if (numbytes > 0)
       print("Got '%s'"/* from %s*/, command); //clientAddr);

	 fflush(stdout);

    while (!readyToProcessCommand); //wait until other thread is readyToProcessCommand
	 	readyToProcessCommand = false;

 	 interpretCommand((string)command);
	 //lets the sender send data now
	 readyToSendData = true;
  }
}

//this thread creates the listener and will send any data produced by listener
void* JCMServer::senderThread(){

	//set up file logging (cannot call print() until here)
	logFilePtr = fopen(fileName, "a");

	print("Got connection from %s\n", clientAddr);

	//create listener thread
	pthread_t listenerTh;
	int *th2rv = (int*) pthread_create(&listenerTh, NULL, &listenerThreadStaticStub, this);

	//commands that the user can input on the server (on the jcm)
   while (!exitThread)
		sendIfReady();

  print("\nClient '%s' has disconnected\n\n", clientAddr);
  fclose(logFilePtr);
  //close(sv->new_fd);
}

void JCMServer::sendIfReady(){
	readyToProcessCommand = true;

	//if other thread has made data, get it (and lock other thread from
	//overwriting data until finished getting)
	if (!readyToSendData)
		return;

	readyToSendData = false; //now finished, no new data to send
	readyToProcessCommand = false;

	int toSendTotalLength = sendDataLen;
	int toSendCurrentLength = MAXDATASIZE;
	int currentDataIndex = 0;
	int numSendIterations = 0;
	string header_str = "invalid header";
	int testDataIndex = 0;

	//calculate number of packets needed to send all data
	while (testDataIndex < toSendTotalLength) {
		//if we're on the last iteration, probably won't be a full length packet
		if (testDataIndex + MAXDATASIZE > toSendTotalLength)
			toSendCurrentLength = toSendTotalLength - testDataIndex;
		testDataIndex += MAXDATASIZE;
		numSendIterations++;
	}
	toSendCurrentLength = MAXDATASIZE; //reset this
	//length is second byte of header packet
	//sendDataHeader[1] = numSendIterations;
	sendDataHeader[1] = toSendTotalLength;

	//Send the header packet only (just two ints (8 bytes): 1st for type, 2nd
	//for number of subsequent packets (usually one))
	if ((send(new_fd, (void*) sendDataHeader, 8, MSG_NOSIGNAL)) == -1){
		perror("send");
		exitThread = true; //used to be BREAK. MAY CAUSE PROBLEMS.
		return;
	}
	if (sendDataHeader[0] == PACKET_TYPE_TEXT)
		header_str = "txt";
	else if (sendDataHeader[0] == PACKET_TYPE_BINARY)
		header_str = "bin";
	else
		print("Invalid header value: %d\n", sendDataHeader[0]);

	// this is simply a busy cycle to allow the client to read the
	// the header so before we overwrite it with more data
	// Not a great solution.
	if (toSendTotalLength > MAXDATASIZE)
		for (int i = 0; i < 4000000; i++);

	//Actually send the data, in 1024 byte increments
	while (currentDataIndex < toSendTotalLength) {
		//if we're on the last iteration, probably won't be a full length packet
		if (currentDataIndex + MAXDATASIZE > toSendTotalLength)
			toSendCurrentLength = toSendTotalLength - currentDataIndex;

			int *ptrVal = static_cast<int*>(sendDataPtr + currentDataIndex);

			//printf("\tvoid* data: %d\n\t", *ptrVal);
			 //print("currentDataIndex: %d\ntoSendCurrentLen: %d"
			// , currentDataIndex, toSendCurrentLength);
			//Sprint("\n\t\tSTRING: %s\n", (u32*)sv->sendDataPtr);

			//send data from other thread
		if ((send(new_fd, (sendDataPtr + currentDataIndex),
				 toSendCurrentLength, MSG_NOSIGNAL)) == -1){
			perror("send");
			exitThread = true; //used to be BREAK. MAY CAUSE PROBLEMS.
			return;
		}

		currentDataIndex += MAXDATASIZE;
	}
	print(", Packets sent: %d (%s)\n", numSendIterations, header_str.c_str());
	//sendDataPtr = nullptr;
}

int JCMServer::start(){
	int sockfd, new_fd; //the socket file descriptor we listen on
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information.
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	//First, set up the hints struct and getaddrinfo

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP automatically

	if ((rv = getaddrinfo(NULL, DEFAULT_PORT, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return 1;
	}

	//With this set up, loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype,
							p->ai_protocol)) == -1) {
					perror("server: socket");
					continue;
			}
			//this is only really here to get rid of a pesky "address already in"
			//"use" error message
			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
							sizeof(int)) == -1) {
					perror("setsockopt");
					exit(1);
			}
			//binds to a specific port: (socket desc, struct sockaddr (in addrinfo)
			//, length)
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
					close(sockfd);
					perror("server: bind");
					continue;
			}
			break;
	}
	//This is necessary in C, perhaps not in c++?
	freeaddrinfo(servinfo); // all done with this structure

	//There's a problem if it didn't find anything to bind to, exit.
	if (p == NULL)  {
			fprintf(stderr, "server: failed to bind\n");
			return 2;
	}

	//makes the socket a passive listener (so you can accept on it)
  if (listen(sockfd, BACKLOG) == -1) {
      perror("listen");
      return 3;
  }

  printf("JCM server started. Waiting for connections...\n\n");


	int *senderThRV; //sender thread return value
	pthread_t senderTh;

	//Here, the server listens for clients, and creates a new thread to handle
	//each client that connects.
	while(1) {  // main accept() loop
			sin_size = sizeof their_addr;
			new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
			if (new_fd == -1) {
					perror("accept");
					continue;
			}

			this->new_fd = new_fd;

			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *)&their_addr),
					s, sizeof s);
			strcpy(clientAddr, s); //copies client address string
			exitThread = false;
			readyToProcessCommand = true;

			//create sender thread which will later create listener thread
			senderThRV = (int*) pthread_create(&senderTh, NULL,
				&senderThreadStaticStub, (JCMServer *)this);
			//close(new_fd);  // parent doesn't need this
	}

	pthread_join(senderTh, NULL);
	print("All threads ended, exiting.");
  return 0;
}