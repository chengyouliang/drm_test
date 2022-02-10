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

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
	uint32_t fb_id;
};

struct buffer_object buf;


typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint64_t u64;
typedef int64_t s64;
typedef char s8;
typedef unsigned char u8;

struct img_desc {
	char file_path[512];
	u32 offs;
	u32 width;
	s32 height;
	u8 *vaddr;
	bool y_invert;
	u16 planes;
	u16 count_bits;
	u32 compression;
	u32 size;
	s32 fd;
};
struct  img_desc *img_load(const char *file)
{
	struct img_desc *desc;

	desc = calloc(1, sizeof(*desc));
	if (!desc)
		return NULL;

	memset(desc, 0, sizeof(*desc));
	desc->fd = open(file, O_RDONLY, 0644);
	if (desc->fd < 0) {
		fprintf(stderr, "image file %s open failed. %s\n", file,
			strerror(errno));
		goto err;
	}

	lseek(desc->fd, 10, SEEK_SET);
	read(desc->fd, &desc->offs, sizeof(u32));
	printf("image offset: %u\n", desc->offs);
	lseek(desc->fd, 18, SEEK_SET);
	read(desc->fd, &desc->width, sizeof(u32));
	read(desc->fd, &desc->height, sizeof(u32));
	read(desc->fd, &desc->planes, sizeof(short));
	read(desc->fd, &desc->count_bits, sizeof(short));
	read(desc->fd, &desc->compression, sizeof(u32));
	read(desc->fd, &desc->size, sizeof(u32));
	if (desc->height > 0) {
		printf("Y-invert\n");
		desc->y_invert = true;
	} else {
		desc->y_invert = false;
		desc->height = (-desc->height);
	}
	printf("image widthxheight: %ux%d\n", desc->width, desc->height);
	printf("image planes: %u\n", desc->planes);
	printf("image count_bits: %u\n", desc->count_bits);
	printf("image compression: %u\n", desc->compression);
	printf("image size: %u\n", desc->size);
	if (desc->width > 1920 ||
	    desc->height > 1080 ||
	    desc->planes > 1 ||
	    (desc->count_bits != 24 && desc->count_bits != 32)) {
		fprintf(stderr, "Not supported.\n");
		printf("Only support:\n");
		printf("\twidth <= 1920 height <= 1080\n");
		printf("\tplanes: 1\n");
		printf("\tcount_bits: 24 or 32\n");
	} else {
		u32 bytes_to_rd = desc->size;
		u8 *p;
		s32 ret;

		desc->vaddr = malloc(desc->size);
		if (!desc->vaddr)
			goto err;
		p = desc->vaddr;
		lseek(desc->fd, desc->offs, SEEK_SET);
		while (bytes_to_rd) {
			ret = read(desc->fd, p, bytes_to_rd);
			if (ret < 0)
				break;
			bytes_to_rd -= ret;
			p += ret;
		}
	}
	close(desc->fd);

	return desc;

err:
	if (desc) {
		if (desc->vaddr)
			free(desc->vaddr);
		free(desc);
	}
	return NULL;
}

void img_unload(struct img_desc *desc)
{
	if (!desc)
		return;

	if (desc->vaddr)
		free(desc->vaddr);
	free(desc);
}

static s32 img_conv_argb8888(struct img_desc *desc, u8 *data, u32 width,
			     u32 height, u32 stride, u32 x, u32 y)
{
	s32 i, j;
	u32 *pixel = (u32 *)data + y * (stride >> 2) + x;
	u8 *src;
	u8 *dst;
	u32 h = desc->height;
	u32 w = desc->width;

	dst = (u8 *)pixel;
	if (desc->y_invert) {
		src = desc->vaddr + desc->size - desc->width * 3;
		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				*(dst + j * 4) = *(src + j * 3);
				*(dst + j * 4 + 1) = *(src + j * 3 + 1);
				*(dst + j * 4 + 2) = *(src + j * 3 + 2);
				*(dst + j * 4 + 3) = 0xFF;
			}
			src -= desc->width * 3;
			dst += stride;
		}
	} else {
		src = desc->vaddr;
		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				*(dst + j * 4) = *(src + j * 3);
				*(dst + j * 4 + 1) = *(src + j * 3 + 1);
				*(dst + j * 4 + 2) = *(src + j * 3 + 2);
				*(dst + j * 4 + 3) = 0xFF;
			}
			src += desc->width * 3;
			dst += stride;
		}
	}
}

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {};
 	struct drm_mode_map_dumb map = {};

	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);

	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);


	memset(bo->vaddr, 0xff, bo->size);

	return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);

	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static uint32_t get_property_id(int fd, drmModeObjectProperties *props,
				const char *name)
{
	drmModePropertyPtr property;
	uint32_t i, id = 0;

	for (i = 0; i < props->count_props; i++) {
		property = drmModeGetProperty(fd, props->props[i]);
		if (!strcmp(property->name, name))
			id = property->prop_id;
		drmModeFreeProperty(property);

		if (id)
			break;
	}

	return id;
}

int main(int argc, char **argv)
{
	int fd;
	drmModeConnector *conn;
	drmModeRes *res;
	drmModePlaneRes *plane_res;
	drmModeObjectProperties *props;
	drmModeAtomicReq *req;
	uint32_t conn_id;
	uint32_t crtc_id;
	uint32_t plane_id;
	uint32_t blob_id;
	uint32_t property_crtc_id;
	uint32_t property_mode_id;
	uint32_t property_active;
	uint32_t property_fb_id;
	uint32_t property_crtc_x;
	uint32_t property_crtc_y;
	uint32_t property_crtc_w;
	uint32_t property_crtc_h;
	uint32_t property_src_x;
	uint32_t property_src_y;
	uint32_t property_src_w;
	uint32_t property_src_h;
	struct img_desc *img;

	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	img = img_load("test.bmp");
	if (!img) {
		printf("img == %p\n", img);
		return -1;
	}
	res = drmModeGetResources(fd);
	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

	drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	plane_res = drmModeGetPlaneResources(fd);
	plane_id = plane_res->planes[0];

	conn = drmModeGetConnector(fd, conn_id);
	
	printf("hdisplay %d",conn->modes[0].hdisplay);
	printf("vdisplay %d",conn->modes[0].vdisplay);
	printf("width %d",img->width);
	printf("height %d",img->height);
	buf.width = img->width;
	buf.height = img->height;

	modeset_create_fb(fd, &buf);
	img_conv_argb8888(img,buf.vaddr,buf.width,buf.height,buf.pitch,0,0);
	drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

	props = drmModeObjectGetProperties(fd, conn_id,	DRM_MODE_OBJECT_CONNECTOR);
	property_crtc_id = get_property_id(fd, props, "CRTC_ID");
	drmModeFreeObjectProperties(props);

	props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	property_active = get_property_id(fd, props, "ACTIVE");
	property_mode_id = get_property_id(fd, props, "MODE_ID");
	drmModeFreeObjectProperties(props);

	drmModeCreatePropertyBlob(fd, &conn->modes[0],
				sizeof(conn->modes[0]), &blob_id);

	req = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(req, crtc_id, property_active, 1);
	drmModeAtomicAddProperty(req, crtc_id, property_mode_id, blob_id);
	drmModeAtomicAddProperty(req, conn_id, property_crtc_id, crtc_id);
	drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	drmModeAtomicFree(req);

	printf("drmModeAtomicCommit SetCrtc\n");
    /* get plane properties */
	props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	property_crtc_id = get_property_id(fd, props, "CRTC_ID");
	property_fb_id = get_property_id(fd, props, "FB_ID");
	property_crtc_x = get_property_id(fd, props, "CRTC_X");
	property_crtc_y = get_property_id(fd, props, "CRTC_Y");
	property_crtc_w = get_property_id(fd, props, "CRTC_W");
	property_crtc_h = get_property_id(fd, props, "CRTC_H");
	property_src_x = get_property_id(fd, props, "SRC_X");
	property_src_y = get_property_id(fd, props, "SRC_Y");
	property_src_w = get_property_id(fd, props, "SRC_W");
	property_src_h = get_property_id(fd, props, "SRC_H");
	drmModeFreeObjectProperties(props);

    /* atomic plane update */
	req = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(req, plane_id, property_crtc_id, crtc_id);
	drmModeAtomicAddProperty(req, plane_id, property_fb_id, buf.fb_id);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_x, 0);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_y, 0);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_w, buf.width);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_h, buf.height);
	drmModeAtomicAddProperty(req, plane_id, property_src_x, 0);
	drmModeAtomicAddProperty(req, plane_id, property_src_y, 0);
	drmModeAtomicAddProperty(req, plane_id, property_src_w, buf.width << 16);
	drmModeAtomicAddProperty(req, plane_id, property_src_h, buf.height << 16);
	drmModeAtomicCommit(fd, req, 0, NULL);
	drmModeAtomicFree(req);

	printf("drmModeAtomicCommit SetPlane\n");
	getchar();

	modeset_destroy_fb(fd, &buf);

	drmModeFreeConnector(conn);
	drmModeFreePlaneResources(plane_res);
	drmModeFreeResources(res);

	close(fd);

	return 0;
}
