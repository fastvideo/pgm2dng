# pgm2dng
PGM to DNG converter

Usually we get raw frame from machine vision or industrial camera as a bitmap of 8/10/12/14/16-bit values which we store as PGM (Portable Gray Map) image. Apart from bitmap we know bit depth and image resolution (width and height) for every frame. But this is not enough for good color reproduction. We also need the following:
1. White balance coefficients (three coefficients for red, green and blue channels)
2. Color correction matrix 3x3 or DCP profile

To get the above data, we need to perform color calibration with Color Checker and calibrated light sources. Camera manufacturers do that calibration by themselves because they posess high quality and well-calibrated light sources. If current illumination is different, one can compute color transform to take that into account, but we need original color calibration data, which is supplied by camera manufacturer.

To describe the situation in more detail, we get raw data at camera colorspace, and we need to transform that data to any colorspace that we know. Usually we need to transform data from camera colorspace to intermediary XYZ colorspace and later on we will know how to transform the image to any colorspace that we need.

Algorithm for PGM2DNG transform
1. Take a shot and capture Color Checker image (optionally save it as PGM file)
2. Recognize all 24 squares from the shot
3. Compute WB coefficients
4. Load Color Checker image to DCamProf software to get DCP profile
5. Create DNG image from WB coefficients, DCP profile and PGM data
6. Optionally view DNG image at Fast CinemaDNG Processor or Raw Therapee



Useful links:

Adobe DNG Software Development Kit (SDK) 1.4 - https://www.adobe.com/support/downloads/dng/dng_sdk.html

DCamProf software - https://www.ludd.ltu.se/~torger/dcamprof.html

RawTherapee - http://rawtherapee.com

Fast CinemaDNG Processor - https://www.fastcinemadng.com

Fastvideo Image & Video Processing SDK for CUDA - https://www.fastcompression.com/products/sdk/sdk.htm
