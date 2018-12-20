#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Node */ 
typedef struct node { 
	int data; // pid
	int priority; // vruntime

	struct node* next; 
}Node; 

Node* newNode(int d, int p) 
{ 
	Node* tmp = (Node*)malloc(sizeof(Node)); 
	tmp->data = d; 
	tmp->priority = p; 
	tmp->next = NULL; 

	return tmp; 
}

/* Return the value at head */
int peek(Node** head) 
{ 
	return (*head)->data; 
} 

/* Removes the element by priority */
void pop(Node** head) 
{ 
	Node* tmp = *head; 
	(*head) = (*head)->next; 
	free(tmp); 
} 

/* Push node order to priority */ 
void push(Node** head, int d, int p) 
{ 
	Node* start = (*head); 
 
	Node* tmp = newNode(d, p); // create new Node

	if ((*head)->priority > p) {	// if head's priority is less than new one's 
		tmp->next = *head; 
		(*head) = tmp; 
	} 
	else { // find a position for new one 
		while (start->next != NULL && start->next->priority < p) { 
			start = start->next; 
		}
		tmp->next = start->next; 
		start->next = tmp; 
	} 
} 

typedef struct sched_entity {
	int weight;
	int on_rq; // 0 is not on rq, 1 is on rq
	int now_running; // 1 is now running
	int time_slice;
	int vruntime;
}sched_entity;

typedef struct process {
	int pid;
	int nice;
	int start_time; // use time()
	int running_time;
	int finish_time; // count for process finish
	struct sched_entity se;
}process;


static const int prio_to_weight[40] = {
/* -20 */ 88761, 71755, 56483, 46273, 36291,
/* -15 */ 29154, 23254, 18705, 14949, 11916,
/* -10 */ 9548, 7620, 6100, 4904, 3906,
/* -5 */ 3121, 2501, 1991, 1586, 1277,
/* 0 */ 1024, 820, 655, 526, 423,
/* 5 */ 335, 272, 215, 172, 137,
/* 10 */ 110, 87, 70, 56, 45,
/* 15 */ 36, 29, 23, 18, 15,
};

void Erase(struct process *pro) {
	pro->se.weight = 0;
	pro->se.on_rq = 0;
	pro->se.time_slice = 0;
	pro->se.vruntime = 0;
}

int main(void) {

	struct process *pro = (process *)malloc(sizeof(struct process) * 50);

	int i = 0, j, num_rq = 0, tot_weight = 0;
	int sched_latency_ns = 18000000;
	int sched_min_granularity = 2250000;
	int sched_nr_latency = 8; /* sched_latency_ns/sched_min_granularity */
	int periods, min_vruntime;
	int what_is_running, del_pid;
	
	Node *que = newNode(0,-5); // it will be poped after the first process node comes; -5 is smaller than 0(first vruntime)

	while (1){ // every cycle is divided by user's input and user can fork or kill process per execution of one process
		if (num_rq > 0) { // update periods by the number of processes on rq
			periods = (num_rq <= sched_nr_latency) ? sched_latency_ns : sched_min_granularity * num_rq;
			for (j = 0; j < i; j++) { // update time slice per every cycle
				pro[j].se.time_slice = (int)((double)periods * (double)pro[j].se.weight / (double)tot_weight);
			}
		}

		printf("1: Add process\n2: Run\n3: Turn off(process)\n4: Turn off(scheduler)\n5: Show Processes\n");
		int input;
		scanf("%d", &input);

		if (input == 1) { // Add process
			pro[i].pid = i;
			printf("\nwhat is nice value of the process?");
			scanf("%d", &pro[i].nice);
			pro[i].start_time = (int)time(NULL);
			pro[i].running_time = 0;
			printf("\nHow long the process running to finish?");
			scanf("%d", &pro[i].finish_time);
			pro[i].se.weight = prio_to_weight[pro[i].nice + 20];
			pro[i].se.on_rq = 1;
			pro[i].se.now_running = 0;
			tot_weight += pro[i].se.weight;
			pro[i].se.time_slice = (int)((double)periods * (double)pro[i].se.weight / (double)tot_weight);

			if (num_rq == 0) {
				pro[i].se.vruntime = 0; // no process in rq
				push(&que, pro[i].pid, pro[i].se.vruntime); // the first node with pid and vruntime
				pop(&que);
			}
			else { // find min_vruntime for new process
				for (j = 0; j < i; j++) {
					if (pro[j].se.on_rq == 1) {
						break;
					}
				}
				min_vruntime = pro[j].se.vruntime;
				for (int k = j; k < i; k++){
					if (pro[k].se.on_rq == 1) {
						min_vruntime = (min_vruntime <= pro[k].se.vruntime) ? min_vruntime : pro[k].se.vruntime;
					}
				}
				pro[i].se.vruntime = min_vruntime;
				push(&que, pro[i].pid, pro[i].se.vruntime);
			}

			i++;
			num_rq++;
		}
		else if (input == 2) { //Run
			what_is_running = peek(&que);
			pro[what_is_running].se.now_running = 1;
			pro[what_is_running].se.vruntime += (int)((double)(pro[what_is_running].se.time_slice) * (double)(1024) / (double)(pro[what_is_running].se.weight));
			pro[what_is_running].se.now_running = 0;
			pro[what_is_running].running_time += pro[what_is_running].se.time_slice;

			/* To update the node's priority */
			pop(&que);
			push(&que, pro[what_is_running].pid, pro[what_is_running].se.vruntime);
			if (pro[what_is_running].running_time >= pro[what_is_running].finish_time) { // a process completes its execution
				printf("\nProcess %d completes execution.\n", pro[what_is_running].pid);
				pro[what_is_running].se.on_rq = 0;
				pop(&que);
				tot_weight -= pro[what_is_running].se.weight;
				Erase(&pro[what_is_running]);
				num_rq--;
			}

			what_is_running = 0;
		}
		else if (input == 3) { // kill the process by user's input
			printf("\ntype which process to stop: ");
			scanf("%d", &del_pid);

			pro[del_pid].se.on_rq = 0;
			pop(&que);
			tot_weight -= pro[del_pid].se.weight;
			Erase(&pro[del_pid]);
			num_rq--;
		}
		else if (input == 4) { // kill the scheduler
			printf("\nscheduler will stop in 3 secs.");
			sleep(3);
			break;
		}
		else if (input == 5) { // display which processes are on rq and their attr.
			if (i > 0) {
				printf("\nlist of processes\n");
				for (j = 0; j < i; j++) {
					if (pro[j].se.on_rq == 1) {
						printf("pid: %d / start time: %d / running time: %d / weight: %d / time slice: %d / vruntime: %d\n", pro[j].pid, pro[j].start_time, pro[j].running_time, pro[j].se.weight, pro[j].se.time_slice, pro[j].se.vruntime);
					}
				}
			}
		}
	}
	return 0;
}
