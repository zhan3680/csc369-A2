#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with 
 * its in_direction
 * 
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    struct lane *cur_lane;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {

        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;

        /* append new car to head of corresponding list */
        cur_lane = &isection.lanes[in_dir];
        cur_car->next = cur_lane->in_cars;
        cur_lane->in_cars = cur_car;
        cur_lane->inc++;
    }

    fclose(f);
}






/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 * 
 */
void init_intersection() {
    /*initialize p_thread_mutex*/    
    int i;
    for(i = 0; i < 4; i++){
        pthread_mutex_init(&((isection.quad)[i]), NULL);
    }

    /*initialize all four lanes*/
    int j;
    for(j = 0; j < 4; j++){
        pthread_mutex_init(&((isection.lanes)[j].lock), NULL);
        (isection.lanes)[j].in_cars = NULL;
        (isection.lanes)[j].out_cars = NULL;
        (isection.lanes)[j].inc = 0;
        (isection.lanes)[j].passed = 0;
        (isection.lanes)[j].buffer = malloc(sizeof(struct car*)*LANE_LENGTH);
        int k;
        for(k = 0; k < LANE_LENGTH; k++){
            ((isection.lanes)[j].buffer)[k] = NULL;    
        }
        (isection.lanes)[j].head = 0;
        (isection.lanes)[j].tail = -1;
        (isection.lanes)[j].capacity = LANE_LENGTH;
        (isection.lanes)[j].in_buf = 0;
    }          
}






/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 * 
 */
void *car_arrive(void *arg) {
    struct lane *l = arg;

    /* avoid compiler warning */
    l = l;

    while(l->in_cars != NULL){
	    /*move car form in_cars to buffer if there is space in buffer, wait otherwise*/
	    pthread_mutex_lock(&(l->lock));  

	    /*check if there is space in the buffer*/
	    while(l->in_buf == l->capacity){
		pthread_cond_wait(&(l->producer_cv),&(l->lock));        
	    }

	    /*when space is available, move car from in_cars to buffer*/
	    if(l->in_cars != NULL){
		l->tail = (l->tail+1)%(l->capacity); 
		(l->buffer)[l->tail] = l->in_cars;
		l->in_cars = (l->in_cars)->next;
		l->in_buf += 1;
	    }

	    /*don't forget to signal and release the lock*/
	    pthread_cond_signal(&(l->consumer_cv));
	    pthread_mutex_unlock(&(l->lock));
    }

    return NULL;
}






/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 * 
 * Note: For testing purposes, each car which gets to cross the 
 * intersection should print the following three numbers on a 
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 * 
 * You may add other print statements, but in the end, please 
 * make sure to clear any prints other than the one specified above, 
 * before submitting your final code. 
 */
void *car_cross(void *arg) {
    struct lane *l = arg;

    /* avoid compiler warning */
    l = l;

    while(1){
            /*try to get access to the intersection if there is car in the buffer, wait otherwise*/
	    pthread_mutex_lock(&(l->lock));
 

            /*all in_cars have finished crossing*/
            if(l->in_cars == NULL && l->passed == l->inc){
                pthread_mutex_unlock(&(l->lock));
                break; 
            }
       	    
	    /*check if there is any car waiting in the buffer*/    
	    while(l->in_buf == 0){
		pthread_cond_wait(&(l->consumer_cv),&(l->lock));      
	    }

	    /*try to get access to the intersection (i.e make sure there is no other car on the path of first car in the buffer*/
	    struct car *first_car_in_line = l->buffer[l->head]; 
	    int *path = compute_path(first_car_in_line->in_dir, first_car_in_line->out_dir);
	    if(path == NULL){
		printf("something is wrong with neither inplementation or car data\n");
		pthread_mutex_unlock(&(l->lock)); 
		exit(1);
	    }
	    int i,section;
	    for(i = 0; i < 3; i++){
		section = path[i];
		if(section != -1){
		    pthread_mutex_lock(&((isection.quad)[section-1])); //section-1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!    
		}
	    }

	    /*after we make sure that the path is clear, we can start crossing the intersection*/

	    /*printing message*/
	    printf("%d %d %d\n",first_car_in_line->in_dir, first_car_in_line->out_dir, first_car_in_line->id);
	    
	    /*add first car in line to out_cars of its out_dir*/
	    struct lane *destination = &((isection.lanes)[first_car_in_line->out_dir]);
	    first_car_in_line->next = destination->out_cars; 
	    destination->out_cars = first_car_in_line;

	    /*after finishing crossing the intersection, release the locks in opposite order in which they are acquired*/
	    int j,section2;
	    for(j = 2; j >= 0; j--){
		section2 = path[j];
		if(section2 != -1){
		    pthread_mutex_unlock(&((isection.quad)[section2-1]));    //section2-1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		}
	    }
	    free(path);

	    /*remove first car in line from buffer*/
	    l->head = (l->head+1)%(l->capacity);
	    l->in_buf -= 1;
	    l->passed += 1; 
	    
	    /*don't forget to signal and release the lock*/
	    pthread_cond_signal(&(l->producer_cv));
	    pthread_mutex_unlock(&(l->lock)); 
    }


    return NULL;
}






/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
    int *path = malloc(sizeof(int)*3);
    int i;
    for(i = 0; i < 3; i++){
        path[i] = -1;
    } 
    if(in_dir == NORTH){
       if(out_dir == NORTH){
           free(path);
           printf("u-turn!\n");
           return NULL;
       }else if(out_dir == SOUTH){
           path[0] = 2;
           path[1] = 3;
           path[2] = -1;
       }else if(out_dir == WEST){
           path[0] = 2;
           path[1] = -1;
           path[2] = -1;       
       }else if(out_dir == EAST){
           path[0] = 2;
           path[1] = 3;
           path[2] = 4;
       }else{
           free(path);
           printf("invalid out_dir!\n");
           return NULL;   
       }
    }else if(in_dir == SOUTH){
       if(out_dir == NORTH){
           path[0] = 1;
           path[1] = 4;
           path[2] = -1; 
       }else if(out_dir == SOUTH){
           free(path);
           printf("u-turn!\n");
           return NULL;
       }else if(out_dir == WEST){
           path[0] = 1;
           path[1] = 2;
           path[2] = 4;       
       }else if(out_dir == EAST){
           path[0] = 4;
           path[1] = -1;
           path[2] = -1;
       }else{
           free(path);
           printf("invalid out_dir!\n");
           return NULL;  
       }
    }else if(in_dir == WEST){
       if(out_dir == NORTH){
           path[0] = 1;
           path[1] = 3;
           path[2] = 4; 
       }else if(out_dir == SOUTH){
           path[0] = 3;
           path[1] = -1;
           path[2] = -1; 
       }else if(out_dir == WEST){
           free(path);
           printf("u-turn!\n");
           return NULL;    
       }else if(out_dir == EAST){
           path[0] = 3;
           path[1] = 4;
           path[2] = -1; 
       }else{
           free(path);
           printf("invalid out_dir!\n");
           return NULL;  
       }
    }else if(in_dir == EAST){
       if(out_dir == NORTH){
           path[0] = 1;
           path[1] = -1;
           path[2] = -1; 
       }else if(out_dir == SOUTH){
           path[0] = 1;
           path[1] = 2;
           path[2] = 3; 
       }else if(out_dir == WEST){
           path[0] = 1;
           path[1] = 2;
           path[2] = -1;    
       }else if(out_dir == EAST){
           free(path);
           printf("u-turn!\n");
           return NULL; 
       }else{
           free(path);
           printf("invalid out_dir!\n");
           return NULL;  
       }
    }else{
        free(path);
        printf("invalid in_dir!\n");
        return NULL;
    }
    return path;
}


