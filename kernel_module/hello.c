#include <linux/init.h>
#include <linux/fs.h> 	     /* file stuff */
#include <linux/kernel.h>    /* printk() */
#include <linux/errno.h>     /* error codes */
#include <linux/module.h>  /* THIS_MODULE */
#include <linux/cdev.h>      /* char device stuff */
#include <linux/miscdevice.h>	/* struct miscdevice and misc_[de]register() */
#include <asm/uaccess.h>  /* copy_to_user() */


//http://elixir.free-electrons.com/linux/latest/source/Documentation/timers/timers-howto.txt
#include <linux/delay.h> /* usleep_range */


#include <linux/mutex.h>	/* mutexes */
#include <linux/sched.h>	/* wait queues */
#include <linux/slab.h>	/* kzalloc() function */

#include <linux/kthread.h>  // for threads
#include <linux/time.h>   // for using jiffies 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valentine Sinitsyn <valentine.sinitsyn@gmail.com>");
MODULE_DESCRIPTION("In-kernel phrase reverser");
static unsigned long buffer_size = 8192;
module_param(buffer_size, ulong, (S_IRUSR | S_IRGRP | S_IROTH));
MODULE_PARM_DESC(buffer_size, "Internal buffer size");





//======================================================
// a system-wide unique wait-queue is the central data structure
// 
// any pending work will be in the queue, and one client can submit as many pending works as they want 
// and retreiveds
// 


struct buffer {
	wait_queue_head_t read_queue;
	struct mutex lock;
	char *data, *end;
	char *read_ptr;
	unsigned long size;
};

static struct buffer *buffer_alloc(unsigned long size)
{
	struct buffer *bufret = NULL;

	bufret = kzalloc(sizeof(*bufret), GFP_KERNEL);
	if (unlikely(!bufret))
		goto out;

	bufret->data = kzalloc(size, GFP_KERNEL);
	if (unlikely(!bufret->data))
		goto out_free;

	init_waitqueue_head(&bufret->read_queue);

	mutex_init(&bufret->lock);

	/* It's unused for now, but may appear useful later */
	bufret->size = size;

 out:
	return bufret;

 out_free:
	kfree(bufret);
	return NULL;
}
static void buffer_free(struct buffer *buffer)
{
	kfree(buffer->data);
	kfree(buffer);
}
//======================================================

static struct buffer *buf = NULL;

static int reverse_open(struct inode *inode, struct file *file)
{
	
	int err = 0;

	/*
	 * Real code can use inode to get pointer to the private
	 * device state.
	 */
    
	//file->private_data = buf_global;
	
    printk(KERN_INFO"reverse_open() buffer created\n");
 out:
	return err;
}

static ssize_t reverse_read(struct file *file, char __user * out,
			    size_t size, loff_t * off)
{
	//struct buffer *buf = file->private_data;
	ssize_t result;

	if (mutex_lock_interruptible(&buf->lock)) {
		result = -ERESTARTSYS;
		goto out;
	}

	while (buf->read_ptr == buf->end) {
		mutex_unlock(&buf->lock);
		if (file->f_flags & O_NONBLOCK) {
			result = -EAGAIN;
			goto out;
		}
		if (wait_event_interruptible
		    (buf->read_queue, buf->read_ptr != buf->end)) {
		    //we are interrupted by some SIGNAL instead of wake_up_interruptible() calls
			result = -ERESTARTSYS;
			goto out;
		}
		if (mutex_lock_interruptible(&buf->lock)) {
    		//we are interrupted by some SIGNAL instead of mutex locked
			result = -ERESTARTSYS;
			goto out;
		}
		
		// we are wokeup by wake_up_interruptible() and
		// we have gain the mutex lock
	}

	size = min(size, (size_t) (buf->end - buf->read_ptr));
	if (copy_to_user(out, buf->read_ptr, size)) {
		result = -EFAULT;
		goto out_unlock;
	}

	buf->read_ptr += size;
	result = size;

 out_unlock:
	mutex_unlock(&buf->lock);
 out:
	return result;
}


static ssize_t reverse_write(struct file *file, 
                 const char __user * in,
			     size_t size, loff_t * unused_off)
{
	//struct buffer *buf = file->private_data;
	ssize_t result;

	if (size > buffer_size) {
		result = -EFBIG;
		goto out;
	}

	if (mutex_lock_interruptible(&buf->lock)) {
		result = -ERESTARTSYS;
		goto out;
	}

	if (copy_from_user(buf->data, in, size)) {
		result = -EFAULT;
		goto out_unlock;
	}

	buf->end = buf->data + size;
	buf->read_ptr = buf->data;

	//if (buf->end > buf->data)
	//	reverse_phrase(buf->data, buf->end - 1);

	wake_up_interruptible(&buf->read_queue);

	result = size;
 out_unlock:
	mutex_unlock(&buf->lock);
 out:
	return result;
}

static int reverse_close(struct inode *inode, struct file *file)
{
	//struct buffer *buf = file->private_data;
	//buffer_free(buf);
    printk(KERN_INFO"reverse_close() buffer free OK\n");
	return 0;
}



//==================================================================

/* Module entry point */
static struct file_operations reverse_fops = {
  .owner = THIS_MODULE,
  .open = reverse_open,
  .release = reverse_close,
  .read = reverse_read,
  .write = reverse_write,
  .llseek = noop_llseek /* Set our device to be non-seekable */
};

/* miscdevice struct to register a minor number for our device */
static struct miscdevice reverse_misc_device = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "reverse",
  .fops = &reverse_fops
};

static int __init reverse_init(void)
{
    if (!buffer_size)
    return -1;

    buf = buffer_alloc(buffer_size);
    if (unlikely(!buf)) {
	    return -1;
    }

    misc_register(&reverse_misc_device);
    printk(KERN_INFO
      "reverse device has been registered, buffer size is %lu bytes\n", 
      buffer_size);
    return 0;
}

static void __exit reverse_exit(void)
{
    misc_deregister(&reverse_misc_device);
    printk(KERN_INFO"reverse device has been unregistered\n");
}

 
module_init(reverse_init);
module_exit(reverse_exit);

















#if 0

//==========================================================================

static const char    g_s_Hello_World_string[] = "Hello world from kernel mode!\n\0";
static const ssize_t g_s_Hello_World_size = sizeof(g_s_Hello_World_string);
static ssize_t device_file_read(
                        struct file *file_ptr
                       , char __user *user_buffer
                       , size_t count
                       , loff_t *position)
{
    static char tmpbuf[PATH_MAX];
    const char * ppath = dentry_path_raw(file_ptr->f_path.dentry,tmpbuf,sizeof(tmpbuf));
    
    //unsigned long msleep_interruptible(unsigned int msecs)
    unsigned int timeout = 10*1000;
    if((timeout = msleep_interruptible(timeout)) > 0){
        printk( KERN_NOTICE "Simple-driver: msleep_interruptible() return prematurelly %d ", timeout);
    }
    
    printk( KERN_NOTICE "Simple-driver: Device file (%s) is read at offset = %i, read bytes count = %u"
                , ppath?ppath:"Unknown"
                , (int)*position
                , (unsigned int)count );
    /* If position is behind the end of a file we have nothing to read */
    if( *position >= g_s_Hello_World_size )
        return 0;
    /* If a user tries to read more than we have, read only as many bytes as we have */
    if( *position + count > g_s_Hello_World_size )
        count = g_s_Hello_World_size - *position;
    if( copy_to_user(user_buffer, g_s_Hello_World_string + *position, count) != 0 )
        return -EFAULT;    
    /* Move reading position */
    *position += count;
    return count;
}


//==========================================================================
static struct file_operations simple_driver_fops = 
{
    .owner   = THIS_MODULE,
    .read    = device_file_read,
};

static int device_file_major_number = 0;
static const char device_name[] = "Simple-driver";

static int register_device(void)
{
    int result = 0;
    printk( KERN_NOTICE "Simple-driver: register_device() is called." );
    result = register_chrdev( 0, device_name, &simple_driver_fops );
    if( result < 0 )
    {
        printk( KERN_WARNING "Simple-driver:  can\'t register character device with errorcode = %i", result );
        return result;
    }
    device_file_major_number = result;
    printk( KERN_NOTICE "Simple-driver: registered character device with major number = %i and minor numbers 0...255"
         , device_file_major_number );
    return 0;
}

void unregister_device(void)
{
    printk( KERN_NOTICE "Simple-driver: unregister_device() is called" );
    if(device_file_major_number != 0)
    {
        unregister_chrdev(device_file_major_number, device_name);
    }
}

//===========================================================================
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lakshmanan");
MODULE_DESCRIPTION("A Simple Hello World module");

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello world!\n");
    int result = register_device();
    return result;// Non-zero return means that the module couldn't be loaded.
}

static void __exit hello_cleanup(void)
{
    unregister_device();
    printk(KERN_INFO "Cleaning up module.\n");
}

module_init(hello_init);
module_exit(hello_cleanup);



#endif

