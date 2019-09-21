#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>

#include "s_header.h"

#define clear() printf("\033[H\033[J")
#define ASK_RATE 10

void read_file(FILE *fp,int *s,int *m,int *l) {
	int i;
	char buffer[6][20];
	for (i=0; fgets(buffer[i], sizeof(buffer),fp)!=NULL; i++) {
	}
	for(i=0; i<3; i++) {
		if (strstr(buffer[i],"S") != NULL) {
			*s = atoi(buffer[i+3]);
		}
		if (strstr(buffer[i],"M") != NULL) {
			*m = atoi(buffer[i+3]);
		}
		if (strstr(buffer[i],"L") != NULL) {
			*l = atoi(buffer[i+3]);
		}
	}
}

int free_position(struct vessel *ves,int s,int e) {
	int i;
	for (i=s; i<e; i++) {
		if (ves[i].status == 0) {
			return i;
		}
	}
	printf("ERROR:no free position\n");
	getchar();
	return s;
}

void print_ves(struct vessel *ves_p,struct shared_data *sh_d,int total_cap) {
	int i;
	for (i=0; i<total_cap; i++) {
		printf("i=%d:",i);
		if (ves_p[i].status == 1) {
			printf("%s %c %d %ld %d",ves_p[i].name,ves_p[i].type,ves_p[i].cost,
				ves_p[i].arr_time,ves_p[i].wait_time);
		}
		else {
			printf("empty");
		}
		printf("\n");
	}
	return;
}


int main(int argc, char **argv) {
	FILE *fp,*fp_hist,*fp_log;
	struct shared_data *sh_data_p;
	struct vessel *ves_p;
	char *sh_p;
	int shmid,i,err,*status,total_cap,*ar,s_cost,m_cost,l_cost,cap1,cap2,cap3;
	int position,cost,seconds,flag,ret_val_in,ret_val_out,found,help,dir,port_status;
	int ret_val,p,vessels_in;
	char charges_file[50],buffer[9][50],type,post_t,free_pos,filename[20],b[100],ch;
	time_t start,secs;

	if (argc != 11) {
		printf("correct syntax: ./port-master -c charges -s shmid -v1 cost1"
			"-v2 cost2 -v3 cost3\n");
		exit(-1);
	}
	else {
		for (i=0; i<argc; i++) {
			if (strcmp(argv[i],"-c") == 0){
				sscanf(argv[i+1],"%s",charges_file);
			}
			if (strcmp(argv[i],"-s") == 0){
				sscanf(argv[i+1],"%d",&shmid);
			}
			if (strcmp(argv[i],"-v1") == 0) {
				sscanf(argv[i+1],"%d",&cap1);
			}
			if (strcmp(argv[i],"-v2") == 0) {
				sscanf(argv[i+1],"%d",&cap2);
			}
			if (strcmp(argv[i],"-v3") == 0) {
				sscanf(argv[i+1],"%d",&cap3);
			}
		}
	}
	total_cap = cap1 + cap2 + cap3;
		// open charges file
	fp = fopen(charges_file,"r");
	if (fp == NULL) {printf("port-master: charges file_open error\n");exit(-8);}
	read_file(fp,&s_cost,&m_cost,&l_cost);
		//open history file
	fp_hist = fopen("history.txt","a");
	if (fp_hist == NULL) {printf("port-master: history file_open error\n");exit(-9);}
		//attach
	sh_p = shmat(shmid,(void*) 0, 0);
	if ( (char*)sh_p == (void *) -1) { printf("port-master: shmat error\n"); exit(-2);}
		// pointers to shared data struct and public ledger
	sh_data_p = (struct shared_data*)sh_p;	
	ves_p = (struct vessel*)(sh_p + sizeof(struct shared_data));
						//open log file
	strcpy(filename,sh_data_p->filename);
	fp_log = fopen(filename,"a");
	if (fp_log == NULL) {printf("vessel: log file_open error\n");exit(-9);}
		// waiting for requests
	port_status = 1;
	vessels_in = 0;
	ch = 'z';
	i = 0;
	flag = 1;
	help = 0;//a flag to know when a vessel is blocked and vessels must get out
	found = 0;//a flag to know when a free spot is found for a blocked vessel
	while (1) {
		if (help == 0 && found == 0) {
			fprintf(fp_log,"master:i am waiting for requests\n");fflush(fp_log);
			sem_wait(&(sh_data_p->sem_request));
		}
		sem_getvalue(&(sh_data_p->sem_req_in),&ret_val_in);
		sem_getvalue(&(sh_data_p->sem_req_out),&ret_val_out);
		if ((ret_val_in > 0 && help == 0) || found == 1)  {
			flag = 0;
			if (found == 0) {
				sem_wait(&(sh_data_p->sem_req_in));
				vessels_in++;
				sem_post(&(sh_data_p->sem_master_in));
				sem_wait(&(sh_data_p->sem_vessel_in));
			}
			sem_wait(&(sh_data_p->sem_mem_lock));
			type = sh_data_p->type;
			post_t = sh_data_p->post_type;
			
			if (type == 's' && (sh_data_p->cur_s > 0) ) {
				free_pos = 's'; flag = 1; (sh_data_p->cur_s)--; (sh_data_p->s_ves)++;
			}
			else if ((type == 'm' || post_t == 'm') && (sh_data_p->cur_m > 0)) {
				free_pos = 'm'; flag = 1; (sh_data_p->cur_m)--; (sh_data_p->m_ves)++;
			}
			else if ((type == 'l' || post_t == 'l') && (sh_data_p->cur_l > 0) ) {
				free_pos = 'l'; flag = 1; (sh_data_p->cur_l)--; (sh_data_p->m_ves)++;
			}
			
			if (flag == 1) {
				if (free_pos == 's') {
					position = free_position(ves_p,0,cap1);
					cost = s_cost*(sh_data_p->park_period);
					sh_data_p->s_income += cost;
				}
				else if (free_pos == 'm') {
					position = free_position(ves_p,cap1,cap1+cap2);
					cost = m_cost*(sh_data_p->park_period);
					sh_data_p->m_income +=cost;
				}
				else if (free_pos == 'l') {
					position = free_position(ves_p,cap1+cap2,cap1+cap2+cap3);
					cost = l_cost*(sh_data_p->park_period);
					sh_data_p->l_income += cost;
				}
						//public ledger writing
				ves_p[position].type = free_pos;
				ves_p[position].status = 1;
				ves_p[position].position = position;
				ves_p[position].cost = cost;
				ves_p[position].arr_time = sh_data_p->arr_time;
				strcpy(ves_p[position].name,sh_data_p->name);
				ves_p[position].wait_time = (int)(difftime(time(NULL),sh_data_p->arr_time));
				sh_data_p->position = position;
						//log file writing
				fprintf(fp_log,"master:i accepted %s's request at position %d\n",
					ves_p[position].name,position);fflush(fp_log);

				sem_post(&(sh_data_p->sem_mem_lock));
				fprintf(fp_log,"master:i will send a signal to %s to get in at %d\n",
					ves_p[position].name,position);fflush(fp_log);
				sem_post(&(sh_data_p->sem_go_in));
				fprintf(fp_log,"master:i am going to wait for %s to get in at %d\n",
					ves_p[position].name,position);fflush(fp_log);
				sem_wait(&(sh_data_p->sem_man)); //wait the vessel to maneuver
				fprintf(fp_log,"master:vessel %s finished maneuver at %d\n",
					ves_p[position].name,position);fflush(fp_log);
				found = 0;
				help = 0;
			}
			else {
				sem_post(&(sh_data_p->sem_mem_lock));
				help = 1;
			}
		}					//"departure" requests
		else if (ret_val_out > 0 || help == 1) {
			if (help ==1 ){
				sem_post(&(sh_data_p->sem_request));}

			sem_wait(&(sh_data_p->sem_req_out));
			vessels_in--;		
			sem_post(&(sh_data_p->sem_master_out));
			sem_wait(&(sh_data_p->sem_vessel_out));
			sem_wait(&(sh_data_p->sem_mem_lock));

			position = sh_data_p->position_out;

			//writing to log file
			fprintf(fp_log,"master:i accepted %s's request at position %d to leave the port\n",
				ves_p[position].name,position);fflush(fp_log);	
			ves_p[position].status = 0;
			ves_p[position].depart_time = time(NULL);
						//keep the history of the port
			p = position;
			fprintf(fp_hist,"%d %s %c %d %ld %ld %d\n",ves_p[p].position,ves_p[p].name,ves_p[p].type,
				ves_p[p].cost,ves_p[p].arr_time,ves_p[p].depart_time,ves_p[p].wait_time);

			if (ves_p[position].type == 's') {
				(sh_data_p->cur_s)++;}
			else if (ves_p[position].type == 'm') {
				(sh_data_p->cur_m)++;}
			else if (ves_p[position].type == 'l') {
				(sh_data_p->cur_l)++;}

			if ((ves_p[position].type == type || ves_p[position].type == post_t) && help == 1) {
				free_pos = ves_p[position].type;
				found = 1;
			}
			sem_post(&(sh_data_p->sem_mem_lock));
			fprintf(fp_log,"master:i will send a signal to %s at %d to get out\n",
				ves_p[position].name,position);fflush(fp_log);
			sem_post(&(sh_data_p->sem_go_out));
			sem_wait(&(sh_data_p->sem_depart));
			fprintf(fp_log,"master:%s at %d just left the port\n",
				ves_p[position].name,position);fflush(fp_log);
		}
		clear();
		printf("sh_m id:%d\n",shmid);
		print_ves(ves_p,sh_data_p,total_cap);
		sleep(1);
		if ((i%ASK_RATE) == 0 && i!=0 &&ch !='y') {
			printf("Do you want to stop?[y/n]\n");
			ch = getchar();
			if (ch =='y') {
				sem_wait(&(sh_data_p->sem_mem_lock));
				sh_data_p->port_status = 0;
				sem_post(&(sh_data_p->sem_mem_lock));
			}
		}
		i = i+1;
		sem_getvalue(&(sh_data_p->sem_req_in),&ret_val_in);
		sem_getvalue(&(sh_data_p->sem_req_out),&ret_val_out);
		if (ret_val_in==0 && ret_val_out==0 && vessels_in == 0) {
			break;
		}
	}
	printf("port is unavailable right now.\n");
		//detach
	err = shmdt(sh_p);
	if (err == -1) {printf("port-master: detach error");exit(-3);}
	fclose(fp); fclose(fp_hist); fclose(fp_log);
	exit(0);
}