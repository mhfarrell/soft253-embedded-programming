#include "mbed.h"
#include "rtos.h"
#include "hts221.h"
#include "LPS25H.h"
#include <time.h>
#define BUFFERSIZE 150

Mutex bufferLock; // bufferLock is a mutex lock that is used to lock the buffer when data is read or wrote to the buffer
Mutex inputLock; // inputLock is a mutext lock that is used to lock the buffer when data is entered over the serial port
Ticker t;         // t is a ticker used to signal the interupt to take place
Thread DataThread; //DataThread is attached to createData method to read data from the sensors
Thread WriteThread; // WriteThread is attached to the write data method to write data to the mail box
Thread AddBuffer;   // AddBuffer is attached to addToBuffer and will add data to the buffer
Thread Consumer; // consumer thread is used to get user input to carry out commands

// struct used to store all of the infromation gathered in the sensor to add to the buffer
typedef struct {
    float tempCelsius; //tempCelsius is used to store the temperature that is collect from the sensor
    float humi; //humi is used to store the humidity that is collected from the sensor
    float press; //press is used to store the pressure that is collected from the sensor
    time_t datetime; // datetime is used to store the date and time that the sample was taken
} mail_t;           // strut name
struct tm timeInfo; // is used to store the date and time



void dataCollection(); // dataCollection is used to signal a thread to read the sensors
void createData(); // createData is used to read the sensor data
void writeData(); // writeDatae will write the data on the sensors to a mail box object
void addToBuffer(); // inBuffer will write the data in the mail box in to the buffer
void writeAll(); // writeAll will write out all information that is in the buffer
void writeN();  // writeN is used to write out an amount of records that the user has specified
void deleteAll(); // read all will read all records in the buffer
void deleteN(); // deleteN will delete the N oldest records
void consumerThread(); // consumerThread is a method that will get user input
void setState(); // setState will change the state of sampling to on or off
// mbox is a mail box used to store mail_t structors to move them from one thread to another
Mail<mail_t,16>mbox;


float sensTempCelsius; //sensTempCelsius is used to store the temperature that is stored
float sensHumi; //sensHumi is used to store the sensor data of the humidity
float sensePress;//sensePress is used to store the sensor data of the pressure
int itemsInBuffer = 0; // itemsInBuffer is an integer value used to store an amount of
//items that have been added to the buffer so far
int oldestRecord =0; // oldestRecord is used to store the oldest record in the buffer
int newestRecord=0 ; // newest record is used to store the newest item in the buffer
mail_t buffer[BUFFERSIZE] = {0}; // buffer is an array of size 150 that will store mail_t structor
int currentIndex =0; // currentIndex is used to locate where the last reading was stored in the buffer
int n =0; // is used to store user input when n records are to be read or deleted
bool sampling = true; // sampling is a used to set if samples are being stored in the buffer or not
char timeFormat[32]; // timeFormat is used to format the date and time into a readable format DD/MM/YYYY HH:MM:SS
float sampleT = 15; // sampleT is the time at which each sample is being taken
char cmd[32]= {0}; // cmd is a char array used to keep track of user input
time_t seconds; // seconds is used to store the amount of seconds

DigitalOut myled(LED1);
I2C i2c2(I2C_SDA, I2C_SCL);
LPS25H barometer(i2c2, LPS25H_V_CHIP_ADDR);
HTS221 humidity(I2C_SDA, I2C_SCL);

// main is where all threads and interupts are started
int main()
{
    timeInfo.tm_mday = 15; // sets the day month year and time to a defult time, we used assignment deadline for this.
    timeInfo.tm_mon = 4;
    timeInfo.tm_year = 2017-1900;
    timeInfo.tm_hour = 10;
    mktime(&timeInfo); // will normalise what is stored in timeInfo into unix time format as part of the ctime library.
    humidity.init();
    humidity.calib();
    printf("SOFT253 simple Temperature Humidity and Pressure Sensor Monitor\n\r");
    printf("Please enter a comand to start.\n\r");
    DataThread.start(createData);  // starts the thread that will get data from the sensors
    WriteThread.start(writeData); // starts the thread that will handle adding data to the mail box
    AddBuffer.start(addToBuffer);  // starts the thread that will add the data to the buffer from the mail box
    Consumer.start(consumerThread); // starts the consumer thread to track user input
    time(NULL); // time is set to null to defult set it to the time that is entered
    t.attach(dataCollection,sampleT); // starts the interupt to sample data every 15 seconds
    while(1) {

    }

}

// consumer thread is used to get input from the user and then proform comands depending on what the user has entered
void consumerThread()
{
    // while loop is used to check for user input after each command
    while(1) {
        cmd[0] ='\0'; // sets the first index of the array to null so that new information can be coppid in to it

        while(cmd[0]=='\0') {
            inputLock.lock(); // locks the input so that nothing can interfer with getting the string
            scanf("%s",cmd); // copies the users string in to the char array
            inputLock.unlock(); // unlocks the array so that other parts of the program can use it
        }
        // compares if the user input is equal to any of the valid inputs that are predefined
        if(strncmp(cmd, "READN",32) == 0) { 
            printf("\nPlease enter the number of recods you want to read\n"); // asks the  user for a number of records to read
            scanf("%d",&n); // gets the number the user enters
            if(n > itemsInBuffer) { // compares if n is larger then the items in the buffer
                writeAll(); // writes all items in the buffer
            } else {
                writeN(); // writes n number of records
            }
        } else if (strncmp(cmd, "SETDATE",32) == 0) {
            printf("Enter current date:\n"); 
            printf("DD MM YYYY[enter]\n"); // asks the user to enter the current date
            scanf("%d %d %d", &timeInfo.tm_mday, &timeInfo.tm_mon, &timeInfo.tm_year); // gets the users input 
            timeInfo.tm_year = timeInfo.tm_year - 1900; // formats the year to unix time
            timeInfo.tm_mon = timeInfo.tm_mon - 1; // sets the month to be useable by unix time as month is stored 0-11
            set_time(mktime(&timeInfo)); // sets the time to a normalised timeInfo, which is unix time format as part of the ctime library.
            printf("You Entered: %d/%d/%d \n", timeInfo.tm_mday, timeInfo.tm_mon, (timeInfo.tm_year+1900)); // returns the time entered to the user
        } else if(strncmp(cmd,"SETTIME",32) ==0) {
            printf("Enter current time(24H):\n"); // informs the user to enter time in a 24 hour clock format
            printf("HH MM SS[enter]\n"); // informs the user how to enter the time
            scanf("%d %d %d",&timeInfo.tm_hour, &timeInfo.tm_min, &timeInfo.tm_sec); // get the user input and sets the variables to the data entered
            set_time(mktime(&timeInfo)); // sets the time to a normalised timeInfo, which is unix time format as part of the ctime library.
            printf("You Entered: %d:%d:%d \n", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec); // returns what time the user entered

        } else if(strncmp(cmd, "READALL",32) == 0) {
            if(itemsInBuffer == 0) { // checks if the buffer has records print out
                printf("\nThere are no record stored yet\n"); // informs the user there are no samples taken yet
            } else {
                writeAll(); // writes all records in the buffer to the user
            }
        } else if(strncmp(cmd, "DELETEALL",32)==0) {
            deleteAll(); // deletes all records in the buffer by setting each index to null
        } else if(strncmp(cmd, "DELETEN",32)==0) {
            printf("\nPlease enter the amount of records you want to remove\n"); // asks the user to enter the amount of records 
                                                                                 //they want to remove
            scanf("%d",&n);     // gets the user input
            deleteN(); // calls method to remove that many records from the buffer
            printf("\n%d records deleted\n", n); // informs the user that that many records have been removed
        } else if(strncmp(cmd, "SETSTATEON",32)==0) {
            sampling = true; // will turn sampling on 
            printf("\nSampling on\n"); // informs the user sampling is on
        } else if(strncmp(cmd, "SETSTATEOFF",32)==0) {
            printf("\nSamping off\n"); // informs the user that sampling is off
            sampling = false; // turns sampling off
        } else if(strncmp(cmd, "SETT",32)==0) {
            printf("\nEnter a new sample rate(0.1 to 60): \n"); // asks the user for a sampling rate
            scanf("%f", &sampleT); // gets the user input
            t.detach(); // detaches the method from the ticker
            if(sampleT < 0.1f || sampleT > 60.0f) { // checks if the entered number is in the correct range
                printf("\nOut of range, reset to default\n"); // informs the user if the number entered is not in the range
                sampleT = 15; // sampleT will be reset to 15 as the defult if the user inputs a number out of range
            } else {
                printf("\nYou have set a new sampling rate of: %2.1f\n", sampleT); // informs the user that the sample time has changed
            }
            t.attach(dataCollection,sampleT); // attached the new timer to the interupt
        } else {
            printf("\nno command found\n"); // informs the user there comand is not valid
        }

    }
}
 // this interupt is used to signal the data collection thread to sample some data
void dataCollection()
{
    seconds = time(NULL); // sets the current stored time to null;
    if(sampling == true) {

        DataThread.signal_set(1); // signals the thread
    }
}
// create data will get data from the sensors
void createData()
{// will loop for ever but waits for the signal before reading data
    while (true) {
        Thread::signal_wait(1);
        sensTempCelsius = 25;
        sensHumi =30;
        barometer.get();
        sensePress = 1;
        WriteThread.signal_set(2); // signals another thread to start
    }
}
// writeN will write out the n most recent records
void writeN()
{
    // sets n to the current index to the users input minus the number of records
    n = currentIndex - n;
    bufferLock.lock(); // locks the buffer so data is not written to the buffer while its being read
    for(int i=n; i <currentIndex; i++) {
        // formats the time to be more human friendly
        strftime(timeFormat, 32, "%d/%m/%Y %X", localtime(&buffer[i].datetime));
        printf("Temperature: %4.2f humidity: %3.1f%% pressure: %6.1f time of sample %s\n",
               buffer[i].tempCelsius, buffer[i].humi, buffer[i].press,timeFormat); // prints out the index in the buffer

    }
    bufferLock.unlock(); // unlocks the buffer so that it can be used 
}
// writeAll will write all items that are stored in the buffer
void writeAll()
{

    bufferLock.lock(); // locks the buffer to that data cant be writen to it while it is being read
    for(int i=0; i <itemsInBuffer; i++) {
        strftime(timeFormat, 32, "%d/%m/%Y %X", localtime(&buffer[i].datetime)); // formats the time to be more human friendly
        printf("\nSample Time: %s Temperature: %4.2f pressure: %6.1f humidity: %3.1f%%\n",
               timeFormat, buffer[i].tempCelsius, buffer[i].press, buffer[i].humi); // print out the index in the buffer

    }
    bufferLock.unlock();
}
// writeData will write the data to the mail box to be used in another thread
void writeData()
{
    set_time(mktime(&timeInfo)); // sets the time to unix time 
    while(true) {       

        Thread::signal_wait(2); // waits for a siganl before continuing
        mail_t *mail = mbox.alloc(); // allocated space in the mail box to store the data
        mail-> tempCelsius = sensTempCelsius; // copies data from global variables to the mail box
        mail-> humi = sensHumi;
        mail-> press = sensePress;
        mail-> datetime = seconds;
        mbox.put(mail); // puts the data in to the mail box 
        ctime(&seconds); // updates the seconds
    }
}
// addToBuffer will data the mail to the buffer
void addToBuffer()
{
    while(true) {
        // check if any mail is in the mail box
        osEvent evt = mbox.get();
        if(evt.status == osEventMail) { // if mail is in the mail box continue
            mail_t *mail = (mail_t*)evt.value.p; // gets the mail 
            bufferLock.lock(); // locks the buffer
            buffer[currentIndex] = *mail; // adds the mail object to the buffer 
            bufferLock.unlock(); // unlocks the buffer
            currentIndex++; // increments the current index
            if(currentIndex == BUFFERSIZE) { // checks if the index is to high
                currentIndex = 0; // resets the index
            }
            if(itemsInBuffer != 150) { // checks if the buffer is full 
                itemsInBuffer++; // increases the number of items in the buffer
                oldestRecord =0; // sets the oldest record
            } else {
                oldestRecord = currentIndex+1; // sets the oldest record
            }


            newestRecord = currentIndex;// sets the newest record


            mbox.free(mail); // frees the space in the mail box
        }
    }
}
// deleteAll will remove all items in the buffer by setting them to null
void deleteAll()
{
    bufferLock.lock(); // locks the buffer 
    for(int i =0; i< BUFFERSIZE; i++) {
        buffer[i].tempCelsius = 0;      //sets all veriables that are in the buffer to null
        buffer[i].humi = 0;
        buffer[i].press = 0;
        buffer[i].datetime =0;

    }
    currentIndex =0; // reset the current index 
    itemsInBuffer =0; // resets the items in the buffer
    printf("\nAll records removed\n\n"); // informs the user that the data has been removed
    bufferLock.unlock(); // unlocks the buffer
}
// delete n will remove the oldest records that are in the buffer
void deleteN()
{
    bufferLock.lock(); // locks the buffer 
    for(int i =0; i <n; i++) {

        buffer[i].tempCelsius = 0; // sets all values in that index to null
        buffer[i].humi = 0;
        buffer[i].press = 0;
        buffer[i].datetime =0;


    }
    bufferLock.unlock(); // unlocks the buffer
}
