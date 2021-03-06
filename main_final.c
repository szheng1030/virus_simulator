#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

volatile int pixel_buffer_start;

void wait_for_vsync();
void draw_line(int x0, int y0, int x1, int y1, short colour);
void swap(int* a, int* b);
void plot_pixel(int x, int y, short color);
void plot_char(int x, int y, char c);
void clear_screen();
void clear_boxes(int x, int y);
void draw_banner();
void draw_border();
void draw_mobile_boundary(int y);
void erase_mobile_boundary(int y);
void draw_banner_text();
void update_banner_data(int healthy, int sick, int recovered);
void init_banner_graph(int healthy, int sick, int recovered);
void reduce_banner_graph(int* healthy, int* prev_healthy, int* sick,
			int* prev_sick, int* recovered, int* prev_recovered);
void extend_banner_graph(int healthy, int prev_healthy, int sick,
			int prev_sick, int recovered, int prev_recovered);
int get_seg7_code(int value);

const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int BANNER_HEIGHT = 36;
const int BORDER_OFFSET = 5;
const int BOX_WIDTH = 5;			// Must be ODD (to calculate centre of box)
const int NUM_BOXES = 50;
const int INITIAL_SICK = 3;
const int SIMU_WIDHT = 308;
const int SIMU_HEIGHT = 192;

double SICK_DURATION = 100.0;
int STATIONARY_COUNT = 5;

int main(void) {

	//Set up A9 timer
	volatile int * a9_timer_load = (int *)0xFFFEC600;
	volatile int * a9_timer_settings = (int *)0xFFFEC608;
	volatile int * a9_timer_read = (int *)0xFFFEC60C;
	*(a9_timer_load) = 2000000;				// Load Value
	*(a9_timer_settings) = 3;				// Auto on, generate output in F

	//Set up seg7 display
	volatile int * seg7_3to0 = (int *)0xFF200020;

	//Set up pixel control buffer
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	pixel_buffer_start = *pixel_ctrl_ptr;

	printf("WELCOME TO OUR VIRUS SIMULATOR!\n");
	printf("CLICK AND RELEASE ANY OF THE KEYS TO INITIALIZE THE SCREEN\n");
	// wait for a KEY to be pushed //
	volatile int* key_ptr = (int*)0xFF200050;
	int key_value = *key_ptr;
	while(!key_value){
		key_value = *key_ptr;
	}
	// wait for the KEY to be released
	while(key_value){
		key_value = *key_ptr;
	}


	/* Structure and data used to represent a person */
	struct Box {
		int x;						// Current xPos, 0-315
		int y;						// Current yPos, 0-235
		int x_mid;
		int y_mid;					// (x_mid, y_mid) is the centre of the box
		int dx;						// Movement in xDir, -1 or 1
		int dy;						// Movement in yDir, -1 or 1
		int state; 					// Current state (Healthy, Sick, Recovered)
		int sick_timer;
		bool boundary_collision_x;
		bool boundary_collision_y;
		bool stationary;
	};

	struct Mobile_Boundary{
		int y;
		int dy;
	};

	struct Box box[NUM_BOXES];
	int healthy_count = 0;
	int sick_count = 0;
	int recovered_count = 00;
	int prev_healthy_count = 0;
	int prev_sick_count = 0;
	int prev_recovered_count = 0;

	struct Mobile_Boundary mobile_boundary;
	mobile_boundary.y = 0;
	mobile_boundary.dy = 0;

	int second = 0;
	int second_ten = 0;
	int minute = 0;
	int minute_ten = 0;

	/* Create intial conditions for each box (person) */
	/* Define: Healthy = 0, Sick = 1, Recovered = 2 */
	srand(time(NULL));

	/* Randomly designate people to begin as "sick" */
	int init_sick[INITIAL_SICK];
	for (int i = 0; i < INITIAL_SICK; i++) {
		init_sick[i] = rand() % NUM_BOXES;
	}

	clear_screen();
	wait_for_vsync();
	draw_banner();
	draw_border();
	wait_for_vsync();
	draw_banner_text();
	
	printf("------------------------------------------------------------------\n");
	printf("YOU CAN NOW SET THE NUMBER OF STATIONARY PEOPLE:\n");
	printf("YOU WILL USE SW[9:5] TO DO THIS. SET ONLY ONE OF THESE SWITCHES TO HIGH AND THEN PRESS AND RELEASE ANY KEY\n");
	printf("REMEMBER: ALL OTHER SWITCHES MUST BE LOW\n");
	printf("SW9 = 0 STATIONARY PEOPLE\n");
	printf("SW8 = 5 STATIONARY PEOPLE\n");
	printf("SW7 = 10 STATIONARY PEOPLE\n");
	printf("SW6 = 15 STATIONARY PEOPLE\n");
	printf("SW5 = 20 STATIONARY PEOPLE\n");
	////  SET STATIONARY COUNT ////
	// all boxes are initially NOT stationary
	for(int i = 0; i < NUM_BOXES; i++){
		box[i].stationary = false;
	}
	
	volatile int* sw_ptr = (int*)0xFF200040;
	int switches_value = *sw_ptr;
	key_value = *key_ptr;
	
	while(!key_value){
		if(switches_value == 512)
			STATIONARY_COUNT = 0;
		else if(switches_value == 256)
			STATIONARY_COUNT = 5;
		else if(switches_value == 128)
			STATIONARY_COUNT = 10;
		else if(switches_value == 64)
			STATIONARY_COUNT = 15;
		else if(switches_value == 32)
			STATIONARY_COUNT = 20;
		   
		switches_value = *sw_ptr;
		key_value = *key_ptr;
	}
	// wait for the KEY to be released
	while(key_value){
		key_value = *key_ptr;
	}
	
	int counter = STATIONARY_COUNT;
	// Make STATIONARY_COUNT boxes stationary
	for(int i = 0; i < counter; i++){
		int index = (rand() % NUM_BOXES);

		if(!box[index].stationary)
			box[index].stationary = true;
		else
			counter++;
	}
	//// END STATIONARY ////

	
	//// SET SICK_DURATION ////
	printf("------------------------------------------------------------------\n");
	printf("NOW, USE SW[9:6] TO SET THE DURATION OF THE DISEASE\n");
	printf("SW9 IS THE SHORTEST AND SW6 IS THE LONGEST\n");
	printf("REMEMBER: ALL OTHER SWITCHES MUST BE LOW\n");
	printf("PRESS AND RELEASE ANY KEY WHEN YOU HAVE CHOSEN THE DURATION\n");
	switches_value = *sw_ptr;
	while(!key_value){
		if(switches_value == 512)
			SICK_DURATION = 50.0;
		else if(switches_value == 256)
			SICK_DURATION = 100.0;
		else if(switches_value == 128)
			SICK_DURATION = 150.0;
		else if(switches_value == 64)
			SICK_DURATION = 200.0;

		switches_value = *sw_ptr;
		key_value = *key_ptr;
	}
	// wait for the KEY to be released
	while(key_value){
		key_value = *key_ptr;
	}
	//// END SICK_DURATION ////

	
	//// SET MOVING BOUNDARY ////
	printf("------------------------------------------------------------------\n");
	printf("YOU'RE NOW IN CONTROL OF THE TURQUOISE BAR AT THE BOTTOM\n");
	printf("USE SW0 TO MOVE THE BAR UP, AND SW1 TO MOVE IT DOWN\n");
	printf("ONCE YOU'RE HAPPY WITH THE BAR'S POSITION, SET SW2 TO HIGH TO START THE SIMULATION\n");
	printf("REMEMBER: ALL OTHER SWITCHES MUST BE LOW\n");
	switches_value = *sw_ptr;
	while(switches_value != 4){
		erase_mobile_boundary(mobile_boundary.y);

		switches_value = *sw_ptr;
		//printf("%d\n", switches_value);

		if(switches_value == 1)
			mobile_boundary.dy = 1;
		if(switches_value == 2)
			mobile_boundary.dy = -1;
		if(switches_value == 0 || switches_value > 2
		   || (mobile_boundary.y >= SCREEN_HEIGHT/2 && mobile_boundary.dy == 1)
		   || (mobile_boundary.y <= 0 && mobile_boundary.dy == -1))
		   mobile_boundary.dy = 0;

		mobile_boundary.y += mobile_boundary.dy;
		
		draw_mobile_boundary(mobile_boundary.y);
		wait_for_vsync();
	}
	//// END SET MOVING BOUNDARY ////

	
	/* Initialization of values */
	int border_difference = ((SCREEN_HEIGHT - 1) - BORDER_OFFSET - mobile_boundary.y) - (BANNER_HEIGHT + BORDER_OFFSET);
	for (int i = 0; i < NUM_BOXES; i++) {
		box[i].x = (i % 10 + 1) * 30;
		box[i].y = 48 + (i / 10) * (border_difference) / 5;		// mobile_boundary.y
		box[i].dx = rand() % 2 * 2 - 1;
		box[i].dy = rand() % 2 * 2 - 1;
		/*
		for (int j = 0; j < STATIC_COUNT; j++) {
			if (i == static_box[j]) {
				box[i].dx = 0;
				box[i].dy = 0;
			}
		}*/
		if(box[i].stationary){
			box[i].dx = 0;
			box[i].dy = 0;
		}

		box[i].sick_timer = SICK_DURATION;
		box[i].state = 0;

		for (int j = 0; j < INITIAL_SICK; j++) {
			if (i == init_sick[j])
				box[i].state = 1;
		}

		// Count number of boxes in each health state
		if (box[i].state == 0) {
			healthy_count++;
			prev_healthy_count++;
		}
		else if (box[i].state == 1) {
			sick_count++;
			prev_sick_count++;
		}
		else if (box[i].state == 2) {
			recovered_count++;
			prev_recovered_count++;
		}
	}

	init_banner_graph(healthy_count, sick_count, recovered_count);
	
	
	//// MAIN LOOP ////
	printf("------------------------------------------------------------------\n");
	printf("HERE WE GO! HAVE FUN!\n");
	
    while (sick_count > 0) {

		// timer
		if (*a9_timer_read != 0) {
			second += 1;
			if (second == 10) {
				second_ten += 1;
				second = 0;
			}
			if (second_ten == 10) {
				minute += 1;
				second_ten = 0;
			}
			if (minute == 10) {
				minute_ten += 1;
				minute = 0;
			}
			if (minute_ten == 10)
				*a9_timer_settings = 0;
			*a9_timer_read = 1;
		}

		*seg7_3to0 = get_seg7_code(minute_ten) * pow(2, 24) +
					get_seg7_code(minute) * pow(2, 16) +
					get_seg7_code(second_ten) * pow(2, 8) +
					get_seg7_code(second);



		// Erase boxes at their previous location
		for (int i = 0; i < NUM_BOXES; i++) {
			clear_boxes(box[i].x, box[i].y);
		}

		// Update positions and states
		for (int i = 0; i < NUM_BOXES; i++) {

			// Increment or decrement x and y
			box[i].x += box[i].dx;
			box[i].y += box[i].dy;

			// Check if box has hit screen borders
			if (box[i].x >= (SCREEN_WIDTH - 1) - BOX_WIDTH - BORDER_OFFSET || box[i].x <= BORDER_OFFSET + 1){
			 	box[i].dx = -box[i].dx;
				box[i].boundary_collision_x = true;
			}
			else
				box[i].boundary_collision_x = false;

			if (box[i].y >= (SCREEN_HEIGHT - 1) - BOX_WIDTH - BORDER_OFFSET - mobile_boundary.y || box[i].y <= BANNER_HEIGHT + 1 + BORDER_OFFSET){
				box[i].dy = -box[i].dy;
				box[i].boundary_collision_y = true;
			}
			else
				box[i].boundary_collision_y = false;


			// Box to Box Collision
			for (int j = 0; j < NUM_BOXES; j++) {
				if (i != j) {
					if( sqrt(pow((box[i].x + 2) - (box[j].x + 2), 2) + pow((box[i].y + 2) - (box[j].y + 2), 2)) <= 5){

						// Quadrant 1
						if(box[i].x <= box[j].x && box[i].y >= box[j].y){
							if(!box[i].stationary){
								if(box[i].boundary_collision_x)
									box[i].dx = 0;
								else
									box[i].dx = -1;
								if(box[i].boundary_collision_y)
									box[i].dy = 0;
								else
									box[i].dy = 1;
							}
							if(!box[j].stationary){
								if(box[j].boundary_collision_x)
									box[j].dx = 0;
								else
									box[j].dx = 1;
								if(box[j].boundary_collision_y)
									box[j].dy = 0;
								else
									box[j].dy = -1;
							}
						}
						// Quadrant 2
						else if(box[i].x >= box[j].x && box[i].y >= box[j].y){
							if(!box[i].stationary){
								if(box[i].boundary_collision_x)
									box[i].dx = 0;
								else
									box[i].dx = 1;
								if(box[i].boundary_collision_y)
									box[i].dy = 0;
								else
									box[i].dy = 1;
							}
							if(!box[j].stationary){
								if(box[j].boundary_collision_x)
									box[j].dx = 0;
								else
									box[j].dx = -1;
								if(box[j].boundary_collision_y)
									box[j].dy = 0;
								else
									box[j].dy = -1;
							}
						}
						// Quadrant 3
						else if(box[i].x >= box[j].x && box[i].y <= box[j].y){
							if(!box[i].stationary){
								if(box[i].boundary_collision_x)
									box[i].dx = 0;
								else
									box[i].dx = 1;
								if(box[i].boundary_collision_y)
									box[i].dy = 0;
								else
									box[i].dy = -1;
							}
							if(!box[j].stationary){
								if(box[j].boundary_collision_x)
									box[j].dx = 0;
								else
									box[j].dx = -1;
								if(box[j].boundary_collision_y)
									box[j].dy = 0;
								else
									box[j].dy = 1;
							}
						}
						// Quadrant 4
						else if(box[i].x <= box[j].x && box[i].y <= box[j].y){
							if(!box[i].stationary){
								if(box[i].boundary_collision_x)
									box[i].dx = 0;
								else
									box[i].dx = -1;
								if(box[i].boundary_collision_y)
									box[i].dy = 0;
								else
									box[i].dy = -1;
							}
							if(!box[j].stationary){
								if(box[j].boundary_collision_x)
									box[j].dx = 0;
								else
									box[j].dx = 1;
								if(box[j].boundary_collision_y)
									box[j].dy = 0;
								else
									box[j].dy = 1;
							}
						}

						// Check for virus spread
						if ( (box[i].state == 0 && box[j].state == 1) ||
							 (box[i].state == 1 && box[j].state == 0) ) {
								 box[i].state = 1;
								 box[j].state = 1;
								 sick_count++;
								 healthy_count--;
						}
					}
				}
			}

			// Increment sick timer
			if (box[i].state == 1)
				box[i].sick_timer--;
			if (box[i].sick_timer == 0) {
				box[i].sick_timer = -1;
				box[i].state = 2;
				sick_count--;
				recovered_count++;
			}
		}

		// Draw boxes
		for (int i = 0; i < NUM_BOXES; i++) {
			for (int j = 0; j < BOX_WIDTH; j++) {
				for (int k = 0; k < BOX_WIDTH; k++) {
					if (box[i].state == 0)			// Healthy
						plot_pixel(box[i].x + j, box[i].y + k, 0xCE79);
					else if (box[i].state == 1)		// Sick
						plot_pixel(box[i].x + j, box[i].y + k, 0xF800);
					else if (box[i].state == 2)		// Recovered
						plot_pixel(box[i].x + j, box[i].y + k, 0x07E8);
				}
			}
		}

		// Update numbers to reflect virus spread and recoveries
		update_banner_data(healthy_count, sick_count, recovered_count);


		// Extend graph on banner if numbers increased
		extend_banner_graph(healthy_count, prev_healthy_count, sick_count,
					prev_sick_count, recovered_count, prev_recovered_count);

		// Erase part of graph if numbers decreased
		reduce_banner_graph(&healthy_count, &prev_healthy_count, &sick_count,
					&prev_sick_count, &recovered_count, &prev_recovered_count);

		// Synchronize prev and current counters
		prev_healthy_count = healthy_count;
		prev_sick_count = sick_count;
		prev_recovered_count = recovered_count;

		// Wait for pixels to be drawn
		wait_for_vsync();
    }
	//// END MAIN LOOP ////
	
	printf("------------------------------------------------------------------\n");
	printf("THE SIMULATION IS NOW OVER. CHECK OUT THE GRAPH TO STUDY THE OUTCOME.\n");
	printf("FEEL FREE TO RUN THE SIMULATION AGAIN. THERE ARE NUMEROUS VARIABLES, SO THE OUTCOME WILL MOST LIKELY NOT BE THE SAME!\n");
	
	// wait for user to exit/stop program
	while(true){}
}


/* Wait for Vertical Synchronization */
void wait_for_vsync(){
	volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
	register int status;

	*pixel_ctrl_ptr = 1;

	status = *(pixel_ctrl_ptr + 3);
	while( (status& 0x01) != 0){
		status = *(pixel_ctrl_ptr + 3);
	}
}


/* Bresenham’s algorithm */
void draw_line(int x0, int y0, int x1, int y1, short color){
	bool is_steep = abs(y1 - y0) > abs(x1 - x0);
	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int delta_x = x1-x0;
	int delta_y = abs(y1-y0);
	int error = -(delta_x / 2);
	int y = y0;
	int y_step = (y0 < y1) ? 1 : -1;

	/* ^^^ Simplified version w/o if
	int y_step = 0;
	if(y0 < y1)
		y_step = 1;
	else
		y_step = -1;
	*/

	for (int x = x0; x <= x1; x++) {
		if(is_steep)
			plot_pixel(y, x, color);
		else
			plot_pixel(x, y, color);

		error = error + delta_y;
		if (error >= 0) {
			y = y + y_step;
			error = error - delta_x;
		}
	}
}


/* Swap 2 integers */
void swap(int* a, int* b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}


/* Draw a pixel */
void plot_pixel(int x, int y, short int color) {
    *(short *)(pixel_buffer_start + (y << 10) + (x << 1)) = color;
}


/* Draw a character */
void plot_char(int x, int y, char c) {
	*(char *)(0xC9000000 + (y << 7) + x) = c;
}


/* Clear screen by changing all pixels to black */
void clear_screen() {
	for(int i = 0; i < SCREEN_WIDTH; i++) {
		for(int j = 0; j < SCREEN_HEIGHT; j++) {
			plot_pixel(i, j, 0x0000);
		}
	}
}


/* Clear a box by drawing black in its previous position */
void clear_boxes(int x, int y) {
	for (int i = 0; i < BOX_WIDTH; i++) {
		for (int j = 0; j < BOX_WIDTH; j++) {
			plot_pixel(x + i, y + j, 0x0000);
		}
	}
}


/* Draw 240x36 banner background to display data */
void draw_banner() {
	for (int i = 0; i < SCREEN_WIDTH; i++) {
		for (int j = 0; j < BANNER_HEIGHT; j++) {
			plot_pixel(i, j, 0x8C71);
		}
	}
}


/* Draw border for simulation */
void draw_border() {
	for (int i = BORDER_OFFSET; i < SCREEN_WIDTH - BORDER_OFFSET; i++) {
		plot_pixel(i, BANNER_HEIGHT + BORDER_OFFSET, 0xE73C);
		plot_pixel(i, (SCREEN_HEIGHT - 1) - BORDER_OFFSET, 0xE73C);
	}
	for (int j = BANNER_HEIGHT + BORDER_OFFSET; j < SCREEN_HEIGHT - BORDER_OFFSET; j++) {
		plot_pixel(BORDER_OFFSET, j, 0xE73C);
		plot_pixel((SCREEN_WIDTH - 1) - BORDER_OFFSET, j, 0xE73C);
	}
}

void draw_mobile_boundary(int y){
	for(int i = BORDER_OFFSET+1; i < (SCREEN_WIDTH - 1) - BORDER_OFFSET; i++){
		plot_pixel(i, (SCREEN_HEIGHT - 1) - BORDER_OFFSET - y, 0x0F3F);
	}
}

void erase_mobile_boundary(int y){
	for(int i = BORDER_OFFSET+1; i < (SCREEN_WIDTH - 1) - BORDER_OFFSET; i++){
		plot_pixel(i, (SCREEN_HEIGHT - 1) - BORDER_OFFSET - y, 0x0000);
	}
}


/* Draw the static text displayed on the banner */
void draw_banner_text() {
	plot_char(3, 1, 67);		// C
	plot_char(4, 1, 79);		// O
	plot_char(5, 1, 85);		// U
	plot_char(6, 1, 78);		// N
	plot_char(7, 1, 84);		// T

	plot_char(3, 3, 72);		// H
	plot_char(4, 3, 101);		// e
	plot_char(5, 3, 97);		// a
	plot_char(6, 3, 108);		// l
	plot_char(7, 3, 116);		// t
	plot_char(8, 3, 104);		// h
	plot_char(9, 3, 121);		// y
	plot_char(10, 3, 58);		// :

	plot_char(3, 5, 83);		// S
	plot_char(4, 5, 105);		// i
	plot_char(5, 5, 99);		// c
	plot_char(6, 5, 107);		// k
	plot_char(7, 5, 58);		// :

	plot_char(3, 7, 82);		// R
	plot_char(4, 7, 101);		// e
	plot_char(5, 7, 99);		// c
	plot_char(6, 7, 111);		// o
	plot_char(7, 7, 118);		// v
	plot_char(8, 7, 101);		// e
	plot_char(9, 7, 114);		// r
	plot_char(10, 7, 101);		// e
	plot_char(11, 7, 100);		// d
	plot_char(12, 7, 58);		// :

	plot_char(30, 1, 71);		// G
	plot_char(31, 1, 82);		// R
	plot_char(32, 1, 65);		// A
	plot_char(33, 1, 80);		// P
	plot_char(34, 1, 72);		// H
}


/* Draw the changing numbers displayed on the banner */
void update_banner_data(int healthy, int sick, int recovered) {
	plot_char(12, 3, healthy / 10 + 48);
	plot_char(13, 3, healthy % 10 + 48);

	plot_char(30, 3, healthy / 10 + 48);
	plot_char(31, 3, healthy % 10 + 48);

	plot_char(9, 5, sick / 10 + 48);
	plot_char(10, 5, sick % 10 + 48);

	plot_char(30, 5, sick / 10 + 48);
	plot_char(31, 5, sick % 10 + 48);

	plot_char(14, 7, recovered / 10 + 48);
	plot_char(15, 7, recovered % 10 + 48);

	plot_char(30, 7, recovered / 10 + 48);
	plot_char(31, 7, recovered % 10 + 48);
}


/* Initializie graph on the banner */
void init_banner_graph(int healthy, int sick, int recovered) {
	for (int i = 135; i < 135 + healthy; i++) {
		plot_pixel(i, 13, 0xFFFF);
		plot_pixel(i, 14, 0xFFFF);
	}

	for (int i = 135; i < 135 + sick; i++) {
		plot_pixel(i, 21, 0xF800);
		plot_pixel(i, 22, 0xF800);
	}

	for (int i = 135; i < 135 + recovered; i++) {
		plot_pixel(i, 29, 0x07E8);
		plot_pixel(i, 30, 0x07E8);
	}
}


/* Reduce graph on banner if numbers decreased */
void reduce_banner_graph(int* healthy, int* prev_healthy, int* sick,
			int* prev_sick, int* recovered, int* prev_recovered) {

	if (*prev_healthy > *healthy) {
		for (int i = 135 + *healthy; i < 135 + *prev_healthy; i++) {
			plot_pixel(i, 13, 0x8C71);
			plot_pixel(i, 14, 0x8C71);
		}
	}

	if (*prev_sick > *sick) {
		//printf("sick bar should derease\n");
		for (int i = 135 + *sick; i < 135 + *prev_sick; i++) {
			plot_pixel(i, 21, 0x8C71);
			plot_pixel(i, 22, 0x8C71);
		}
	}

	if (*prev_recovered > *recovered) {
		for (int i = 135 + *recovered; i < 135 + *prev_recovered; i++) {
			plot_pixel(i, 29, 0x8C71);
			plot_pixel(i, 30, 0x8C71);
		}
	}
}


/* Extend graph on banner if numbers increased */
void extend_banner_graph(int healthy, int prev_healthy, int sick,
			int prev_sick, int recovered, int prev_recovered) {
	if (healthy > prev_healthy) {
		for (int i = 135 + prev_healthy; i < 135 + healthy; i++) {
			plot_pixel(i, 13, 0xFFFF);
			plot_pixel(i, 14, 0xFFFF);
		}
	}

	if (sick > prev_sick) {
		for (int i = 135 + prev_sick; i < 135 + sick; i++) {
			plot_pixel(i, 21, 0xF800);
			plot_pixel(i, 22, 0xF800);
		}
	}

	if (recovered > prev_recovered) {
		for (int i = 135 + prev_recovered; i < 135 + recovered; i++) {
			plot_pixel(i, 29, 0x07E8);
			plot_pixel(i, 30, 0x07E8);
		}
	}
}

int get_seg7_code(int value) {
	switch(value) {
		case 0:
			return 63;
		case 1:
			return 6;
		case 2:
			return 91;
		case 3:
			return 79;
		case 4:
			return 102;
		case 5:
			return 109;
		case 6:
			return 125;
		case 7:
			return 7;
		case 8:
			return 127;
		case 9:
			return 111;
		default:
			return 63;
	}
}
