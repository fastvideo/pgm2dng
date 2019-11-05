/*****************************************************************************/
// Copyright 2015-2016 Adobe Systems Incorporated
// All Rights Reserved.
//
// NOTICE:  Adobe permits you to use, modify, and distribute this file in
// accordance with the terms of the Adobe license agreement accompanying it.
/*****************************************************************************/

/* $Id: //mondo/camera_raw_main/camera_raw/dng_sdk/source/dng_big_table.h#2 $ */ 
/* $DateTime: 2016/04/16 13:14:05 $ */
/* $Change: 1072294 $ */
/* $Author: erichan $ */

/*****************************************************************************/

#ifndef __dng_big_table__
#define __dng_big_table__

/*****************************************************************************/

#include "dng_camera_profile.h"

/*****************************************************************************/

class dng_big_table
    {

    protected:

        enum BigTableTypeEnum
            {
            btt_LookTable = 0,
            btt_RGBTable  = 1
            };

    private:

        mutable dng_fingerprint fFingerprint;

    protected:

        dng_big_table ()
            {
            }

    public:

        virtual ~dng_big_table ()
            {
            }

        virtual bool IsValid () const = 0;

        const dng_fingerprint & Fingerprint () const;

        bool operator== (const dng_big_table &table) const
            {
            return Fingerprint () == table.Fingerprint ();
            }

        bool operator!= (const dng_big_table &table) const
            {
            return !(*this == table);
            }

        bool DecodeFromString (const dng_string &block1,
                               dng_memory_allocator &allocator);
        
        dng_memory_block* EncodeAsString (dng_memory_allocator &allocator) const;
        
        bool ReadFromXMP (const dng_xmp &xmp,
                          const char *ns,
                          const char *path);

        void WriteToXMP (dng_xmp &xmp,
                         const char *ns,
                         const char *path) const;

    protected:

        void RecomputeFingerprint ()
            {
            fFingerprint.Clear ();
            (void) Fingerprint ();
            }

        virtual void GetStream (dng_stream &stream) = 0;

        virtual void PutStream (dng_stream &stream,
                                bool forFingerprint) const = 0;

    };

/*****************************************************************************/

class dng_look_table : public dng_big_table
    {

    private:

        enum
            {
            kLookTableVersion = 1
            };

    private:

		// 3-D hue/sat table to apply a "look".
		
		dng_hue_sat_map fMap;

		// Value (V of HSV) encoding for look table.

		uint32 fEncoding;

    public:

        dng_look_table ();

        virtual ~dng_look_table ()
            {
            }

        void Set (const dng_hue_sat_map &map,
                  uint32 encoding);

        virtual bool IsValid () const;

        void SetInvalid ();

        const dng_hue_sat_map & Map () const
            {
            return fMap;
            }

        uint32 Encoding () const
            {
            return fEncoding;
            }

    protected:

        virtual void GetStream (dng_stream &stream);

        virtual void PutStream (dng_stream &stream,
                                bool forFingerprint) const;

    };

/*****************************************************************************/

#endif
	
/*****************************************************************************/
