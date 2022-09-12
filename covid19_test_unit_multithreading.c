#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<semaphore.h>
#include <stdbool.h>



#define THREAD_COUNT 69     //THREADS COUNT => People Threads
#define ROOM_COUNT 8        //ROOM COUNT => Place to make test
#define STAFF_COUNT 8       //STAFF COUNT => Each room has 1 staff

//Arguments for the function *PeopleOperation*
//Used two arguments
typedef struct people_thread_arguments {
    int threadId;	    //threadId, peopleId
    int which_room;	    //people will enter to which room? roomId
};

/*functions*/
void *peopleOperations(void* pt_a);  	//people threads function
void *rooms(int wr);			//staff thread function
void randwait(int secs);		

int determineRoom();			//choose best possible room
int leastUsedRoom();			//to prevent starvation
/***********/

/*Semaphores*/
sem_t roomwait[ROOM_COUNT];		//to wait for the room to be accessible------> people wait, staff post => initial 3
sem_t roomcondition[ROOM_COUNT];	//3 people required to start the test--------> staff wait, people post => initial 0
sem_t roomendop[ROOM_COUNT];		//room end operation, wait for operation-----> people wait, staff post => initial 0
sem_t startop[ROOM_COUNT];		//room start operation, wait for start op----> people wait, staff post => initial 0
sem_t toprint[ROOM_COUNT];		//this is additional, to print who enters to room------> people wait, people post => inital 1
/************/

//threads will be called from these arrays
pthread_t stid[STAFF_COUNT];		//staff threads array
pthread_t tid[THREAD_COUNT];		//people threads array


/*Global variables which could be affected by threads*/
int toPreventStarving[ROOM_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0}; 		     //to prevent starving, choosen least used room by using this array

int toPrint[ROOM_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};			     //this is additional, how many people came to room so far

char idsInRoom[ROOM_COUNT][20] = { " ", " ", " ", " ", " ", " ", " ",  " "}; //for print again same at toPrint array, keeps string like |id||id||id|
									     //id shows which peoples in the room that asked for
int allDone = THREAD_COUNT;		//to check all people are done

//main
int main(int argc, char *argv[]){

	//initialize semaphores, all semaphores are in arrays, for ex; roomwait area for room1 is in roomwait[1]
	for(int r = 0; r < ROOM_COUNT; r++){
		sem_init(&roomwait[r], 0, 3);
    		sem_init(&roomcondition[r], 0, 0);
    		sem_init(&roomendop[r], 0, 0);
		sem_init(&startop[r], 0, 0);
		sem_init(&toprint[r], 0, 1);
	}

	//rooms staff thread creation
	for(int s = 0; s < STAFF_COUNT; s++){
		//serr? != 0 returns error
		int serr = pthread_create(&stid[s], NULL, &rooms, (void *)(s));		//staff thread creation, goes to rooms method
		if (serr != 0)
			printf("Thread creation error: [%s]", strerror(serr));		//error message

	}

	/*people treads creations, THREAD_COUNT is a decimal that shows how many people will come to hospital*/
		
	struct people_thread_arguments *pta;						//arguments structure for peopleOperation method
	
	int i = 0;
	while (i < THREAD_COUNT) {
		
		pta = malloc(sizeof(struct people_thread_arguments));			//malloc
		(*pta).threadId = i;							//people thread id
		(*pta).which_room = determineRoom();					//people enter which room?, determineRoom return (int)room id

		//err? != 0 return error
		int err = pthread_create(&(tid[i]), NULL, &peopleOperations, (void *)pta);	//people threads creation, goes peopleOperation method
		if (err != 0)
			printf("Thread creation error: [%s]", strerror(err));		//error message
		i++;
	}

	//pthread joins, people threads tid[]
	for (i = 0; i < THREAD_COUNT; i++) {
        	pthread_join(tid[i],NULL);
    	}
	
	//end of main
	return 0;
}

/*people operations, people threads enter this*/  /**Important, I designed like that, because there is no area which is used by two different room**/
void *peopleOperations(void* pt_a){

	//arguments which came from pt_a, from thread creation
	struct people_thread_arguments *p_t_a = (struct people_thread_arguments*)pt_a;

	int num = (*p_t_a).threadId;		//num -- people thread id
	int whrm = (*p_t_a).which_room;		//whrm -- which room that people thread in
	
	/* wait for the room to be available,,, roomwait[which room],,,, 3 people can enter room, roomwait starts with 3 */
	sem_wait(&roomwait[whrm]);


	/* this is additional, to print clearly rooms, such as room0 --- | 0 || 1 || 2 |, which means 0, 1 ,2 are in room0  */ 
	/* only one person enter this area at same time for a room*/
	sem_wait(&toprint[whrm]);
	toPrint[whrm]++;			//how many people came into room so far	
	char addedString[20];			//temporary string to update string that will be printed
	sprintf(addedString, "| %d |", num);	//int to string
	strcat(idsInRoom[whrm], addedString);	//add this to current string, like | 0 | + | 1 | --> | 0 || 1 |
	printf("%d enters to room%d --- %s\n", num, whrm, idsInRoom[whrm]);		//for ex; 1 enters to room1 --- |0||2||1|
	if(toPrint[whrm] % 3 == 0){		//if count %3 == 0
		sprintf(idsInRoom[whrm], " ");	//which means 3 people condition provided, so update as room empty 
	}
	sem_post(&toprint[whrm]);		//to post to print area is available
	/*******************************/

	
	sem_post(&roomcondition[whrm]);		//3 people are in room? Warn staff that +1 people is ready to test
	

	sem_wait(&startop[whrm]);		//people waits staff to start test(operation), if 3 people in room, test starts
	

	sem_wait(&roomendop[whrm]);		//people waits staff to end test(operation), if staff post roomendop, test ends


	printf("%d leaving from %d\n", num, whrm);	//prints which peoples leave from room


	sem_post(&roomwait[whrm]);		//people post that room available to enter, if there is a waiting people thread, enters to room
}


/*rooms operations, staff threads enter this*/ /**important, I designed like this because there is no obvious connection between rooms**/
void *rooms(int wr){
	//allDone ---> all people are done?, each loop, allDone => allDone - 3
	while(allDone > 0){
		
		int value; 								//semaphore value
      		sem_getvalue(&roomcondition[wr], &value);				//get semaphore value, wr ---> which room 
		if(value == 0){								//if value == 0, then there is no people in room
			printf("Ventilating the room %d\n", wr);			//so, staff ventilates the room
		}

		
		/*staff waits for the room to be provide the condition which is 3 people have to be in the room*/ /*starts with 0*/	
		sem_wait(&roomcondition[wr]);						//wait room condition, 0 -> -1, if people post staff enters
		
		sem_getvalue(&roomcondition[wr], &value); 				//get room condition value
		if(value == 0){								//if it is 0, then there is only 1 person thread in room
			printf("2 more people to start at room %d\n", wr);		
		}
		
		sem_wait(&roomcondition[wr]);						//wait for 2.person				
		
		sem_getvalue(&roomcondition[wr], &value); 
		if(value == 0){
			printf("1 more people to start at room %d\n", wr);
		}

		sem_wait(&roomcondition[wr]);						//wait for 3.person, if person thread post then enters

		printf("%d -Tests are begining!!\n", wr);				//3 people is provided, so tests can begin

		sem_post(&startop[wr]);							//staff posts people that operation(test) starts 
		sem_post(&startop[wr]);							
		sem_post(&startop[wr]);							//3 people

		printf("%d -Test phase!!\n", wr);					//test in progress
		printf("%d times room%d worked\n", toPreventStarving[wr]/3, wr);	//prints that how many times that room worked
		randwait(3);								//waits x seconds to finish the tests
		
		sem_post(&roomendop[wr]);						//staff posts people that operation(test) ended.
		sem_post(&roomendop[wr]);
		sem_post(&roomendop[wr]);

		allDone = allDone - 3;							//3 people are done
	}
}


void randwait(int secs) {
	int len = 1; // Generate an arbit number...
	sleep(len);
}

/*determines the most likely place to place the thread*/
int determineRoom(){

	//get least used room, to prevent starvation
	int checkRoom = leastUsedRoom();
	
	//increase how many people came to the room, if it is 3n, that means the room used n times
	toPreventStarving[checkRoom]++;

	return checkRoom; 	

}
/*determines least used room*/
int leastUsedRoom(){

	int roomValue = 1000;						//that means a room can be used maximum 1000 times
	int roomID = ROOM_COUNT + 1;					//not exit room
	for (int k = 0; k < ROOM_COUNT; k++ ){				//look all of them and pick least used room
		if((int)(toPreventStarving[k] / 3) < roomValue){
			roomValue = toPreventStarving[k] / 3;		//toPreventStarving[k] / 3 -----> n, which means room used n times
			roomID = k;					//update roomID as k
		}	
	}
	if(roomValue == 1000){						//if it is still 1000, so roomValue has not changed
		roomID = 0;						//then pick default room0, this could arrange different, it is choice
	}
	return roomID;							//return roomID
}
