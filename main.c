#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

volatile int pixel_buffer_start; // global variable

void wait_for_vsync();
void draw_line(int x0, int y0, int x1, int y1, short int colour);
void swap(int* a, int* b);
void plot_pixel(int x, int y, short int line_color);
void clear_screen();
void clear_boxes(int x, int y);

const int WIDTH = 5;
const int NUM_BOXES = 32;

int main(void)
{
	struct Box
	{
		int x;
		int y;
		int dx;
		int dy;
	};
	
	
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	
    // declare other variables(not shown)
	struct Box box[NUM_BOXES];
	for(int i = 0; i < NUM_BOXES; i++){
		box[i].x = i*10;
		box[i].y = i*7;
		//srand(time(NULL));
		box[i].dx = (rand() % 2) * 2 - 1;
		box[i].dy = (rand() % 2) * 2 - 1;
	}

    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer
    /* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
	
	clear_screen();

    while (1)
    {
		
		//1) draw boxes and lines
		//2) wait_for_vsync()
		//3) draw boxes and lines into black
		//4) pixel_buffer_start = *(pixel_ctrl_ptr + 1);
		//5) update position of x and y of each box
		
		//// DRAW ////
		//draw boxes
		for(int i = 0; i < NUM_BOXES; i++){
			for(int j = 0; j < WIDTH; j++){
				for(int k = 0; k < WIDTH; k++){
					plot_pixel(box[i].x+j, box[i].y+k, 0xF800);
				}
			}
		}
		
		//draw lines
		/*
		for(int i = 0; i < 8; i++){
			
			
			draw_line(x_box[i]+(width/2), y_box[i]+(width/2), 
					  x_box[(i+1) % 8]+(width/2), y_box[(i+1) % 8]+(width/2), 0x07E0);
		}
		*/
		
		wait_for_vsync();
		
		//erase boxes and lines
		for(int i = 0; i < NUM_BOXES; i++){
			clear_boxes(box[i].x, box[i].y);
			//draw_line(x_box[i]+(width/2), y_box[i]+(width/2), 
					 // x_box[(i+1) % 8]+(width/2), y_box[(i+1) % 8]+(width/2), 0x0000);
		}
		
		
		pixel_buffer_start = *(pixel_ctrl_ptr + 1);
		
		// update positions
		for(int i = 0; i < NUM_BOXES; i++){
			// Boundaries
			if(box[i].x == 319-WIDTH && box[i].dx > 0){
				box[i].dx = -box[i].dx;
			}
			else if(box[i].x == 0 && box[i].dx < 0){
				box[i].dx = -box[i].dx;
			}
			
			if(box[i].y == 239-WIDTH && box[i].dy > 0){
				box[i].dy = -box[i].dy;
			}
			else if(box[i].y == 0 && box[i].dy < 0){
				box[i].dy = -box[i].dy;
			}
			
			// Collisions
			for(int j = 0; j < NUM_BOXES; j++){
				if(i != j){
					// left-right collision
					if( (box[i].x + WIDTH == box[j].x) && (abs(box[i].y - box[j].y) < WIDTH) ){
						box[i].dx = -box[i].dx;
						box[j].dx = -box[j].dx;
					}
					// right-left collision
					if( (box[i].x - WIDTH == box[j].x) && (abs(box[i].y - box[j].y) < WIDTH) ){
						box[i].dx = -box[i].dx;
						box[j].dx = -box[j].dx;
					}
					// top-bottom collision
					if( (box[i].y + WIDTH == box[j].y) && (abs(box[i].x - box[j].x) < WIDTH) ){
						box[i].dy = -box[i].dy;
						box[j].dy = -box[j].dy;
					}
					// bottom-top collision
					if( (box[i].y - WIDTH == box[j].y) && (abs(box[i].x - box[j].x) < WIDTH) ){
						box[i].dy = -box[i].dy;
						box[j].dy = -box[j].dy;
					}
					
				}
			}
			
			box[i].x += box[i].dx;
			box[i].y += box[i].dy;
		}
    }
}

// code for subroutines (not shown)


void wait_for_vsync(){
	volatile int *pixel_ctrl_ptr = 0xFF203020;
	register int status;

	*pixel_ctrl_ptr = 1; // start the synchronization process

	status = *(pixel_ctrl_ptr + 3);

	while( (status& 0x01) != 0){
		status = *(pixel_ctrl_ptr + 3);
	}
}


void draw_line(int x0, int y0, int x1, int y1, short int colour){
	bool is_steep = abs(y1 - y0) > abs(x1 - x0);
	
	if(is_steep){
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if(x0 > x1){
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int delta_x = x1-x0;
	int delta_y = abs(y1-y0);
	int error = -(delta_x / 2);
	int y = y0;

	int y_step = 0;
	if(y0 < y1)
		y_step = 1;
	else
		y_step = -1;
	
	int x = 0;
	for(x = x0; x < x1; x++){
		if(is_steep)
			plot_pixel(y, x, colour);
		else
			plot_pixel(x, y, colour);

		error = error + delta_y;
		if(error >= 0){
			y = y + y_step;
			error = error - delta_x;
		}
	}
}

void swap(int* a, int* b){
	int temp = *a;
	*a = *b;
	*b = temp;
}


void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

/*
void clear_screen(){
	volatile int VGA = 0xc8000000;
	int comp = 0;

	int x = 0;
	int y = 0;
	for(x = 0; x < 320; x++){
		for(y = 0; y < 240; y++){
			comp = (y << 10) + (x << 1);
			*(short int*)(VGA + comp) = 0x0;
		}
	}
}
*/

void clear_screen(){
	for(int i = 0; i < 320; i++){
		for(int j = 0; j < 240; j++){
			plot_pixel(i, j, 0x0000);	
		}
	}
}

void clear_boxes(int x, int y){
	for(int i = 0; i < WIDTH; i++){
		for(int j = 0; j < WIDTH; j++){
			plot_pixel(x+i, y+j, 0x0000);
		}
	}
}
