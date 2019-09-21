#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <fcntl.h>

#include "s_header.h"


int main(int argc, char **argv) {
	FILE *fp;
	int i,err,shmid,x,p_period,mantime,total_cap,*ar,position,seconds,mycost;
	int cost_per_h;
	time_t tm;
	char type,post_type,temp_buf[20];
	struct shared_data *sh_data_p;
	struct vessel *ves_p;
	char *sh_p,filename[20];

	if (argc != 11 && argc != 9) { 
		printf("correct syntax: ./vessel -t type -u post_type -p park_period"
		 "-m mantime -s shmid\n");
		exit(-1);
	}
	else {
		for (i=0; i<argc; i++) {
			if(strcmp(argv[i],"-t") == 0) {
				sscanf(argv[i+1],"%c",&type);
			}
			if(strcmp(argv[i],"-u") == 0) {
				sscanf(argv[i+1],"%c",&post_type);
			}
			if(strcmp(argv[i],"-p") == 0){
				sscanf(argv[i+1],"%d",&p_period);
			}
			if(strcmp(argv[i],"-m") == 0) {
				sscanf(argv[i+1],"%d",&mantime);
			}
			if (strcmp(argv[i],"-s") == 0){
				sscanf(argv[i+1],"%d",&shmid);
			}
		}
	}

	sh_p = shmat(shmid,(void*) 0, 0);
	if ( (char*)sh_p == (void *) -1) { printf("vessel: shmat error\n"); exit(-2);}
		// pointers to shared data struct and public ledger
	sh_data_p = (struct shared_data*)sh_p;
	ves_p = (struct vessel*)(sh_p + sizeof(struct shared_data));
					//open log file
	strcpy(filename,sh_data_p->filename);
	fp = fopen(filename,"a");
	if (fp == NULL) {printf("vessel: log file_open error\n");exit(-9);}

	time(&tm); //arrive time
	sem_wait(&(sh_data_p->sem_mem_lock));	//check port's status
	if (sh_data_p->port_status == 0) {
		sem_post(&(sh_data_p->sem_mem_lock));
		printf("port-master is unavailable\ni left the port\n");
		err = shmdt(sh_data_p);
		if (err == -1) {printf("port-master: detach error");exit(-3);}
		exit(0);	
	}
	sem_post(&(sh_data_p->sem_mem_lock));

	sem_post(&(sh_data_p->sem_request)); //sending a request
	sem_post(&(sh_data_p->sem_req_in));	//sending a request for insertion
	sem_wait(&(sh_data_p->sem_master_in)); //waiting for a signal from master
	fprintf(fp,"vessel %d:port-master got my request and now i am waiting \n",
					getpid());fflush(fp);
	sem_wait(&(sh_data_p->sem_mem_lock));
			//writing data to shared struct for port-master 
	sh_data_p->type = type;
	sh_data_p->post_type = post_type;
	sh_data_p->park_period = p_period;
	sh_data_p->arr_time = tm;
	sprintf(sh_data_p->name,"vessel_%d",getpid());

	sem_post(&(sh_data_p->sem_mem_lock));

	sem_post(&(sh_data_p->sem_vessel_in)); //done the writing to the sh_segment
	fprintf(fp,"vessel %d:i wrote my info to sh_mem and im waiting for master to get me in\n",
		getpid()); fflush(fp);
	sem_wait(&(sh_data_p->sem_go_in));	//wait for a signal to go in
	fprintf(fp,"vessel %d:now i am going into port\n",
		getpid()); fflush(fp);
	sem_wait(&(sh_data_p->sem_mem_lock));	// reading the position
	position = sh_data_p->position;
	sem_post(&(sh_data_p->sem_mem_lock));
				//man time
	sleep(mantime);
	fprintf(fp,"vessel %d:i finished manoeuver\n",
					getpid());	fflush(fp);
	sem_post(&(sh_data_p->sem_man)); //end of maneuver

	sem_wait(&(sh_data_p->sem_mem_lock));	//find the cost 
	if (ves_p[position].type == 's') {cost_per_h = sh_data_p->s_cost;}
	if (ves_p[position].type == 'm') {cost_per_h = sh_data_p->m_cost;}
	if (ves_p[position].type == 'l') {cost_per_h = sh_data_p->l_cost;}
	mycost = (int)((difftime(time(NULL),ves_p[position].arr_time)))*cost_per_h;
	//printf("my cost = %d\n",mycost );
	sem_post(&(sh_data_p->sem_mem_lock));
				//parking time
	sleep(p_period);
	fprintf(fp,"vessel %d:i finished my parking\n",
		getpid()); fflush(fp);
				//vessel is now leaving
	fprintf(fp,"vessel %d:i will send a request to port-master to leave the port_status"
		"and wait\n",getpid());fflush(fp);
	sem_post(&(sh_data_p->sem_request));
	sem_post(&(sh_data_p->sem_req_out));
	sem_wait(&(sh_data_p->sem_master_out));
	sem_wait(&(sh_data_p->sem_mem_lock));
	//writing the position of the vessel to shared struct so 
	//port-master knows what vessel info to delete.
	sh_data_p->position_out = position;

	sem_post(&(sh_data_p->sem_mem_lock));
	sem_post(&(sh_data_p->sem_vessel_out));
	fprintf(fp,"vessel %d:i wrote my info to sh_mem and im waiting for master to get me out\n",
		getpid()); fflush(fp);
	sem_wait(&(sh_data_p->sem_go_out));
	fprintf(fp,"vessel %d:now i am leaving the port\n",
		getpid()); fflush(fp);
	sem_post(&(sh_data_p->sem_depart));
	fprintf(fp,"vessel %d:port-master just got noticed that i left the port\n",
		getpid()); fflush(fp);

	err = shmdt(sh_p);
	if (err == -1) {printf("port-master: detach error");exit(-3);}
	fclose(fp);
	exit(0);
}