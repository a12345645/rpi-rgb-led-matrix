#include "led-matrix.h"

#include "pixel-mapper.h"
#include "graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <vector>

#include <opencv2/opencv.hpp>

using namespace rgb_matrix;
using namespace cv;
using namespace std;

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

        ty = y % (height_ / 2);
        tx = x + (((y / (height_ / 2)) - 1) & (matrix_->width() / 2));

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
    int width = 0, height = 0;
    int interval = 1000000;
    void Load(char *path)
    {
        Mat image;
        image = imread(path, 1);

        SetImage(image);
    }

    void LoadGif(char *path)
    {
        VideoCapture capture;
        Mat frame;
        frame = capture.open(path); //讀取gif檔案

        double fps = capture.get(CAP_PROP_FPS);
        interval = 1000000 / fps;

        if(!capture.isOpened())
        {
            printf("can not open ...\n");
            return ;
        }
        while (capture.read(frame))
        {
            SetImage(frame);
        }
        capture.release();
    }

    void SetImage(Mat &image)
    {
        int rx = image.rows / 2, ry = image.cols / 2;
        int m = std::min(image.rows, image.cols);
        rx -= m / 2;
        ry -= m / 2;
        Rect rect(ry, rx, m, m);
        Mat tempImg(image, rect);

        Mat dstimg = tempImg;
        resize(tempImg, dstimg, Size(width, height), 0, 0, INTER_LINEAR);

        Pixel *pimg = new Pixel[dstimg.cols * dstimg.rows];
        for (int i = 0; i < dstimg.rows; i++) {
            for (int j = 0; j < dstimg.cols; j++) {
                Vec3b bgrPixel = dstimg.at<Vec3b>(Point(j, i));
                pimg[i * dstimg.cols + j].blue = bgrPixel[0];
                pimg[i * dstimg.cols + j].green = bgrPixel[1];
                pimg[i * dstimg.cols + j].red = bgrPixel[2];
            }
        }
        Image *fimg = new Image;
        fimg->image = pimg;
        fimg->height = dstimg.rows;
        fimg->width = dstimg.cols;

        imgs.push_back(fimg);
    }

    struct Pixel
    {
        Pixel() : red(0), green(0), blue(0) {}
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    struct Image
    {
        Image() : width(-1), height(-1), image(NULL) {}
        ~Image() { Delete(); }
        void Delete()
        {
            delete[] image;
            Reset();
        }
        void Reset()
        {
            image = NULL;
            width = -1;
            height = -1;
        }
        inline bool IsValid() { return image && height > 0 && width > 0; }
        const Pixel &getPixel(int x, int y)
        {
            static Pixel black;
            if (x < 0 || x >= width || y < 0 || y >= height)
                return black;
            return image[x + width * y];
        }

        int width;
        int height;
        Pixel *image;
    };

    vector<Image *> imgs;
};

class ImageRunner
{
public:
    ImageRunner(RGBMatrix *m, int width, int height) : matrix_(m)
    {
        imageDisplay = new ImageDisplay(matrix_, width, height);
        imgLdr.width = width;
        imgLdr.height = height;
    }
    ~ImageRunner()
    {
        delete imageDisplay;
    }
    void ImageLoad(char *path)
    {
        imgLdr.Load(path);
    }
    void GifLoad(char *path)
    {
        imgLdr.LoadGif(path);
    }
    void Run()
    {
        while (!interrupt_received) {
            for (size_t t = 0; t < imgLdr.imgs.size(); t++) {
                ImageLoader::Image *img = imgLdr.imgs[t];
                for (int i = 0; i < img->height; i++) {
                    for (int j = 0; j < img->width; j++) {
                        ImageLoader::Pixel p = img->image[i * img->width + j];
                        imageDisplay->SetPixel(j, i, p.red, p.green, p.blue);
                    }
                }
                usleep(imgLdr.interval);
            }
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

    int opt;
    opt = getopt(argc, argv, "i:g:");
    switch (opt) {
        case 'i':
            for (int i = optind; i < argc; i++) {
                image_runner->ImageLoad(argv[i]);
            } 
        break;
        case 'g':
            image_runner->GifLoad(optarg);
        break;
        default: /* '?' */
        ;
    }
    

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    printf("Press <CTRL-C> to exit and reset LEDs\n");

    image_runner->Run();

    delete image_runner;
    delete matrix;

    printf("Received CTRL-C. Exiting.\n");
    return 0;
}
