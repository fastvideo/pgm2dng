/*****************************************************************************/
// Copyright 2015 Adobe Systems Incorporated
// All Rights Reserved.
//
// NOTICE:  Adobe permits you to use, modify, and distribute this file in
// accordance with the terms of the Adobe license agreement accompanying it.
/*****************************************************************************/

/* $Id: //mondo/camera_raw_main/camera_raw/dng_sdk/source/dng_local_string.cpp#2 $ */ 
/* $DateTime: 2015/08/06 23:46:40 $ */
/* $Change: 1034773 $ */
/* $Author: tknoll $ */

/*****************************************************************************/

#include "dng_local_string.h"

/*****************************************************************************/

dng_local_string::dng_local_string ()

    :   fDefaultText ()
    ,   fDictionary  ()

    {

    }

/*****************************************************************************/

dng_local_string::dng_local_string (const dng_string &s)

    :   fDefaultText (s)
    ,   fDictionary  ()

    {

    }

/*****************************************************************************/

dng_local_string::~dng_local_string ()
    {

    }

/*****************************************************************************/

void dng_local_string::Clear ()
    {

    fDefaultText.Clear ();

    fDictionary.clear ();

    }

/*****************************************************************************/

void dng_local_string::SetDefaultText (const dng_string &s)
    {

    fDefaultText = s;

    }

/*****************************************************************************/

void dng_local_string::AddTranslation (const dng_string &language,
                                       const dng_string &translation)
    {

    fDictionary.push_back (dictionary_entry (language,
                                             translation));

    }

/*****************************************************************************/

const dng_string & dng_local_string::LocalText (const dng_string &locale) const
    {

    // Pass 1 - try for a match starting with the entire locale string.

    if (locale.Length () >= 5)
        {

        for (uint32 index = 0; index < TranslationCount (); index++)
            {

            if (Language (index).StartsWith (locale.Get (), false))
                {

                return Translation (index);

                }

            }

        }

    // Pass 2 - try for a language only match.

    if (locale.Length () >= 2)
        {

        dng_string languageOnly (locale);

        languageOnly.Truncate (2);

        for (uint32 index = 0; index < TranslationCount (); index++)
            {

            if (Language (index).StartsWith (languageOnly.Get (), false))
                {

                return Translation (index);

                }

            }

        }

    // Otherwise use default text.

    return DefaultText ();

    }

/*****************************************************************************/

bool dng_local_string::operator== (const dng_local_string &s) const
    {

    if (DefaultText () != s.DefaultText ())
        {
        return false;
        }

    if (TranslationCount () != s.TranslationCount ())
        {
        return false;
        }

    for (uint32 index = 0; index < TranslationCount (); index++)
        {

        if (Language (index) != s.Language (index))
            {
            return false;
            }

        if (Translation (index) != s.Translation (index))
            {
            return false;
            }

        }

    return true;

    }
		
/*****************************************************************************/
