#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <semaphore.h>
#include <syscall.h>
#define RUNTIME 10
#define PERIOD 100//ms
#define SECTOMSEC 1e3
#define SECTONSEC 1e9
#define MSECTONSEC 1e6
#define STR_SIZE 200 

uint32_t log_id=0;
struct timespec start_time={0,0};
struct timespec current_time={0,0};
sem_t sem_thread2;
sem_t sem_logfile;
sem_t sem_stat;
time_t present_time;
struct tm *time_and_date;

void* analyze_file(void* ptr)
{
	uint8_t* hash=(uint8_t*)calloc(26,1);
	uint8_t* str=(uint8_t*)calloc(STR_SIZE,1);
	uint8_t temp=0;
	uint32_t i=0,n=0,file_size=0;
	pid_t process_id=getpid();
	pid_t thread_id=syscall(SYS_gettid);
	FILE* fptr=fopen("gdb.txt", "r");
	fseek(fptr,0,SEEK_END);
	file_size=ftell(fptr);
	fseek(fptr,0,SEEK_SET);
	for(i=0;i<file_size;i++)
	{
		n=fread(&temp,1,1,fptr);
		if((temp>='a')&&(temp<='z'))
		{
			*(hash+temp-'a')+=1;
		}
		else if ((temp>='A')&&(temp<='Z'))
		{
			*(hash+temp-'A')+=1;
		}
	}
	fclose(fptr);	
	sem_wait(&sem_logfile);
	printf("Thread 1\n");
	fptr=fopen((uint8_t*)ptr,"a");
	present_time=time(NULL);
	time_and_date = localtime(&present_time);
	sprintf(str,"\n\nLog ID : %d\tThread 1\nTime & Date : %sProcess Id = %d\tThread ID = %d\nThe alphabets that occurs less than 100 times are:\n",log_id++,asctime(time_and_date),process_id,thread_id);
	n=fwrite(str,1,strlen(str),fptr);
	bzero((void *)str,STR_SIZE);
	for(i=0;i<26;i++)
	{
		if(*(hash+i)<100)
		{
			sprintf(str,"%c:%d\t",'a'+i,*(hash+i));
			n=fwrite(str,1,strlen(str),fptr);	
		}
	}
	fclose(fptr);
	sem_post(&sem_logfile);
	free(str);
	free(hash);
	return (void *)ptr;
}

void* cpu_util(void* ptr)
{
	clock_gettime(CLOCK_REALTIME,&current_time);
	uint8_t* str=(uint8_t*)calloc(STR_SIZE,1);
	uint8_t temp=0;
	uint32_t n=0,file_size=0,i=0;
	FILE* fptr,*stat;
	pid_t process_id=getpid();
	pid_t thread_id=syscall(SYS_gettid);
	sem_post(&sem_thread2);
	while(current_time.tv_sec - start_time.tv_sec < RUNTIME)
	{	
		sem_wait(&sem_thread2);
		sem_wait(&sem_logfile);
		printf("Thread 2\n");
		fptr=fopen((uint8_t*)ptr,"a");
		present_time=time(NULL);
		time_and_date = localtime(&present_time);
		sprintf(str,"\n\nLog ID : %d\tThread 2\nTime & Date : %sProcess Id = %d\tThread ID = %d\nCPU Utilization:\n",log_id++,asctime(time_and_date),process_id,thread_id);		
		n=fwrite(str,1,strlen(str),fptr);		
		sem_wait(&sem_stat);
		system("cat /proc/stat>cpu_util.txt");
		stat=fopen("cpu_util.txt", "r");		
		fseek(stat,0,SEEK_END);
		file_size=ftell(stat);
		fseek(stat,0,SEEK_SET);
		for(i=0;i<file_size;i++)
		{
			n=fread(&temp,1,1,stat);
			n=fwrite(&temp,1,1,fptr);
		}
		fclose(stat);
		sem_post(&sem_stat);
		fclose(fptr);
		sem_post(&sem_logfile);
		clock_gettime(CLOCK_REALTIME,&current_time);
	}
	return (void *)ptr;
}

static void run_thread2(int sig, siginfo_t *si, void *uc)
{
	sem_post(&sem_thread2);
}

int32_t main(int32_t argc, uint8_t **argv)
{
	pthread_t thread_1,thread_2;
	int32_t error=0;
	uint8_t dummy=0;	
	void* ptr=&dummy;
	uint8_t* filename;
	timer_t timerid;
	struct sigevent signal_event;
	struct itimerspec timer_data;
	struct sigaction signal_action;
	sigset_t mask;
	if(argc==1)
	{
		printf("Format:%s <filename> \n",*argv);
		kill(getpid(),SIGINT);
	}
	else
	{
		filename=*(argv+1);
	}
	// Timer init	
	signal_action.sa_flags = SA_SIGINFO;
	signal_action.sa_sigaction = run_thread2;//Function to be executed
	sigemptyset(&signal_action.sa_mask);
	if (sigaction(SIGRTMIN, &signal_action, NULL) == -1)
        {
		printf("sigaction\n");
		kill(getpid(),SIGINT);
	}
	sigemptyset(&mask);
        sigaddset(&mask, SIGRTMIN);
        if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
	{        
		printf("sigprocmask\n");
		kill(getpid(),SIGINT);
	}
        signal_event.sigev_notify = SIGEV_SIGNAL;
        signal_event.sigev_signo = SIGRTMIN;
        signal_event.sigev_value.sival_ptr = &timerid;
        if (timer_create(CLOCK_REALTIME, &signal_event, &timerid) == -1)
	{        
		printf("timer_create\n");
		kill(getpid(),SIGINT);
	}
	/* Start the timer */
        timer_data.it_value.tv_sec = 0;
        timer_data.it_value.tv_nsec = PERIOD*MSECTONSEC;
        timer_data.it_interval.tv_sec = timer_data.it_value.tv_sec;
        timer_data.it_interval.tv_nsec = timer_data.it_value.tv_nsec;
	if (timer_settime(timerid, 0, &timer_data, NULL) == -1)
	{	
        	printf("timer_settime,\n");
		kill(getpid(),SIGINT);
	}
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
        {	
        	printf("sigprocmask\n");
		kill(getpid(),SIGINT);
	}
	// Timer init done
	// Create Threads

	error = pthread_create(&thread_1, NULL, analyze_file, (void*)filename);
	if(error)
	{
		printf("Error Creating Thread 1\n");
		kill(getpid(),SIGINT);
	}
	error = pthread_create(&thread_2, NULL, cpu_util, (void*)filename);
	if(error)
	{
		printf("Error Creating Thread 2\n");
		kill(getpid(),SIGINT);
	}
	//Start threads	
	error=remove(filename); 
	sem_post(&sem_logfile);
	sem_post(&sem_stat);
	clock_gettime(CLOCK_REALTIME,&start_time);
	pthread_join(thread_1,&ptr);
	pthread_join(thread_2,&ptr);

	// exit process

	kill(getpid(),SIGINT);
	return 0;
}

/*References-
http://man7.org/linux/man-pages/man2/timer_create.2.html

*/
