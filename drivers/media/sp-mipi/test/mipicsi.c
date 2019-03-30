#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>           /* getting information about files attributes */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

/* wrapped errno display function by v4l2 API */
static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

/* wrapped ioctrl function by v4l2 API */
static int xioctl(int fd, int request, void * arg)
{
	int r;
	do{
		r = ioctl(fd, request, arg);
	}
	while(-1 == r && EINTR == errno);
	return r;
}

struct buffer {
        void   *start;
        size_t  length;
};

static char            *dev_name;
static int              fd = -1;
struct buffer          *framebuf;
static unsigned int     n_buffers;
static int              frame_count = 10;
static int              req_count = 2;  // must above 2
static int              frame_number = 0;

static void close_device(void)
{
    if (-1 == close(fd))
        errno_exit("close");        
    //fd = -1;
}

static void unmap_buf(void)
{
    unsigned int i;

    // Release the request buffer
    for (i=0; i< n_buffers; i++) 
    {
		if (-1 == munmap(framebuf[i].start, framebuf[i].length))
			errno_exit("munmap");

    }	
    free(framebuf);
}

static void stop_capturing(void)
{
    enum v4l2_buf_type type;

    //stream off
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
        errno_exit("VIDIOC_STREAMOFF");
    }
}

static void mainloop(void)
{
    unsigned int count;
    struct v4l2_buffer buf;
    char filename[15];

    count = frame_count;

    while (count-- > 0) {
        // de-queue buffer (when buffer is fill)
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            errno_exit("VIDIOC_DQBUF");
        }
        printf("DQBUF end\n");

        //assert(buf.index < n_buffers);
        
        // Process frame        

        sprintf(filename, "frame-%d.raw", frame_number);

        FILE *fp = fopen(filename,"wb");    
        if (-1 == fp) {
            fprintf(stderr, "Cannot open '%s': %d, %s\n",
                        dev_name, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
        fwrite(framebuf[buf.index].start, 1, buf.length, fp);
        printf("Capture one frame saved in %s\n", filename);

        if (-1 == fclose(fp)) {
            fprintf(stderr, "Cannot close '%s': %d, %s\n",
                        dev_name, errno, strerror(errno));
              exit(EXIT_FAILURE);
        }        
        frame_number++;

        // Re-queen buffer
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            errno_exit("VIDIOC_QBUF");
        }
    }
}

static void start_capturing(void)
{
    unsigned int i;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;

    // Queen buffer
    for (i = 0; i < n_buffers; ++i) {  
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            errno_exit("VIDIOC_QBUF");
        }
    }

    // stream on
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
            errno_exit("VIDIOC_STREAMON");
}

static void init_mmap(void)
{
	// request buffer
    struct v4l2_requestbuffers req;

    CLEAR(req);
    req.count = req_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } 
        else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }
    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
        exit(EXIT_FAILURE);
    }

    framebuf = calloc(req.count, sizeof(*framebuf));
    if (!framebuf) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
 
	// query buffer, mmap
    struct v4l2_buffer buf;  

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        CLEAR(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            errno_exit("VIDIOC_QUERYBUF");
        }

        framebuf[n_buffers].length = buf.length;
        framebuf[n_buffers].start = (char *) mmap(
                            NULL,
                            buf.length,
                            PROT_READ|PROT_WRITE,
                            MAP_SHARED,
                            fd, buf.m.offset
        );
        if (MAP_FAILED == framebuf[n_buffers].start)
            errno_exit("mmap");

        printf("Frame buffer %d: address=0x%x, length=%x\n", 
                n_buffers, (unsigned int)framebuf[n_buffers].start, framebuf[n_buffers].length);
    }
}

static void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    char fmtstr[8];

    // Enumerate image formats
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index=0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while(-1 != xioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc))
    {
        printf("SUPPORT\t%d.%s\n",fmtdesc.index,fmtdesc.description);
        fmtdesc.index++;
    }

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
                fprintf(stderr, "%s is no V4L2 device\n", dev_name);
                exit(EXIT_FAILURE);
        } 
        else {
                errno_exit("VIDIOC_QUERYCAP");
        }
    }		
	printf( "Driver Caps:\n"
			"  Driver: \"%s\"\n"
			"  Card: \"%s\"\n"
			"  Bus: \"%s\"\n"
			"  Version: %d.%d\n"
			"  Capabilities: %08x\n",
					cap.driver,
					cap.card,
					cap.bus_info,
					(cap.version>>16)&0xff,
					(cap.version>>24)&0xff,
					cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
        exit(EXIT_FAILURE);
    }  

    // set fmt
    CLEAR(fmt); 
    fmt.fmt.pix.width = 1280;
    fmt.fmt.pix.height = 800;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit("VIDIOC_S_FMT");
    }

	// get fmt
//    CLEAR(fmt); 
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt)) {
        errno_exit("VIDIOC_G_FMT");
    }
    // Print Stream Format
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);
    printf("Stream Format Informations:\n");
    printf(" type: %d\n", fmt.type);
    printf(" width: %d\n", fmt.fmt.pix.width);
    printf(" height: %d\n", fmt.fmt.pix.height);
    printf(" pixelformat: %s\n", fmtstr);
    printf(" field: %d\n", fmt.fmt.pix.field);
    printf(" bytesperline: %d\n", fmt.fmt.pix.bytesperline);
    printf(" sizeimage: %d\n", fmt.fmt.pix.sizeimage);
    printf(" colorspace: %d\n", fmt.fmt.pix.colorspace);

    init_mmap();
}

static void open_device(void)
{
    struct stat st;

    if (-1 == stat(dev_name, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                    dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    //fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    fd = open(dev_name, O_RDWR, 0);
    if (-1 == fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                    dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Version 1.3\n"
                 "Options:\n"
                 "-h | --help          Print this message\n"
                 "-d | --device name   Video device name [%s]\n"
                 "-r | --read          Number of request buffer [%i]\n"
                 "-c | --count         Number of frames to grab [%i]\n"
                 "",
                 argv[0], dev_name, req_count, frame_count);
}

static const char short_options[] = "hd:r:c:";

static const struct option
long_options[] = {
        { "help",       no_argument,       NULL, 'h' },
        { "device",     required_argument, NULL, 'd' },
        { "req_count",  required_argument, NULL, 'r' },
        { "count",      required_argument, NULL, 'c' },
        { 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
    dev_name = "/dev/video0";

    for (;;) {
        int idx;
        int c;

        c = getopt_long(argc, argv, short_options, long_options, &idx);

        if (-1 == c)
                break;

        switch (c) {
        case 0: /* getopt_long() flag */
                break;

        case 'h':
                usage(stdout, argc, argv);
                exit(EXIT_SUCCESS);

        case 'd':
                dev_name = optarg;
                break;

        case 'r':
                errno = 0;
                req_count = strtol(optarg, NULL, 0);
                if (errno)
                        errno_exit(optarg);
                break;

        case 'c':
                errno = 0;
                frame_count = strtol(optarg, NULL, 0);
                if (errno)
                        errno_exit(optarg);
                break;

        default:
                usage(stderr, argc, argv);
                exit(EXIT_FAILURE);
        }
    }
    open_device();
    init_device();
    start_capturing();
    mainloop();
    stop_capturing();
    unmap_buf();
    close_device();

    fprintf(stderr, "\n");
    return 0;
}