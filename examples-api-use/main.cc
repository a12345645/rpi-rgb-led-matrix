#include "led-matrix.h"

#include "pixel-mapper.h"
#include "graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

using namespace rgb_matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo)
{
    interrupt_received = true;
}

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

class ImageLoader
{
public:
    ImageLoader() {}
    ~ImageLoader() {}
};

class ImageRunner
{
public:
    ImageRunner(RGBMatrix *m, int width, int height) : matrix_(m)
    {
        imageDisplay = new ImageDisplay(matrix_, width, height);
    }
    ~ImageRunner()
    {
        delete imageDisplay;
    }
    void Run()
    {
        
        
        while (!interrupt_received)
        {
            usleep(10000);
        }
    }

private:
    RGBMatrix *const matrix_;
    ImageLoader imgLdr;
    ImageDisplay *imageDisplay;
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

    int imgw = 64, imgh = 64;

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
        return 1;

    ImageRunner *image_runner = NULL;

    image_runner = new ImageRunner(matrix, imgw, imgh);

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    printf("Press <CTRL-C> to exit and reset LEDs\n");

    image_runner->Run();

    delete image_runner;
    delete matrix;

    printf("Received CTRL-C. Exiting.\n");
    return 0;
}
