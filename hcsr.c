
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#include "hcsr_header.h"

static int num = 5;     //number of hcsr devices
int FIFO_SIZE = 5;
int count =0;
module_param(num,int,0000);

//time stamp counter - calculates time
#if defined(__i386__)
static __inline__ unsigned long long rdtsc(void)

{
  unsigned long long int x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}

#elif defined(__x86_64__)
static __inline__ unsigned long long rdtsc(void)

{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif


static void my_wq_function( struct work_struct *work)
{
  struct HCSR_DEV *pointer;
  my_wqp *workp = container_of(work, struct my_work, my_work);
  pointer = workp->hcsr;
  //calling measure
  measure();
}


//function store - using to store time and distance from read
int store(struct fifo_buffer *buf)
{
  //  for(i=0;i<5;i++){
  if(pointer->tail!=pointer->head){
    buf->timestamp = pointer->buffer[pointer->tail].timestamp;
    buf->dist = pointer->buffer[pointer->tail].dist;
    pointer->tail +=1;
    if(pointer->tail == 5){
      pointer->tail = 0;
    }
    else
    return i;
  }
  else if(pointer->head==pointer->tail)
  {
    printk("buffer empty\n");
  }
  return 0;
}

static ssize_t hcsr_read(struct file *file, char *buf,size_t count,loff_t *ppos)
{

  //struct HCSR_DEV *read_ptr = file->private_data;
    int headval,tailval,flag;
  struct fifo_buffer *buff = (struct fifo_buffer * )kmalloc(sizeof(struct fifo_buffer), GFP_KERNEL);
  struct miscdevice *misc = file->private_data;
  struct HCSR_DEV *read_ptr ;
  read_ptr = container_of(misc,struct HCSR_DEV,hcsr_misc_dev);

  headval = read_ptr->head;
  tailval = read_ptr->tail;
  flag=read_ptr->flag;

  printk(KERN_INFO "head value %d\n",headval);
  printk(KERN_INFO "tail value %d\n",tailval);

  ///////////////////////////////////////////////
  if(headval==tailval)
  {
    printk(KERN_ALERT "buffer empty\n");
    if(flag==1){
      while(flag==1)
      {
        printk("on-going measurement. Wait until done....\n");
      }
      store(buff);
      copy_to_user(buf,buff,sizeof(struct fifo_buffer));
      kfree(buff);
    }
    else if(flag==0)
    {
      measure();
      printk("triggering and measuring done by read\n");
      store(buff);
      copy_to_user(buf,buff,sizeof(struct fifo_buffer));
      kfree(buff);
    }
  }

  else
  {
    store(buff);
    copy_to_user(buf,buff,sizeof(struct fifo_buffer));
    kfree(buf);
  }
  return 0;
}

static ssize_t hcsr_write(struct file *file, const char *val ,size_t count,loff_t *ppos)
{
  int data;
  int ret;
  my_wqp *work;
  int copy;
  //struct HCSR_DEV *pointer = file->private_data;
  struct miscdevice *misc = file->private_data;
  struct HCSR_DEV *pointer = container_of(misc,struct HCSR_DEV,hcsr_misc_dev);

  //get_user(copy,val);
  data = copy_from_user(&copy,val,sizeof(int));
  work = (my_wqp *)kmalloc(sizeof(my_wqp), GFP_KERNEL);
  //  printk("space allocated \n");

  if(data!=0){
    if(pointer->flag==0){  //no ongoing measurement operation
      //printk("clearing per device buffer\n");
      pointer->head = 0;
      pointer->tail = 0;

      if (work) {
        INIT_WORK( (struct work_struct *)work, my_wq_function );
        work->hcsr = pointer;
        ret = queue_work( my_wq, (struct work_struct *)work );
        printk(KERN_INFO "work passed to wq function \n");
      }
    }
    else if(pointer->flag==1){

      printk(KERN_INFO "on-going measurement\n");
      return -EINVAL;
    }
  }
  else {
    printk(KERN_INFO"input integer passed is zero\n");
  }

  return 0;
}

static long hcsr_ioctl(struct file *file, unsigned int ioctl, unsigned long val)
{
  int res;
  //struct parameters *buf;
  char copy[2];
  //struct HCSR_DEV *pointer = file->private_data;
  struct miscdevice *misc = file->private_data;
  struct HCSR_DEV *pointer = container_of(misc,struct HCSR_DEV,hcsr_misc_dev);


  printk("ioctl entered\n");

  //cmdbuf= (struct cmd*)kmalloc(sizeof(struct cmd), GFP_KERNEL);

  res = copy_from_user(copy, (void *)val,sizeof(copy));
  if(res)
  printk("error in copying data from user...\n");

  // printk("copy[0] = %d\n",copy[0]);
  // printk("copy[1] = %d\n",copy[1]);

  switch(ioctl)
  {
    case CONFIG_PINS:
    pin_table(copy[0],copy[1],file);
    printk(KERN_INFO "pin configuration done\n");
    break;

    case SET_PARAMETER:
    //printk("name parameters: %s\n",buf->name);
    pointer->m = copy[0];
    pointer->delta = copy[1];
    printk(KERN_INFO "opeartion parameters set (m and delta)\n");
    break;

    default:
    return -EINVAL;
  }

  return 0;
}

//open function
int hcsr_open(struct inode *inode, struct file *file)
{
  struct miscdevice *misc = file->private_data;
  printk(KERN_INFO "hcsr device opened successfully\n");
  pointer = container_of(misc,struct HCSR_DEV,hcsr_misc_dev);
  return 0;
}


// close or release function
int hcsr_drv_release(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "HCSR device released\n");
  return 0;
}

static struct file_operations hcsr_fops = {
  .owner			         = THIS_MODULE,
  .unlocked_ioctl      = hcsr_ioctl,
  .read                = hcsr_read,
  .write               = hcsr_write,
  .open			           = hcsr_open,
  .release		         = hcsr_drv_release,
};

void request(int *gpio_map)
{

  int i,a,req;
  //printk("requested entered \n");
  for(i=0;i<4;i++)
  {
    //printk("for loop entered................ \n");
    printk("gpio pin:%d\n",*gpio_map);
    a = *gpio_map;
    printk("a:%d\n",a);
    if(a>0)
    {
      printk("if loop entered.... \n");
      //int req;
      req = gpio_request(a,"sysfs");
      if(req!=0)
      printk("gpio request error! \n");

      // gpio_export(a,0);
      gpio_map++;
    }
  }

}

void set_direction(int *gpio_map,int d)
{
  int a;
  //gpio_map++;
  for(i=0;i<2;i++){
    a=*gpio_map;
    printk("pin in set direction function %d\n",a);
    if(d==0 && a<=64)
    {
      gpio_direction_output(a,0);
      printk("output pin \n");
    }
    if(d==1)
    {
      gpio_direction_input(a);
      printk("input pin \n");
    }
    gpio_map++;
    printk("direction set...\n");
  }
}

  void pin_set_value(int *gpio_map,int d)
  {

    int i,a,b;

    printk("set val entered\n");

    gpio_map++;
    a=b=*gpio_map;
    printk("current gpio pin %d\n",a);

    if(d==0){    //trigger
      for(i=1;i<4;i++)
      {
        if(a>0){
          printk("set val for trigger...\n");
          gpio_set_value_cansleep(a,0);
        }
        a= (int)++gpio_map;
      }
    }

    printk("b value before d==1:%d\n",b);


    //printk("current gpio pin - incremented twice  %d\n",a);
    if(d==1) //echo
    {
      if(b>0){
        printk("set val for echo..\n");
        gpio_set_value_cansleep(b,1);
      }

      gpio_map++;
      b=*gpio_map;

      for(i=2;i<4;i++)
      {
        if(b>0){
          printk("setting val for echo \n");
          gpio_set_value_cansleep(b,0);
          printk("setting val for echo donee \n");
        }
        b=(int)++gpio_map;
      }
    }
  }

  //irq handler
  //#################################################################################
  static irq_handler_t irq_handler(unsigned int irq, void *dev_id){

    struct HCSR_DEV* pointer = (struct HCSR_DEV *)dev_id;
    int value;

    value = gpio_get_value(pointer->echo_pin);
    //  printk("irq handler entered\n");

    //printk("gpio get value.......%d\n",value);
    if (value == 0){
      tsc2 = rdtsc();
      distance= ((int)(tsc2-tsc1)*340);
      div_u64(distance, 8000000);
      if(count<=pointer->m +2)
      {
      pointer->sum[count]=distance;
      count++;
      }
      else
      {
      	count=0;
      }
      // printk("distance in handler = %d\n",distance);
      irq_set_irq_type(irq_num,IRQ_TYPE_EDGE_RISING);
      //printk("irq type edge rising\n");
    }

    else if(value == 1) {
      tsc1 = rdtsc();
      irq_set_irq_type(irq_num,IRQ_TYPE_EDGE_FALLING);
      //  printk("irq type edge falling\n");
    }

    return (irq_handler_t) IRQ_HANDLED;
  }

  void measure(void){

    //struct hcsr_dev *hcsr = file->private_data;

    int new_sum=0;
    int average;

    //printk("is pointer working, trig pin check in measure %d\n",pointer->trigger_pin);

    pointer->flag=1;
    for(i=0;i<pointer->m +2;i++){       //m+2 triggers

      gpio_set_value_cansleep(pointer->trigger_pin,0);
      mdelay(100);
      gpio_set_value_cansleep(pointer->trigger_pin,1);
      mdelay(100);
      gpio_set_value_cansleep(pointer->trigger_pin,0);
      //printk("triggering done\n");
      // sleep();
      printk("delta from user= %d ]n",pointer->delta);
      udelay(pointer->delta);

      printk("distance value from handler %d \n",distance);

      new_sum+=pointer->sum[i];
    }


    average = pointer->m;
    div_u64(new_sum,average);

    // HCSR_devp->tail = (HCSR_devp->tail +1);

    // HCSR_devp->buffer[HCSR_devp->tail].time_stamp = rdtsc();
    // HCSR_devp->buffer[HCSR_devp->tail].value=sum;

    //for(i=0;i<FIFO_SIZE;i++){
    if(pointer->head +1 ==pointer->tail){
      printk("Buffer is full........i=%d\n",i);
    }
    else if(pointer->head +1 == FIFO_SIZE && pointer->tail==0){
      printk("Buffer is full........i=%d\n",i);
    }
    else{
      pointer->buffer[pointer->head].timestamp=rdtsc();
      pointer->buffer[pointer->head].dist=new_sum;


      printk("timestamp = %lu\n", (unsigned long)pointer->buffer[pointer->head].timestamp);
      printk("sum value =  %ld\n",  pointer->buffer[pointer->head].dist);

      pointer->head +=1;

      if(pointer->head ==FIFO_SIZE){
        pointer->head=0;
      }
    }


    // HCSR_devp->head= (HCSR_devp->head +1);
    //
    // if(HCSR_devp->head == HCSR_devp->tail)
    // HCSR_devp->tail = (HCSR_devp->tail +1);

    pointer->flag=0;


  }


  //function to request irq for echo pin
  ///////////////////////////////////////////////////////////////////////////////////

  void irq_request(int *gpio_map)
  {

    int a,req;
    printk("irq request entered\n");
    a=*gpio_map;

    irq_num = gpio_to_irq(a);
    printk("associated irq num for given gpio %d\n",irq_num);
    if(irq_num<0)
    printk("error - irq number \n");
    req = request_irq(irq_num,(irq_handler_t)irq_handler, IRQF_TRIGGER_RISING,"interrupt",(void*)pointer);
    if (req < 0)
    printk("Error in request_irq\n");
  }


  void pin_table(int par1, int par2, struct file *file){  //par 1 - trig pin

    int i;
    int val;

    switch (par1){
      case 0:
      gpio_num_t[0] = 11;
      gpio_num_t[1] = 32;
      gpio_num_t[2] = -1;
      gpio_num_t[3] = -1;

      //  printk("gpio numbers..\n");
      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 0\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 11;
      pin_set_value(gpio_num_t,0);

      break;

      case 1:
      gpio_num_t[0] = 12;
      gpio_num_t[1] = 28;
      gpio_num_t[2] = 45;
      gpio_num_t[3] = -1;

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 1\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 12;
      pin_set_value(gpio_num_t,0);

      break;

      case 2:
      gpio_num_t[0] = 13;
      gpio_num_t[1] = 34;
      gpio_num_t[2] = 77;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 2\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 13;
      pin_set_value(gpio_num_t,0);

      break;

      case 3:
      gpio_num_t[0] = 14;
      gpio_num_t[1] = 16;
      gpio_num_t[2] = 76;
      gpio_num_t[3] = 64;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 3\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 14;
      pin_set_value(gpio_num_t,0);

      break;

      case 4:
      gpio_num_t[0] = 6;
      gpio_num_t[1] = 36;
      gpio_num_t[2] = -1;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 4\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 6;
      pin_set_value(gpio_num_t,0);

      break;

      case 5:
      gpio_num_t[0] = 0;
      gpio_num_t[1] = 18;
      gpio_num_t[2] = 66;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 5\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 0;
      pin_set_value(gpio_num_t,0);

      break;

      case 6:
      gpio_num_t[0] = 1;
      gpio_num_t[1] = 20;
      gpio_num_t[2] = 68;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 6\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 1;
      pin_set_value(gpio_num_t,0);

      break;

      case 7:
      gpio_num_t[0] = 38;
      gpio_num_t[1] = 39;
      gpio_num_t[2] = -1;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 7\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 38;
      pin_set_value(gpio_num_t,0);

      break;

      case 8:
      gpio_num_t[0] = 40;
      gpio_num_t[1] = 41;
      gpio_num_t[2] = -1;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 8\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 40;
      pin_set_value(gpio_num_t,0);

      break;

      case 9:
      gpio_num_t[0] = 4;
      gpio_num_t[1] = 22;
      gpio_num_t[2] = 70;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 9\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 4;
      pin_set_value(gpio_num_t,0);

      break;

      case 10:
      gpio_num_t[0] = 10;
      gpio_num_t[1] = 26;
      gpio_num_t[2] = 74;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 10\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 10;
      pin_set_value(gpio_num_t,0);

      break;

      case 11:
      gpio_num_t[0] = 5;
      gpio_num_t[1] = 24;
      gpio_num_t[2] = 44;
      gpio_num_t[3] = 72;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 11\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 5;
      pin_set_value(gpio_num_t,0);

      break;

      case 12:
      gpio_num_t[0] = 15;
      gpio_num_t[1] = 42;
      gpio_num_t[2] = -1;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 12\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 15;
      pin_set_value(gpio_num_t,0);

      break;

      case 13:
      gpio_num_t[0] = 7;
      gpio_num_t[1] = 30;
      gpio_num_t[2] = 46;
      gpio_num_t[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_t[i]);
      }

      printk("Arduino pin 13\n");

      request(gpio_num_t);
      set_direction(gpio_num_t,0);
      pointer->trigger_pin = 7;
      pin_set_value(gpio_num_t,0);

      break;

    }
    printk("trigger pin done\n");

    switch(par2){

      case 0:
      gpio_num_e[0] = 11;
      gpio_num_e[1] = 32;
      gpio_num_e[2] = -1;
      gpio_num_e[3] = -1;


      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 0\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 11;
      pin_set_value(gpio_num_e,1);

      val= gpio_get_value(pointer->echo_pin);
      printk("val is : %d\n",val);

      break;

      case 1:
      gpio_num_e[0] = 12;
      gpio_num_e[1] = 28;
      gpio_num_e[2] = 45;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 1\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);

      set_direction(gpio_num_e,1);
      pointer->echo_pin = 12;
      pin_set_value(gpio_num_e,1);

      val= gpio_get_value(pointer->echo_pin);
      printk("val is : %d\n",val);

      break;

      case 2:
      gpio_num_e[0] = 13;
      gpio_num_e[1] = 34;
      gpio_num_e[2] = 77;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 2\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);

      set_direction(gpio_num_e,1);
      pointer->echo_pin = 13;
      pin_set_value(gpio_num_e,1);

      val= gpio_get_value(pointer->echo_pin);
      printk("val is : %d\n",val);

      break;

      case 3:
      gpio_num_e[0] = 14;
      gpio_num_e[1] = 16;
      gpio_num_e[2] = 76;
      gpio_num_e[3] = 64;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 3\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 14;
      pin_set_value(gpio_num_e,1);

      break;

      case 4:
      gpio_num_e[0] = 6;
      gpio_num_e[1] = 36;
      gpio_num_e[2] = -1;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 4\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 4;
      pin_set_value(gpio_num_e,1);

      break;

      case 5:
      gpio_num_e[0] = 0;
      gpio_num_e[1] = 18;
      gpio_num_e[2] = 66;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 5\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 0;
      pin_set_value(gpio_num_e,1);

      break;

      case 6:
      gpio_num_e[0] = 1;
      gpio_num_e[1] = 20;
      gpio_num_e[2] = 68;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 6\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 1;
      pin_set_value(gpio_num_e,1);

      break;

      case 7:
      gpio_num_e[0] = 38;
      gpio_num_e[1] = 39;
      gpio_num_e[2] = -1;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 7\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 38;
      pin_set_value(gpio_num_e,1);

      break;

      case 8:
      gpio_num_e[0] = 40;
      gpio_num_e[1] = 41;
      gpio_num_e[2] = -1;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 8\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 40;
      pin_set_value(gpio_num_e,1);

      case 9:
      gpio_num_e[0] = 4;
      gpio_num_e[1] = 22;
      gpio_num_e[2] = 70;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 9\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 4;
      pin_set_value(gpio_num_e,1);

      break;

      case 10:
      gpio_num_e[0] = 10;
      gpio_num_e[1] = 26;
      gpio_num_e[2] = 74;
      gpio_num_e[3] = -1;

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 10\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 10;
      pin_set_value(gpio_num_e,1);

      break;

      case 11:
      gpio_num_e[0] = 5;
      gpio_num_e[1] = 24;
      gpio_num_e[2] = 44;
      gpio_num_e[3] = 72;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }

      printk("Arduino echo pin 11\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 5;
      pin_set_value(gpio_num_e,1);

      break;
      case 12:
      gpio_num_e[0] = 15;
      gpio_num_e[1] = 42;
      gpio_num_e[2] = -1;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d",gpio_num_e[i]);
      }
      printk("Arduino echo pin 12\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 15;
      pin_set_value(gpio_num_e,1);

      break;

      case 13:
      gpio_num_e[0] = 7;
      gpio_num_e[1] = 30;
      gpio_num_e[2] = 46;
      gpio_num_e[3] = -1;

      printk("gpio numbers..\n");

      for(i=0;i<4;i++){
        printk("%d\n",gpio_num_e[i]);
      }
      printk("Arduino echo pin 13\n");

      request(gpio_num_e);
      irq_request(gpio_num_e);
      set_direction(gpio_num_e,1);
      pointer->echo_pin = 7;
      pin_set_value(gpio_num_e,1);

      break;

    }
  }

  //init Function
  //
  static int __init m_init(void)
  {
    char name[20];
    int val;
    //int num;

    printk(KERN_INFO "init entered\n");

    for (i=0;i<num;i++)
    {

      HCSR_devp[i] = kmalloc(sizeof(struct HCSR_DEV), GFP_KERNEL);
      if(!HCSR_devp[i])
      {
        printk("BAD kmalloc in per device struct \n");
        return -ENOMEM;
      }

      memset(HCSR_devp[i], 0, sizeof(struct HCSR_DEV));
      memset(name, 0, 20 * sizeof(char));
      //hcsr_devp->tail=0;

      HCSR_devp[i]->hcsr_misc_dev.minor=i;

      sprintf(name, "hcsr_%d",i);
      HCSR_devp[i]->hcsr_misc_dev.name=name;
      HCSR_devp[i]->hcsr_misc_dev.fops=&hcsr_fops;


      printk("registering....%s\n",HCSR_devp[i]->hcsr_misc_dev.name);
      val = misc_register(&HCSR_devp[i]->hcsr_misc_dev);
      printk("HCSR device registered sccessfully\n");

      if(val){
        printk("HCSR device registration Unsuccessful\n");
        return val;
      }
    }

    my_wq = create_workqueue("my_queue");// create work queue
    printk("work queue created\n");

    // hcsr_devp[i]->head = 0;
    // hcsr_devp[i]->tail = 0;
    return 0;
  }

  static void __exit m_exit(void)
  {

    int val,i,j;
    printk("exit loop......\n");

    for(j=0;j<4;j++)
    {
      if(gpio_num_t[j]>0)
      gpio_free(gpio_num_t[j]);

      if(gpio_num_e[j]>0)
      gpio_free(gpio_num_e[j]);
    }

    for (i = 0 ; i < num; i++) {
      // kfree(HCSR_dev[i]->);
      // printk("FIFO Buffer Freed\n");
      val = misc_deregister(&HCSR_devp[i]->hcsr_misc_dev);
      if(val){
        printk("Unregistration failed\n");
      }
      printk("Misc driver de-initialized\n");
      kfree(HCSR_devp[i]);
      printk("Freed per-device structure\n");
    }
  }

  module_init(m_init);
  module_exit(m_exit);
  MODULE_LICENSE("GPL v2");
