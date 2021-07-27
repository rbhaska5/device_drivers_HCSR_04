//hcsr main code

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "main_header.h"


int main(int argc, char **argv) {


  int fd0;
  int fd1;
  char pins[2];
  int write_parameter;
  struct fifo_buffer *read_buffer = (struct fifo_buffer*)malloc(sizeof(struct fifo_buffer));
  printf("main entered...\n");

  //hcsr device 1
  fd0 = open("/dev/hcsr_0",O_RDWR);
  printf("HCSR_0 opened \n");
  if (fd0 < 0 ){
    printf("error in opening the device\n");
    return 0;
  }

  //setting command CONFIG PINS

  pins[0] = 4;  //arduino pin 4 as trigger pin
  pins[1] = 11; //arduino pin 11 as echo pin
  printf("user input: trigger pin =  %d, echo pin =  ,%d\n",pins[0],pins[1]);
  ioctl(fd0,CONFIG_PINS,&pins);

  //setting command SET_PARAMETER
  pins[0] = 5;  //number of samples
  pins[1] = 50; //delta value
  printf("user input: number of samples =  %d, delta =  ,%d\n",pins[0],pins[1]);
  ioctl(fd0,SET_PARAMETER,&pins);


  //read and write functions

  write_parameter = 2;   //any non-zero value for write to occur
  write(fd0,&write_parameter,sizeof(write_parameter));
  sleep(5);

  read(fd0,read_buffer,sizeof(struct fifo_buffer));
  sleep(5);

  write_parameter = 0;   //any non-zero value
  write(fd0,&write_parameter,sizeof(write_parameter));
  sleep(5);

  //HCSR device 2
  fd1 = open("/dev/hcsr_1",O_RDWR);
  printf("HCSR_1 opened \n");
  if (fd1 < 0 ){
    printf("error in opening the device\n");
    return 0;
  }

  //setting command CONFIG PINS
  pins[0] = 5; //arduino pin 5 as trigger pin
  pins[1] = 13; //arduino pin 13 as echo pin
  printf("user input: trigger pin =  %d, echo pin =  ,%d\n",pins[0],pins[1]);
  ioctl(fd0,CONFIG_PINS,&pins);

  //setting command SET_PARAMETER
  pins[0] = 7;  //number of samples
  pins[1] = 50; //delta value
  printf("user input: number of samples =  %d, delta =  ,%d\n",pins[0],pins[1]);
  ioctl(fd0,SET_PARAMETER,&pins);


  //read and write functions

  write_parameter = 3;   //any non-zero value
  write(fd0,&write_parameter,sizeof(write_parameter));
  sleep(5);

  read(fd0,read_buffer,sizeof(struct fifo_buffer));
  sleep(5);

  close(fd0);
  close(fd1);

  return 0;
}
