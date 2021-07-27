#define NO_OF_DEVICES 100

#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/unistd.h>

#define CONFIG_PINS _IOW('q',0,int)
#define SET_PARAMETER _IOW('q',1,int)


int gpio_num_t[4];  //storing gpio trigger pins
int gpio_num_e[4];  //storing gpio echo pins
int irq_num;
int distance;
uint64_t tsc1=0, tsc2=0;   //time stamp counter


void pin_table(int, int, struct file *);  //for pin configuration
void measure(void);
int i ;
struct HCSR_DEV *pointer;


struct fifo_buffer{
  unsigned int timestamp;
  unsigned long int dist;
}buffer;

struct HCSR_DEV{

  struct miscdevice hcsr_misc_dev;
  int trigger_pin;
  int m;
  int delta;
  int echo_pin;
  struct fifo_buffer buffer[5];
  int head;
  int tail;
  int flag;
  int sum[50];

}*HCSR_devp[NO_OF_DEVICES];


struct parameters{
  int par1;
  int par2;
  char name[40];
};

struct workqueue_struct *my_wq;
typedef struct my_work{
  struct work_struct  my_work;
  struct HCSR_DEV *hcsr;
} my_wqp;





