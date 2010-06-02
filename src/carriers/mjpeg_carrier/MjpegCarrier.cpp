// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include "MjpegCarrier.h"

extern "C" {
#include <jpeglib.h>
}

#include <yarp/sig/Image.h>
#include <yarp/sig/ImageNetworkHeader.h>

using namespace yarp::os::impl;
using namespace yarp::sig;

typedef struct {
    struct jpeg_destination_mgr pub;

    JOCTET *buffer;
    int bufsize;
    JOCTET cache[1000000];  // need to make this variable...
} net_destination_mgr;

typedef net_destination_mgr *net_destination_ptr;

void send_net_data(JOCTET *data, int len, void *client) {
    //printf("Send %d bytes\n", len);
    Protocol *p = (Protocol *)client;
    char hdr[1000];
    sprintf(hdr,"Content-Type: image/jpeg\r\n\
Content-Length: %d\r\n\r\n", len);
    Bytes hbuf(hdr,strlen(hdr));
    p->os().write(hbuf);
    Bytes buf((char *)data,len);
    p->os().write(buf);
    sprintf(hdr,"\r\n--boundarydonotcross\r\n");
    Bytes hbuf2(hdr,strlen(hdr));
    p->os().write(hbuf2);

}

static void init_net_destination(j_compress_ptr cinfo) {
    //printf("Initializing destination\n");
    net_destination_ptr dest = (net_destination_ptr)cinfo->dest;
    dest->buffer = &(dest->cache[0]);
    dest->bufsize = sizeof(dest->cache);
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = dest->bufsize;
}

static boolean empty_net_output_buffer(j_compress_ptr cinfo) {
    net_destination_ptr dest = (net_destination_ptr)cinfo->dest;
    printf("Empty buffer - PROBLEM\n");
    send_net_data(dest->buffer,dest->bufsize-dest->pub.free_in_buffer,
                  cinfo->client_data);
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = dest->bufsize;
    return TRUE;
}

static void term_net_destination(j_compress_ptr cinfo) {
    net_destination_ptr dest = (net_destination_ptr)cinfo->dest;
    //printf("Terminating net %d %d\n", dest->bufsize,dest->pub.free_in_buffer);
    send_net_data(dest->buffer,dest->bufsize-dest->pub.free_in_buffer,
                  cinfo->client_data);
}

void jpeg_net_dest(j_compress_ptr cinfo) {
    net_destination_ptr dest;

      //ERREXIT(cinfo, JERR_BUFFER_SIZE);

    /* The destination object is made permanent so that multiple JPEG images
     * can be written to the same buffer without re-executing jpeg_net_dest.
     */
    if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)
            (*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                        sizeof(net_destination_mgr));
    }

    dest = (net_destination_ptr) cinfo->dest;
    dest->pub.init_destination = init_net_destination;
    dest->pub.empty_output_buffer = empty_net_output_buffer;
    dest->pub.term_destination = term_net_destination;
}

bool MjpegCarrier::write(Protocol& proto, SizedWriter& writer) {
    ImageNetworkHeader hdr;
    char *header_buf = (char*)(&hdr);
    int header_len = sizeof(hdr);
    const char *img_buf = NULL;
    int img_len = 0;
    for (int i=0; i<writer.length(); i++) {
        const char *data = writer.data(i);
        int len = writer.length(i);
        //printf("block %d length %d\n", i, len);
        if (header_len<len) {
            len = header_len;
        }
        if (len>0) {
            memcpy(header_buf,data,len);
            header_len -= len;
            header_buf += len;
        }
        if (header_len == 0) {
            img_buf = data+len;
            img_len = writer.length(i)-len;
        }
    }
    //printf("Passing on a %dx%d image\n", hdr.width, hdr.height);
    if (hdr.imgSize!=img_len) {
        printf(" size mismatch\n");
        //exit(1);
        return false;
    }
    int w = hdr.width;
    int h = hdr.height;
    int row_stride = hdr.imgSize/hdr.height;
    JOCTET *data = (JOCTET*)img_buf;

	JSAMPROW row_pointer[1];

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    cinfo.client_data = &proto;
    jpeg_create_compress(&cinfo);
    jpeg_net_dest(&cinfo);
    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo); 
    //jpeg_set_quality(&cinfo, 85, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        //printf("Writing row %d...\n", cinfo.next_scanline);
        row_pointer[0] = data + cinfo.next_scanline * row_stride;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return true; //proto.os().isOk();
}

bool MjpegCarrier::reply(Protocol& proto, SizedWriter& writer) {
    return false;
}
