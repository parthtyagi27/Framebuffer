#include "framebuffer.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/kd.h>
#include <linux/input.h>

int fb = 0;
int console;
//int keyboard;
//struct input_event keyEvent;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long int screensize = 0;
unsigned char *frameBuffer = 0;
int x = 0, y = 0;
long int location = 0;
double yScale = 1024.0;
double xScale = 1280.0;
double targetXRes;
double targetYRes;

int openFrameBuffer()
{
	fb = open("/dev/fb0", O_RDWR);
	if (fb == -1) 
	{
		printf("Error: cannot open framebuffer device\n");
		return -1;
	}
	printf("The framebuffer device was opened successfully.\n");

	// Get fixed screen information
	if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo) == -1) 
	{
		printf("Error reading fixed information\n");
		return -1;
	}

	// Get variable screen information
	if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		printf("Error reading variable information\n");
		return -1;
	}
	return 1;
}

int loadConsole()
{
	console = open("/dev/tty0", O_RDWR);
	if (console == -1) 
	{
		printf("Error: cannot open framebuffer device\n");
		return -1;
	}
	return 1;
}

int enableConsoleGraphics()
{
	if (ioctl(console, KDSETMODE, KD_GRAPHICS) == -1)
	{
		printf("Error setting console\n");
		return -1;
	}
	return 1;
}

int disableConsoleGraphics()
{
	if(ioctl(console, KDSETMODE, KD_TEXT) == -1)
	{
		printf("Could not switch to text\n");
		return -1;
	}
	return 1;
}

void loadKeyBoard()
{
	keyboard = open("/dev/input/event2", O_RDONLY);
}

void closeKeyBoard()
{
	close(keyboard);
}

int loadScale(double xRes, double yRes)
{
	printf("Loading Scale (%f, %f): \n", xRes, yRes);
	if (xRes < 1.0 || yRes < 1 || xRes > vinfo.xres || yRes > vinfo.yres) 
	{
		printf("ERROR:Use proper scaling!\n");
		return -1;
	} 
	else 
	{
		targetXRes = xRes;
		targetYRes = yRes;
		xScale = (double) (vinfo.xres / xRes);
		yScale = (double) (vinfo.yres / yRes);
		printf("xRes = %f, yRes = %f, xScale = %f, yScale = %f\n", targetXRes, targetYRes, xScale, yScale);
	}
	return 1;
}

int drawPixel(int xPos, int yPos, int r, int g, int b, int a)
{
	printf("Draw Request: (%d, %d) -> ", xPos, yPos);
	if ((xPos < 0 || xPos >= targetXRes) || (yPos < 0 || yPos >= targetYRes)) 
	{
		printf("ERROR: Pixel position out of bounds\n");
		return -1;
	}
	xPos *= xScale;
	yPos *= yScale;
	printf("Drawing pixel at: (%d, %d). RGB = (%d, %d, %d)\n", xPos, yPos, r, g, b);
	for (int i = 0; i < xScale; i++)
	{
		for (int j = 0; j < yScale; j++)
		{
			location = (xPos + vinfo.xoffset + i) * (vinfo.bits_per_pixel/8) + (yPos + vinfo.yoffset + j) * finfo.line_length;
			*(frameBuffer + location) = b;
			*(frameBuffer + location + 1) = g;
			*(frameBuffer + location + 2) = r;
			*(frameBuffer + location + 3) = a;

		}
	}
	return 1;
}

int clearScreen(unsigned char r, unsigned char g, unsigned char b)
{
	printf("Starting to clear screen...\n");
	for (y = 0; y < targetYRes; y++)
	{
		for (x = 0; x < targetXRes; x++)
		{
			drawPixel(x, y, r, g, b, 0);
		}
	}
	printf("Cleared Screen\n");

}


int loadFrameBuffer()
{
	printf("Starting framebuffer...\n");
	//Redirect printf to log.txt - Overwrites last log file
	freopen("log.txt", "w", stdout);
	loadKeyBoard();
	// Open the file for reading and writing
	if(openFrameBuffer() != 1)
		return -1;

	printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

	if(loadConsole() != 1)
		return -1;

	enableConsoleGraphics();

	// Figure out the size of the screen in bytes
	screensize = finfo.smem_len;
	// Map the device to memory
	frameBuffer = (unsigned char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);

	if ((unsigned char)frameBuffer == -1) 
	{
		printf("Error: failed to map framebuffer device to memory\n");
		return -1;
	}
	printf("The framebuffer device was mapped to memory successfully.\n");
}

int closeFrameBuffer()
{
	munmap(frameBuffer, screensize);
	close(fb);
	closeKeyBoard();
	disableConsoleGraphics();
	close(console);
	return 1;
}