// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//
// This code is public domain
// (but note, once linked against the led-matrix library, this is
// covered by the GPL v2)
//
// This is a grab-bag of various demos and not very readable.
#include "led-matrix.h"

#include "pixel-mapper.h"
#include "graphics.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <algorithm>

using std::max;
using std::min;

using namespace rgb_matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo)
{
    interrupt_received = true;
}

class DemoRunner {
protected:
  DemoRunner(Canvas *canvas) : canvas_(canvas) {}
  inline Canvas *canvas() { return canvas_; }

public:
  virtual ~DemoRunner() {}
  virtual void Run() = 0;

private:
  Canvas *const canvas_;
};

class CanvasMapping
{
public:
    int tx, ty;
    CanvasMapping(int width, int height): height_(height), width_(width){
        
    }
    virtual ~CanvasMapping() {}
    int CoordTrans(int x, int y)
    {
        if (0 > x || x >= width_ || 0 > y || y >= height_) {
            return 0;
        }
        int tmpx = x >> 4, tmpy = y >> 3;
        int offset = (tmpx & 0b1) + (tmpy & 0b1) - 1;
        tmpx -= offset;
        tmpy -= offset;

        tx = (x & 0b1111) | (tmpx << 4);
        ty = (y & 0b111) | (tmpy << 3);
        return 1;
    }

private:
    const int width_;
    const int height_;
};


class SimpleSquare : public DemoRunner
{
public:
    SimpleSquare(Canvas *m) : DemoRunner(m) {}
    void Run() override
    {
        const int width = canvas()->width() - 1;
        const int height = canvas()->height() - 1;

        CanvasMapping *canvasMapping = new CanvasMapping(width + 1, height + 1);
        int x = 0, y = 0;
        uint8_t r = 255, g = 255, b = 255;
        while (!interrupt_received)
        {   
            for (int i = 0 ; i < 16; i++, x++) {
                canvasMapping->CoordTrans(x, y);
                canvas()->SetPixel(canvasMapping->tx, canvasMapping->ty, r, g, b);
            }
            if (x > width)
            {
                y++;
                x = 0;
                if (y > height)
                {            std::cout << "x " << x << " y " << y << std::endl;
                    y = 0;
                    r++;
                    g++;
                    b++;
                }
            }
            usleep(100000);
        }
    }
};

int main(int argc, char *argv[])
{
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    // These are the defaults when no command-line flags are given.
    matrix_options.rows = 32;
    matrix_options.cols = 64;
    matrix_options.chain_length = 2;
    matrix_options.parallel = 1;
    matrix_options.multiplexing = 2;
    
    runtime_opt.gpio_slowdown = 4;

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
        return 1;

    Canvas *canvas = matrix;

    // The DemoRunner objects are filling
    // the matrix continuously.
    DemoRunner *demo_runner = NULL;

    demo_runner = new SimpleSquare(canvas);

    // Set up an interrupt handler to be able to stop animations while they go
    // on. Each demo tests for while (!interrupt_received) {},
    // so they exit as soon as they get a signal.
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    printf("Press <CTRL-C> to exit and reset LEDs\n");

    // Now, run our particular demo; it will exit when it sees interrupt_received.
    demo_runner->Run();

    delete demo_runner;
    delete canvas;

    printf("Received CTRL-C. Exiting.\n");
    return 0;
}
