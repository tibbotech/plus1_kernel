

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include<poll.h>

#define BUFFER_CNT_MAX 32

typedef struct VideoBuffer {
   void   *start;
    size_t  length;
} VideoBuffer;
 
VideoBuffer framebuf[BUFFER_CNT_MAX];   


static int xioctl(int fd,int request, void *arg)
{
  int r;
  do r=ioctl(fd,request,arg);
  while(-1 ==r && EINTR == errno);
		  
	return r;
}

 
int print_caps(int fd)
{
    struct v4l2_capability caps={};
		
    perror("print_caps enter");
		
    if(-1 == xioctl(fd,VIDIOC_QUERYCAP,&caps))
    {
        perror("Querying cap fail");
        return 1; 	   
    }
		
	printf( "Driver Caps:\n"
			"  Driver: \"%s\"\n"
			"  Card: \"%s\"\n"
			"  Bus: \"%s\"\n"
			"  Version: %d.%d\n"
			"  Capabilities: %08x\n",
					caps.driver,
					caps.card,
					caps.bus_info,
					(caps.version>>16)&0xff,
					(caps.version>>24)&0xff,
					caps.capabilities);
	
	  return 0;    
}
	

int main()
{
    int fd;
	int i,ret;

	// open camera
	fd=open("/dev/video0",O_RDWR);
	if(fd ==-1)	{
        perror("Opening video device");     //No such file or directory
		return -1;				
	}
		
	if(print_caps(fd)) {
		return 1;
	}

    // Enumerate image formats
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index=0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
    {
        printf("SUPPORT\t%d.%s\n",fmtdesc.index,fmtdesc.description);
        fmtdesc.index++;
    }

	// get camera support format
	struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
   
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
			
    if (ret < 0) {
        printf("VIDIOC_G_FMT failed (%d)\n", ret);
        return ret;
    }

    // Print Stream Format
    printf("Stream Format Informations:\n");
    printf(" type: %d\n", fmt.type);
    printf(" width: %d\n", fmt.fmt.pix.width);
    printf(" height: %d\n", fmt.fmt.pix.height);
    char fmtstr[8];
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);
    printf(" pixelformat: %s\n", fmtstr);
    printf(" field: %d\n", fmt.fmt.pix.field);
    printf(" bytesperline: %d\n", fmt.fmt.pix.bytesperline);
    printf(" sizeimage: %d\n", fmt.fmt.pix.sizeimage);
    printf(" colorspace: %d\n", fmt.fmt.pix.colorspace);
    printf(" priv: %d\n", fmt.fmt.pix.priv);

	// request buffer
    struct v4l2_requestbuffers reqbuf;

    reqbuf.count = 2; // must above 2
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    
    ret = ioctl(fd , VIDIOC_REQBUFS, &reqbuf);    
    if(ret < 0) {
        printf("VIDIOC_REQBUFS failed (%d) %d\n", ret,reqbuf.count);
        return ret;
    }

	// query buffer , mmap , qbuf	
    struct v4l2_buffer buf;   
   
    for (i = 0; i < reqbuf.count; i++) 
    {
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        ret = ioctl(fd , VIDIOC_QUERYBUF, &buf);
        if(ret < 0) {
            printf("VIDIOC_QUERYBUF (%d) failed (%d)\n", i, ret);
            return ret;
        }

        framebuf[i].length = buf.length;
        framebuf[i].start = (char *) mmap(
                            NULL,
                            buf.length,
                            PROT_READ|PROT_WRITE,
                            MAP_SHARED,
                            fd, 
                            buf.m.offset
        );
        if (framebuf[i].start == MAP_FAILED) {
            printf("mmap (%d) failed: %s\n", i, strerror(errno));
        return -1;
        }
        printf("Frame buffer %d: address=0x%x, length=%x\n", 
                i, (unsigned int)framebuf[i].start, framebuf[i].length);

        // Queen buffer
        ret = ioctl(fd , VIDIOC_QBUF, &buf);
        if (ret < 0) {
            printf("VIDIOC_QBUF (%d) failed (%d)\n", i, ret);
           return -1;
        }
    }

	// stream on
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        printf("VIDIOC_STREAMON failed (%d)\n", ret);
        return ret;
    }

    // de-queue buffer (when buffer is fill)
    static int out_cnt = 1;
    static int frame_number = 0;
    for (i = 0; i < out_cnt; i++) 
    {
    // Get frame
        ret = ioctl(fd, VIDIOC_DQBUF, &buf);
        if (ret < 0) {
            printf("VIDIOC_DQBUF failed (%d)\n", ret);
            return ret;
        }
        printf("DQBUF end\n");

        // Process the frame
        frame_number++;
        char filename[15];
        sprintf(filename, "frame-%d.raw", frame_number);
        FILE *fp=fopen(filename,"wb");    

        if (fp < 0) {
            printf("open frame data file failed\n");
            return -1;
        }        
        
        fwrite(framebuf[buf.index].start, 1, buf.length, fp);
        fclose(fp);	
        printf("Capture one frame saved in %s\n", filename);

        // Re-queen buffer
        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        if (ret < 0) {
            printf("VIDIOC_QBUF failed (%d)\n", ret);
            return ret;
        }
    }

	//stream off
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        printf("VIDIOC_STREAMOFF failed (%d)\n", ret);
        return ret;
    }

    // Release the resource
    for (i=0; i< out_cnt; i++) 
    {
        munmap(framebuf[i].start, framebuf[i].length);
    }	
	printf("close fd\n");
	close(fd);

	return 0;
}  
	
