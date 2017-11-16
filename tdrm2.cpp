#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>


class DRM
{
public:
    DRM(const char *node = "/dev/dri/card0"){
	    int ret;
	    uint64_t has_dumb;

	    fd = open(node, O_RDWR | O_CLOEXEC);
	    if (fd < 0) {
		    ret = -errno;
		    fprintf(stderr, "cannot open '%s': %m\n", node);
		    return ret;
	    }

	    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 ||
	        !has_dumb) {
		    fprintf(stderr, "drm device '%s' does not support dumb buffers\n",
			    node);
		    close(fd);
		    return -EOPNOTSUPP;
	    }
	    *out = fd;
	    return 0;
    }
    
    int fd;    
};




