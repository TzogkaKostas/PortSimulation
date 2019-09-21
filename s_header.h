#include <semaphore.h>

struct vessel {
	int position;
	int cost;
	char name[30];
	time_t arr_time;
	time_t depart_time;
	int wait_time;
	char type;
	int status;
};

struct shared_data {
	int s_cap,m_cap,l_cap; //capacity
	int s_cost,m_cost,l_cost; //costs
	int cur_s,cur_m,cur_l; //current counters(free spots)
	int s_ves,m_ves,l_ves; //counters
	char type,post_type,name[20]; 
	int park_period,position;
	int position_out;
	int s_income,m_income,l_income;
	int port_status;
	time_t arr_time;
	char filename[20];

	sem_t sem_mem_lock;
	sem_t sem_request,sem_req_in,sem_req_out;
	sem_t sem_master_in,sem_master_out;
	sem_t sem_vessel_in,sem_vessel_out;
	sem_t sem_go_in,sem_go_out;
	sem_t sem_man,sem_park,sem_depart;
};
