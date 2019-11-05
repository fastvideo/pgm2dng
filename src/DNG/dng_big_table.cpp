/*****************************************************************************/
// Copyright 2015-2016 Adobe Systems Incorporated
// All Rights Reserved.
//
// NOTICE:  Adobe permits you to use, modify, and distribute this file in
// accordance with the terms of the Adobe license agreement accompanying it.
/*****************************************************************************/

/* $Id: //mondo/camera_raw_main/camera_raw/dng_sdk/source/dng_big_table.cpp#2 $ */ 
/* $DateTime: 2016/04/16 13:14:05 $ */
/* $Change: 1072294 $ */
/* $Author: erichan $ */

/*****************************************************************************/

#include "dng_big_table.h"

#include "dng_memory_stream.h"
#include "dng_stream.h"
#include "dng_xmp.h"

#include "zlib.h"

/*****************************************************************************/

const dng_fingerprint & dng_big_table::Fingerprint () const
	{

	if (!fFingerprint.IsValid () && IsValid ())
		{

		dng_md5_printer_stream stream;

		stream.SetLittleEndian ();

		PutStream (stream, true);

		fFingerprint = stream.Result ();

		}

	return fFingerprint;

	}

/*****************************************************************************/

bool dng_big_table::ReadFromXMP (const dng_xmp &xmp,
								 const char *ns,
								 const char *path)
	{
	
	dng_fingerprint fingerprint;
	
	if (!xmp.GetFingerprint (ns, path, fingerprint))
		{
		
		return false;
		
		}
	
	// Read in the table data.
	
	dng_string tablePath;
	
	tablePath.Set ("Table_");
	
	tablePath.Append (dng_xmp::EncodeFingerprint (fingerprint).Get ());
	
	dng_string block1;
	
	if (!xmp.GetString (XMP_NS_CRS,
						tablePath.Get (),
						block1))
		{
		
		DNG_REPORT ("Missing big table data");
		
		return false;
		
		}
	
	bool ok = DecodeFromString (block1,
								xmp.Allocator ());
		
	block1.Clear ();
		
	// Validate fingerprint match.
	
	DNG_ASSERT (Fingerprint () == fingerprint,
				"dng_big_table fingerprint mismatch");
	
	// It worked!

	return ok;

	}

/*****************************************************************************/

bool dng_big_table::DecodeFromString (const dng_string &block1,
                                      dng_memory_allocator &allocator)
    {

    // Decode the text to binary.

    AutoPtr<dng_memory_block> block2;

    uint32 compressedSize = 0;

        {

        // This binary to text encoding is very similar to the Z85
        // encoding, but the exact charactor set has been adjusted to
        // encode more cleanly into XMP.

        static uint8 kDecodeTable [96] =
            {

            0xFF,   // space
            0x44,   // !
            0xFF,   // "
            0x54,   // #
            0x53,   // $
            0x52,   // %
            0xFF,   // &
            0x49,   // '
            0x4B,   // (
            0x4C,   // )
            0x46,   // *
            0x41,   // +
            0xFF,   // ,
            0x3F,   // -
            0x3E,   // .
            0x45,   // /

            0x00, 0x01, 0x02, 0x03, 0x04,   // 01234
            0x05, 0x06, 0x07, 0x08, 0x09,   // 56789

            0x40,   // :
            0xFF,   // ;
            0xFF,   // <
            0x42,   // =
            0xFF,   // >
            0x47,   // ?
            0x51,   // @

            0x24, 0x25, 0x26, 0x27, 0x28,   // ABCDE
            0x29, 0x2A, 0x2B, 0x2C, 0x2D,   // FGHIJ
            0x2E, 0x2F, 0x30, 0x31, 0x32,   // KLMNO
            0x33, 0x34, 0x35, 0x36, 0x37,   // PQRST
            0x38, 0x39, 0x3A, 0x3B, 0x3C,   // UVWXY
            0x3D,                           // Z

            0x4D,   // [
            0xFF,   // backslash
            0x4E,   // ]
            0x43,   // ^
            0xFF,   // _
            0x48,   // `

            0x0A, 0x0B, 0x0C, 0x0D, 0x0E,   // abcde
            0x0F, 0x10, 0x11, 0x12, 0x13,   // fghij
            0x14, 0x15, 0x16, 0x17, 0x18,   // klmno
            0x19, 0x1A, 0x1B, 0x1C, 0x1D,   // pqrst
            0x1E, 0x1F, 0x20, 0x21, 0x22,   // uvwxy
            0x23,                           // z

            0x4F,   // {
            0x4A,   // |
            0x50,   // }
            0xFF,   // ~
            0xFF    // del

            };

        uint32 encodedSize = block1.Length ();

        uint32 maxCompressedSize = (encodedSize + 4) / 5 * 4;

        block2.Reset (allocator.Allocate (maxCompressedSize));

        uint32 phase = 0;
        uint32 value;

        uint8 *dPtr = block2->Buffer_uint8 ();

        for (const char *sPtr = block1.Get (); *sPtr; sPtr++)
            {

            uint8 e = (uint8) *sPtr;

            if (e < 32 || e > 127)
                {
                continue;
                }

            uint32 d = kDecodeTable [e - 32];

            if (d > 85)
                {
                continue;
                }

            phase++;

            if (phase == 1)
                {
                value = d;
                }

            else if (phase == 2)
                {
                value += d * 85;
                }

            else if (phase == 3)
                {
                value += d * (85 * 85);
                }

            else if (phase == 4)
                {
                value += d * (85 * 85 * 85);
                }

            else
                {

                value += d * (85 * 85 * 85 * 85);

                dPtr [0] = (uint8) (value      );
                dPtr [1] = (uint8) (value >>  8);
                dPtr [2] = (uint8) (value >> 16);
                dPtr [3] = (uint8) (value >> 24);

                dPtr += 4;

                compressedSize += 4;

                phase = 0;

                }

            }

        if (phase > 3)
            {

            dPtr [2] = (uint8) (value >> 16);

            compressedSize++;

            }

        if (phase > 2)
            {

            dPtr [1] = (uint8) (value >> 8);

            compressedSize++;

            }

        if (phase > 1)
            {

            dPtr [0] = (uint8) (value);

            compressedSize++;

            }

        }

    // Decompress the data.

    AutoPtr<dng_memory_block> block3;

        {

        if (compressedSize < 5)
            {
            return false;
            }

        const uint8 *sPtr = block2->Buffer_uint8 ();

        // Uncompressed size is stored in first four bytes of decoded data,
        // little endian order.

        uint32 uncompressedSize = (((uint32) sPtr [0])      ) +
                                  (((uint32) sPtr [1]) <<  8) +
                                  (((uint32) sPtr [2]) << 16) +
                                  (((uint32) sPtr [3]) << 24);

        block3.Reset (allocator.Allocate (uncompressedSize));

        uLongf destLen = uncompressedSize;

        int zResult = ::uncompress (block3->Buffer_uint8 (),
                                    &destLen,
                                    sPtr + 4,
                                    compressedSize - 4);

        if (zResult != Z_OK)
            {
            return false;
            }

        block2.Reset ();

        }

    // Now read in the table data from the uncompressed stream.

        {

        dng_stream stream (block3->Buffer      (),
                           block3->LogicalSize ());

        stream.SetLittleEndian ();

        GetStream (stream);

        block3.Reset ();

        }

    // Force recomputation of fingerprint.

    RecomputeFingerprint ();

    return true;

    }

/*****************************************************************************/

void dng_big_table::WriteToXMP (dng_xmp &xmp,
                                const char *ns,
                                const char *path) const
    {
        
    const dng_fingerprint &fingerprint = Fingerprint ();
    
    if (!fingerprint.IsValid ())
        {
        
        xmp.Remove (ns, path);
        
        return;
        
        }
    
    xmp.SetFingerprint (ns, path, fingerprint);
    
    dng_string tablePath;
    
    tablePath.Set ("Table_");
    
    tablePath.Append (dng_xmp::EncodeFingerprint (fingerprint).Get ());
    
    if (xmp.Exists (XMP_NS_CRS, tablePath.Get ()))
        {
        
        return;
        
        }
        
    AutoPtr<dng_memory_block> block;
    
    block.Reset (EncodeAsString (xmp.Allocator ()));

    xmp.Set (XMP_NS_CRS,
             tablePath.Get (),
             block->Buffer_char ());
        

    }

/*****************************************************************************/

dng_memory_block* dng_big_table::EncodeAsString (dng_memory_allocator &allocator) const
    {

    // Stream to a binary block..

    AutoPtr<dng_memory_block> block1;

        {

        dng_memory_stream stream (allocator);

        stream.SetLittleEndian ();

        PutStream (stream, false);

        block1.Reset (stream.AsMemoryBlock (allocator));

        }

    // Compress the block.

    AutoPtr<dng_memory_block> block2;

    uint32 compressedSize;

        {

        uint32 uncompressedSize = block1->LogicalSize ();

        uint32 safeCompressedSize = uncompressedSize + (uncompressedSize >> 8) + 64;

        block2.Reset (allocator.Allocate (safeCompressedSize + 4));

        // Store uncompressed size in first four bytes of compressed block.

        uint8 *dPtr = block2->Buffer_uint8 ();

        dPtr [0] = (uint8) (uncompressedSize      );
        dPtr [1] = (uint8) (uncompressedSize >>  8);
        dPtr [2] = (uint8) (uncompressedSize >> 16);
        dPtr [3] = (uint8) (uncompressedSize >> 24);

        uLongf dCount = safeCompressedSize;

        int zResult = ::compress2 (dPtr + 4,
                                   &dCount,
                                   block1->Buffer_uint8 (),
                                   uncompressedSize,
                                   Z_DEFAULT_COMPRESSION);
                                  
        if (zResult != Z_OK)
            {
            ThrowMemoryFull ();
            }

        compressedSize = (uint32) dCount + 4;

        block1.Reset ();

        }

    // Encode binary to text.

    AutoPtr<dng_memory_block> block3;

        {

        // This binary to text encoding is very similar to the Z85
        // encoding, but the exact charactor set has been adjusted to
        // encode more cleanly into XMP.

        static const char *kEncodeTable =
            "0123456789"
            "abcdefghij" 
            "klmnopqrst" 
            "uvwxyzABCD"
            "EFGHIJKLMN" 
            "OPQRSTUVWX" 
            "YZ.-:+=^!/" 
            "*?`'|()[]{"
            "}@%$#";

        uint32 safeEncodedSize = compressedSize +
                                 (compressedSize >> 2) +
                                 (compressedSize >> 6) +
                                 16;

        block3.Reset (allocator.Allocate (safeEncodedSize));

        uint8 *sPtr = block2->Buffer_uint8 ();

        sPtr [compressedSize    ] = 0;
        sPtr [compressedSize + 1] = 0;
        sPtr [compressedSize + 2] = 0;

        uint8 *dPtr = block3->Buffer_uint8 ();

        while (compressedSize)
            {

            uint32 x0 = (((uint32) sPtr [0])      ) +
                        (((uint32) sPtr [1]) <<  8) +
                        (((uint32) sPtr [2]) << 16) +
                        (((uint32) sPtr [3]) << 24);

            sPtr += 4;

            uint32 x1 = x0 / 85;

            *dPtr++ = kEncodeTable [x0 - x1 * 85];

            uint32 x2 = x1 / 85;

            *dPtr++ = kEncodeTable [x1 - x2 * 85];

            if (!--compressedSize)
                break;

            uint32 x3 = x2 / 85;

            *dPtr++ = kEncodeTable [x2 - x3 * 85];

            if (!--compressedSize)
                break;

            uint32 x4 = x3 / 85;

            *dPtr++ = kEncodeTable [x3 - x4 * 85];

            if (!--compressedSize)
                break;

            *dPtr++ = kEncodeTable [x4];

            compressedSize--;

            }

        *dPtr = 0;

        block2.Reset ();

        }

    return block3.Release();

    }

/*****************************************************************************/

dng_look_table::dng_look_table ()

	:	fMap      ()
	,	fEncoding (encoding_Linear)

    {

    }

/*****************************************************************************/

void dng_look_table::Set (const dng_hue_sat_map &map,
                          uint32 encoding)
    {

    fMap      = map;
    fEncoding = encoding;

    RecomputeFingerprint ();

    }

/*****************************************************************************/

bool dng_look_table::IsValid () const
    {

    return fMap.IsValid ();

    }

/*****************************************************************************/

void dng_look_table::SetInvalid ()
    {

    fMap.SetInvalid ();

    RecomputeFingerprint ();

    }

/*****************************************************************************/

void dng_look_table::GetStream (dng_stream &stream)
    {

    dng_look_table saveTable = *this;

    try
        {

        if (stream.Get_uint32 () != btt_LookTable)
            {
            ThrowBadFormat ("Not a look table");
            }

        if (stream.Get_uint32 () != kLookTableVersion)
            {
            ThrowBadFormat ("Unknown look table version");
            }

        uint32 hueDivisions = stream.Get_uint32 ();
        uint32 satDivisions = stream.Get_uint32 ();
        uint32 valDivisions = stream.Get_uint32 ();

        if (hueDivisions < 1 || satDivisions < 1 || valDivisions < 1)
            {
            ThrowBadFormat ();
            }

        fMap.SetDivisions (hueDivisions,
                           satDivisions,
                           valDivisions);

        uint32 count = fMap.DeltasCount ();

        dng_hue_sat_map::HSBModify * deltas = fMap.GetDeltas ();

        for (uint32 index = 0; index < count; index++)
            {

            deltas->fHueShift = stream.Get_real32 ();
            deltas->fSatScale = stream.Get_real32 ();
            deltas->fValScale = stream.Get_real32 ();

            deltas++;

            }

        fEncoding = stream.Get_uint32 ();

        if (fEncoding != encoding_Linear &&
            fEncoding != encoding_sRGB)
            {
            ThrowBadFormat ("Unknown look table encoding");
            }

        }

    catch (...)
        {

        *this = saveTable;

        throw;

        }

    }

/*****************************************************************************/

void dng_look_table::PutStream (dng_stream &stream,
                                bool /* forFingerprint */) const
    {

    DNG_REQUIRE (IsValid (), "Invalid Look Table");

    stream.Put_uint32 (btt_LookTable);

    stream.Put_uint32 (kLookTableVersion);

    uint32 hueDivisions;
    uint32 satDivisions;
    uint32 valDivisions;

    fMap.GetDivisions (hueDivisions,
                       satDivisions,
                       valDivisions);

    stream.Put_uint32 (hueDivisions);
    stream.Put_uint32 (satDivisions);
    stream.Put_uint32 (valDivisions);

    uint32 count = fMap.DeltasCount ();

    const dng_hue_sat_map::HSBModify * deltas = fMap.GetConstDeltas ();

    for (uint32 index = 0; index < count; index++)
        {

        stream.Put_real32 (deltas->fHueShift);
        stream.Put_real32 (deltas->fSatScale);
        stream.Put_real32 (deltas->fValScale);

        deltas++;

        }

    stream.Put_uint32 (fEncoding);

    }

/*****************************************************************************/
