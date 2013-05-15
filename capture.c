#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <string.h>

#include "spi.h"
#include "alsa.h"
    
float *in;
fftwf_complex *out;
fftwf_plan p;
int data_size = 1024;

unsigned char display_buf[128][32];
unsigned char output_buf[512];

void clear_display_buf()
{
    memset(display_buf, 0, 128*32);
    memset(output_buf, 0, 512);
}

void randomise_screen()
{
    for (int i=0; i<128; i++) {
        for (int j=0; j<32; j++) {
            display_buf[i][j] = rand()%2;
        }
    }
}

void display_screen()
{
    unsigned char *points;

    static int count = 0;
    static struct timeval tv, last_tv;
    float fps;

    if (count == 0) {
        gettimeofday(&last_tv, NULL);
    }

    for (int i=0; i<128; i++) {
        for (int j=0; j<4; j++) {
            points = &display_buf[i][j*8];
            output_buf[j*128 + i] = 0;
            for (int k=0; k<8; k++) {
                output_buf[j*128 + i] += (points[k] << k);
            }
        }
    }
    spi_write(output_buf, 512);

    count++;
    if (count == 500) {
        gettimeofday(&tv, NULL);
        fps = 500.0 / ((tv.tv_sec - last_tv.tv_sec) + (tv.tv_usec - last_tv.tv_usec)/1000000.0);
        printf("FPS: %f\n", fps);

        count = 0;
    }
}


void process_audio (short* buf)
{
    int i, j, k, l;
    int step = 1;
    int upto = 0;
    float tmp, tmp2;

    float bands[128];

    double divider;

    for (i=0; i<data_size; i++) {
        in[i] = buf[i];
    }

    fftwf_execute(p);


    double five_band[5] = { 0, 0, 0, 0, 0 };
    static double led_avg[5] = { 0, 0, 0, 0, 0 };

    // Now calculate LED version..
    for (int i=0; i<2; i++) {
        five_band[0] += cabs(out[i]);
    }
    for (int i=2; i<5; i++) {
        five_band[1] += cabs(out[i]);
    }
    for (int i=5; i<13; i++) {
        five_band[2] += cabs(out[i]);
    }
    for (int i=13; i<24; i++) {
        five_band[3] += cabs(out[i]);
    }
    for (int i=24; i<200; i++) {
        five_band[4] += cabs(out[i]);
    }

    for (int i=0; i<5; i++) {
        led_avg[i] = (led_avg[i] * 0.997) + (five_band[i] * 0.003);
    }

    for (int i=0; i<5; i++) {
        if (five_band[i] > (led_avg[i] * 1.4)) {
            if (i<=3) { switch_led(i, 1); }
            switch_laser(i, 1);
        }
        else {
            if (i<=3) { switch_led(i, 0); }
            switch_laser(i, 0);
        }
    }

    clear_display_buf();

    for (int i=0; i<5; i++) {
        k = round( five_band[i] / led_avg[i] * 20.0 );
        if (k > 32) { k = 32; }
        else if (k < 0) { k = 0; }

        for (l = 0; l<k; l++) {
            for (j = i*25; j < i*25 + 25; j++) {
                display_buf[j][l] = 1;
            }
        }
    }

    display_screen();

}

void print_audio (short* buf, int length)
{
    int i;
    printf("******************************");
    for (i=0; i < length; i++) {
        printf("%5d\n", buf[i]);
    }
}
    
int main(int argc, char** argv) {
    short *audio_buf;

    puts("Initialising LCD..");
    init_lcd_screen();

/*
 * To benchmark screen performance, I used this..
 * It achieved a steady 1229 fps!
 * With the randomise_screen() inside the while loop, 261 fps.
 * Apparently rand() is expensive..

    while (1) {
        randomise_screen();
        display_screen();
    }
 */

    // in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * data_size);
    in = (float*) fftwf_malloc(sizeof(float) * data_size);
    // out has data_size/2 + 1 elements
    out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * 513);
    p = fftwf_plan_dft_r2c_1d(data_size, in, out, FFTW_MEASURE);
    // p = fftwf_plan_dft_1d(data_size, in, in, FFTW_FORWARD, FFTW_MEASURE);
        // use FFTW_MEASURE, and then re-use plan
        // or FFTW_ESTIMATE for a once-of use

    puts("Opening audio device..");
    if (capture_setup("hw:1,0", 44100)) {
        puts("Failed to setup audio.. Exiting.");
        exit(1);
    }

    puts("Capturing audio..");
    audio_buf = malloc(sizeof(short) * data_size);
    while (1) {
        capture_audio(audio_buf, data_size);
        process_audio(audio_buf);
    }

    capture_finish();
    fftwf_destroy_plan(p);
    fftwf_free(in);
    fftwf_free(out);
    exit(0);
}

