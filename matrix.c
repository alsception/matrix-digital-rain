#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <stdbool.h> // Include this header to use bool, true, and false
#include <string.h>
#include <time.h>   //Used for random
#include <locale.h> // use for japanese lang

// Colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_WHITE_BG "\e[47m"
#define ANSI_COLOR_DARK_GRAY "\033[38;2;43;43;43m"
#define ANSI_COLOR_DARK_GREEN "\033[32m"

#define ANSI_COLOR_MAIN_FONT "\x1b[32m"
#define ANSI_COLOR_DROP "\e[1;92m" /*"\x1b[37m"*/
#define ANSI_COLOR_HI_BLACK "\e[0;90m"

// Variables
long long int cycle = 0;
int rows = 0;
int rowsPrevious = 0;
int columns = 0;
int columnsPrevious = 0;
//!ATTENTION:
//If you set debugMode to true, and padding bottom/top > 0, then theres some issues rendering...
int paddingBottom = 0; //if debug mode this should be 1 smaller
int paddingRight = 0;//This is more like padding right (but only after first rain cycle ends :/ )...
int paddingLeft = 0;
int paddingTop = 0; 
int max_length = 15;  // Maximum length
int min_length = 5;
int direction = 'D'; // R for right, L for left, U for up, D for down
int cf = 1000; //Number of cycles to completely redraw the screen (Constant redrawing causes flashing, but neccessary)
int maxLength = 1000;
int numDrops = 220;  // Total number of tail segments

bool cursorVisible = false;
bool pausa = false;

bool debugMode = false/*true*/;
//bool debugMode = true;

int millis = 34;
/**
 * Here are the recommended frame delays in milliseconds (ms) for various refresh rates:
 *
 * choose between 17 and 67.
 * Also it should be smaller millis number if columns are > then 200

    60 FPS (Smooth animation): 16.67 ms per frame
    (1 second ÷ 60 frames = ~16.67 ms/frame)

    30 FPS (Moderate smoothness): 33.33 ms per frame
    (1 second ÷ 30 frames = ~33.33 ms/frame)

    24 FPS (Cinematic feel): 41.67 ms per frame
    (1 second ÷ 24 frames = ~41.67 ms/frame)

    15 FPS (Retro/slow-paced): 66.67 ms per frame
    (1 second ÷ 15 frames = ~66.67 ms/frame)

    10 FPS (Old-school/very retro): 100 ms per frame
    (1 second ÷ 10 frames = 100 ms/frame) */

typedef struct {
    int x;
    int y;
    int length;
} Position;

Position *drops;

typedef struct {
    int *x;
    int *y;
    int *c;
} TailSegment;

TailSegment *tailSegments;

/**
 * TODO:
 * 1. If columns(width) < certain size, then dont print the header line.
 * 2. If rows <5 OR columns < 155 -> exit with message
 * 3. update commit instructions: gcc src/snake.c -o bin/snake 
 * 
 * //TODO: (2)
 * *************************************************************************************
 * - ON RESIZE -> CLEAR SCREEN 
 * *************************************************************************************
 * 
 */

// Function to enable non-canonical mode
// https://stackoverflow.com/questions/358342/canonical-vs-non-canonical-terminal-input
void enableNonCanonicalMode()
{
    struct termios newSettings;
    tcgetattr(STDIN_FILENO, &newSettings);
    newSettings.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
}

// Function to restore terminal to original settings
void disableNonCanonicalMode()
{
    struct termios originalSettings;
    tcgetattr(STDIN_FILENO, &originalSettings);
    originalSettings.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &originalSettings);
}

void freeTails() {
    for (int i = 0; i < numDrops; i++) {
        free(tailSegments[i].x);
        free(tailSegments[i].y);
    }
    free(tailSegments);
}

void cleanUp()
{
    disableNonCanonicalMode();  
    freeTails();
}

Position* initializeDrops() 
{
    //Instead of hardcoding we assign drops dynamicaly with this parameters:
    //1. n - number of drops to initialize
    //2. max number for x
    //3. max number for y
    //4. max length
    //5. points 2,3,4 should be random numbers from 0 to max

    printf("initializing drops...\n");
    int n = numDrops;           // Number of drops
    int max_x = columns;      // Maximum x value
    int max_y = rows;      // Maximum y value
    max_length = (rows-5)/2;  // Maximum length
    min_length = 5;

    // Allocate memory for n Position structures
    drops = (Position*)malloc(n * sizeof(Position));
    printf("Memory allocated\n");
    if (drops == NULL) 
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Seed the random number generator
    srand((unsigned int)time(NULL));

    printf("initializing drops positions\n");

    // Initialize each Position with random values
    for (int i = 0; i < n; i++) {
        printf("initializing drop %i \n",i);
        drops[i].x = rand() % (max_x + 1);        // Random x in range [0, max_x]
        drops[i].y = rand() % (max_y + 1);        // Random y in range [0, max_y]
        drops[i].length = min_length + rand() % (max_length - min_length + 1); // Random length in range [min_length, max_length]

        printf("drop created x,y,l: %i,%i,%i \n",drops[i].x,drops[i].y,drops[i].length);
    }
    system("clear");
}


void getWindowSize()
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return;
    }
    rows = w.ws_row;
    columns = w.ws_col;
    int maxLength = rows * columns;
}

char getRandomLatinChar(){
    char randomletter = 'A' + (random() % 26);//56
    return randomletter;
}

char getRandomFullLatinChar(){
    char randomletter = '!' + (random() % (126-32));//full latin alfabet is going from 32(!) to 126
    return randomletter;
}

char getRandomLatinChar2(){
    //What we dont need is empty space, dot, comma... bc it doesnt look good
    char randomletter = 48 + (random() % (93-48));//full latin alfabet is going from 32(!) to 126
    return randomletter;
}

char getRandomJapanese(){
    //NOT WORKING
    // Define Unicode range for Katakana
    int katakana_start = 0x30A0;
    int katakana_end = 0x30FF;

    // Generate a random Katakana character
    int random_char = katakana_start + rand() % (katakana_end - katakana_start + 1);

    return random_char;
}

char getRandomJapanese2()
{
    //NOT WORKING
    const char *japanese = "カタカナカタカナ";
    int r = rand() % (5 + 1);    
    return japanese[r];
}

char getRandomChar(){
    return getRandomFullLatinChar();
}

void initTails(int numDrops, int maxLength) {
    tailSegments = (TailSegment *)malloc(numDrops * sizeof(TailSegment));
    
    for (int i = 0; i < numDrops; i++) {
        tailSegments[i].x = (int *)malloc(maxLength * sizeof(int));
        tailSegments[i].y = (int *)malloc(maxLength * sizeof(int));
        tailSegments[i].c = (int *)malloc(maxLength * sizeof(int));
        
        // Initialize each segment's arrays
        for (int j = 0; j < maxLength; j++) {
            tailSegments[i].x[j] = 0;
            tailSegments[i].y[j] = 0;
            tailSegments[i].c[j] = getRandomChar();
        }
    }
}

void initTail()
{  
    initTails(numDrops,150);
}


void initialize()
{
    // Set the locale to UTF-8 to use other chars
    setlocale(LC_ALL, "");
    system("clear");
    
    // Initial size print and frame
    // This is important to get rows and columns    
    getWindowSize();

    //TODO:
    //Take millis from command line or menu

    //TODO 
    /*** PLS REALOCATE MEMORY IF WINDOW SIZE IS CHANGED ***/

    initializeDrops();

    initTail();

    enableNonCanonicalMode();
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // Set input to non-blocking mode
}


// Function to read keypresses including arrow keys
int readKeyPress() 
{
    int ch = fgetc(stdin);

    // Check if it's an escape sequence
    if (ch == '\x1b') { // Escape character
        if (fgetc(stdin) == '[') { // Skip the '[' character
            ch = fgetc(stdin); // Read the third character
            // We will mask arrow keys to WASD, that way we can use both
            switch (ch) {
                case 'A': return 'W'; // Up arrow
                case 'B': return 'S'; // Down arrow
                //case 'C': return 'D'; // Right arrow. will not use because of debug mode
                case 'D': return 'A'; // Left arrow
                default: return -1; // Unknown escape sequence
            }
        }
    }
    return ch; // Return the actual character if it's not an arrow key
}

int handleKeypress()
{

    // Check for keyboard input
    int ch = readKeyPress();
    if (ch != EOF)
    {
        if ((ch == 'a' || ch == 'A') && (direction != 'R'))
            direction = 'L';

        //else if ((ch == 'd' || ch == 'D') && (direction != 'L'))
        //    direction = 'R';

        else if ((ch == 'w' || ch == 'W') && (direction != 'D'))
            direction = 'U';

        else if ((ch == 's' || ch == 'S') && (direction != 'U'))
            direction = 'D';

        else if (ch == 'p' || ch == 'P')
            pausa = !pausa;

        else if (ch == 'd' || ch == 'D'){
            debugMode = !debugMode;    
            system("clear");
        }
            

        else if (ch == 'r' || ch == 'R')
            initialize();//reset

        else if (ch == 'q' || ch == 'Q')
            return 0; // Quit the game

            //TODO: add pause
    }
    return 1;
}

void resetCursorPosition()
{
    getWindowSize();//update window size

    //TODO 
    //If window size changed -> then clear the screen
    //system("clear");

    printf("\r"); //go to start of the line
    for(int i = 0; i< rows; i++)
    {
        printf("\033[A");
    } 
}

// Return true if rain drop is present at given x,y position
bool checkDrop(int x, int y)
{
    for (int i = 0; i < numDrops; i++) 
    {
        if (x == drops[i].x && y == drops[i].y)
            return true;
    }
   return false;
}

// Return true if rain tail lastindex is present at given x,y position
bool isLastTailElement(int x, int y)
{
    for (int i = 0; i < numDrops; i++) 
    {
        for (int j = drops[i].length-2; j < drops[i].length; j++) 
        {
            if (x == tailSegments[i].x[j] && y == tailSegments[i].y[j]) 
            {
                return true;
            }
        }
    }
    return false;
}

// Return true if rain tail lastindex is present at given x,y position
bool isMiddleTailElement(int x, int y)
{
    for (int i = 0; i < numDrops; i++) 
    {
        for (int j = drops[i].length/2-1; j <= drops[i].length/2; j++) 
        {
            if (x == tailSegments[i].x[j] && y == tailSegments[i].y[j]) 
            {
                return true;
            }
        }
    }
    return false;
}

//isMiddleElement?

// Return index of snake's tail element if present at given x,y position
// Otherwise return -1
int checkTail(int x, int y)
{
   for (int i = 0; i < numDrops; i++) 
   {
        for (int j = 0; j < drops[i].length; j++) 
        {
            if (x == tailSegments[i].x[j] && y == tailSegments[i].y[j]) 
            {
                //return i;  // Return the index of the segment
                return tailSegments[i].c[j];//actually return char
            }
        }
    }
    return -1;
}

// Upper line with informations
void printHeaderLine()
{   
    printf(
        "\r" ANSI_COLOR_BLUE "Terminal size: "
        "y:%d rows | "
        "x:%d columns | "
        "y-offset: %d | "
        "refresh rate: %d | "
        "numDrops: %d | "
        "debugMode: %d | " 
        "     \n"//empty space is need 
        ANSI_COLOR_RESET, rows, columns, paddingBottom, millis, numDrops, debugMode);
        // Print Katakana characters
    
    //printf("\rカタカナ ");

    // You can also use Unicode escape sequences
    //printf("\u30AB\u30BF\u30AB\u30CA");
}

void printGameOverScreen()
{
    printf("\nWake up, Neo...\n");
    printf("Cycles rained: %lld \n", cycle);
    printf("\e[?25h"); // Reenable cursor
}


void printTail(int tailIndex, int c, bool isLast, bool isMiddle)
{
    char cc = getRandomChar();

    if( isMiddle )
    {
        printf(ANSI_COLOR_MAIN_FONT);
        printf("%c", cc);  
    }       
    else
    {
        printf(ANSI_COLOR_MAIN_FONT);
        printf("%c", c);  
    }
        
    printf(ANSI_COLOR_RESET);
}

void printDrop()
{
    char c = getRandomChar();
    printf( ANSI_COLOR_DROP );
    //printf("\e[0;102m"); backgroun
    printf( "%c", c );  
    printf( ANSI_COLOR_RESET );
}

void printEmptyContent( int x, int y, int width, int depth )
{
    if(debugMode)
    {
        printf( ANSI_COLOR_HI_BLACK );
        printf(".");
        printf( ANSI_COLOR_RESET );
    }else{
        printf(" ");
    }

}

void printContent()
{
    int depth = rows - 1 - paddingBottom;
    if(debugMode) depth--;
    int width = columns - 1;
    int tailIndexChar = -1;
    bool hasDrop = false;
    bool isLast = false;
    bool isMiddle = false;

    //Screen printing is done line by line starting from top
    for (int y = 0; y <= depth; y++)
    {
        if (y > 0) printf("\n"); // Start at new line

        for (int x = 0; x <= width; x++)
        {
            if(x<paddingLeft || y < paddingTop){
                printEmptyContent( x, y, width, depth );
            }
            else
            {
                // 1. Check if the screen cell contains drop or tail
                hasDrop = checkDrop( x, y );
                tailIndexChar = checkTail( x, y );
                isLast = isLastTailElement( x, y );
                isMiddle = isMiddleTailElement( x, y );

                // 2. Print appropriate element
                if ( hasDrop )
                {
                    printDrop( tailIndexChar );
                }
                else if ( tailIndexChar >= 0 )
                {
                    printTail( tailIndexChar,tailIndexChar,isLast, isMiddle);
                }
                else
                {
                    printEmptyContent( x, y, width, depth );
                }
            }
        }
    }
    fflush(stdout);
}

void gameOver()
{
    disableNonCanonicalMode();
    //free memori...
    printGameOverScreen();
    exit(0);
}

void updateDropPositionDown()
{
    for (int i = 0; i < numDrops; i++) 
    {
	    //Move it one position down by y axis
        drops[i].y++;

        //If arrived at the end -> go back to top
        if (drops[i].y > rows)
        {

            drops[i].y = 0;

            //Also shift it to the right after the cycle ends, so it doesnt look stuck

            drops[i].x++;

            if(drops[i].x > columns - paddingRight-1)
            {
                drops[i].x = 0;
            }
        }

    }
}

void updateDropPosition()
{
    //there is only direction 'D'

    if(direction == 'D'){
        updateDropPositionDown();
    }

}

void updateTailPosition()
{
    //Every element (i) of the rains tail takes the x,y coordinates of the previous element (i-1)
    //And first tail element takes head's position

    for (int segment = 0; segment < numDrops; segment++) {
        // Shift the tail positions
        for (int i = drops[segment].length - 1; i > 0; i--) {
            tailSegments[segment].x[i] = tailSegments[segment].x[i - 1];
            tailSegments[segment].y[i] = tailSegments[segment].y[i - 1];
            tailSegments[segment].c[i] = tailSegments[segment].c[i - 1];
        }

        // Update the first element with the drop's position
        tailSegments[segment].x[0] = drops[segment].x;
        tailSegments[segment].y[0] = drops[segment].y;
        tailSegments[segment].c[0] = getRandomChar();
        /* if(cycle%3){
            tailSegments[segment].c[0] = getRandomChar();
        }
 */
    }
}


void updateRainData()
{
    // Update tail before head, because it must follow the drops previous position
    updateTailPosition();

    // Update head position based on current direction
    updateDropPosition();
}

void render()
{
    resetCursorPosition();    

    if( debugMode ) printHeaderLine();    

    printContent();
    
    if( !cursorVisible ) printf("\e[?25l"); // Remove cursor and flashing
}

void refreshScreen()
{
    if(!pausa) 
        updateRainData();
    render();    
}

void processArguments(int argc, char **argv)
{
    char gm[] = "god-mode";
   
    for (int i = 1; i < argc; ++i)
    {
        int result;
        
        result = strcmp(gm, argv[i]);
        if (result == 0) 
        {
            /* godMode = true;
            printf("God mode activated\n"); */
        } 

       usleep(1000 * 1000);
    }
}

int main(int argc, char **argv)
{
    if(argc > 0)
        processArguments(argc,argv);    

    initialize();
        
    while (1)
    {
        cycle++;
        if(!handleKeypress())
            break;
        refreshScreen();        
        usleep(millis * 1000); // Sleep for defined milliseconds
    }

    cleanUp();
    printGameOverScreen();

    return 0;
}
