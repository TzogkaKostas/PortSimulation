#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "s_header.h"

#define clear() printf("\033[H\033[J")

void print_ves(struct vessel *ves_p,struct shared_data *sh_d) {
	int i,total_cap;
	total_cap = sh_d->s_cap+sh_d->m_cap+sh_d->l_cap;
	for (i=0; i<total_cap; i++) {
		printf("i=%d:",i);
		if (ves_p[i].status == 1) {
			printf("%d %ld %d %c %s",ves_p[i].cost,
				ves_p[i].arr_time,ves_p[i].wait_time,ves_p[i].type,
				ves_p[i].name);
		}
		else {
			printf("empty");
		}
		printf("\n");
	}
	return;
}

void wait_time(struct vessel *ves_p,struct shared_data *sh_d,int total_cap,
	int *total,int *small,int *medium,int *large){
	int i;
	*total = 0; *small = 0; *medium = 0; *large = 0;
	for (i=0; i<total_cap; i++) {
		*total += ves_p[i].wait_time;
		if (ves_p[i].type == 's') {
			*small += ves_p[i].wait_time;
		}
		else if (ves_p[i].type == 'm') {
			*medium += ves_p[i].wait_time;
		}
		else if (ves_p[i].type == 'l') {
			*large += ves_p[i].wait_time; 
		}
	}
	return;
}

volatile sig_atomic_t sig_flag = 1;

void sig_handler() {
	sig_flag = 0;
}


int main(int argc, char **argv) {
	int shmid,time,stat_times,i,err,total_wait_t,s_wait_t,m_wait_t,l_wait_t;
	int cur_total_cap,min,max,total_cap,total_vessels;
	double avg_income,s_avg_income,m_avg_income,l_avg_income,avg_wait,s_avg_wait,m_avg_wait,l_avg_wait;
	char *sh_p;
	struct vessel *ves_p;
	struct shared_data *sh_data_p;

	signal(SIGINT,sig_handler);
	if (argc != 7) {
		printf("correct syntax: ./monitor -d time -t statstime -s shmid\n");
		exit(-1);
	}

	else {
		for (i=0; i<argc; i++) {
			if (strcmp(argv[i],"-d") == 0){
				sscanf(argv[i+1],"%d",&time);
			}
			if (strcmp(argv[i],"-t") == 0){
				sscanf(argv[i+1],"%d",&stat_times);
			}
			if (strcmp(argv[i],"-s") == 0){
				sscanf(argv[i+1],"%d",&shmid);
			}
		}
	}
	if (stat_times > time) {printf("monitor:time has to be smaller than stat_time\n");exit(-3);}
	sh_p = shmat(shmid,(void*) 0, 0);
	if ( (char*)sh_p == (void *) -1) { printf("monitor : shmat error\n"); exit(-2);}

	sh_data_p = (struct shared_data*)sh_p;	
	ves_p = (struct vessel*)(sh_p + sizeof(struct shared_data));

	while(sig_flag == 1) {
		sleep(time);
		sem_wait(&(sh_data_p->sem_mem_lock));
		clear();
		print_ves(ves_p,sh_data_p);
		sleep(time - stat_times);
		//statistics
		total_cap = sh_data_p->s_cap+sh_data_p->m_cap+sh_data_p->l_cap;
		total_vessels = sh_data_p->s_ves+sh_data_p->m_ves+sh_data_p->l_ves;	
		wait_time(ves_p,sh_data_p,total_cap,&total_wait_t,&s_wait_t,&m_wait_t,&l_wait_t);
		if (total_vessels != 0) {
			avg_income =  (sh_data_p->s_income + sh_data_p->m_income + sh_data_p->l_income)/total_vessels;
			avg_wait = total_wait_t/total_vessels;
		}
		else {
			avg_income = 0;avg_wait=0;}
		if (sh_data_p->s_ves!= 0) {
			s_avg_income = (double)sh_data_p->s_income/sh_data_p->s_ves;
			s_avg_wait = (double)s_wait_t/sh_data_p->s_ves;}
		else { 
			s_avg_income = 0;s_avg_wait=0;}
		if (sh_data_p->m_ves!= 0) {
			m_avg_income = (double)sh_data_p->m_income/sh_data_p->m_ves;
			m_avg_wait = (double)m_wait_t/sh_data_p->m_ves;
		}
		else {
			m_avg_income = 0;m_avg_wait = 0;}
		if (sh_data_p->l_ves!= 0) {
			l_avg_income = (double)sh_data_p->l_income/sh_data_p->l_ves;
			l_avg_wait = (double)l_wait_t/sh_data_p->l_ves;
		}
		else {
			l_avg_income = 0;l_avg_wait=0;}
		printf("total income:%d\ntotal avg income:%f\nsmall avg income:%f\n"
			"medium avg income:%f\nlarge avg income%f\ntotal avg wait time:%f\n"
			"small avg wait time:%f\nmedium avg wait time:%f\nlarge avg wait time:%f\n",
			(sh_data_p->s_income + sh_data_p->m_income + sh_data_p->l_income),
			avg_income,s_avg_income,m_avg_income,l_avg_income,avg_wait,s_avg_wait,
			m_avg_wait,l_avg_wait);
		sem_post(&(sh_data_p->sem_mem_lock));
	}
	printf("Monitor:Finished\n");
	err = shmdt(sh_p);
	if (err == -1) {printf("monitor: detach error");exit(-3);}
	exit(0);
}