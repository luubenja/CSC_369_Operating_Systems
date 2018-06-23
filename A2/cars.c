#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"
#include "assert.h"
#include <errno.h>

#define LOCK 1
#define UNLOCK 0

extern struct intersection isection;

/* Possible turns a car can make */
enum turn {
    RIGHT_TURN = 1,
    NO_TURN,
    LEFT_TURN,
    U_TURN
};

/* Quadrants of intersection ordered in priority 1 > 2 > 3 > 4 */
enum quad {
    QUAD_1 = 1,
    QUAD_2,
    QUAD_3,
    QUAD_4
};

enum turn compute_turn(enum direction in_dir, enum direction out_dir);

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

	int i;

	for (i = 0; i < 4; i++){

		// Initialize locks and condition vars for intersection and lanes 
        pthread_mutex_init(&isection.quad[i], NULL);
		pthread_mutex_init(&isection.lanes[i].lock, NULL);
		pthread_cond_init(&isection.lanes[i].producer_cv, NULL);
		pthread_cond_init(&isection.lanes[i].consumer_cv, NULL); 
	
		// Set capacity of each lane to LANE_LENGTH
		isection.lanes[i].capacity = LANE_LENGTH;
        
        // Initialize buffer of each lane
        isection.lanes[i].buffer = malloc(sizeof(struct car *) * LANE_LENGTH);
        
	}

}

/**
 *  Helper Function - write_to_buf
 *
 *  Write the address of the head car in the in_cars list to
 *  this lane's buffer.
 *
 */
void write_to_buf(struct lane *l){
    
    if (l->in_buf != l->capacity){
        l->buffer[l->tail] = l->in_cars;
        l->in_buf++;
        l->in_cars = (l->in_cars)->next;
        l->tail = (l->tail + 1) % l->capacity;
    }
    
}

/**
 *  Helper Function - read_from_buf
 *
 *  Read (and return address of) the next car at head index from
 *  lane's buffer.
 *
 */
struct car *read_from_buf(struct lane *l){
    
    if (l->in_buf != 0){
        struct car *temp;
        temp = l->buffer[l->head];
        l->head = (l->head + 1) % l->capacity;
        return temp;
    }
    
    return NULL;
}

/**
 *  Helper Function - set_isection_locks
 *
 *  Depending on value of set_to_lock (lock = 1 or unlock = 0), execute
 *  desired pthread routine (mutex_lock or mutex_unlock) on each quad lock
 *  as ordered by path_arr.
 *
 */
int set_isection_locks(int quads_req, int *path_arr, int set_to_lock){
    
    int (*mutex_fn)(pthread_mutex_t *) = set_to_lock ? pthread_mutex_lock : pthread_mutex_unlock;
    int i, lock_setting_count = 0;
        
    for (i = 0; i < quads_req; i++){
        mutex_fn(&isection.quad[path_arr[i]]);
    }
    
    if (quads_req != (lock_setting_count = i)){
        return -EINVAL;
    }
    
    return lock_setting_count;
    
}

/**
 *  Helper Function - complete_cross
 *
 *  Add to the head of cross_lane new car.
 *
 */
int complete_cross(struct car *car, struct lane *cross_lane){
    
    car->next = cross_lane->out_cars;
    cross_lane->out_cars = car;
    
    return 1;
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
    
    int i;
    
    // Continue producing for inc amount of loops
    for(i = 0; i < l->inc; i++){
        
        pthread_mutex_lock(&l->lock);
        
        while(l->in_buf == l->capacity){
            pthread_cond_wait(&l->producer_cv, &l->lock);
        }
        
        // Load car from in_cars list into buffer
        write_to_buf(l);

        pthread_cond_signal(&l->consumer_cv);
        pthread_mutex_unlock(&l->lock);
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
    
    int i, locks_acquired, locks_released, cross_complete, num_of_quads;
    int *path;
    
    // Continue consuming for inc amount of loops
    for (i = 0; i < l->inc; i++){
        
        pthread_mutex_lock(&l->lock);
        
        while(!(l->in_buf)){
            pthread_cond_wait(&l->consumer_cv, &l->lock);
        }
        
        // Get next car to cross intersection from this lane's buffer
        struct car *crossing_car = read_from_buf(l);
        
        if (crossing_car == NULL){
            fprintf(stderr, "Unable to read next crossing car");
            pthread_mutex_unlock(&l->lock);
            exit(1);
        }

        l->in_buf--;

        // Release lock and signal producer. At this stage, producer can continue
        // it's work, while the consumer attempts to acquire quad locks.
        pthread_cond_signal(&l->producer_cv);
        pthread_mutex_unlock(&l->lock);
        
        // Get the required quads count and sorted path array containing lock order
        num_of_quads = (int) compute_turn(crossing_car->in_dir, crossing_car->out_dir);
        path = compute_path(crossing_car->in_dir, crossing_car->out_dir);
        
        // Lock required quadrants before attempting intersection cross
        if ((locks_acquired = set_isection_locks(num_of_quads, path, LOCK)) != num_of_quads){
            fprintf(stderr, "Unable to acquire locks for car ID: %d", crossing_car->id);
            exit(1);
        }
        
        // Insert car into the outgoing lanes out_cars list
        if (!(cross_complete = complete_cross(crossing_car, &isection.lanes[crossing_car->out_dir]))){
            fprintf(stderr, "Unable to complete isection cross for car ID: %d", crossing_car->id);
            exit(1);
        }
        
        l->passed++;
        
        // Once intersection cross is complete, release the associated locks
        if ((locks_released = set_isection_locks(num_of_quads, path, UNLOCK)) != locks_acquired){
            fprintf(stderr, "Unable to release locks after crossing car ID: %d", crossing_car->id);
            exit(1);
        }
        
        printf("%d %d %d\n", crossing_car->in_dir, crossing_car->out_dir, crossing_car->id);
        
        // Free mem allocated for path array
        free(path);
    }
    
    // Free mem allocated for buffer
    free(l->buffer);

    return NULL;
}

/**
 *  Helper Function - build_path_array
 *
 *  Based on turn_type, it mallocs an array containing the correct order of quad numbers that
 *  are required to complete a turn.
 *
 */
int *build_path_array(enum direction in_dir, enum turn turn_type){
    
    int arr_size = turn_type;
    int i = 0;
    
    // Malloc space for path array
    int *path = (int *) malloc(sizeof(int) * arr_size);
    
    // Set path values based on in_dir and turn_type
    switch(turn_type){
            
        case U_TURN:
            // A U_TURN requires all quads (priority: 1 > 2 > 3 > 4)
            for (i = 0; i < arr_size; i++) path[i] = i + 1;
            break;
        case RIGHT_TURN:
            switch(in_dir){
                case NORTH:
                    path[0] = QUAD_2;
                    break;
                case WEST:
                    path[0] = QUAD_3;
                    break;
                case SOUTH:
                    path[0] = QUAD_4;
                    break;
                case EAST:
                    path[0] = QUAD_1;
                    break;
                default:
                    fprintf(stderr, "Illegal in_dir #: %u", in_dir);
                    exit(1);
            }
            break;
        case LEFT_TURN:
            switch(in_dir){
                case NORTH:
                    path[0] = QUAD_2;
                    path[1] = QUAD_3;
                    path[2] = QUAD_4;
                    break;
                case WEST:
                    path[0] = QUAD_1;
                    path[1] = QUAD_3;
                    path[2] = QUAD_4;
                    break;
                case SOUTH:
                    path[0] = QUAD_1;
                    path[1] = QUAD_2;
                    path[2] = QUAD_4;
                    break;
                case EAST:
                    path[0] = QUAD_1;
                    path[1] = QUAD_2;
                    path[2] = QUAD_3;
                    break;
                default:
                    fprintf(stderr, "Illegal in_dir #: %u", in_dir);
                    exit(1);
            }
            break;
        case NO_TURN:
            switch(in_dir){
                case NORTH:
                    path[0] = QUAD_2;
                    path[1] = QUAD_3;
                    break;
                case WEST:
                    path[0] = QUAD_3;
                    path[1] = QUAD_4;
                    break;
                case SOUTH:
                    path[0] = QUAD_1;
                    path[1] = QUAD_4;
                    break;
                case EAST:
                    path[0] = QUAD_1;
                    path[1] = QUAD_2;
                    break;
                default:
                    fprintf(stderr, "Illegal in_dir #: %u", in_dir);
                    exit(1);
            }
            break;
        default:
            fprintf(stderr, "Illegal turn_type #: %u", turn_type);
            exit(1);
    }

    return path;
}

/**
 *  Helper Function - compute_turn
 *
 *  Returns the type of turn a car makes based on in_dir and out_dir.
 *
 */
enum turn compute_turn(enum direction in_dir, enum direction out_dir){
    
    // Precondition: in_dir/out_dir values must be within range: [0, 3]
    if (((int)in_dir < 0 || (int)in_dir > 3) || ((int)out_dir < 0 || (int)out_dir > 3)){
        fprintf(stderr, "Invalid direction(s) provided: %u, %u", in_dir, out_dir);
        exit(1);
    }
    
    if (in_dir == out_dir)
        return U_TURN;
    if ((in_dir + 1) % 4 - out_dir == 0)
        return RIGHT_TURN;
    if (in_dir - (out_dir + 1) % 4 == 0)
        return LEFT_TURN;
    
    // Implies that car is going straight through the intersection (no turns)
    return NO_TURN;
    
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
    
    enum turn turn_type = compute_turn(in_dir, out_dir);
    
    return build_path_array(in_dir, turn_type);
    
}

