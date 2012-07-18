#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <sndfile.h> //libsndfile lib

//anything higher than RANGE*maximum_foud_powe_peak is on
#define RANGE  0.1           
//minimum power level for the loudest tone
#define POWERTHRESHOLD 100.0

#define SAMPLERATE  8000
#define BLOCKSIZE 200 //window of samples to analyse each iteration

#define NUM_FREQ 8

#define ROW1 0
#define ROW2 1
#define ROW3 2
#define ROW4 3

#define COL1 4
#define COL2 5
#define COL3 6
#define COL4 7

#define PI 3.141592

int DTMF_freq[] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};

//coefficients: 2 * cos((2*pi*k)/N)
float coef[NUM_FREQ];

void DTMF_MakeCoeff(int samplerate, int blocksize) {
    int c;
    int k;
    for(c = 0; c < NUM_FREQ; c++) {
      k = 0.5 + (blocksize * DTMF_freq[c]) / samplerate;
      coef[c] = 2 * cos((2 * PI * k) / blocksize);	
      //printf("Coef[%d Hz] = %g\n", DTMF_freq[c], coef[c]);
    }
}

//function to calculate powers; dtmf_freq - pointer to 
//freq array; c - pointer to coefficients array
float * DTMF_CalcPower(float * sample_block, int blocksize, int * dtmf_freq, float * c) {

  float * power = malloc(NUM_FREQ * sizeof(float));
  float Q0[NUM_FREQ], Q1[NUM_FREQ], Q2[NUM_FREQ];
  int k, n;
  
  for (k = 0; k < NUM_FREQ; k++) {
    Q0[k] = 0.0;
    Q1[k] = 0.0;
    Q2[k] = 0.0;
  }

  for(n = 0; n < blocksize; n++) {
    for(k = 0; k < NUM_FREQ; k++) {
      Q2[k] = Q1[k];
      Q1[k] = Q0[k];
      Q0[k] = c[k] * Q1[k] - Q2[k] + sample_block[n];
    }
  }

  for(k = 0; k < NUM_FREQ; k++) {   
    power[k] = Q1[k] * Q1[k] + Q2[k] * Q2[k] - c[k] * Q1[k] * Q2[k];
    //printf("Power of [%d Hz]: %f\n", dtmf_freq[k], power[k]);
  }
 
  return power;
}

char DTMF_Decode(float * sample_block, int samplerate, int blocksize) {

  float maxpower = 0.0;
   
  int detected[NUM_FREQ]; //flag to store detected freq
  char last_key_pressed;
  int pause_found = 0;
  int on_count = 0;
  int k;
  
  DTMF_MakeCoeff(samplerate, blocksize);

  float * power = DTMF_CalcPower(sample_block, blocksize, DTMF_freq, coef);

  if (!power) {
    printf("Cannot allocate memmory for power storage\n");
    exit(0);
  }

  for(k = 0; k < NUM_FREQ; k++) {
    if(power[k] > maxpower) {
      maxpower = power[k]; 
    }
  }

  if(maxpower < POWERTHRESHOLD) { 
    //printf("Not enough power to hold signal.\n");
    return 1; //pause or not enough power
  }
  
  for(k = 0; k < NUM_FREQ; k++) {
    if(power[k] > RANGE * maxpower) {
      //printf("Power[%d] of %d Hz = %f\n", k, DTMF_freq[k], power[k]); 
      detected[k] = 1;
      on_count++;
      if (on_count > 2) {
        printf("Multiple keys pressed.\n");
      }
    } else {
      detected[k] = 0;
    }	    
  }
      
  if(detected[ROW1] == 1) {
    if(detected[COL1] == 1) return '1';
    if(detected[COL2] == 1) return '2';
    if(detected[COL3] == 1) return '3';
    if(detected[COL4] == 1) return 'a';
  } else if(detected[ROW2] == 1) {
    if(detected[COL1] == 1) return '4';
    if(detected[COL2] == 1) return '5';
    if(detected[COL3] == 1) return '6';
    if(detected[COL4] == 1) return 'b';
  } else if(detected[ROW3] == 1) {
    if(detected[COL1] == 1) return '7';
    if(detected[COL2] == 1) return '8';
    if(detected[COL3] == 1) return '9';
    if(detected[COL4] == 1) return 'c';
  } else if(detected[ROW4] == 1) {
    if(detected[COL1] == 1) return '*';
    if(detected[COL2] == 1) return '0';
    if(detected[COL3] == 1) return '#';
    if(detected[COL4] == 1) return 'd';
  }
  
  free(power);
      
  return 0;
}

int main(int argc,char *argv[]) {

  SNDFILE * sndfile;
  SF_INFO sndfileinfo;
  sndfileinfo.format = 0; //set to zero before sf_open()

  char * file_name = "11224455.wav";
  sndfile = sf_open(file_name, SFM_READ, &sndfileinfo);

  if (!sndfile)
    {	
      printf ("Could not open soundfile '%s'\n", file_name) ;
      exit(0);
    }
  
  printf("Channels: %d\n", sndfileinfo.channels);
  printf("Sample rate: %d Hz\n", sndfileinfo.samplerate);
  printf("Samples per channel: %d\n", ((int)sndfileinfo.frames));

  float duration = sndfileinfo.frames / (1.0 * sndfileinfo.samplerate);
  printf("Duration: %.1f sec\n", duration);

  int num_frames = sndfileinfo.frames * sndfileinfo.channels;

  // Allocate space for the data to be read, then read it.
  float * buf = (float *) malloc(num_frames * sizeof(float));
  int read_frames = sf_read_float(sndfile, buf, num_frames);
  
  //printf("Number of frames: %d\n", num_frames);
  //printf("Read frames: %d\n", read_frames);
  printf("--------------------------\n");
  
  sf_close(sndfile);

  int cur_pos = 0;
  float * ptr;
  
  char last_key_pressed;
  int pause_found = 0;
    
  while((cur_pos + BLOCKSIZE) <= read_frames) {
    //printf("Current position: %d\n", cur_pos);
    ptr = &buf[cur_pos];

    char DTMF_key = DTMF_Decode(ptr, SAMPLERATE, BLOCKSIZE);
    
    if (DTMF_key > 0) {
      if (DTMF_key != 1) {
        if (last_key_pressed != DTMF_key) {
          printf("Key pressed: %c\n", DTMF_key);
          last_key_pressed = DTMF_key;
          pause_found = 0;
        } 
        else if((DTMF_key == last_key_pressed) && pause_found == 1) {
          printf("Key pressed: %c\n", DTMF_key);
          last_key_pressed = DTMF_key;
          pause_found = 0;
        }
      } else {
        //printf("Pause found\n");
        pause_found = 1;
      }
    } else {
      printf("No DTMF signal detected\n");
    }

    cur_pos+=BLOCKSIZE;
  }

  free(buf);
  return 0;
re
}

