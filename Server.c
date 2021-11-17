//gcc cFile4.c -lxdo -lX11 -o cFile5.exe -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <xdo.h>

//------------------ Defines ------------------\\

#define BUF_SIZE 8
#define IP_ADDRESS "127.0.0.1"
#define PORT     20001
#define MAX_SIZE 100
#define KEY_SIZE 30

//------------------ Function Declarations ------------------\\

void typeSpecialKey(xdo_t * x);
void typeEnglishKey(xdo_t * x);
void myStrCat(char* str1, char* str2, char* container);
int myStrLen(char* str);
int myStrCmp (char* str1, char* str2);
char *myStrCpy(char *dest, const char *src);
void initCommands();
void * sendNumFunc();
void * generateKeysFunc();
void * fillArray();

//------------------ Global Variables ------------------\\

int num2Send, numOfKeyStrokes, threadStop = 1, startIndex = 0, editorID, pythonID;

pthread_t thread, thread1, thread2;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER, lock2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER, cond2 = PTHREAD_COND_INITIALIZER;

xdo_t * x;
Window window_ret;

char randBuff[MAX_SIZE];
char *specialKeys[78]; // not really used.
char keys[KEY_SIZE] = {'`', '~', '!', '@', '#', '$', '%', '^', '&', '*',
		       '(', ')', '-', '_', '=', '+', ' ', '<', '>', ',',
		       '.', '?', '[', ']', '{', '}', '/', '\\', '\'', '\"'};


//------------------ Signal Functions ------------------\\

void sigGenerateKeys(int signo){
    xdo_get_active_window(x, &window_ret);
    pthread_create(&thread1, NULL, generateKeysFunc, NULL);
    signal(SIGUSR2, sigGenerateKeys);
}

void sigSendUDPMessage(int signo){
    pthread_cond_signal(&cond1);
    signal(SIGUSR1,sigSendUDPMessage);
}

void sigStop(int signo){
    threadStop = 0;
    signal(SIGRTMAX,sigStop);
}

//------------------ Main Function ------------------\\

int main(int argc, char *argv[]) {
    pthread_mutex_lock(&lock); // lock the lock
    editorID = atoi(argv[2]); // store gedit windowID (sent by python)
    pythonID = atoi(argv[1]); // stpre python PID (sent by python)
    //inits
    x = xdo_new(NULL);
    srand(time(NULL));

    while ((numOfKeyStrokes = rand()%5+2)==num2Send);// generate the very first number to send
    num2Send = numOfKeyStrokes;

    if (signal(SIGUSR2, sigGenerateKeys)<0){
        perror("SIGUSR2 failed.");
        exit(-1);
    }
    if (signal(SIGUSR1, sigSendUDPMessage)<0){
        perror("SIGUSR1 failed.");
        exit(-2);
    }
    if (signal(SIGRTMAX, sigStop)<0){
        perror("SIGRTMAX failed.");
        exit(-3);
    }
    if(pthread_create(&thread2, NULL, sendNumFunc, NULL)){ // Create a new thread for sending UDP.
            printf("ERROR_FROM_EX2\n");
	        exit(-4);
	}
    if(pthread_create(&thread, NULL, fillArray, NULL)){ // Create a new thread to fill the array.
            printf("ERROR_FROM_EX2\n");
	        exit(-5);
	}
    while(1){
	if (startIndex > MAX_SIZE-15){
	    pthread_cond_signal(&cond2); // release thread to refill the random array with new chars
	}
        pause(); // wait for signals
    }
}

//------------------ Thread Function ------------------\\
//fills randBuff with random characters
void * fillArray(){
	while(threadStop == 1){
            for(int i=0;i<MAX_SIZE;i++){
	        int funcType = rand()%100;
                if(funcType < 70){ // insert a letter
                   char randomletter = 'a' + (rand()%26);
                   randBuff[i] = randomletter;
                }		
                else if (funcType >= 85){ // insert a digit
                    char randomDigit = '0' + (rand()%10);
                    randBuff[i] = randomDigit;
                }
		else{ // insert a special key
		    randBuff[i] = keys[rand()%KEY_SIZE];
		}
	    }
	    pthread_cond_wait(&cond2, &lock); // waits here untill released
	}
	printf("Thread: exiting.\n");
	pthread_exit(NULL);
}

//------------------ Thread1 Function ------------------\\
//triggers keyboard interrupts according to the characters in toPress.
void * generateKeysFunc(){
	int myStartIndex, myKeyStrokes;

	// calculate our part of randBuff
	pthread_mutex_lock(&lock2); 
		myStartIndex = startIndex;
		myKeyStrokes = numOfKeyStrokes;
		while ((numOfKeyStrokes = rand()%3+2)==num2Send);
       		num2Send = numOfKeyStrokes;
		startIndex = (myStartIndex+myKeyStrokes)%MAX_SIZE;
	pthread_mutex_unlock(&lock2);

	char toPress[myKeyStrokes+1];
	// copy our part to toPress
	strncpy(toPress, (randBuff + myStartIndex), myKeyStrokes);

	xdo_focus_window(x, editorID); // switch focus to gedit
	xdo_wait_for_window_focus(x, editorID, 1); // wait for focus to change

	xdo_enter_text_window(x, editorID, toPress, 1); // trigger the interrupts only into gedit

	xdo_focus_window(x, window_ret); // switch back to previous window
	xdo_wait_for_window_focus(x, window_ret, 1); // wait for focus to change

	kill(pythonID,SIGUSR1); // signal python that we'r done
	pthread_exit(NULL);
}

			    //------------------ Thread2 Function ------------------\\
// sends a number via UDP
void * sendNumFunc(){
    /****************** Vars *********************/
	struct sockaddr_in  dest;	// Holds Destination socket (IP+PORT)
	int socket_fd;			// Holds socket file descriptor
	unsigned int ssize;		// Holds size of dest
	struct hostent *hostptr;	// Holds host information
	char buf[BUF_SIZE+1];		// Used for writing/ Reading from buffer
	int retVal=0;			// Holds return Value for recvfrom() / sendto()
	char *cRetVal=NULL;		// Holds return Value for fgets()
    /****************** Initialization *********************/
	socket_fd = socket (AF_INET, SOCK_DGRAM, 0);	// Create socket
	if (socket_fd==-1)
		{ perror("Create socket"); exit(1);}	// Validate the socket

	if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)!=0)
		{ perror("Blocking"); exit(1);} 	// Set socket for non-blocking

	bzero((char *) &dest, sizeof(dest)); 		// Clearing the struct
	dest.sin_family = (short) AF_INET;		// Setting IPv4
	dest.sin_port = htons((u_short)20001);		// Setting port
	dest.sin_addr.s_addr = inet_addr(IP_ADDRESS); 	// Setting IP address

//	hostptr = gethostbyname(argv[1]);					// Getting host name
//	bcopy(hostptr->h_addr, (char *)&dest.sin_addr,hostptr->h_length);	// Setting host name into struct

	ssize = sizeof(dest);							// Get dest size
    /****************** Code *********************/
        char temp2[2];
	temp2[1] = '\0';
	while (threadStop == 1)
	{
	    pthread_cond_wait(&cond1, &lock); // wait until released
            temp2[0] = num2Send;
            retVal = sendto(socket_fd,temp2,BUF_SIZE,0,(struct sockaddr *)&dest, sizeof(dest));//Send number
            if (retVal<0) {perror("Write socket error"); exit(2);}// Make sure it was sent
	}
	printf("Thread2: closing socket\n");
	close(socket_fd);// Closing socket
	printf("Thread2: exiting.\n");
	pthread_exit(NULL);
}

//------------------ Misc Functions ------------------\\
// Concatenating s1 with s2 storing in con
void myStrCat(char* s1, char* s2, char* con){
	if (s1 == NULL || s2 == NULL || con == NULL){
		perror("Null has been sent");
		exit(-1);
	}
	int i=0;
	while(*s1 != '\0')
		con[i++] = *s1++;
	while(*s2 != '\0')
		con[i++] = *s2++;
	con[i] = '\0';
}

// Returns 0 if str1=str2.
int myStrCmp (char* str1, char* str2){
	char* s1 = str1;
	char* s2 = str2;
	while(*s1!='\0' && *s2!='\0'){
		if(*s1==*s2){
			s1++;
			s2++;
		}
		else
			return 1;
	}
	return 0;
}

// Returns the size of a string.
int myStrLen(char* str){
	int count =0;
	char* c = str;
	while(*c != '\0'){
		count++;
		c++;
	}
	return count;
}

// Copies src string to dest
char *myStrCpy(char *dest, const char *src){
	char *save = dest;
	while(*dest++ = *src++);
	return save;
}

void typeSpecialKey(xdo_t * x){ // not implemented.
    int chosenKey = rand()%78;
    int size = 12 + myStrLen(specialKeys[chosenKey]);
    char command[size];
    myStrCat("xdotool key ",specialKeys[chosenKey],command);
    system(command);
}

void typeEnglishKey(xdo_t * x){ // not implemented. using inline instead.
    //char randomletter = ' ' + (rand()%96);
    char randomletter = 'a' + (rand()%26);
    char test[2];
    test[0] = randomletter;
    test[1] = '\0';
    xdo_enter_text_window(x, CURRENTWINDOW, test, 10);
}


void initCommands(){ // xdotool keys. not implemented.
    specialKeys[0] = "grave"; // `
    specialKeys[1] = "asciitilde"; // ~
    specialKeys[2] = "exclam"; // !
    specialKeys[3] = "at"; // @
    specialKeys[4] = "numbersign";  // #
    specialKeys[5] = "quoteleft"; // $
    specialKeys[6] = "percent"; // %
    specialKeys[7] = "asciicircum"; // ^
    specialKeys[8] = "ampersand"; // &
    specialKeys[9] = "asterisk"; // *
    specialKeys[10] = "parenleft"; // (
    specialKeys[11] = "parenright"; // )
    specialKeys[12] = "emdash"; // -
    specialKeys[13] = "underscore"; //  _
    specialKeys[14] = "equal"; //  =
    specialKeys[15] = "plus"; //  +
    specialKeys[16] = "backspace"; // backspace
    specialKeys[17] = "backslash"; //
    specialKeys[18] = "bar"; // |
    specialKeys[19] = "bracketright"; // ]
    specialKeys[20] = "braceleft"; // }
    specialKeys[21] = "bracketleft"; // [
    specialKeys[22] = "braceright"; // {
    specialKeys[23] = "Tab"; // tab
    specialKeys[24] = "enter"; // enter
    specialKeys[25] = "apostrophe"; // '
    specialKeys[26] = "quotedbl"; // "
    specialKeys[27] = "semicolon"; // ;
    specialKeys[28] = "colon"; // :
    specialKeys[29] = "Up"; // upArrow
    specialKeys[30] = "Down"; // downArrow
    specialKeys[31] = "Left"; // leftArrow
    specialKeys[32] = "Right"; // rightArrow
    specialKeys[33] = "Shift_R"; // right_shift
    specialKeys[34] = "slash"; // /
    specialKeys[35] = "question"; // ?
    specialKeys[36] = "period"; // .
    specialKeys[37] = "greater"; // >
    specialKeys[38] = "comma"; // ,
    specialKeys[39] = "less"; // <
    specialKeys[40] = "Shift_L"; // left_shift
    specialKeys[41] = "Control_R"; // right_ctrl
    specialKeys[42] = "Alt_R"; // right_alt
    specialKeys[43] = "space"; // space
    specialKeys[44] = "Alt_L"; //left_alt
    specialKeys[45] = "Control_L"; //left_ctrl
    specialKeys[46] = "End"; // end
    specialKeys[47] = "Page_Down"; // page_down
    specialKeys[48] = "Page_Up"; // page_up
    specialKeys[49] = "Home"; // Home
    specialKeys[50] = "Delete"; // Del
    specialKeys[51] = "Pause"; // Pause

    //NUMPAD
    specialKeys[52] = "KP_Subtract"; // -
    specialKeys[53] = "KP_Multiply"; // *
    specialKeys[54] = "KP_Divide"; // /
    specialKeys[55] = "KP_Add"; // +
    specialKeys[56] = "KP_Enter"; // num_enter
    specialKeys[57] = "KP_Page_Up"; // num_pageUp
    specialKeys[58] = "KP_9"; // num_9
    specialKeys[59] = "KP_Up"; // num_upArrow
    specialKeys[60] = "KP_8"; // num_8
    specialKeys[61] = "KP_Home"; // num_Home
    specialKeys[62] = "KP_7"; // num_7
    specialKeys[63] = "KP_Right"; // num_rightArrow
    specialKeys[64] = "KP_6"; // num_6
    specialKeys[65] = "KP_5"; // num_5
    specialKeys[66] = "KP_Left"; // num_leftArrow
    specialKeys[67] = "KP_4"; // num_4
    specialKeys[68] = "KP_Page_Down"; // num_pageDown
    specialKeys[69] = "KP_3"; // num_3
    specialKeys[70] = "KP_Down"; // num_downArrow
    specialKeys[71] = "KP_2"; // num_2
    specialKeys[72] = "KP_End"; // num_end
    specialKeys[73] = "KP_1"; // num_1
    specialKeys[74] = "KP_Delete"; // num_del
    specialKeys[75] = "KP_Decimal"; // num_.
    specialKeys[76] = "KP_Insert"; // num_insert
    specialKeys[77] = "KP_0"; // num_0
}

