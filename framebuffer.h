#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

struct input_event keyEvent;
int keyboard;

int openFrameBuffer();
int loadConsole();
int enableConsoleGraphics();
int disableConsoleGraphics();
void loadKeyBoard();
void closeKeyBoard();
int loadScale(double xRes, double yRes);
int drawPixel(int xPos, int yPos, int r, int g, int b, int a);
int loadFrameBuffer();
int clearScreen(unsigned char r, unsigned char g, unsigned char b);
int closeFrameBuffer();
#endif