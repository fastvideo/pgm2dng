#ifndef DNG_EXT_IMAGE_WRITER_H
#define DNG_EXT_IMAGE_WRITER_H

#include "dng_image_writer.h"

#include "dng_abort_sniffer.h"
#include "dng_area_task.h"
#include "dng_bottlenecks.h"
#include "dng_camera_profile.h"
#include "dng_color_space.h"
#include "dng_exif.h"
#include "dng_flags.h"
#include "dng_exceptions.h"
#include "dng_host.h"
#include "dng_ifd.h"
#include "dng_image.h"
#include "dng_jpeg_image.h"
#include "dng_lossless_jpeg.h"
#include "dng_memory_stream.h"
#include "dng_negative.h"
#include "dng_pixel_buffer.h"
#include "dng_preview.h"
#include "dng_read_image.h"
#include "dng_stream.h"
#include "dng_string_list.h"
#include "dng_tag_codes.h"
#include "dng_tag_values.h"
#include "dng_utils.h"
#include "dng_xmp.h"

class dng_ext_image_writer : public dng_image_writer
{
public:
    dng_ext_image_writer();
    void WriteDNGEx (dng_host &host,
                     dng_stream &stream,
                     const dng_negative &negative,
                     const dng_metadata &constMetadata,
                     const dng_preview_list *previewList = nullptr,
                     uint32 maxBackwardVersion = dngVersion_SaveDefault,
                     bool uncompressed = true);

    int rawBpp{12};

protected:
    void WriteData (dng_host &host,
                    const dng_ifd &ifd,
                    dng_stream &stream,
                    dng_pixel_buffer &buffer,
                    AutoPtr<dng_memory_block> &compressedBuffer,
                    bool usinfMultipleThreads) override;
};

#endif // DNG_EXT_IMAGE_WRITER_H
