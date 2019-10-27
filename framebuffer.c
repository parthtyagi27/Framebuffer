#include "structs.h"
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
#include <pthread.h>
#include <time.h>
#include <png.h>
#include <jpeglib.h>

int fb = 0;
int console;
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

FILE * output;

pthread_t imageThread;
time_t start;
time_t end;
float deltaTime = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Opens framebuffer into fb.
 * Retrieves information like the resolution and bits per pixel of the framebuffer.
 * RETURN: 1 -> The framebuffer has loaded successfully.
 * RETURN: -1 -> There was an error loading the framebuffer. 
*/

int openFrameBuffer()
{
	// Open framebuffer.
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

/**
 * Opens the current console into console.
 * RETURN: 1 -> The console has loaded successfully.
 * RETURN: -1 -> There was an error loading the console. 
*/

int loadConsole()
{
	console = open("/dev/tty0", O_RDWR);
	if (console == -1) 
	{
		printf("Error: cannot open console device\n");
		return -1;
	}
	return 1;
}

/**
 * Enables Console Graphics.
 * Uses an ioctl call to stop the console from overwriting the framebuffer's graphics.
 * Essentially allows us to draw to the framebuffer and not worry about stddout overwriting our graphics.
 * RETURN: 1 -> The console's graphics were loaded successfully.
 * RETURN: -1 -> There was an error loading graphics. 
*/

int enableConsoleGraphics()
{
	if (ioctl(console, KDSETMODE, KD_GRAPHICS) == -1)
	{
		printf("Error loading console graphics!\n");
		return -1;
	}
	return 1;
}

/**
* Disables Console Graphics.
* Uses an ioctl call to set the console to text mode.
* Essentially allows us to turn off the framebuffer graphics to use the OS.
* RETURN: 1 -> The text mode was loaded successfully.
* RETURN: -1 -> There was an error loading text mode. 
*/

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
	writing = true;
	pthread_mutex_lock(&mutex);
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
	writing = false;
	pthread_mutex_unlock(&mutex);
	return 1;
}

void getPixelData(struct pixelData * data)
{

	location = (data->x * xScale + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (data->y * yScale + vinfo.yoffset) * finfo.line_length;
	data->b = *(frameBuffer + location);
	data->g = *(frameBuffer + location + 1);
	data->r = *(frameBuffer + location + 2);
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
//	freopen("log.err", "w", stderr);
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

	if (frameBuffer == NULL) 
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
	if (fclose(output) != 0)
		printf("Error closing image file!\n");
	return 1;
}

void* writeImage(void* args)
{

	while(true)
	{
		start = time(NULL);
		sleep(5);
		end = time(NULL);
		deltaTime += end - start;

		if (deltaTime >= 60 * 0.25)
		{
			deltaTime = 0;
			output = fopen("state.ppm", "wb+");
			if (output == NULL)
			{
				printf("Error opening image file!\n");
				return -1;
			}
			printf("Writing PPM Header\n");
			fprintf(output, "P3\n%d %d\n255\n", (int)vinfo.xres, (int)vinfo.yres);

			struct pixelData data;
			pthread_mutex_lock(&mutex);
			for (double i = 0; i < vinfo.yres; i++)
			{
				for (double j = 0; j < vinfo.xres; j++)
				{
					data.x = j;
					data.y = i;
					//getPixelData(&data);

					location = (data.x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (data.y + vinfo.yoffset) * finfo.line_length;
					data.b = *(frameBuffer + location);
					data.g = *(frameBuffer + location + 1);
					data.r = *(frameBuffer + location + 2);
					if (j == 0)
						fprintf(output, "%d %d %d", data.r, data.g, data.b);
					else
						fprintf(output, " %d %d %d", data.r, data.g, data.b);

					//			printf("Pixel Data at: (%d, %d) = (%d,%d,%d)\n", j, i, data.r, data.g, data.b);
				}
				//printf("Finished writing row: %d\n", i);
				fprintf(output, "\n");
			}
			pthread_mutex_unlock(&mutex);
			printf("Finished writing to file\n");
		}
	}
}


void* writePngImage(void* args)
{

	while(true)
	{
		start = time(NULL);
		sleep(5);
		end = time(NULL);
		deltaTime += end - start;

		if (deltaTime >= 60 * 0.5 && writing == false)
		{
			printf("Starting PNG Write\n");
			deltaTime = 0;
			output = fopen("state.png", "wb+");
			if (output == NULL)
			{
				printf("Error opening image file!\n");
				return -1;
			}
			png_structp png = NULL;
			png_infop info = NULL;
			png_byte* rowPointers = NULL;

			png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (png == NULL)
			{
				printf("Error: Could not allocate Png struct\n");
				return -1;
			}

			info = png_create_info_struct(png);

			if (info == NULL)
			{
				printf("Error: Could not allocate info struct\n");
				return -1;
			}
			png_set_IHDR(png, info, vinfo.xres, vinfo.yres, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			png_init_io(png, output);
			png_write_info(png, info);
			png_bytep row;
			row = (png_bytep) malloc(sizeof(png_byte) * vinfo.xres * 3);
			for (int y = 0; y < vinfo.yres; y++)
			{
				for (int x = 0; x < vinfo.xres; x++)
				{
					location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y + vinfo.yoffset) * finfo.line_length;
					row[x * 3] = *(frameBuffer + location + 2);
					row[x * 3 + 1] = *(frameBuffer + location + 1);
					row[x * 3 + 2] = *(frameBuffer + location);
				}
				png_write_row(png, row);
			}
			// png_set_rows(png, info, rowPointers);
			// png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
			png_write_end(png, info);
			printf("Finished Writing!\n");
		}
	}
}

void* writeJpgImage(void* args)
{

	while(true)
	{
		start = time(NULL);
		sleep(5);
		end = time(NULL);
		deltaTime += end - start;

		if (deltaTime >= 60 * 0.5 && writing == false)
		{
			printf("Starting JPEG Write\n");
			deltaTime = 0;
			output = fopen("state.jpg", "wb+");
			if (output == NULL)
			{
				printf("Error opening image file!\n");
				return -1;
			}

			struct jpeg_compress_struct cinfo;
			struct jpeg_error_mgr error;
			JSAMPROW rowPointer[1];
			int rowStride;

			cinfo.err = jpeg_std_error(&error);
			jpeg_create_compress(&cinfo);
			jpeg_stdio_dest(&cinfo, output);

			cinfo.image_width = vinfo.xres;
			cinfo.image_height = vinfo.yres;
			cinfo.input_components = 3;
			cinfo.in_color_space = JCS_RGB;

			jpeg_set_defaults(&cinfo);
			jpeg_set_quality(&cinfo, 100, TRUE);

			jpeg_start_compress(&cinfo, TRUE);
			rowStride = cinfo.image_width * 3;

			unsigned char imageBuffer[vinfo.xres * 3];

			pthread_mutex_lock(&mutex);
			for (int y = 0; y < vinfo.yres; y++)
			{

				for (int x = 0; x < vinfo.xres; x++)
				{
					location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y + vinfo.yoffset) * finfo.line_length;
					imageBuffer[x * 3] = *(frameBuffer + location + 2);
					imageBuffer[x * 3 + 1] = *(frameBuffer + location + 1);
					imageBuffer[x * 3 + 2] = *(frameBuffer + location);
				}
				rowPointer[0] = imageBuffer;
				jpeg_write_scanlines(&cinfo, rowPointer, 1);
			}
			pthread_mutex_unlock(&mutex);
			jpeg_finish_compress(&cinfo);
			jpeg_destroy_compress(&cinfo);
			printf("Finished Writing!\n");
		}
	}
}

int saveState()
{
	printf("Creating mutex lock = %d\n", pthread_mutex_init(&mutex, NULL));
	return (pthread_create(&imageThread, NULL, writeJpgImage, NULL));
}

