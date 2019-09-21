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

#define NUM_VESSEL 40

void read_file(FILE *fp,int *s,int *m,int *l,int *s2,int *m2,int *l2) {
	int i;
	char buffer[9][50];
	for (i=0; fgets(buffer[i], sizeof(buffer),fp)!=NULL; i++) {
	}
	for(i=0; i<3; i++) {
		if (strstr(buffer[i],"S") != NULL) {
			*s = atoi(buffer[i+3]);
			*s2 = atoi(buffer[i+6]);
		}
		if (strstr(buffer[i],"M") != NULL) {
			*m = atoi(buffer[i+3]);
			*m2 = atoi(buffer[i+6]);
		}
		if (strstr(buffer[i],"L") != NULL) {
			*l = atoi(buffer[i+3]);
			*l2 = atoi(buffer[i+6]);
		}
	}
}

int main(int argc, char **argv) {
	FILE *fp,*fp_log;
	sem_t *my_sem;
	struct shared_data *sh_data_p;
	struct vessel *ves_p;
	char *sh_p;
	char config_file[50],charges_file[50],shmid_buf[10],p_buf[20],m_buf[20],cap_buf[20];
	char v1_buf[20],v2_buf[20],v3_buf[20],rand_type[3][5],type_buf[5],mon_time_buf[20];
	char mon_stat_buf[20],post_buf[5],t_temp[5],fd_buffer[10];
	pid_t pid1,pid2,pid3;
	int shmid,ret_v,i,cost1,cost2,cost3,s_cap,m_cap,l_cap,share_size,*temp;
	int err,total_cap,mantime,park_period,monitor_time,mon_stat_time;

	strcpy(rand_type[0],"s"); strcpy(rand_type[1],"m"); strcpy(rand_type[2],"l");

	if (argc != 5) {
		printf("correct syntax: ./myport -l configfile -c charges_file\n");
		exit(-1);
	}
	else {
		for (i=0; i<argc; i++) {
			if (strcmp(argv[i],"-l") == 0){
				strcpy(config_file,argv[i+1]);
			}
			if (strcmp(argv[i],"-c") == 0){
				strcpy(charges_file,argv[i+1]);
			}
		}
	}
	fp = fopen(config_file,"r");
	if (fp == NULL) {printf("myport:config file_open error\n");exit(-8);}
	read_file(fp,&s_cap,&m_cap,&l_cap,&cost1,&cost2,&cost3);

			//open log file
	fp_log = fopen("log.txt","w+");
	if (fp_log == NULL) {printf("vessel: log file_open error\n");exit(-9);}

	total_cap= s_cap + m_cap + l_cap;
	share_size = sizeof(struct shared_data) + total_cap*sizeof(struct vessel);
	shmid = shmget(IPC_PRIVATE,share_size,IPC_CREAT | 0666);
	if (shmid == -1) {printf("myport: shmget error\n");exit(-5);}

	sh_p = shmat(shmid,(void*) 0, 0);
	if ( (char*)sh_p == (char*) -1) { printf("myport: shmat error\n"); exit(-2);}

	sh_data_p = (struct shared_data*)sh_p;
				//semaphore initialization
	ret_v = sem_init(&(sh_data_p->sem_mem_lock),1,1);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_request),1,0);
	if (ret_v == -1) {printf("myport: sem_init1 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_req_in),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_req_out),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_master_in),1,0);
	if (ret_v == -1) {printf("myport: sem_init1 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_master_out),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_vessel_in),1,0);
	if (ret_v == -1) {printf("myport: sem_init1 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_vessel_out),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_go_in),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_go_out),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_man),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_park),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}
	ret_v = sem_init(&(sh_data_p->sem_depart),1,0);
	if (ret_v == -1) {printf("myport: sem_init3 error\n");exit(-7);}

	sh_data_p->cur_s = s_cap; sh_data_p->cur_m = m_cap; sh_data_p->cur_l = l_cap; 
	sh_data_p->s_cap = s_cap; sh_data_p->m_cap = m_cap; sh_data_p->l_cap = l_cap;
	sh_data_p->s_cost = cost1; sh_data_p->m_cost = cost2; sh_data_p->l_cost = cost3;
	sh_data_p->s_income = 0; sh_data_p->m_income = 0; sh_data_p->l_income = 0;
	sh_data_p->port_status = 1; //this means that the port is open for requests
	strcpy(sh_data_p->filename,"log.txt");

	sprintf(shmid_buf,"%d",shmid);
	sprintf(v1_buf,"%d",s_cap);sprintf(v2_buf,"%d",m_cap);sprintf(v3_buf,"%d",l_cap);
	if ((pid1=fork()) == -1) {printf("myport: fork1(port-master) error\n");exit(-2);}
	if (pid1 == 0) {
		sprintf(cap_buf,"%d",total_cap);
		execl("port-master","port-master","-s",shmid_buf,"-c",charges_file,
			"-v1",v1_buf,"-v2",v2_buf,"-v3",v3_buf,NULL);
		printf("myport: execl port-master error\n");
		exit(-11);
	}

	for (i=0; i<NUM_VESSEL; i++) { 
		if ((pid2=fork()) == -1) {printf("myport: fork2(vessel) error\n");exit(-3);}
		if (pid2 == 0) {
			srand(getpid());
			sleep(rand()%10);
			mantime = rand()%5; sprintf(m_buf,"%d",mantime);
			park_period = rand()%5;sprintf(p_buf,"%d",park_period);
			
			strcpy(type_buf,rand_type[rand()%3]);
			if (strcmp(type_buf,"s") == 0) {
				if (rand()%2 == 1) {
					strcpy(post_buf,rand_type[rand()%2+1]);}
				else {
					strcpy(post_buf,"z");}
			}
			else if(strcmp(type_buf,"m") == 0) {
				if (rand()%2 == 1) {
					strcpy(post_buf,"l");}
				else {
					strcpy(post_buf,"z");}
			}
			else if(strcmp(type_buf,"l") == 0) {
				strcpy(post_buf,"z");
			}		
			execl("vessel","vessel","-t",type_buf,"-u",post_buf,"-p",p_buf,"-m",m_buf,"-s"
				,shmid_buf,NULL);
			printf("myport: execl vessel error\n");
			exit(-12);
		}
	}
	
	/*if ((pid3=fork()) == -1) {printf("myport: fork3(monitor) error\n");exit(-4);}
	if (pid3 == 0) {
		monitor_time = 2; sprintf(mon_time_buf,"%d",monitor_time);
		mon_stat_time = 6; sprintf(mon_stat_buf,"%d",mon_stat_time);
		execl("monitor","monitor","-d",mon_time_buf,"-t",mon_stat_buf,"-s",shmid_buf,
			NULL);
		printf("myport: execl monitor error\n");
		exit(-13);
	}*/

	printf("%d\n",shmid );

	for (i=0; i<NUM_VESSEL+1; i++) { wait(NULL);} //wait for all vessels and port-master to terminate

	sem_destroy(&(sh_data_p->sem_mem_lock));
	sem_destroy(&(sh_data_p->sem_request));
	sem_destroy(&(sh_data_p->sem_req_in));
	sem_destroy(&(sh_data_p->sem_req_out));
	sem_destroy(&(sh_data_p->sem_master_in));
	sem_destroy(&(sh_data_p->sem_master_out));
	sem_destroy(&(sh_data_p->sem_vessel_in));
	sem_destroy(&(sh_data_p->sem_vessel_out));
	sem_destroy(&(sh_data_p->sem_go_in));
	sem_destroy(&(sh_data_p->sem_go_out));
	sem_destroy(&(sh_data_p->sem_man));
	sem_destroy(&(sh_data_p->sem_park));
	sem_destroy(&(sh_data_p->sem_depart));
	printf("all semaphores got destroyed\n");
	err = shmctl(shmid, IPC_RMID, 0);
	if (err == -1){printf("myport: shmctl error\n");exit(-8);}
	fclose(fp_log);
	exit(0);
}
