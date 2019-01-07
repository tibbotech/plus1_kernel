

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
	

#define CAPTURE_FILE "capture.raw"
#define BUFFER_COUNT 2

typedef struct VideoBuffer {
   void   *start;
    size_t  length;
} VideoBuffer;
 
VideoBuffer framebuf[BUFFER_COUNT];   


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
					(caps.version>>16)&&0xff,
					(caps.version>>24)&&0xff,
					caps.capabilities);
					
					
		perror("print_caps enter");
	
	  return 0;    
}
	

int main()
{

	int fd,fm;
	int i,ret;

	
	// open camera

	fd=open("/dev/video0",O_RDWR);
		
	if(fd ==-1)
	{
		perror("opening video device fail");
		return 1;
				
	}
	
	perror("opening video device success");
		
	if(print_caps(fd))
	{
		return 1;
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
    printf(" raw_date: %s\n", fmt.fmt.raw_data);
		
	// req one buffer
	
    struct v4l2_requestbuffers reqbuf;    
    
    reqbuf.count = 1;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    
    ret = ioctl(fd , VIDIOC_REQBUFS, &reqbuf);
    
    if(ret < 0) {
        printf("VIDIOC_REQBUFS failed (%d) %d\n", ret,reqbuf.count);
        return ret;
    }
		

	// query buffer , mmap , qbuf
	
    struct v4l2_buffer buf;
   
	void *map_base, *virt_addr;
	unsigned len = 4;
	off_t target;
   	unsigned page_size, mapped_size, offset_in_page;
		
    
    fm = open("/dev/mem", (O_RDONLY | O_SYNC)); // O_RDWR

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

        // mmap buffer
        framebuf[i].length = buf.length;
        target = buf.m.offset;             
        page_size = getpagesize();		
		offset_in_page = (unsigned)target & (page_size - 1);
		len =  framebuf[i].length;
		mapped_size = (offset_in_page + len + page_size - 1) & ~(page_size - 1);

		//printf("target=0x%x len=0x%x mapped_size=0x%x\n",	target, len,  mapped_size);
                 
		framebuf[i].start = mmap(NULL, mapped_size, PROT_READ, MAP_SHARED, fm, target & ~(off_t)(page_size - 1));
		framebuf[i].start = framebuf[i].start + offset_in_page;
		
        printf("Frame buffer %d: address=0x%x, length=%d %d\n", i, (unsigned int)framebuf[i].start, framebuf[i].length,offset_in_page);
    
           
        if (framebuf[i].start == MAP_FAILED) {
            printf("mmap (%d) failed: %s\n", i, strerror(errno));
            return -1;
        }
    
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


   // Get frame
    ret = ioctl(fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        printf("VIDIOC_DQBUF failed (%d)\n", ret);
        return ret;
    }


    // Process the frame


    FILE *fp = fopen("capture.raw", "wb");
    if (fp < 0) {
        printf("open frame data file failed\n");
        return -1;
    }
	
    fwrite(framebuf[buf.index].start, 1, buf.length, fp);
    fclose(fp);
	
    printf("Capture one frame saved in %s\n", CAPTURE_FILE);


    // Re-queen buffer
    ret = ioctl(fd, VIDIOC_QBUF, &buf);
    if (ret < 0) {
        printf("VIDIOC_QBUF failed (%d)\n", ret);
        return ret;
    }


	//stream off

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        printf("VIDIOC_STREAMOFF failed (%d)\n", ret);
        return ret;
    }


	
    // Release the resource
    for (i=0; i< 4; i++) 
    {
        munmap(framebuf[i].start, framebuf[i].length);
    }	
		
	printf("close fd");
	close(fd);
	close(fm);
		
	return 0;
		
}  
	
