#include "dng_ext_image_writer.h"
#include "dng_host.h"
#include "dng_pixel_buffer.h"
#include <cstdio>

class RangeTags
{

private:

    uint32 fActiveAreaData [4]{};

    tag_uint32_ptr fActiveArea;

    uint32 fMaskedAreaData [kMaxMaskedAreas * 4]{};

    tag_uint32_ptr fMaskedAreas;

    tag_uint16_ptr fLinearizationTable;

    uint16 fBlackLevelRepeatDimData [2]{};

    tag_uint16_ptr fBlackLevelRepeatDim;

    dng_urational fBlackLevelData [kMaxBlackPattern *
    kMaxBlackPattern *
    kMaxSamplesPerPixel];

    tag_urational_ptr fBlackLevel;

    dng_memory_data fBlackLevelDeltaHData;
    dng_memory_data fBlackLevelDeltaVData;

    tag_srational_ptr fBlackLevelDeltaH;
    tag_srational_ptr fBlackLevelDeltaV;

    uint16 fWhiteLevelData16 [kMaxSamplesPerPixel]{};
    uint32 fWhiteLevelData32 [kMaxSamplesPerPixel]{};

    tag_uint16_ptr fWhiteLevel16;
    tag_uint32_ptr fWhiteLevel32;

public:

    RangeTags(dng_tiff_directory &directory,
              const dng_negative &negative);

};


/******************************************************************************/

RangeTags::RangeTags(dng_tiff_directory &directory,
                     const dng_negative &negative)

    :	fActiveArea(tcActiveArea,
                    fActiveAreaData,
                    4)

    ,	fMaskedAreas(tcMaskedAreas,
                     fMaskedAreaData,
                     0)

    ,	fLinearizationTable(tcLinearizationTable,
                            nullptr,
                            0)

    ,	fBlackLevelRepeatDim(tcBlackLevelRepeatDim,
                             fBlackLevelRepeatDimData,
                             2)

    ,	fBlackLevel(tcBlackLevel,
                    fBlackLevelData)

    ,	fBlackLevelDeltaH(tcBlackLevelDeltaH)
    ,	fBlackLevelDeltaV(tcBlackLevelDeltaV)

    ,	fWhiteLevel16(tcWhiteLevel,
                      fWhiteLevelData16)

    ,	fWhiteLevel32(tcWhiteLevel,
                      fWhiteLevelData32)

{

    const dng_image &rawImage(negative.RawImage());

    const dng_linearization_info *rangeInfo = negative.GetLinearizationInfo();

    if(rangeInfo)
    {

        // ActiveArea:

        {

            const dng_rect &r = rangeInfo->fActiveArea;

            if(r.NotEmpty())
            {

                fActiveAreaData[0] = uint32(r.t);
                fActiveAreaData[1] = uint32(r.l);
                fActiveAreaData[2] = uint32(r.b);
                fActiveAreaData[3] = uint32(r.r);

                directory.Add(&fActiveArea);

            }

        }

        // MaskedAreas:

        if(rangeInfo->fMaskedAreaCount)
        {

            fMaskedAreas.SetCount(rangeInfo->fMaskedAreaCount * 4);

            for(uint32 index = 0; index < rangeInfo->fMaskedAreaCount; index++)
            {

                const dng_rect &r = rangeInfo->fMaskedArea[index];

                fMaskedAreaData[index * 4 + 0] = uint32(r.t);
                fMaskedAreaData[index * 4 + 1] = uint32(r.l);
                fMaskedAreaData[index * 4 + 2] = uint32(r.b);
                fMaskedAreaData[index * 4 + 3] = uint32(r.r);

            }

            directory.Add(&fMaskedAreas);

        }

        // LinearizationTable:

        if(rangeInfo->fLinearizationTable.Get())
        {

            fLinearizationTable.SetData (rangeInfo->fLinearizationTable->Buffer_uint16()     );
            fLinearizationTable.SetCount(rangeInfo->fLinearizationTable->LogicalSize () >> 1);

            directory.Add(&fLinearizationTable);

        }

        // BlackLevelRepeatDim:

        {

            fBlackLevelRepeatDimData[0] = uint16(rangeInfo->fBlackLevelRepeatRows);
            fBlackLevelRepeatDimData[1] = uint16(rangeInfo->fBlackLevelRepeatCols);

            directory.Add(&fBlackLevelRepeatDim);

        }

        // BlackLevel:

        {

            uint32 index = 0;

            for(uint16 v = 0; v < rangeInfo->fBlackLevelRepeatRows; v++)
            {

                for(uint32 h = 0; h < rangeInfo->fBlackLevelRepeatCols; h++)
                {

                    for(uint32 c = 0; c < rawImage.Planes(); c++)
                    {

                        fBlackLevelData [index++] = rangeInfo->BlackLevel(v, h, c);

                    }

                }

            }

            fBlackLevel.SetCount(rangeInfo->fBlackLevelRepeatRows *
                                 rangeInfo->fBlackLevelRepeatCols * rawImage.Planes());

            directory.Add(&fBlackLevel);

        }

        // BlackLevelDeltaH:

        if(rangeInfo->ColumnBlackCount())
        {

            uint32 count = rangeInfo->ColumnBlackCount();

            fBlackLevelDeltaHData.Allocate(count * sizeof(dng_srational));

            auto *blacks = static_cast<dng_srational*>(fBlackLevelDeltaHData.Buffer());

            for(uint32 col = 0; col < count; col++)
            {

                blacks [col] = rangeInfo->ColumnBlack(col);

            }

            fBlackLevelDeltaH.SetData (blacks);
            fBlackLevelDeltaH.SetCount(count );

            directory.Add(&fBlackLevelDeltaH);

        }

        // BlackLevelDeltaV:

        if(rangeInfo->RowBlackCount())
        {

            uint32 count = rangeInfo->RowBlackCount();

            fBlackLevelDeltaVData.Allocate(count * sizeof(dng_srational));

            auto *blacks = static_cast<dng_srational*>(fBlackLevelDeltaVData.Buffer());

            for(uint32 row = 0; row < count; row++)
            {

                blacks [row] = rangeInfo->RowBlack(row);

            }

            fBlackLevelDeltaV.SetData (blacks);
            fBlackLevelDeltaV.SetCount(count );

            directory.Add(&fBlackLevelDeltaV);

        }

    }

    // WhiteLevel:

    // Only use the 32-bit data type ifwe must use it since there
    // are some lazy(non-Adobe) DNG readers out there.

    bool needs32 = false;

    fWhiteLevel16.SetCount(rawImage.Planes());
    fWhiteLevel32.SetCount(rawImage.Planes());

    for(uint32 c = 0; c < fWhiteLevel16.Count(); c++)
    {

        fWhiteLevelData32 [c] = negative.WhiteLevel(c);

        if(fWhiteLevelData32 [c] > 0x0FFFF)
        {
            needs32 = true;
        }

        fWhiteLevelData16[c] = uint16(fWhiteLevelData32[c]);

    }

    if(needs32)
    {
        directory.Add(&fWhiteLevel32);
    }

    else
    {
        directory.Add(&fWhiteLevel16);
    }

}

class MosaicTags
{

private:

    uint16 fCFARepeatPatternDimData [2]{};

    tag_uint16_ptr fCFARepeatPatternDim;

    uint8 fCFAPatternData [kMaxCFAPattern * kMaxCFAPattern]{};

    tag_uint8_ptr fCFAPattern;

    uint8 fCFAPlaneColorData [kMaxColorPlanes];

    tag_uint8_ptr fCFAPlaneColor;

    tag_uint16 fCFALayout;

    tag_uint32 fGreenSplit;

public:

    MosaicTags(dng_tiff_directory &directory,
               const dng_mosaic_info &info);

};


/******************************************************************************/

MosaicTags::MosaicTags(dng_tiff_directory &directory,
                       const dng_mosaic_info &info)

    :	fCFARepeatPatternDim(tcCFARepeatPatternDim,
                             fCFARepeatPatternDimData,
                             2)

    ,	fCFAPattern(tcCFAPattern,
                    fCFAPatternData)

    ,	fCFAPlaneColor(tcCFAPlaneColor,
                       fCFAPlaneColorData)

    ,	fCFALayout(tcCFALayout,
                   uint16(info.fCFALayout))

    ,	fGreenSplit(tcBayerGreenSplit,
                    info.fBayerGreenSplit)

{

    if(info.IsColorFilterArray())
    {

        // CFARepeatPatternDim:

        fCFARepeatPatternDimData[0] = uint16(info.fCFAPatternSize.v);
        fCFARepeatPatternDimData[1] = uint16(info.fCFAPatternSize.h);

        directory.Add(&fCFARepeatPatternDim);

        // CFAPattern:

        fCFAPattern.SetCount(uint32(info.fCFAPatternSize.v * info.fCFAPatternSize.h));

        for(int32 r = 0; r < info.fCFAPatternSize.v; r++)
        {

            for(int32 c = 0; c < info.fCFAPatternSize.h; c++)
            {

                fCFAPatternData [r * info.fCFAPatternSize.h + c] = info.fCFAPattern [r] [c];

            }

        }

        directory.Add(&fCFAPattern);

        // CFAPlaneColor:

        fCFAPlaneColor.SetCount(info.fColorPlanes);

        for(uint32 j = 0; j < info.fColorPlanes; j++)
        {

            fCFAPlaneColorData [j] = info.fCFAPlaneColor [j];

        }

        directory.Add(&fCFAPlaneColor);

        // CFALayout:

        fCFALayout.Set(uint16(info.fCFALayout));

        directory.Add(&fCFALayout);

        // BayerGreenSplit: (only include ifthe pattern is a Bayer pattern)

        if(info.fCFAPatternSize == dng_point(2, 2) &&
                info.fColorPlanes    == 3)
        {

            directory.Add(&fGreenSplit);

        }

    }

}

/*****************************************************************************/

class ColorTags
{

private:

    uint32 fColorChannels;

    tag_matrix fCameraCalibration1;
    tag_matrix fCameraCalibration2;

    tag_string fCameraCalibrationSignature;

    tag_string fAsShotProfileName;

    dng_urational fAnalogBalanceData [4];

    tag_urational_ptr fAnalogBalance;

    dng_urational fAsShotNeutralData [4];

    tag_urational_ptr fAsShotNeutral;

    dng_urational fAsShotWhiteXYData [2];

    tag_urational_ptr fAsShotWhiteXY;

    tag_urational fLinearResponseLimit;

public:

    ColorTags(dng_tiff_directory &directory,
              const dng_negative &negative);

};

/******************************************************************************/

ColorTags::ColorTags(dng_tiff_directory &directory,
                     const dng_negative &negative)

    :	fColorChannels(negative.ColorChannels())

    ,	fCameraCalibration1(tcCameraCalibration1,
                            negative.CameraCalibration1())

    ,	fCameraCalibration2(tcCameraCalibration2,
                            negative.CameraCalibration2())

    ,	fCameraCalibrationSignature(tcCameraCalibrationSignature,
                                    negative.CameraCalibrationSignature())

    ,	fAsShotProfileName(tcAsShotProfileName,
                           negative.AsShotProfileName())

    ,	fAnalogBalance(tcAnalogBalance,
                       fAnalogBalanceData,
                       fColorChannels)

    ,	fAsShotNeutral(tcAsShotNeutral,
                       fAsShotNeutralData,
                       fColorChannels)

    ,	fAsShotWhiteXY(tcAsShotWhiteXY,
                       fAsShotWhiteXYData,
                       2)

    ,	fLinearResponseLimit(tcLinearResponseLimit,
                             negative.LinearResponseLimitR())

{

    if(fColorChannels > 1)
    {

        uint32 channels2 = fColorChannels * fColorChannels;

        if(fCameraCalibration1.Count() == channels2)
        {

            directory.Add(&fCameraCalibration1);

        }

        if(fCameraCalibration2.Count() == channels2)
        {

            directory.Add(&fCameraCalibration2);

        }

        if(fCameraCalibration1.Count() == channels2 ||
                fCameraCalibration2.Count() == channels2)
        {

            if(negative.CameraCalibrationSignature().NotEmpty())
            {

                directory.Add(&fCameraCalibrationSignature);

            }

        }

        if(negative.AsShotProfileName().NotEmpty())
        {

            directory.Add(&fAsShotProfileName);

        }

        for(uint32 j = 0; j < fColorChannels; j++)
        {

            fAnalogBalanceData [j] = negative.AnalogBalanceR(j);

        }

        directory.Add(&fAnalogBalance);

        if(negative.HasCameraNeutral())
        {

            for(uint32 k = 0; k < fColorChannels; k++)
            {

                fAsShotNeutralData [k] = negative.CameraNeutralR(k);

            }

            directory.Add(&fAsShotNeutral);

        }

        else if(negative.HasCameraWhiteXY())
        {

            negative.GetCameraWhiteXY(fAsShotWhiteXYData [0],
                    fAsShotWhiteXYData [1]);

            directory.Add(&fAsShotWhiteXY);

        }

        directory.Add(&fLinearResponseLimit);

    }

}

/******************************************************************************/

class ProfileTags
{

private:

    tag_uint16 fCalibrationIlluminant1;
    tag_uint16 fCalibrationIlluminant2;

    tag_matrix fColorMatrix1;
    tag_matrix fColorMatrix2;

    tag_matrix fForwardMatrix1;
    tag_matrix fForwardMatrix2;

    tag_matrix fReductionMatrix1;
    tag_matrix fReductionMatrix2;

    tag_string fProfileName;

    tag_string fProfileCalibrationSignature;

    tag_uint32 fEmbedPolicyTag;

    tag_string fCopyrightTag;

    uint32 fHueSatMapDimData[3];

    tag_uint32_ptr fHueSatMapDims;

    tag_data_ptr fHueSatData1;
    tag_data_ptr fHueSatData2;

    tag_uint32 fHueSatMapEncodingTag;

    uint32 fLookTableDimData[3];

    tag_uint32_ptr fLookTableDims;

    tag_data_ptr fLookTableData;

    tag_uint32 fLookTableEncodingTag;

    tag_srational fBaselineExposureOffsetTag;

    tag_uint32 fDefaultBlackRenderTag;

    dng_memory_data fToneCurveBuffer;

    tag_data_ptr fToneCurveTag;

public:

    ProfileTags(dng_tiff_directory &directory,
                const dng_camera_profile &profile);

};

/******************************************************************************/

ProfileTags::ProfileTags(dng_tiff_directory &directory,
                         const dng_camera_profile &profile)

    :	fCalibrationIlluminant1(tcCalibrationIlluminant1,
                                uint16(profile.CalibrationIlluminant1()))

    ,	fCalibrationIlluminant2(tcCalibrationIlluminant2,
                                uint16(profile.CalibrationIlluminant2()))

    ,	fColorMatrix1(tcColorMatrix1,
                      profile.ColorMatrix1())

    ,	fColorMatrix2(tcColorMatrix2,
                      profile.ColorMatrix2())

    ,	fForwardMatrix1(tcForwardMatrix1,
                        profile.ForwardMatrix1())

    ,	fForwardMatrix2(tcForwardMatrix2,
                        profile.ForwardMatrix2())

    ,	fReductionMatrix1(tcReductionMatrix1,
                          profile.ReductionMatrix1())

    ,	fReductionMatrix2(tcReductionMatrix2,
                          profile.ReductionMatrix2())

    ,	fProfileName(tcProfileName,
                     profile.Name(),
                     false)

    ,	fProfileCalibrationSignature(tcProfileCalibrationSignature,
                                     profile.ProfileCalibrationSignature(),
                                     false)

    ,	fEmbedPolicyTag(tcProfileEmbedPolicy,
                        profile.EmbedPolicy())

    ,	fCopyrightTag(tcProfileCopyright,
                      profile.Copyright(),
                      false)

    ,	fHueSatMapDims(tcProfileHueSatMapDims,
                       fHueSatMapDimData,
                       3)

    ,	fHueSatData1(tcProfileHueSatMapData1,
                     ttFloat,
                     profile.HueSatDeltas1().DeltasCount() * 3,
                     profile.HueSatDeltas1().GetConstDeltas())

    ,	fHueSatData2(tcProfileHueSatMapData2,
                     ttFloat,
                     profile.HueSatDeltas2().DeltasCount() * 3,
                     profile.HueSatDeltas2().GetConstDeltas())

    ,	fHueSatMapEncodingTag(tcProfileHueSatMapEncoding,
                              profile.HueSatMapEncoding())

    ,	fLookTableDims(tcProfileLookTableDims,
                       fLookTableDimData,
                       3)

    ,	fLookTableData(tcProfileLookTableData,
                       ttFloat,
                       profile.LookTable().DeltasCount() * 3,
                       profile.LookTable().GetConstDeltas())

    ,	fLookTableEncodingTag(tcProfileLookTableEncoding,
                              profile.LookTableEncoding())

    ,	fBaselineExposureOffsetTag(tcBaselineExposureOffset,
                                   profile.BaselineExposureOffset())

    ,	fDefaultBlackRenderTag(tcDefaultBlackRender,
                               profile.DefaultBlackRender())

    ,	fToneCurveTag(tcProfileToneCurve,
                      ttFloat,
                      0,
                      nullptr)

{

    if(profile.HasColorMatrix1())
    {

        uint32 colorChannels = profile.ColorMatrix1().Rows();

        directory.Add(&fCalibrationIlluminant1);

        directory.Add(&fColorMatrix1);

        if(fForwardMatrix1.Count() == colorChannels * 3)
        {

            directory.Add(&fForwardMatrix1);

        }

        if(colorChannels > 3 && fReductionMatrix1.Count() == colorChannels * 3)
        {

            directory.Add(&fReductionMatrix1);

        }

        if(profile.HasColorMatrix2())
        {

            directory.Add(&fCalibrationIlluminant2);

            directory.Add(&fColorMatrix2);

            if(fForwardMatrix2.Count() == colorChannels * 3)
            {

                directory.Add(&fForwardMatrix2);

            }

            if(colorChannels > 3 && fReductionMatrix2.Count() == colorChannels * 3)
            {

                directory.Add(&fReductionMatrix2);

            }

        }

        if(profile.Name().NotEmpty())
        {

            directory.Add(&fProfileName);

        }

        if(profile.ProfileCalibrationSignature().NotEmpty())
        {

            directory.Add(&fProfileCalibrationSignature);

        }

        directory.Add(&fEmbedPolicyTag);

        if(profile.Copyright().NotEmpty())
        {

            directory.Add(&fCopyrightTag);

        }

        bool haveHueSat1 = profile.HueSatDeltas1().IsValid();

        bool haveHueSat2 = profile.HueSatDeltas2().IsValid() &&
                profile.HasColorMatrix2();

        if(haveHueSat1 || haveHueSat2)
        {

            uint32 hueDivs = 0;
            uint32 satDivs = 0;
            uint32 valDivs = 0;

            if(haveHueSat1)
            {

                profile.HueSatDeltas1().GetDivisions(hueDivs,
                                                     satDivs,
                                                     valDivs);

            }

            else
            {

                profile.HueSatDeltas2().GetDivisions(hueDivs,
                                                     satDivs,
                                                     valDivs);

            }

            fHueSatMapDimData [0] = hueDivs;
            fHueSatMapDimData [1] = satDivs;
            fHueSatMapDimData [2] = valDivs;

            directory.Add(&fHueSatMapDims);

            // Don't bother including the ProfileHueSatMapEncoding tag unless it's
            // non-linear.

            if(profile.HueSatMapEncoding() != encoding_Linear)
            {

                directory.Add(&fHueSatMapEncodingTag);

            }

        }

        if(haveHueSat1)
        {

            directory.Add(&fHueSatData1);

        }

        if(haveHueSat2)
        {

            directory.Add(&fHueSatData2);

        }

        if(profile.HasLookTable())
        {

            uint32 hueDivs = 0;
            uint32 satDivs = 0;
            uint32 valDivs = 0;

            profile.LookTable().GetDivisions(hueDivs,
                                             satDivs,
                                             valDivs);

            fLookTableDimData [0] = hueDivs;
            fLookTableDimData [1] = satDivs;
            fLookTableDimData [2] = valDivs;

            directory.Add(&fLookTableDims);

            directory.Add(&fLookTableData);

            // Don't bother including the ProfileLookTableEncoding tag unless it's
            // non-linear.

            if(profile.LookTableEncoding() != encoding_Linear)
            {

                directory.Add(&fLookTableEncodingTag);

            }

        }

        // Don't bother including the BaselineExposureOffset tag unless it's both
        // valid and non-zero.

        if(profile.BaselineExposureOffset().IsValid())
        {

            if(profile.BaselineExposureOffset().As_real64() != 0.0)
            {

                directory.Add(&fBaselineExposureOffsetTag);

            }

        }

        if(profile.DefaultBlackRender() != defaultBlackRender_Auto)
        {

            directory.Add(&fDefaultBlackRenderTag);

        }

        if(profile.ToneCurve().IsValid())
        {

            // Tone curve stored as pairs of 32-bit coordinates.  Probably could do with
            // 16-bits here, but should be small number of points so...

            auto toneCurvePoints = uint32(profile.ToneCurve().fCoord.size());

            fToneCurveBuffer.Allocate(toneCurvePoints * 2 * sizeof(real32));

            real32 *points = fToneCurveBuffer.Buffer_real32();

            fToneCurveTag.SetCount(toneCurvePoints * 2);
            fToneCurveTag.SetData (points);

            for(uint32 i = 0; i < toneCurvePoints; i++)
            {

                // Transpose coordinates so they are in a more expected
                // order(domain -> range).

                points [i * 2    ] = real32(profile.ToneCurve().fCoord[i].h);
                points [i * 2 + 1] = real32(profile.ToneCurve().fCoord[i].v);

            }

            directory.Add(&fToneCurveTag);

        }

    }

}



/******************************************************************************/

dng_ext_image_writer::dng_ext_image_writer() = default;

void pack12bit(dng_host &host,
               dng_stream &stream,
               dng_pixel_buffer &buffer,
               AutoPtr<dng_memory_block> &compressedBuffer)
{
    uint32 w = buffer.Area().W();
    uint32 h = buffer.Area().H() * buffer.Planes();
    unsigned long dBytes = w * h * 3 / 2;

    compressedBuffer.Reset(host.Allocate(uint32(dBytes)));
    uint8 *dBuffer = compressedBuffer->Buffer_uint8();

    for(uint32 r = 0; r < h; r++)
    {
        unsigned short* srcPtr = static_cast<unsigned short*>(buffer.fData) + r * w;
        uint8* dstPtr = dBuffer + r * w * 3 / 2;
        for(uint32 c = 0; c < w; c+=2)
        {
            unsigned short p0 = srcPtr[0];
            unsigned short p1 = srcPtr[1];

            dstPtr[0] =  uint8(p0 >> 4);
            dstPtr[1] = ((p0 << 4) & 0xFF) | (p1 >> 8);
            dstPtr[2] =  uint8(p1);

            dstPtr += 3;
            srcPtr += 2;
        }
    }
    stream.Put(dBuffer, uint32(dBytes));
}

void pack14bit(dng_host &host,
               dng_stream &stream,
               dng_pixel_buffer &buffer,
               AutoPtr<dng_memory_block> &compressedBuffer)
{
    uint32 w = buffer.Area().W();
    uint32 h = buffer.Area().H() * buffer.Planes();
    unsigned long dBytes = w * h * 7 / 4;

    compressedBuffer.Reset(host.Allocate(uint32(dBytes)));
    unsigned char *dBuffer = compressedBuffer->Buffer_uint8();

    for(uint32 r = 0; r < h; r++)
    {
        unsigned short* srcPtr = static_cast<unsigned short*>(buffer.fData) + r * w;
        unsigned char* dstPtr = dBuffer + r * w * 7 / 4;
        for(uint32 c = 0; c < w; c+=8)
        {
            unsigned short p0 = srcPtr[0];
            unsigned short p1 = srcPtr[1];
            unsigned short p2 = srcPtr[2];
            unsigned short p3 = srcPtr[3];
            unsigned short p4 = srcPtr[4];
            unsigned short p5 = srcPtr[5];
            unsigned short p6 = srcPtr[6];
            unsigned short p7 = srcPtr[7];

            /* 14-bit encoding:
            00xxxxxx xxxxxxxx
            hi          lo
            aaaaaaaa aaaaaabb
            bbbbbbbb bbbbcccc
            cccccccc ccdddddd
            dddddddd eeeeeeee
            eeeeeeff ffffffff
            ffffgggg gggggggg
            gghhhhhh hhhhhhhh
            */
            dstPtr[0]  = unsigned char(p0 >> 6);
            dstPtr[1]  = unsigned char((p0 << 2)  + (p1 >> 12));

            dstPtr[2]  = unsigned char(p1 >> 4);
            dstPtr[3]  = unsigned char((p1 << 4) + (p2 >> 10));

            dstPtr[4]  = unsigned char(p2 >> 2);
            dstPtr[5]  = unsigned char((p2 << 6) + (p3 >> 8));

            dstPtr[6]  = unsigned char(p3);
            dstPtr[7]  = unsigned char(p4 >> 6);

            dstPtr[8]  = unsigned char((p4 << 2) + (p5 >> 12));
            dstPtr[9]  = unsigned char(p5 >> 4);

            dstPtr[10] = unsigned char((p5 << 4) + (p6 >> 10));
            dstPtr[11] = unsigned char(p6 >> 2);

            dstPtr[12] = unsigned char((p6 << 6) + (p7 >> 8));
            dstPtr[13] = unsigned char(p7);

            dstPtr += 14;
            srcPtr += 8;
        }
    }
    stream.Put(dBuffer, uint32(dBytes));
}

void dng_ext_image_writer::WriteData(dng_host &host,
                                     const dng_ifd &ifd,
                                     dng_stream &stream,
                                     dng_pixel_buffer &buffer,
                                     AutoPtr<dng_memory_block> &compressedBuffer,
                                     bool usingMultipleThreads)
{


    uint32 w = buffer.Area().W();
    uint32 h = buffer.Area().H() * buffer.Planes();

    if(ifd.fCompression == ccUncompressed)
    {
		if(rawBpp == 8)
			stream.Put(static_cast<unsigned char*>(buffer.fData), w * h);
		else if(rawBpp == 12)
            pack12bit(host, stream, buffer, compressedBuffer);
        else if(rawBpp == 14)
            pack14bit(host, stream, buffer, compressedBuffer);
		else
			dng_image_writer::WriteData(host, ifd, stream, buffer, compressedBuffer, usingMultipleThreads);
    }
    else
    {
        dng_image_writer::WriteData(host, ifd, stream, buffer, compressedBuffer, usingMultipleThreads);
    }
 
}

void dng_ext_image_writer::WriteDNGEx(dng_host &host,
                                      dng_stream &stream,
                                      const dng_negative &negative,
                                      const dng_metadata &constMetadata,
                                      const dng_preview_list *previewList,
                                      uint32 maxBackwardVersion,
                                      bool uncompressed)
{
	//We implemented only 12 and 14 bit packing, so other options are
	//passed to base class implementation 
	if(rawBpp != 8 && rawBpp != 12 && rawBpp != 14)
	{
		dng_image_writer::WriteDNG(host, stream, negative, constMetadata, previewList, maxBackwardVersion, uncompressed);
		return;
	}

    uint32 j;

    // Clean up metadata per MWG recommendations.
    AutoPtr<dng_metadata> metadata(constMetadata.Clone(host.Allocator()));
    //    CleanUpMetadata(host,
    //                     *metadata,
    //                     kMetadataSubset_All,
    //                     "image/dng");

    uint32 compression = uncompressed ? ccUncompressed : ccJPEG;

    // Does this image have a transparency mask?
    bool hasTransparencyMask =(negative.RawTransparencyMask() != nullptr);

    uint32 dngVersion = dngVersion_Current;
    uint32 dngBackwardVersion = dngVersion_1_1_0_0;

    dngBackwardVersion = Max_uint32(dngBackwardVersion,
                                    negative.OpcodeList1().MinVersion(false));

    dngBackwardVersion = Max_uint32(dngBackwardVersion,
                                    negative.OpcodeList2().MinVersion(false));

    dngBackwardVersion = Max_uint32(dngBackwardVersion,
                                    negative.OpcodeList3().MinVersion(false));

    if(negative.GetMosaicInfo() &&
            negative.GetMosaicInfo()->fCFALayout >= 6)
    {
        dngBackwardVersion = Max_uint32(dngBackwardVersion, dngVersion_1_3_0_0);
    }

    if(dngBackwardVersion > dngVersion)
    {
        return;
    }

    // Find best thumbnail from preview list, ifany.
    const dng_preview *thumbnail = nullptr;
    if(previewList)
    {
        uint32 thumbArea = 0;
        for(j = 0; j < previewList->Count(); j++)
        {
            auto *imagePreview = dynamic_cast<const dng_image_preview *>(&previewList->Preview(j));
            if(imagePreview)
            {
                uint32 thisArea = imagePreview->fImage->Bounds().W() *
                        imagePreview->fImage->Bounds().H();
                if(!thumbnail || thisArea < thumbArea)
                {
                    thumbnail = &previewList->Preview(j);
                    thumbArea = thisArea;
                }
            }

            auto* jpegPreview = dynamic_cast<const dng_jpeg_preview *>(&previewList->Preview(j));

            if(jpegPreview)
            {
                uint32 thisArea = uint32(jpegPreview->fPreviewSize.h * jpegPreview->fPreviewSize.v);
                if(!thumbnail || thisArea < thumbArea)
                {
                    thumbnail = &previewList->Preview(j);
                    thumbArea = thisArea;
                }
            }
        }
    }

    // Create the main IFD
    dng_tiff_directory mainIFD;

    // Create the IFD forthe raw data. ifthere is no thumnail, this is
    // just a reference the main IFD.  Otherwise allocate a new one.
    AutoPtr<dng_tiff_directory> rawIFD_IfNotMain;

    if(thumbnail)
        rawIFD_IfNotMain.Reset(new dng_tiff_directory);

    dng_tiff_directory &rawIFD(thumbnail ? *rawIFD_IfNotMain : mainIFD);

    // Include DNG version tags.
    uint8 dngVersionData [4];

    dngVersionData[0] = uint8(dngVersion >> 24);
    dngVersionData[1] = uint8(dngVersion >> 16);
    dngVersionData[2] = uint8(dngVersion >>  8);
    dngVersionData[3] = uint8(dngVersion      );

    tag_uint8_ptr tagDNGVersion(tcDNGVersion, dngVersionData, 4);

    mainIFD.Add(&tagDNGVersion);

    uint8 dngBackwardVersionData[4];

    dngBackwardVersionData[0] = uint8(dngBackwardVersion >> 24);
    dngBackwardVersionData[1] = uint8(dngBackwardVersion >> 16);
    dngBackwardVersionData[2] = uint8(dngBackwardVersion >>  8);
    dngBackwardVersionData[3] = uint8(dngBackwardVersion      );

    tag_uint8_ptr tagDNGBackwardVersion(tcDNGBackwardVersion, dngBackwardVersionData, 4);

    mainIFD.Add(&tagDNGBackwardVersion);

    // The main IFD contains the thumbnail, ifthere is a thumbnail.
    AutoPtr<dng_basic_tag_set> thmBasic;

    if(thumbnail)
    {
        thmBasic.Reset(thumbnail->AddTagSet(mainIFD));
    }

    // Get the raw image we are writing.
    const dng_image &rawImage(negative.RawImage());


    // Get a copy of the mosaic info.
    dng_mosaic_info mosaicInfo;
    if(negative.GetMosaicInfo())
        mosaicInfo = *(negative.GetMosaicInfo());

    // Create a dng_ifd record forthe raw image.
    dng_ifd info;

    info.fImageWidth  = rawImage.Width();
    info.fImageLength = rawImage.Height();

    info.fSamplesPerPixel = rawImage.Planes();

    info.fPhotometricInterpretation = piCFA;
    info.fCompression = compression;

    info.fBitsPerSample[0] = uint32(rawBpp);


    // forlossless JPEG compression, we often lie about the
    // actual channel count to get the predictors to work across
    // same color mosaic pixels.
    uint32 fakeChannels = 1;

    if(info.fCompression == ccJPEG)
    {
        if(mosaicInfo.IsColorFilterArray())
        {
            if(mosaicInfo.fCFAPatternSize.h == 4)
                fakeChannels = 4;
            else if(mosaicInfo.fCFAPatternSize.h == 2)
                fakeChannels = 2;

            // However, lossless JEPG is limited to four channels,
            // so compromise might be required.
            while(fakeChannels * info.fSamplesPerPixel > 4 &&
                  fakeChannels > 1)
                fakeChannels >>= 1;
        }
    }

    // Figure out tile sizes.
    if(info.fCompression == ccJPEG)
    {
        uint32 tileCols = 1;
        uint32 tileRows = 1;
        if(info.fImageWidth % 16 == 0)
            tileCols = 8;
        else if(info.fImageWidth % 8 == 0)
            tileCols = 4;
        else if(info.fImageWidth % 4 == 0)
            tileCols = 2;
        //Adobe Premiere cannot open DNGs with more than one rows of tiles

        //        if(info.fImageLength % 4 == 0)
        //            tileRows = 2;

        if(tileRows == 1 && tileCols == 1)
            info.SetSingleStrip();
        else
        {
            info.fTileLength = info.fImageLength / tileRows;
            info.fTileWidth = info.fImageWidth / tileCols;

            info.fUsesTiles  = true;
            info.fUsesStrips = false;
        }
    }
    else
    {
        info.fTileLength = info.fImageLength;
        info.fTileWidth = info.fImageWidth;

        info.fUsesTiles  = true;
        info.fUsesStrips = false;

        info.SetSingleStrip();
    }

    // Basic information.
    dng_basic_tag_set rawBasic(rawIFD, info);

    // DefaultScale tag.
    dng_urational defaultScaleData [2];
    defaultScaleData [0] = negative.DefaultScaleH();
    defaultScaleData [1] = negative.DefaultScaleV();

    tag_urational_ptr tagDefaultScale(tcDefaultScale,
                                      defaultScaleData,
                                      2);

    rawIFD.Add(&tagDefaultScale);

    // Best quality scale tag.
    tag_urational tagBestQualityScale(tcBestQualityScale,
                                      negative.BestQualityScale());
    rawIFD.Add(&tagBestQualityScale);

    // DefaultCropOrigin tag.
    dng_urational defaultCropOriginData [2];
    defaultCropOriginData [0] = negative.DefaultCropOriginH();
    defaultCropOriginData [1] = negative.DefaultCropOriginV();

    tag_urational_ptr tagDefaultCropOrigin(tcDefaultCropOrigin,
                                           defaultCropOriginData,
                                           2);

    rawIFD.Add(&tagDefaultCropOrigin);

    // DefaultCropSize tag.
    dng_urational defaultCropSizeData [2];
    defaultCropSizeData [0] = negative.DefaultCropSizeH();
    defaultCropSizeData [1] = negative.DefaultCropSizeV();

    tag_urational_ptr tagDefaultCropSize(tcDefaultCropSize,
                                         defaultCropSizeData,
                                         2);

    rawIFD.Add(&tagDefaultCropSize);

    // DefaultUserCrop tag.
    //    dng_urational defaultUserCropData [4];

    //    defaultUserCropData [0] = negative.DefaultUserCropT();
    //    defaultUserCropData [1] = negative.DefaultUserCropL();
    //    defaultUserCropData [2] = negative.DefaultUserCropB();
    //    defaultUserCropData [3] = negative.DefaultUserCropR();

    //    tag_urational_ptr tagDefaultUserCrop(tcDefaultUserCrop,
    //                                          defaultUserCropData,
    //                                          4);

    //    rawIFD.Add(&tagDefaultUserCrop);

 
    // Range mapping tag set.
    RangeTags rangeSet(rawIFD, negative);

    // Mosaic pattern information.
    MosaicTags mosaicSet(rawIFD, mosaicInfo);

    // Chroma blur radius.
    tag_urational tagChromaBlurRadius(tcChromaBlurRadius,
                                      negative.ChromaBlurRadius());

    if(negative.ChromaBlurRadius().IsValid())
        rawIFD.Add(&tagChromaBlurRadius);

    // Anti-alias filter strength.
    tag_urational tagAntiAliasStrength(tcAntiAliasStrength,
                                       negative.AntiAliasStrength());

    if(negative.AntiAliasStrength().IsValid())
        rawIFD.Add(&tagAntiAliasStrength);

    // Profile and other color related tags.
    std::vector<uint32> extraProfileIndex;

    AutoPtr<ProfileTags> profileSet;
    AutoPtr<ColorTags> colorSet;

    if(!negative.IsMonochrome())
    {
        const dng_camera_profile &mainProfile(*negative.ComputeCameraProfileToEmbed(constMetadata));
        profileSet.Reset(new ProfileTags(mainIFD, mainProfile));

        colorSet.Reset(new ColorTags(mainIFD, negative));

        // Build list of profile indices to include in extra profiles tag.
        uint32 profileCount = negative.ProfileCount();
        extraProfileIndex.reserve(profileCount);
        //
        for(uint32 index = 0; index < profileCount; index++)
        {
            const dng_camera_profile &profile(negative.ProfileByIndex(index));

            if(&profile != &mainProfile)
            {
                if(profile.WasReadFromDNG())
                    extraProfileIndex.push_back(index);
            }
        }
    }

    // Extra camera profiles tag.
    auto extraProfileCount = uint32(extraProfileIndex.size());
    dng_memory_data extraProfileOffsets(extraProfileCount * uint32(sizeof(uint32)));
    tag_uint32_ptr extraProfileTag(tcExtraCameraProfiles,
                                   extraProfileOffsets.Buffer_uint32(),
                                   extraProfileCount);

    if(extraProfileCount)
        mainIFD.Add(&extraProfileTag);

    // Other tags.
    tag_uint16 tagOrientation(tcOrientation,uint16(negative.ComputeOrientation(constMetadata).GetTIFF()));
    mainIFD.Add(&tagOrientation);

    tag_srational tagBaselineExposure(tcBaselineExposure,
                                      negative.BaselineExposureR());
    mainIFD.Add(&tagBaselineExposure);

    tag_urational tagBaselineNoise(tcBaselineNoise,
                                   negative.BaselineNoiseR());
    mainIFD.Add(&tagBaselineNoise);

    tag_urational tagNoiseReductionApplied(tcNoiseReductionApplied,
                                           negative.NoiseReductionApplied());

    if(negative.NoiseReductionApplied().IsValid())
        mainIFD.Add(&tagNoiseReductionApplied);

    tag_dng_noise_profile tagNoiseProfile(negative.NoiseProfile());

    if(negative.NoiseProfile().IsValidForNegative(negative))
        mainIFD.Add(&tagNoiseProfile);

    tag_urational tagBaselineSharpness(tcBaselineSharpness,
                                       negative.BaselineSharpnessR());

    mainIFD.Add(&tagBaselineSharpness);

    tag_string tagUniqueName(tcUniqueCameraModel,
                             negative.ModelName(),
                             true);

    mainIFD.Add(&tagUniqueName);

    tag_string tagLocalName(tcLocalizedCameraModel,
                            negative.LocalName(),
                            false);

    if(negative.LocalName().NotEmpty())
        mainIFD.Add(&tagLocalName);

    tag_urational tagShadowScale(tcShadowScale,
                                 negative.ShadowScaleR());
    mainIFD.Add(&tagShadowScale);

    tag_uint16 tagColorimetricReference(tcColorimetricReference,
                                        uint16(negative.ColorimetricReference()));
    if(negative.ColorimetricReference() != crSceneReferred)
        mainIFD.Add(&tagColorimetricReference);

    bool useNewDigest =(maxBackwardVersion >= dngVersion_1_4_0_0);

    if(compression == ccLossyJPEG)
        negative.FindRawJPEGImageDigest(host);
    else
    {
        if(useNewDigest)
            negative.FindNewRawImageDigest(host);
        else
            negative.FindRawImageDigest(host);
    }

    tag_uint8_ptr tagRawImageDigest(useNewDigest ? tcNewRawImageDigest : tcRawImageDigest,
                                    compression == ccLossyJPEG ?
                                        negative.RawJPEGImageDigest().data :
                                        (useNewDigest ? negative.NewRawImageDigest().data
                                                      : negative.RawImageDigest  ().data),
                                    16);

    mainIFD.Add(&tagRawImageDigest);

    negative.FindRawDataUniqueID(host);
    tag_uint8_ptr tagRawDataUniqueID(tcRawDataUniqueID,
                                     negative.RawDataUniqueID().data,
                                     16);

    if(negative.RawDataUniqueID().IsValid())
        mainIFD.Add(&tagRawDataUniqueID);

    tag_string tagOriginalRawFileName(tcOriginalRawFileName,
                                      negative.OriginalRawFileName(),
                                      false);

    if(negative.HasOriginalRawFileName())
        mainIFD.Add(&tagOriginalRawFileName);

    negative.FindOriginalRawFileDigest();
    tag_data_ptr tagOriginalRawFileData(tcOriginalRawFileData,
                                        ttUndefined,
                                        negative.OriginalRawFileDataLength(),
                                        negative.OriginalRawFileData     ());

    tag_uint8_ptr tagOriginalRawFileDigest(tcOriginalRawFileDigest,
                                           negative.OriginalRawFileDigest().data,
                                           16);

    if(negative.OriginalRawFileData())
    {
        mainIFD.Add(&tagOriginalRawFileData);
        mainIFD.Add(&tagOriginalRawFileDigest);
    }

    // XMP metadata.
    tag_xmp tagXMP(metadata->GetXMP());
    if(tagXMP.Count())
        mainIFD.Add(&tagXMP);

    // Exiftags.
    exif_tag_set exifSet(mainIFD,
                         *metadata->GetExif(),
                         metadata->IsMakerNoteSafe(),
                         metadata->MakerNoteData (),
                         metadata->MakerNoteLength(),
                         true);

    // Private data.
    tag_uint8_ptr tagPrivateData(tcDNGPrivateData,
                                 negative.PrivateData(),
                                 negative.PrivateLength());

    if(negative.PrivateLength())
        mainIFD.Add(&tagPrivateData);

    // Opcode list 1.
    AutoPtr<dng_memory_block> opcodeList1Data(negative.OpcodeList1().Spool(host));
    tag_data_ptr tagOpcodeList1(tcOpcodeList1,
                                ttUndefined,
                                opcodeList1Data.Get() ? opcodeList1Data->LogicalSize() : 0,
                                opcodeList1Data.Get() ? opcodeList1Data->Buffer    () : nullptr);

    if(opcodeList1Data.Get())
        rawIFD.Add(&tagOpcodeList1);

    // Opcode list 2.
    AutoPtr<dng_memory_block> opcodeList2Data(negative.OpcodeList2().Spool(host));
    tag_data_ptr tagOpcodeList2(tcOpcodeList2,
                                ttUndefined,
                                opcodeList2Data.Get() ? opcodeList2Data->LogicalSize() : 0,
                                opcodeList2Data.Get() ? opcodeList2Data->Buffer    () : nullptr);

    if(opcodeList2Data.Get())
        rawIFD.Add(&tagOpcodeList2);

    // Opcode list 3.
    AutoPtr<dng_memory_block> opcodeList3Data(negative.OpcodeList3().Spool(host));
    tag_data_ptr tagOpcodeList3(tcOpcodeList3,
                                ttUndefined,
                                opcodeList3Data.Get() ? opcodeList3Data->LogicalSize() : 0,
                                opcodeList3Data.Get() ? opcodeList3Data->Buffer    () : nullptr);

    if(opcodeList3Data.Get())
        rawIFD.Add(&tagOpcodeList3);

    // Transparency mask, ifany.
    AutoPtr<dng_ifd> maskInfo;
    AutoPtr<dng_tiff_directory> maskIFD;
    AutoPtr<dng_basic_tag_set> maskBasic;

    if(hasTransparencyMask)
    {
        // Create mask IFD.
        maskInfo.Reset(new dng_ifd);
        maskInfo->fNewSubFileType = sfTransparencyMask;
        maskInfo->fImageWidth  = negative.RawTransparencyMask()->Bounds().W();
        maskInfo->fImageLength = negative.RawTransparencyMask()->Bounds().H();
        maskInfo->fSamplesPerPixel = 1;
        maskInfo->fBitsPerSample [0] = negative.RawTransparencyMaskBitDepth();
        maskInfo->fPhotometricInterpretation = piTransparencyMask;
        maskInfo->fCompression = uncompressed ? ccUncompressed  : ccDeflate;
        maskInfo->fPredictor   = uncompressed ? cpNullPredictor : cpHorizontalDifference;

        if(negative.RawTransparencyMask()->PixelType() == ttFloat)
        {
            maskInfo->fSampleFormat [0] = sfFloatingPoint;
            if(maskInfo->fCompression == ccDeflate)
                maskInfo->fPredictor = cpFloatingPoint;
        }

        if(maskInfo->fCompression == ccDeflate)
            maskInfo->FindTileSize(512 * 1024);
        else
            maskInfo->SetSingleStrip();

        // Create mask tiff directory.
        maskIFD.Reset(new dng_tiff_directory);

        // Add mask basic tag set.
        maskBasic.Reset(new dng_basic_tag_set(*maskIFD, *maskInfo));

    }

    // Add other subfiles.
    uint32 subFileCount = thumbnail ? 1 : 0;

    if(hasTransparencyMask)
        subFileCount++;

    // Add previews.
    uint32 previewCount = previewList ? previewList->Count() : 0;
    AutoPtr<dng_tiff_directory> previewIFD [kMaxDNGPreviews];
    AutoPtr<dng_basic_tag_set> previewBasic [kMaxDNGPreviews];

    for(j = 0; j < previewCount; j++)
    {
        if(thumbnail != &previewList->Preview(j))
        {
            previewIFD [j] . Reset(new dng_tiff_directory);
            previewBasic [j] . Reset(previewList->Preview(j).AddTagSet(*previewIFD [j]));
            subFileCount++;
        }
    }

    // And a link to the raw and JPEG image IFDs.
    uint32 subFileData [kMaxDNGPreviews + 2];
    tag_uint32_ptr tagSubFile(tcSubIFDs,
                              subFileData,
                              subFileCount);

    if(subFileCount)
        mainIFD.Add(&tagSubFile);

    // Skip past the header and IFDs fornow.

    uint32 currentOffset = 8;
    currentOffset += mainIFD.Size();

    uint32 subFileIndex = 0;
    if(thumbnail)
    {
        subFileData [subFileIndex++] = currentOffset;
        currentOffset += rawIFD.Size();
    }

    if(hasTransparencyMask)
    {
        subFileData [subFileIndex++] = currentOffset;
        currentOffset += maskIFD->Size();
    }

    for(j = 0; j < previewCount; j++)
    {
        if(thumbnail != &previewList->Preview(j))
        {
            subFileData [subFileIndex++] = currentOffset;
            currentOffset += previewIFD [j]->Size();
        }
    }

    exifSet.Locate(currentOffset);
    currentOffset += exifSet.Size();
    stream.SetWritePosition(currentOffset);

    // Write the extra profiles.
    if(extraProfileCount)
    {
        for(j = 0; j < extraProfileCount; j++)
        {
            extraProfileOffsets.Buffer_uint32()[j] = uint32(stream.Position());
            uint32 index = extraProfileIndex[j];
            const dng_camera_profile &profile(negative.ProfileByIndex(index));
            tiff_dng_extended_color_profile extraWriter(profile);
            extraWriter.Put(stream, false);
        }
    }

    // Write the thumbnail data.
    if(thumbnail)
    {
        thumbnail->WriteData(host,
                             *this,
                             *thmBasic,
                             stream);
    }

    // Write the preview data.
    for(j = 0; j < previewCount; j++)
    {
        if(thumbnail != &previewList->Preview(j))
        {
            previewList->Preview(j).WriteData(host,
                                              *this,
                                              *previewBasic [j],
                                              stream);
        }
    }

    // Write the raw data.
    WriteImage(host,
               info,
               rawBasic,
               stream,
               rawImage,
               fakeChannels);

    // Write transparency mask image.
    if(hasTransparencyMask)
    {
        WriteImage(host,
                   *maskInfo,
                   *maskBasic,
                   stream,
                   *negative.RawTransparencyMask());

    }

    // Trim the file to this length.
    stream.SetLength(stream.Position());

    // DNG has a 4G size limit.
    if(stream.Length() > 0x0FFFFFFFFL)
        ThrowImageTooBigDNG();

    // Write TIFF Header.
    stream.SetWritePosition(0);
    stream.Put_uint16(stream.BigEndian() ? byteOrderMM : byteOrderII);
    stream.Put_uint16(42);
    stream.Put_uint32(8);

    // Write the IFDs.
    mainIFD.Put(stream);
    if(thumbnail)
        rawIFD.Put(stream);

    if(hasTransparencyMask)
        maskIFD->Put(stream);

    for(j = 0; j < previewCount; j++)
    {
        if(thumbnail != &previewList->Preview(j))
            previewIFD [j]->Put(stream);
    }

    exifSet.Put(stream);
    stream.Flush();
}
