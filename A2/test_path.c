#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "assert.h"

/* directions */
enum direction {
    NORTH,
    WEST,
    SOUTH,
    EAST,
    MAX_DIRECTION
};

/* possible turns a car can make */
enum turn {
    NO_TURN,
    LEFT_TURN,
    RIGHT_TURN,
    U_TURN
};

/* Quadrants of intersection */
enum quad {
    QUAD_1 = 1,
    QUAD_2,
    QUAD_3,
    QUAD_4
};

/**
 *  Helper function
 *  Based on turn_type, it mallocs an array containing the correct order of quad numbers that
 *  are required to complete a turn.
 */
int *build_path_array(enum direction in_dir, enum turn turn_type, int arr_size){
    
    // At this point turn_type should be within range: [0, 3]
    assert((int)turn_type >= 0 && (int)turn_type < 4);
    
    // Malloc additional space in array for 0 (NULL) value. Indicates end of array.
    int *path = (int *) malloc(sizeof(int) * (arr_size + 1));
    int i;
    
    switch(turn_type){
            
        case U_TURN:
            printf("switch case: U_TURN\n");
            for (i = 0; i < arr_size; i++) path[i] = i + 1;
            break;
        case RIGHT_TURN:
            printf("switch case: RIGHT_TURN\n");
            if (in_dir == NORTH) {
                path[0] = QUAD_2;
            } else if (in_dir == WEST){
                path[0] = QUAD_3;
            } else if (in_dir == SOUTH){
                path[0] = QUAD_4;
            } else {
                path[0] = QUAD_1;
            }
            break;
        case LEFT_TURN:
            printf("switch case: LEFT/RIGHT_TURN\n");
            if (in_dir == NORTH) {
                path[0] = QUAD_2;
                path[1] = QUAD_3;
                path[2] = QUAD_4;
            } else if (in_dir == WEST){
                path[0] = QUAD_1;
                path[1] = QUAD_3;
                path[2] = QUAD_4;
            } else if (in_dir == SOUTH){
                path[0] = QUAD_1;
                path[1] = QUAD_2;
                path[2] = QUAD_4;
            } else {
                path[0] = QUAD_1;
                path[1] = QUAD_2;
                path[2] = QUAD_3;
            }
            break;
        case NO_TURN:
            printf("switch case: NO_TURN\n");
            if (in_dir == NORTH) {
                path[0] = QUAD_2;
                path[1] = QUAD_3;
            } else if (in_dir == WEST){
                path[0] = QUAD_3;
                path[1] = QUAD_4;
            } else if (in_dir == SOUTH){
                path[0] = QUAD_1;
                path[1] = QUAD_4;
            } else {
                path[0] = QUAD_1;
                path[1] = QUAD_2;
            }
            break;
        default:
            printf("can't recognize direction");
    }
    
    // Set last element in array to NULL to indicate end of list;
    path[arr_size] = 0;
    
    return path;
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
	
    // Precondition: in_dir/out_dir values must be within range: [0, 3]
    assert((int)in_dir >= 0 && (int)in_dir < 4);
    assert((int)out_dir >= 0 && (int)out_dir < 4);

    if (in_dir == out_dir){
        return build_path_array(in_dir, U_TURN, 4);
    } else if (out_dir - in_dir == 1 || out_dir - in_dir == -3){
        return build_path_array(in_dir, RIGHT_TURN, 1);
    } else if (out_dir - in_dir == -1 || out_dir - in_dir == 3){
        return build_path_array(in_dir, LEFT_TURN, 3);
    } else {
        // Implies that car is going straight through the intersection
        return build_path_array(in_dir, NO_TURN, 2);
    }
}

int *path_arr;


int main(void){
	
	// Test U-Turns
	path_arr = compute_path(NORTH, NORTH);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 3 && path_arr[3] == 4);
    
    path_arr = compute_path(WEST, WEST);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 3 && path_arr[3] == 4);
    
    path_arr = compute_path(SOUTH, SOUTH);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 3 && path_arr[3] == 4);
    
    path_arr = compute_path(EAST, EAST);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 3 && path_arr[3] == 4);
    
    // Test RIGHT Turns
    path_arr = compute_path(NORTH, WEST);
    assert(path_arr[0] == 2);
    
    path_arr = compute_path(WEST, SOUTH);
    assert(path_arr[0] == 3);
    
    path_arr = compute_path(SOUTH, EAST);
    assert(path_arr[0] == 4);
    
    path_arr = compute_path(EAST, NORTH);
    assert(path_arr[0] == 1);
    
    // Test LEFT Turns
    path_arr = compute_path(NORTH, EAST);
    assert(path_arr[0] == 2 && path_arr[1] == 3 && path_arr[2] == 4);
    
    path_arr = compute_path(WEST, NORTH);
    assert(path_arr[0] == 1 && path_arr[1] == 3 && path_arr[2] == 4);
    
    path_arr = compute_path(SOUTH, WEST);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 4);
    
    path_arr = compute_path(EAST, SOUTH);
    assert(path_arr[0] == 1 && path_arr[1] == 2 && path_arr[2] == 3);
    
    // Test straight
    path_arr = compute_path(NORTH, SOUTH);
    assert(path_arr[0] == 2 && path_arr[1] == 3);
    
    path_arr = compute_path(WEST, EAST);
    assert(path_arr[0] == 3 && path_arr[1] == 4);
    
    path_arr = compute_path(SOUTH, NORTH);
    assert(path_arr[0] == 1 && path_arr[1] == 4);
    
    path_arr = compute_path(EAST, WEST);
    assert(path_arr[0] == 1 && path_arr[1] == 2);
    
    
	return 0;
}


