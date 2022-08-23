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

#include <math.h>
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

class DemoRunner
{
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
    CanvasMapping(int width, int height) : width_(width), height_(height) {}
    ~CanvasMapping() {}
    int CoordTrans(int x, int y)
    {
        if (0 > x || x >= width_ || 0 > y || y >= height_)
            return 0;

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

class ImageDisplay : public Canvas
{
public:
    ImageDisplay(RGBMatrix *m, int width, int height) : matrix_(m), width_(width), height_(height)
    {
        canvMap_ = new CanvasMapping(matrix_->width(), matrix_->height());
    }
    ~ImageDisplay()
    {
        delete canvMap_;
    };
    virtual void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
    {
        if (x < 0 || x >= width_ || y < 0 || y >= height_)
            return;

        int tx, ty;

        if (y < height_ / 2) {
            ty = y;
            tx = x + (matrix_->width() / 2);
        }
        else {
            ty = y - height_ / 2;
            tx = x;
        }
        if (canvMap_->CoordTrans(tx, ty))
            matrix_->SetPixel(canvMap_->tx, canvMap_->ty, r, g, b);
    }
    virtual int width() const
    {
        return width_;
    }
    virtual int height() const
    {
        return height_;
    }
    virtual void Fill(uint8_t r, uint8_t g, uint8_t b)
    {
        matrix_->Fill(r, g, b);
    }
    virtual void Clear()
    {
        matrix_->Clear();
    }
private:
    RGBMatrix *matrix_;
    const int width_;
    const int height_;
    CanvasMapping *canvMap_;
};

class SimpleSquare : public DemoRunner
{
public:
    SimpleSquare(RGBMatrix *m) : DemoRunner(m), matrix_(m) {}
    void Run() override
    {
        ImageDisplay * imageDisplay = new ImageDisplay(matrix_, 64, 64);
        int rotation = 0;
        const int cent_x = 64 / 2;
        const int cent_y = 64 / 2;

        const int rotate_square = min(64, 64) * 1.41;
        const int min_rotate = cent_x - rotate_square / 2;
        const int max_rotate = cent_x + rotate_square / 2;

        const int display_square = min(64, 64) * 0.7;
        const int min_display = cent_x - display_square / 2;
        const int max_display = cent_x + display_square / 2;

        const float deg_to_rad = 2 * 3.14159265 / 360;

        while (!interrupt_received)
        {
            ++rotation;
            usleep(10000);
            rotation %= 360;
            for (int x = min_rotate; x < max_rotate; ++x)
            {
                for (int y = min_rotate; y < max_rotate; ++y)
                {
                    float rot_x, rot_y;
                    Rotate(x - cent_x, y - cent_x,
                           deg_to_rad * rotation, &rot_x, &rot_y);
                    if (x >= min_display && x < max_display &&
                        y >= min_display && y < max_display)
                    { // within display square
                        imageDisplay->SetPixel(rot_x + cent_x, rot_y + cent_y,
                                           scale_col(x, min_display, max_display),
                                           255 - scale_col(y, min_display, max_display),
                                           scale_col(y, min_display, max_display));
                    }
                    else
                    {
                        // black frame.
                        imageDisplay->SetPixel(rot_x + cent_x, rot_y + cent_y, 0, 0, 0);
                    }
                }
            }
        }
    }

private:
    RGBMatrix *matrix_;
    void Rotate(int x, int y, float angle,
                float *new_x, float *new_y)
    {
        *new_x = x * cosf(angle) - y * sinf(angle);
        *new_y = x * sinf(angle) + y * cosf(angle);
    }
    uint8_t scale_col(int val, int lo, int hi)
    {
        if (val < lo)
            return 0;
        if (val > hi)
            return 255;
        return 255 * (val - lo) / (hi - lo);
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

    // The DemoRunner objects are filling
    // the matrix continuously.
    DemoRunner *demo_runner = NULL;

    demo_runner = new SimpleSquare(matrix);

    // Set up an interrupt handler to be able to stop animations while they go
    // on. Each demo tests for while (!interrupt_received) {},
    // so they exit as soon as they get a signal.
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    printf("Press <CTRL-C> to exit and reset LEDs\n");

    // Now, run our particular demo; it will exit when it sees interrupt_received.
    demo_runner->Run();

    delete demo_runner;
    delete matrix;

    printf("Received CTRL-C. Exiting.\n");
    return 0;
}
