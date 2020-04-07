/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

// logging
#include <plog/Log.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

enum io_method {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
};

struct buffer {
	void   *start;
	size_t  length;
};

char            *dev_name = (char *) "/dev/video0";
enum io_method   io = IO_METHOD_MMAP;
int              fd = -1;
struct buffer          *buffers;
unsigned int     n_buffers;
int		out_buf;
int              force_format;
int              frame_count = 70;

void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, unsigned long int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

extern void process_image(void *ptr, int size, struct v4l2_buffer buf);
static void original_process_image(const void *p, int size)
{
	if (out_buf)
		fwrite(p, size, 1, stdout);

	fflush(stderr);
	fprintf(stderr, ".");
	fflush(stdout);
}

extern void startLoopCallback(void);
static int read_frame(void)
{
	struct v4l2_buffer buf;
	unsigned int i;

    startLoopCallback();

	switch (io) {
    LOGV << " io = " << io << " IO_METHOD_READ = " << IO_METHOD_READ;
	case IO_METHOD_READ:
		if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
            LOGE << "error IO_METHOD_READ : " << errno;
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("read");
			}
		}

		process_image(buffers[0].start, buffers[0].length, buf);
		break;

	case IO_METHOD_MMAP:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            LOGE << "error IO_METHOD_MMAP : " << errno;
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}

		assert(buf.index < n_buffers);

		process_image(buffers[buf.index].start, buf.bytesused, buf);

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
		break;

	case IO_METHOD_USERPTR:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            LOGE << "error IO_METHOD_USERPTR : " << errno;
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long)buffers[i].start
			    && buf.length == buffers[i].length)
				break;

		assert(i < n_buffers);

		process_image((void *)buf.m.userptr, buf.bytesused, buf);

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
		break;
	}

	return 1;
}

void mainloop(void)
{
	unsigned int count;

	count = frame_count;
    LOGV << "count = " << count;

	while (count-- > 0) {
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r) {
				if (EINTR == errno)
					continue;
				errno_exit("select");
			}

			if (0 == r) {
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}
            LOGV << "ready to read_frame() count = " << count;

			if (read_frame())
				break;
			/* EAGAIN - continue select loop. */
		}
	}
}

void stop_capturing(void)
{
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
			errno_exit("VIDIOC_STREAMOFF");
		break;
	}
}

void start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long)buffers[i].start;
			buf.length = buffers[i].length;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
		break;
	}
}

void uninit_device(void)
{
	unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		free(buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap(buffers[i].start, buffers[i].length))
				errno_exit("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free(buffers[i].start);
		break;
	}

	free(buffers);
}

static void init_read(unsigned int buffer_size)
{
	buffers = (buffer*) calloc(1, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	buffers[0].length = buffer_size;
	buffers[0].start = malloc(buffer_size);

	if (!buffers[0].start) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
}

static void init_mmap(void)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
			 dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = (buffer*) calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap(NULL /* start anywhere */,
			      buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");
	}
}

static void init_userp(unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count  = 4;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "user pointer i/o\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	buffers = (buffer*) calloc(4, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = malloc(buffer_size);

		if (!buffers[n_buffers].start) {
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

void init_device(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",
				 dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
			 dev_name);
		exit(EXIT_FAILURE);
	}

	switch (io) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf(stderr, "%s does not support read i/o\n",
				 dev_name);
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming i/o\n",
				 dev_name);
			exit(EXIT_FAILURE);
		}
		break;
	}


	/* Select video input, video standard and tune here. */


	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}


	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (force_format) {
		fmt.fmt.pix.width       = 640;
		fmt.fmt.pix.height      = 480;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

		if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
			errno_exit("VIDIOC_S_FMT");

		/* Note VIDIOC_S_FMT may change width and height. */
	} else {
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
			errno_exit("VIDIOC_G_FMT");
	}

	switch (io) {
	case IO_METHOD_READ:
		init_read(fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap();
		break;

	case IO_METHOD_USERPTR:
		init_userp(fmt.fmt.pix.sizeimage);
		break;
	}
}

void close_device(void)
{
	if (-1 == close(fd))
		errno_exit("close");

	fd = -1;
}

void open_device(void)
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

	fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

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
		 "-d | --device name   Video device name [%s]\n"
		 "-h | --help          Print this message\n"
		 "-m | --mmap          Use memory mapped buffers [default]\n"
		 "-r | --read          Use read() calls\n"
		 "-u | --userp         Use application allocated buffers\n"
		 "-o | --output        Outputs stream to stdout\n"
		 "-f | --format        Force format to 640x480 YUYV\n"
		 "-c | --count         Number of frames to grab [%i]\n"
		 "",
		 argv[0], dev_name, frame_count);
}

static const char short_options[] = "d:hmruofc:";

static const struct option
long_options[] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help",   no_argument,       NULL, 'h' },
	{ "mmap",   no_argument,       NULL, 'm' },
	{ "read",   no_argument,       NULL, 'r' },
	{ "userp",  no_argument,       NULL, 'u' },
	{ "output", no_argument,       NULL, 'o' },
	{ "format", no_argument,       NULL, 'f' },
	{ "count",  required_argument, NULL, 'c' },
	{ 0, 0, 0, 0 }
};

int original_main(int argc, char **argv)
{
	//dev_name = (char *) "/dev/video0";

	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv,
				short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			dev_name = optarg;
			break;

		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);

		case 'm':
			io = IO_METHOD_MMAP;
			break;

		case 'r':
			io = IO_METHOD_READ;
			break;

		case 'u':
			io = IO_METHOD_USERPTR;
			break;

		case 'o':
			out_buf++;
			break;

		case 'f':
			force_format++;
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
	uninit_device();
	close_device();
	fprintf(stderr, "\n");
	return 0;
}

//
// other v4l stuff from sdlcam.c
//
#include "libv4l2.h"
#include <linux/videodev2.h>
#include "libv4l-plugin.h"

// obtained by query by v4l2-ctl
struct FramosId {
    long                      group_hold = 0x009a2003 ;
    long                     sensor_mode = 0x009a2008 ;
    long                            gain = 0x009a2009 ;
    long                        exposure = 0x009a200a ;
    long                      frame_rate = 0x009a200b ;
    long            sensor_configuration = 0x009a2032 ;
    long          sensor_mode_i2c_packet = 0x009a2033 ;
    long       sensor_control_i2c_packet = 0x009a2034 ;
    long                     bypass_mode = 0x009a2064 ;
    long                 override_enable = 0x009a2065 ;
    long                    height_align = 0x009a2066 ;
    long                      size_align = 0x009a2067 ;
    long                write_isp_format = 0x009a2068 ;
    long        sensor_signal_properties = 0x009a2069 ;
    long         sensor_image_properties = 0x009a206a ;
    long       sensor_control_properties = 0x009a206b ;
    long               sensor_dv_timings = 0x009a206c ;
    long                low_latency_mode = 0x009a206d ;
    long                    test_pattern = 0x009a206f ;
    long                  streaming_mode = 0x009a2070 ;
    long                  operation_mode = 0x009a2071 ;
    long                     black_level = 0x009a2074 ;
    long             global_shutter_mode = 0x009a207b ;
    long                    sensor_modes = 0x009a2082 ;
} id;

struct v4l2_query_ext_ctrl v4l2_q_ctrl(long id) {

    struct v4l2_query_ext_ctrl  ext_control;
    ext_control.id = id;

    int ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &ext_control);
    if (ret < 0)
        printf("FAILED control ID %d : T=%d N=%s min=%lld max=%lld\n", ext_control.id, ext_control.type, ext_control.name, ext_control.minimum, ext_control.maximum);

    //printf("SUCCESS control ID %d : T=%d N=%s min=%lld max=%lld\n", ext_control.id, ext_control.type, ext_control.name, ext_control.minimum, ext_control.maximum);
    return ext_control;
}

struct v4l2_query_ext_ctrl query_gain(void) {
    return v4l2_q_ctrl(id.gain);
}

struct v4l2_query_ext_ctrl query_expo(void) {
    return v4l2_q_ctrl(id.exposure);
}


long v4l2_g_ctrl(long id) {

    struct v4l2_ext_controls ext_controls;
    struct v4l2_ext_control  ext_control;
    ext_control.id = id;

    ext_controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ext_controls.count = 1;
    ext_controls.controls = &ext_control;

    int ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_controls);
    if (ret < 0)
        printf("FAILED to get control ID %d = %lld \n", ext_control.id, ext_control.value64);
    //printf("SUCCESS control ID %d = %lld \n", ext_control.id, ext_control.value64);
    return ext_control.value64;
}

long get_gain(void) {
    return v4l2_g_ctrl(id.gain);
}

long get_expo(void) {
    return v4l2_g_ctrl(id.exposure);
}

long get_frame_rate(void) {
    return v4l2_g_ctrl(id.frame_rate);
}

float get_fps(void) {
    long frame_rate = get_frame_rate();
    float fps = (float) frame_rate / (float) 1000000;
    return fps;
}



int v4l2_s_ctrl( long id, long value) {

      struct v4l2_ext_controls ext_controls;
      struct v4l2_ext_control  ext_control;
      ext_control.id = id;
      ext_control.value64 = value;

      ext_controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
      ext_controls.count = 1;
      ext_controls.controls = &ext_control;

      int ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_controls);
      if (ret < 0)
          printf("FAILED to set control ID %d = %lld \n", ext_control.id, ext_control.value64);
      return ret;
}

int set_gain(long value) {
    return v4l2_s_ctrl(id.gain, value);
}

int set_expo(long value) {
    return v4l2_s_ctrl(id.exposure, value);
}

int set_frame_rate(long value) {
    return v4l2_s_ctrl(id.frame_rate, value);
}

int set_fps(float fps) {
    long frame_rate = 1000000 * fps;
    return v4l2_s_ctrl(id.frame_rate, frame_rate);
}



//
//static void cam_exposure_limits(struct sdl *m, struct v4l2_queryctrl *qctrl)
//{
//        qctrl->id = V4L2_CID_EXPOSURE_ABSOLUTE;
//
//        if (v4l2_ioctl(fd, VIDIOC_QUERYCTRL, qctrl)) {
//                printf("Exposure absolute limits failed\n");
//                exit(1);
//        }
//
//        /* Minimum of 11300 gets approximately same range on ISO and
//         * exposure axis. */
//        if (qctrl->minimum < 500)
//                qctrl->minimum = 500;
//}
//
//static void cam_set_exposure(struct sdl *m, double v)
//{
//        int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
//        double res;
//        double range;
//        struct v4l2_queryctrl qctrl = { .id = cid };
//        struct v4l2_control ctrl = { .id = cid };
//
//        cam_exposure_limits(m, &qctrl);
//
//        if (v4l2_ioctl(fd, VIDIOC_G_CTRL, &ctrl)) {
//                printf("Can't get exposure parameters\n");
//                exit(1);
//        }
//
//        range = log2(qctrl.maximum) - log2(qctrl.minimum);
//        res = log2(qctrl.minimum) + v*range;
//        res = exp2(res);
//
//        v4l2_s_ctrl(fd, V4L2_CID_EXPOSURE_ABSOLUTE, res);
//}
//
//static double cam_convert_exposure(struct sdl *m, int v)
//{
//        int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
//        double res;
//        struct v4l2_queryctrl qctrl = { .id = cid };
//
//        cam_exposure_limits(m, &qctrl);
//        res = (log2(v) - log2(qctrl.minimum)) / (log2(qctrl.maximum) - log2(qctrl.minimum));
//
//        return res;
//}
//
//
//static double cam_get_exposure(struct sdl *m)
//{
//        int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
//        struct v4l2_control ctrl = { .id = cid };
//
//        if (v4l2_ioctl(fd, VIDIOC_G_CTRL, &ctrl))
//                return -1;
//
//        return cam_convert_exposure(m, ctrl.value);
//}
//
//
//static char *fmt_name(struct v4l2_format *fmt)
//{
//        switch (fmt->fmt.pix.pixelformat) {
//        case V4L2_PIX_FMT_SGRBG10:
//                return "GRBG10";
//        case V4L2_PIX_FMT_RGB24:
//                return "RGB24";
//        default:
//                return "unknown";
//        }
//}
//
//
//static void sdl_sync_settings(struct sdl *m)
//{
//        printf("Autofocus: "); v4l2_s_ctrl(fd, V4L2_CID_FOCUS_AUTO, m->do_focus);
//        printf("Autogain: " ); v4l2_s_ctrl(fd, V4L2_CID_AUTOGAIN, m->do_exposure);
//        printf("Autowhite: "); v4l2_s_ctrl(fd, V4L2_CID_AUTO_WHITE_BALANCE, m->do_white);
//        v4l2_s_ctrl(fd, 0x009c0901, m->do_flash ? 2 : 0);
//}
//
