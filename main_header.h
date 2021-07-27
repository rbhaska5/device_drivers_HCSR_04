
#include <linux/ioctl.h>
#include <linux/unistd.h>

#define CONFIG_PINS _IOW('q',0,int)
#define SET_PARAMETER _IOW('q',1,int)


struct fifo_buffer{    //needed to read values from kernel
  unsigned int timestamp;
  unsigned long int dist;
}buffer;


struct parameters{   //parameters - config pins, m, delta
  int par1;
  int par2;
  char name[40];
};






