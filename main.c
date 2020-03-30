#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

volatile int pixel_buffer_start; // global variable

void wait_for_vsync();
void draw_line(int x0, int y0, int x1, int y1, short colour);
void swap(int* a, int* b);
void plot_pixel(int x, int y, short color);
void plot_char(int x, int y, char c);
void clear_screen();
void clear_boxes(int x, int y);
void draw_banner();
void draw_border();
void draw_banner_data();
void update_banner_data(int healthy, int sick, int recovered);

const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int BANNER_HEIGHT = 36;
const int BORDER_OFFSET = 5;
const int BOX_WIDTH = 5;
const int NUM_BOXES = 35;
const int INITIAL_SICK = 3;
const double SICK_DURATION = 1000.0;

const int SIMU_WIDHT = 308;
const int SIMU_HEIGHT = 192;

int main(void) {

	// Double buffer currently not working, using single for now
	/* Set up and pixel controller and buffer
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    *(pixel_ctrl_ptr + 1) = 0xC8000000; 			// Set up On-chip buffer
    wait_for_vsync();								// Swap front/back
    pixel_buffer_start = *pixel_ctrl_ptr;			// Initialize ptr to buffer
    clear_screen(); 								// Clear ptr buffer
    *(pixel_ctrl_ptr + 1) = 0xC8000000;				// Set up SDRAM buffer
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
	clear_screen();									// Clear other buffer */

	//Set up pixel control buffer
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	pixel_buffer_start = *pixel_ctrl_ptr;

	/* Structure and data used to represent a person */
	struct Box {
		int x;						// Current xPos, 0-315
		int y;						// Current yPos, 0-235
		int dx;						// Movement in xDir, -1 or 1
		int dy;						// Movement in yDir, -1 or 1
		int state; 				// Current state (Healthy, Sick, Recovered)
		int sick_timer;
	};

	struct Box box[NUM_BOXES];

	/* Create intial conditions for each box (person) */
	/* Define: Healthy = 0, Sick = 1, Recovered = 2 */
	srand(time(NULL));

	/* Randomly designate people to begin as "sick" */
	int init_sick[INITIAL_SICK];
	for (int i = 0; i < INITIAL_SICK; i++) {
		init_sick[i] = rand() % NUM_BOXES;
	}

	/* Initialzation of values */
	for (int i = 0; i < NUM_BOXES; i++) {
		box[i].x = (i % 20 + 1) * 15;
		box[i].y = (i / 20 + 3) * 18;
		box[i].dx = rand() % 2 * 2 - 1;
		box[i].dy = rand() % 2 * 2 - 1;
		box[i].sick_timer = SICK_DURATION;
		box[i].state = 0;
		for (int j = 0; j < INITIAL_SICK; j++) {
			if (i == init_sick[j])
				box[i].state = 1;
		}
	}

	clear_screen();
	wait_for_vsync();

	draw_banner();
	draw_border();
	wait_for_vsync();

	draw_banner_data();

    while (true) {

		int healthy_count = 0;
		int sick_count = 0;
		int recovered_count = 0;

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
			if (box[i].x >= (SCREEN_WIDTH - 1) - BOX_WIDTH - BORDER_OFFSET || box[i].x <= BORDER_OFFSET + 1)
			 	box[i].dx = -box[i].dx;
			if (box[i].y >= (SCREEN_HEIGHT - 1) - BOX_WIDTH - BORDER_OFFSET || box[i].y <= BANNER_HEIGHT + 1 + BORDER_OFFSET)
				 box[i].dy = -box[i].dy;

			//////////////////////
			//   NOT WORKING   ///
			//////////////////////
			/*// Box to Box Collision
			for (int j = 0; j < NUM_BOXES; j++) {
				if (i != j) {
					// Right-Left / Left-Right Collisions
					if( (box[i].x + BOX_WIDTH == box[j].x && abs(box[i].y - box[j].y) == BOX_WIDTH) ||
						(box[i].x - BOX_WIDTH == box[j].x && abs(box[i].y - box[j].y) == BOX_WIDTH) ) {
						box[i].dx = -box[i].dx;
						box[j].dx = -box[j].dx;

						// Check for virus spread
						if ( (box[i].state == 0 && box[j].state == 1) ||
							 (box[i].state == 1 && box[j].state == 0) ) {
								 box[i].state = 1;
								 box[j].state = 1;
						}
					}
					// Top-Bottom / Bottom-Top Collisions
					if( (box[i].y + BOX_WIDTH == box[j].y && abs(box[i].x - box[j].x) == BOX_WIDTH) ||
						(box[i].y - BOX_WIDTH == box[j].y && abs(box[i].x - box[j].x) == BOX_WIDTH) ) {
						box[i].dy = -box[i].dy;
						box[j].dy = -box[j].dy;

						// Check for virus spread
						if ( (box[i].state = 0 && box[j].state == 1) ||
							 (box[i].state == 1 && box[j].state == 0) ) {
								 box[i].state = 1;
								 box[j].state = 1;
						}
					}
				}
			}*/

			// Increment sick timer
			if (box[i].state == 1)
				box[i].sick_timer--;
			if (box[i].sick_timer == 0)
				box[i].state = 2;

			// Count number of boxes in each health state
			if (box[i].state == 0)
				healthy_count++;
			else if (box[i].state == 1)
				sick_count++;
			else if (box[i].state == 2)
				recovered_count++;
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

		update_banner_data(healthy_count, sick_count, recovered_count);

		/* Wait for pixels to be drawn and swap buffers */
		wait_for_vsync();
        //pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }
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


/* Draw the static text displayed on the banner */
void draw_banner_data() {
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
}


/* Draw the changing numbers displayed on the banner */
void update_banner_data(int healthy, int sick, int recovered) {
	plot_char(12, 3, healthy / 10 + 48);
	plot_char(13, 3, healthy % 10 + 48);

	plot_char(9, 5, sick / 10 + 48);
	plot_char(10, 5, sick % 10 + 48);

	plot_char(14, 7, recovered / 10 + 48);
	plot_char(15, 7, recovered % 10 + 48);
}
