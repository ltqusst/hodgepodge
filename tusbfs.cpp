#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>

//======USBFS===================
#include <usb.h>
#include <linux/usbdevice_fs.h>
#include <asm/byteorder.h>


int main(int argc, char *argv[])
{
    int fd = open(argv[1], O_RDONLY);
    if(fd < 0)
    {
        perror("open error");
        exit(-1);
    }
    
    usbdevfs_hub_portinfo getdrv;
    
    if(ioctl (fd, USBDEVFS_HUB_PORTINFO, &getdrv) < 0)
    {
        perror("ioctl USBDEVFS_HUB_PORTINFO error");
        exit(-1);
    }
    
    printf("interface:%d\n",getdrv.nports);
   // printf("driver:%s\n",getdrv.driver);
    
    close(fd);
}

