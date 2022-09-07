#include "led-matrix.h"

#include "pixel-mapper.h"
#include "graphics.h"
#include "config.hpp"
#include "set_serial_attribute.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <functional>

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
    int interval = 1000;
    void Load(char *path)
    {
        Mat image;
        image = imread(path, 1);

        SetImage(image);
    }

    void LoadGif(const char *path)
    {
        VideoCapture capture;
        Mat frame;
        frame = capture.open(path); //讀取gif檔案
        cout << "path " <<  path << endl;
        if(!capture.isOpened()){
            cout << "Can not open the gif." << endl;
            return;
        }

        double fps = capture.get(CAP_PROP_FPS);
        interval = 1000000 / fps;

        if (!capture.isOpened())
        {
            printf("can not open ...\n");
            return;
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
        for (int i = 0; i < dstimg.rows; i++)
        {
            for (int j = 0; j < dstimg.cols; j++)
            {
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
        this->width = width;
        this->height = height;
    }
    ~ImageRunner()
    {
        delete imageDisplay;
        for (size_t i = 0; i < imgLdr.size(); i++)
            delete imgLdr[i];
    }
    std::function<int(void)> checkImg = []()
    { return -1; };
    void ImageLoad(char *path)
    {
        ImageLoader *iml = new ImageLoader();
        imgLdr.push_back(iml);
        imgLdr[imgLdr.size() - 1]->width = width;
        imgLdr[imgLdr.size() - 1]->height = height;
        imgLdr[imgLdr.size() - 1]->Load(path);
    }
    void GifLoad(const char *path)
    {
        ImageLoader *iml = new ImageLoader();
        imgLdr.push_back(iml);
        imgLdr[imgLdr.size() - 1]->width = width;
        imgLdr[imgLdr.size() - 1]->height = height;
        imgLdr[imgLdr.size() - 1]->LoadGif(path);
    }
    void Run()
    {
        if (imgLdr.size() == 0)
        {
            cout << "no image." << endl;
            exit(1);
        }
        size_t t = 0;
        int imgptr = 0;
        while (!interrupt_received)
        {
            int p = checkImg();
            if (p != -1 &&  imgptr != p && p < (int)imgLdr.size()) {
                imgptr = p;
                t = 0;
            }
            
            ImageLoader::Image *img = imgLdr[imgptr]->imgs[t++];
            for (int i = 0; i < img->height; i++)
            {
                for (int j = 0; j < img->width; j++)
                {
                    ImageLoader::Pixel p = img->image[i * img->width + j];
                    imageDisplay->SetPixel(j, i, p.red, p.green, p.blue);
                }
            }
            if (t >= imgLdr[imgptr]->imgs.size())
                t = 0;
            usleep(imgLdr[imgptr]->interval);
        }
    }
private:
    int width, height;
    RGBMatrix *const matrix_;
    vector<ImageLoader *> imgLdr;
    ImageDisplay *imageDisplay;
};

class LoraRecver
{
public:
    LoraRecver(int t)
    {
        tmi = t - 1;
        if (LoRaSerial.OpenPort(PORT_FD)) {
             LoRaSerial.setup(9600, 0);
        } else {
            exit(1);
        }
    }
    ~LoraRecver()
    {
        LoRaSerial.ClosePort();
    }
    std::function<int(void)> recvLora = [=]() {
        if(LoRaSerial.readBuffer(buff,sizeof(buff)) == -1) {
            return -1;
        }
        if (buff[0] == 0xab && buff[1] == 0x0a) {
            if (buff[2] == 0x00) {
                cout << "interrupt protocol" << endl;
                return 2;
            } else {
                uint8_t G_C = buff[3];
                uint8_t R_C = buff[4];
                printf("G_C:%02x ; R_C:%02x\r\n",G_C,R_C);
                if (buff[2] == 0x01 || buff[2] == 0x11) {
                    if ((G_C & (1 << tmi)) > 0) {
                        //show green GIF
                        cout << "show green GIF" << endl;
                        return 0;
                    } else if ((R_C & (1 << tmi)) > 0) {
                        //show red GIF
                        cout << "show red GIF" << endl;
                        return 1;
                    }
                } 
            }
        }
        return -1;
    };
private:
    serialPort LoRaSerial;
    uint8_t buff[512];
    int tmi = 0;
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
    matrix_options.brightness = 50;

    runtime_opt.gpio_slowdown = 4;

    int imgw = 64, imgh = 64;

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
        return 1;

    ImageRunner *image_runner = NULL;

    image_runner = new ImageRunner(matrix, imgw, imgh);
    
    Config config(CONFIG_FILE_PATH);
    if (config.config == nullptr) {
        cout << "unsuccessful open config.txt" << endl;
        exit(1);
    }

    image_runner->GifLoad(config.config->green.c_str());
    image_runner->GifLoad(config.config->red.c_str());
    image_runner->GifLoad(config.config->warning.c_str());
    
    LoraRecver lorarecv(config.config->RTM);
    image_runner->checkImg = lorarecv.recvLora;

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    printf("Press <CTRL-C> to exit and reset LEDs\n");

    image_runner->Run();

    delete image_runner;
    delete matrix;

    printf("Received CTRL-C. Exiting.\n");
    return 0;
}
