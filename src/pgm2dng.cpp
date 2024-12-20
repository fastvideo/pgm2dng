// pgm2dng.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <vector>
#include <fstream>
#include <iostream>
//#include <filesystem>
#ifdef _MSC_VER
#include <Windows.h>
#endif

#include "ctype.h"

#include "dng_color_space.h"
#include "dng_date_time.h"
#include "dng_exceptions.h"
#include "dng_file_stream.h"
#include "dng_globals.h"
#include "dng_host.h"
#include "dng_ifd.h"
#include "dng_ext_image_writer.h"
#include "dng_info.h"
#include "dng_camera_profile.h"
#include "dng_linearization_info.h"
#include "dng_mosaic_info.h"
#include "dng_negative.h"
#include "dng_preview.h"
#include "dng_render.h"
#include "dng_simple_image.h"
#include "dng_tag_codes.h"
#include "dng_tag_types.h"
#include "dng_tag_values.h"
#include "dng_xmp.h"
#include "dng_xmp_sdk.h"

#include "dng_image_writer.h"

#include <tiffio.h>

#include "cxxopts.hpp"

#ifndef __GNUC__
static const char *helpCompiledFor = "Compiled for Windows-7/8/10 [x64]\n";
#else
static const char *helpCompiledFor = "Compiled for Linux\n";
#endif
static const char *helpCommonInfo =
"\n" \
"This software is prepared for non-commercial use only. It is free for personal and educational (including non-profit organization) use. Distribution of this software without any permission from Fastvideo is NOT allowed. NO warranty and responsibility is provided by the authors for the consequences of using it.\n" \
"\n" ;
static const char *projectName = "PGM to DNG converter";
//extern const char *helpProject;

#ifdef _WIN32
#ifndef FOPEN
#define FOPEN(fHandle,filename,mode) fopen_s(&fHandle, filename, mode)
#endif
#ifndef FOPEN_FAIL
#define FOPEN_FAIL(result) (result != 0)
#endif
#ifndef SSCANF
#define SSCANF sscanf_s
#endif
#else
#ifndef FOPEN
#define FOPEN(fHandle,filename,mode) (fHandle = fopen(filename, mode))
#endif
#ifndef FOPEN_FAIL
#define FOPEN_FAIL(result) (result == NULL)
#endif
#ifndef SSCANF
#define SSCANF sscanf
#endif
#endif

const unsigned int PGMHeaderSize = 0x40;

template<typename T> static inline T _uSnapUp(T a, T b) 
{
	return a + (b - a % b) % b;
}

bool endsWith(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

std::string str2lower(const std::string &inp)
{
    auto data = inp;
    std::transform(data.begin(), data.end(), data.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return data;
}

std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

// // Function to check if a file has a given extension
bool hasExtension(const std::string& filePath, const std::vector<std::string>& extensions) {
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) return false;  // No extension found

    std::string fileExt = filePath.substr(dotPos);  // Get the file extension part

    for (const std::string& ext : extensions) {
        if (fileExt == ext) {
            return true;
        }
    }

    return false;
}

// Function to split a string by a delimiter and return a vector of substrings
std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

// Function to clean up extensions (remove leading '*')
std::vector<std::string> cleanExtensions(const std::vector<std::string>& rawExtensions) {
    std::vector<std::string> cleanedExtensions;
    for (const auto& ext : rawExtensions) {
        if (ext.size() > 1 && ext[0] == '*') {
            cleanedExtensions.push_back(ext.substr(1)); // Remove the leading '*'
        } else {
            cleanedExtensions.push_back(ext);
        }
    }
    return cleanedExtensions;
}

std::vector<std::string> getFilesFromDirectory(const std::string &folder)
{
#ifdef _MSC_VER
    using namespace std;
    vector<string> names;
    string search_path = folder + "/*.*";
    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            // read all (real) files in current folder
            // , delete '!' read other 2 default folder . and ..
            if(! (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
                names.push_back(folder + "/" + fd.cFileName);
            }
        }while(::FindNextFile(hFind, &fd));
        ::FindClose(hFind);
    }
    return names;
#else
    /// TODO: need realization for linux
    return {};
#endif
}

// Function to get files in a directory with specified extensions
std::vector<std::string> getFilesWithExtensions(const std::string& directoryPath,
                                                const std::string& extensions)
{
    //namespace fs = std::filesystem;
    std::vector<std::string> result;
    std::vector<std::string> extList = splitString(extensions, ',');
    extList = cleanExtensions(extList);

    auto files = getFilesFromDirectory(directoryPath);

    for(const auto &entry: files){
        if (hasExtension(entry, extList)) {
            result.push_back(entry);
        }
    }

    // try {
    //     // Iterate through the directory
    //     for (const auto& entry : fs::directory_iterator(directoryPath)) {
    //         if (entry.is_regular_file()) {
    //             std::string fileExtension = entry.path().extension().string();
    //             for (const auto& ext : extList) {
    //                 if (fileExtension == ext) {
    //                     result.push_back(entry.path().string());
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // } catch (const fs::filesystem_error& e) {
    //     std::cerr << "Filesystem error: " << e.what() << std::endl;
    // } catch (const std::exception& e) {
    //     std::cerr << "General error: " << e.what() << std::endl;
    // }

    return result;
}

std::string replaceFileExtension(const std::string& filePath, const std::string& newExtension)
{
    // Find the last occurrence of '.' in the file path
    size_t dotPos = filePath.find_last_of('.');

    // Find the last occurrence of '/' or '\' to ensure we are modifying the file name, not a directory
    size_t slashPos = filePath.find_last_of("/\\");

    // Check if the dot is part of the file name and not in a directory name
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        // Replace the extension
        return filePath.substr(0, dotPos) + "." + newExtension;
    } else {
        // No valid extension found, simply append the new extension
        return filePath + "." + newExtension;
    }
}

//////////////////////////////////////////

int helpPrint(void) 
{
	printf("%s", projectName);
	printf("%s", helpCompiledFor);
	printf("%s", helpCommonInfo);
	return 0;
}

int usagePrint(void) 
{
	printf("Usage:\n");
	printf("pgm2dng.exe <Optiona>\n");
	printf("Where <Options> are:\n");
	printf("--in=<path/to/input/file> - path to pgm file (mandatory)\n");
	printf("--out=<path/to/output/file> - path to dng file (mandatory)\n");
	printf("--mono - write monochrome dng\n");
	printf("--dcp=<path/to/dcp/file> - path to dcp (digital camera profile) file (mandatory for color dng)\n");
	printf("--pattern=<pattern> - CFA pattern. Alowed values: RGGB, GBRG, GRBG, BGGR (mandatory for color dng)\n");
	printf("--wp=<R,G,B> - comma separated white point values (mandatory for color dng)\n");
	printf("--white=<white> - white level (optional, dafault is maximem for a given bitdepth)\n");
	printf("--black=<black> - black level (optional, default 0)\n");
	printf("--compress - compress image with lossless jpeg\n");
    printf("--channel=<R|G|B> - select channel of an image\n");
    printf("--in_dir=<path> - path for input directory\n");
    printf("--out_dir=<path> - path for output directory\n");
    printf("--mask=<file mask> - mask for input files. Example: *.pgm,*.tif,*.tiff\n");
    return 0;
}

int loadPGM(const char *file, 
	std::vector<unsigned char>& data, 
	unsigned int &width, 
	unsigned &wPitch, 
	unsigned int &height, 
	unsigned &bitsPerChannel) 
{
	FILE *fp = NULL;

	if (FOPEN_FAIL(FOPEN(fp, file, "rb")))
		return 0;

	unsigned int startPosition = 0;

	// check header
	char header[PGMHeaderSize] = { 0 };
	while (header[startPosition] == 0) {
		startPosition = 0;
		if (fgets(header, PGMHeaderSize, fp) == NULL)
			return 0;

		while (isspace(header[startPosition])) startPosition++;
	}

	bool fvHeader = false;
	int strOffset = 2;
	if (strncmp(&header[startPosition], "P5", 2) != 0) 
	{
		return 0;
	}


	// parse header, read maxval, width and height
	unsigned int maxval = 0;
	unsigned int i = 0;
	int readsCount = 0;

	if ((i = SSCANF(&header[startPosition + strOffset], "%u %u %u", &width, &height, &maxval)) == EOF)
		i = 0;

	while (i < 3) 
	{
		if (fgets(header, PGMHeaderSize, fp) == NULL)
			return 0;

		if (header[0] == '#')
			continue;

		if (i == 0) {
			if ((readsCount = SSCANF(header, "%u %u %u", &width, &height, &maxval)) != EOF)
				i += readsCount;
		}
		else if (i == 1) {
			if ((readsCount = SSCANF(header, "%u %u", &height, &maxval)) != EOF)
				i += readsCount;
		}
		else if (i == 2) {
			if ((readsCount = SSCANF(header, "%u", &maxval)) != EOF)
				i += readsCount;
		}
	}
	bitsPerChannel = int(log(maxval + 1) / log(2));

	const int bytePerPixel = _uSnapUp<unsigned>(bitsPerChannel, 8) / 8;

	wPitch = width * bytePerPixel;

	data.resize(wPitch * height);
	unsigned char *d = data.data();

	for (i = 0; i < height; i++) {

		if (fread(&d[i * wPitch], sizeof(unsigned char), width * bytePerPixel, fp) == 0)
			return 0;

		if (bytePerPixel == 2 && !fvHeader) {
			unsigned short *p = reinterpret_cast<unsigned short *>(&d[i * wPitch]);
			for (unsigned int x = 0; x < wPitch / bytePerPixel; x++) {
				unsigned short t = p[x];
				t = (t << 8) | (t >> 8);
				p[x] = t;
			}
		}
	}

	fclose(fp);
	return 1;
}

void TIFFReaderHandler(const char* module, const char* fmt, va_list ap)
{
    // ignore errors and warnings (or handle them your own way)
    (void)(module);
    (void)(fmt);
    (void)(ap);
    //std::cout << fmt << std::endl;
    vprintf(fmt, ap);
    std::cout << std::endl << std::flush;
}

void write2pgm(const std::string& fn,
               const std::vector<unsigned char>& in,
               unsigned w, unsigned h, unsigned p)
{
    std::ofstream ofs(fn, std::ios_base::binary);
    ofs << "P5\n" << w <<" " << h << " " << "4092" << "\n";
    auto tmp = in;
    uint16_t *d = reinterpret_cast<uint16_t*>(tmp.data());
    for(int i = 0; i < tmp.size()/2; ++i){
        d[i] = (d[i] >> 8) | (d[i] << 8);
    }
    ofs.write((char*)tmp.data(), tmp.size());
    ofs.close();
}

template<typename T>
void copy_channel(std::vector<unsigned char>& out,
                  const std::vector<unsigned char>& in,
                  uint32_t& pIn,
                  uint32_t w, uint32_t h, uint32_t chs,
                  const std::string& channel)
{
    uint32_t pOut = w * sizeof(T);
    out.resize(pOut * h);
    int ch = channel == "R"? 0 : (channel == "G"? 1: 2);
    for(int row = 0; row < h; ++row){
        const T* ptrI = reinterpret_cast<const T*>(in.data()  + pIn  * row);
        T* ptrO = reinterpret_cast<T*>(out.data() + pOut * row);
        for(int x = 0; x < w; ++x){
            ptrO[x] = ptrI[x * chs + ch];
        }
    }
    pIn = pOut;
}

int loadTIFF(const char *file,
             const std::string channel,
             std::vector<unsigned char>& _data,
             unsigned int &width,
             unsigned &wPitch,
             unsigned int &height,
             unsigned &bitsPerChannel)
{
    TIFFSetWarningHandler(TIFFReaderHandler);
    TIFFSetErrorHandler(TIFFReaderHandler);

    auto tif = TIFFOpen(file, "r");

    if(tif == nullptr)
        return false;

    width = 0;
    height = 0;
    uint32_t sampleSize = 0;
    uint32_t samples = 0;
    uint32_t config = 0;
    tdata_t buf = nullptr;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &sampleSize);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);

    if(config != PLANARCONFIG_CONTIG)
    {
        TIFFClose(tif);
        return false;
    }

    if(/*samples != 1 || */(sampleSize != 8 /*&& sampleSize != 12*/ && sampleSize!= 16))
    /// removed because some tiff files return samples == 0
    {
        TIFFClose(tif);
        return false;
    }
    int rowSize = int(TIFFScanlineSize(tif));
    buf = _TIFFmalloc(rowSize);
    if(buf == nullptr)
    {
        TIFFClose(tif);
        return false;
    }

    wPitch = width * sizeof(unsigned short) * samples;

    std::vector<unsigned char> data;

    data.resize(wPitch * height);
    //auto mSize = wPitch * height;

    short* dst = (short*)data.data();

    if(sampleSize == 8)
    {
        for(uint32_t row = 0; row < height; row++)
        {
            TIFFReadScanline(tif, buf, row);
            auto* src = static_cast<unsigned char*>(buf);
            for(unsigned x = 0; x < width * samples; x++)
            {
                dst[0] = src[0];
                dst+=1;
                src+=1;
            }
        }
    }
    else if(sampleSize == 16)
    {
        for(uint32_t row = 0; row < height; row++)
        {
            TIFFReadScanline(tif, buf, row);
            memcpy(dst, buf, wPitch);
            dst += width * samples;
        }
    }
    bitsPerChannel = sampleSize;

    if(buf){
        _TIFFfree(buf);
    }

    if(samples == 1){
        _data = data;
    }else{
        if(sampleSize == 8){
            copy_channel<uint8_t>(_data, data, wPitch, width, height, samples, channel);
        }else if(sampleSize == 16){
            copy_channel<uint16_t>(_data, data, wPitch, width, height, samples, channel);
            //write2pgm(std::string(file) + "_test.pgm", _data, width, height, wPitch);
        }
    }

    TIFFClose(tif);
    return 1;
}

struct Args{
    std::string inputFile;
    std::string outputFile;
    std::string in_dir;
    std::string out_dir;
    std::string mask{"*.pgm,*.tif,*.tiff"};
    std::string dcpFile;
    std::vector<float> wp = std::vector<float>(3);
    bool compression = false;
    bool isMonochrome = false;

    int blackLevel = -1;
    int whiteLevel = -1;

    uint16_t bayerType = 1; //RGGB

    std::string channel{"R"};
};

class Worker{
public:
    unsigned int width = 0;
    unsigned wPitch = 0;
    unsigned int height = 0;
    unsigned bitsPerChannel = 0;
    std::vector<unsigned char> pgmData;

    int compute(const Args &args)
    {
        using namespace std;

        if(args.in_dir.empty() || args.out_dir.empty()){
            if(args.inputFile.empty() || args.outputFile.empty()){
                usagePrint();
                return -1;
            }
            if(auto res = loadFromFile(args.inputFile, args) != 0){
                return res;
            }
            save2dng(args.outputFile, args);
        }else{
            packedWork(args);
        }

        return 0;
    }

    int loadFromFile(const std::string &inputFile, const Args &args){
        auto inpF = str2lower(inputFile);

        if(endsWith(inpF, ".tif") || endsWith(inpF, ".tiff")){
            if(loadTIFF(inputFile.c_str(), args.channel, pgmData, width, wPitch, height, bitsPerChannel)  != 1)
            {
                printf("File %s could not be loaded\n", args.inputFile.c_str());
                return 1;
            }

        }else{
            if(loadPGM(inputFile.c_str(), pgmData, width, wPitch, height, bitsPerChannel)  != 1)
            {
                printf("File %s could not be loaded\n", args.inputFile.c_str());
                return 1;
            }
        }
        return 0;
    }

    void save2dng(const std::string& outputFile, const Args &args){

        // SETTINGS: BAYER PATTERN
        uint8_t colorPlanes = 1;
        uint8_t colorChannels = args.isMonochrome ? 1 : 3;

        // SETTINGS: Whitebalance, Orientation "normal"
        dng_orientation orient = dng_orientation::Normal();

        // SETTINGS: Names
        const char* makeerStr = "Fastvideo";
        const char* cameraModelStr = "PGM to DNG";
        const char* profileName = cameraModelStr;
        const char* profileCopyrightStr = makeerStr;

        // Calculate bit limit
        uint16_t bitLimit = 0x01 << bitsPerChannel;

        unsigned short* imageData = (unsigned short*)pgmData.data() ;

        // Create DNG
        dng_host DNGHost;

        DNGHost.SetSaveDNGVersion(dngVersion_SaveDefault);
        DNGHost.SetSaveLinearDNG(false);

        dng_rect imageBounds(height, width);
        AutoPtr<dng_image> image(DNGHost.Make_dng_image(imageBounds, colorPlanes, bitsPerChannel == 8 ? ttByte : ttShort));
        dng_pixel_buffer buffer;

        buffer.fArea = imageBounds;
        buffer.fPlane = 0;
        buffer.fPlanes = 1;
        buffer.fRowStep = buffer.fPlanes * width;
        buffer.fColStep = buffer.fPlanes;
        buffer.fPlaneStep = 1;
        buffer.fPixelType = bitsPerChannel == 8 ? ttByte : ttShort;
        buffer.fPixelSize = bitsPerChannel == 8 ? TagTypeSize(ttByte) : TagTypeSize(ttShort);
        buffer.fData = pgmData.data();

        image->Put(buffer);

        AutoPtr<dng_negative> negative(DNGHost.Make_dng_negative());

        negative->SetModelName(cameraModelStr);
        negative->SetLocalName(cameraModelStr);
        if(!args.isMonochrome)
        {
            negative->SetColorKeys(colorKeyRed, colorKeyGreen, colorKeyBlue);
            negative->SetBayerMosaic(args.bayerType);

            dng_vector cameraNeutral(3);
            cameraNeutral[0] = args.wp.at(0);
            cameraNeutral[1] = args.wp.at(1);
            cameraNeutral[2] = args.wp.at(2);
            negative->SetCameraNeutral(cameraNeutral);

            // Add camera profile to negative
            AutoPtr<dng_camera_profile> profile(new dng_camera_profile);
            dng_file_stream profileStream(args.dcpFile.c_str());
            if (profile->ParseExtended(profileStream))
                negative->AddProfile(profile);
        }

        negative->SetColorChannels(colorChannels);

        // Set linearization table
        //if (compression)
        //{
        //	std::vector<unsigned short> logLut;
        //	std::vector<unsigned short> expLut;

        //	int nEntries = 1 << bitsPerChannel;

        //	logLut.resize(nEntries);
        //	expLut.resize(nEntries);
        //	//y = -41.45 * (1 - exp(0.00112408 * x))
        //	//x = log(1. + y / 41.45) / 0.00112408

        //	//y = -0.1100386 - (0.04655303 / 0.001124336) * (1 - exp(0.001124336 * x))
        //	//y = -0.1100386 - 41,4049092 * (1 - exp(0.001124336 * x))

        //	for (int j = 0; j < nEntries; j++)
        //	{
        //		logLut[j] = (unsigned short)(log(1. + j / 41.45) / 0.00112408);
        //		expLut[j] = (unsigned short)(-41.45 * (1 - exp(0.00112408 * j)));
        //	}
        //	AutoPtr<dng_memory_block> linearizetionCurve(DNGHost.Allocate(nEntries * sizeof(unsigned short)));
        //	for (int32 i = 0; i < nEntries; i++)
        //	{
        //		uint16 *pulItem = linearizetionCurve->Buffer_uint16() + i;
        //		*pulItem = (uint16)(logLut[i]);
        //	}
        //	negative->SetLinearization(linearizetionCurve);
        //}

        negative->SetBlackLevel(args.blackLevel);
        negative->SetWhiteLevel(args.whiteLevel <= 0 ? (1 << bitsPerChannel) - 1 : args.whiteLevel);

        negative->SetDefaultScale(dng_urational(1, 1), dng_urational(1, 1));
        negative->SetBestQualityScale(dng_urational(1, 1));
        negative->SetDefaultCropOrigin(0, 0);
        negative->SetDefaultCropSize(width, height);
        negative->SetBaseOrientation(orient);

        negative->SetBaselineExposure(0);
        negative->SetNoiseReductionApplied(dng_urational(0, 1));
        negative->SetBaselineSharpness(1);

        // DNG EXIF
        dng_exif *poExif = negative->GetExif();
        poExif->fMake.Set_ASCII(makeerStr);
        poExif->fModel.Set_ASCII(cameraModelStr);
        poExif->fMeteringMode = 2;
        poExif->fExposureBiasValue = dng_srational(0, 0);


        // Write DNG file
        negative->SetStage1Image(image);
        negative->SynchronizeMetadata();
        negative->RebuildIPTC(true);

        dng_file_stream DNGStream(outputFile.c_str(), true);
        AutoPtr<dng_ext_image_writer> imgWriter(new dng_ext_image_writer());
        imgWriter->rawBpp = bitsPerChannel;
        imgWriter->WriteDNGEx(DNGHost, DNGStream, *negative.Get(),
                              negative->Metadata(), 0, dngVersion_SaveDefault,
                              !args.compression);
    }

    void packedWork(const Args args)
    {
        auto files = getFilesWithExtensions(args.in_dir, args.mask);
        if(files.empty()){
            std::cout << "The input directory don't contains files by mask ";
            std::cout <<"\"" << args.mask <<"\"" << std::endl;
            return;
        }
        for(auto file: files){
            auto outputFile = replaceFileExtension(file, "dng");
            if(auto res = loadFromFile(file, args) != 0){
                std::cout << "Can not load file " << file << std::endl;
                continue;
            }
            save2dng(outputFile, args);
        }
    }
};

int main(int argc, char *argv[])
{
	using namespace std;

    Args args;
    Worker worker;

	cxxopts::Options options("PGM2DNG", "PGM to DNG command line converter");

	options
		.allow_unrecognised_options()
		.add_options()
		("in", "Input pgm file", cxxopts::value<std::string>())
		("out", "Output dng file", cxxopts::value<std::string>())
        ("mono", "Write monochrome dng", cxxopts::value<bool>(args.isMonochrome), "0")
		("dcp", "dcp profile", cxxopts::value<std::string>())
		("pattern", "CFA pattern", cxxopts::value<std::string>(), "RGGB")
		("wp", "White point", cxxopts::value<std::vector<float>>())
		("white", "White level", cxxopts::value<int>())
		("black", "Black level", cxxopts::value<int>(), "0")
        ("channel", "Channel", cxxopts::value<std::string>(), "R")
        ("in_dir", "Input directory", cxxopts::value<std::string>())
        ("out_dir", "Output directory", cxxopts::value<std::string>())
        ("mask", "Mask for input files", cxxopts::value<std::string>())
        ("compress", "Compress image to losseless jpeg", cxxopts::value<bool>(args.compression), "0")
		("h,help", "help");

	
	options.parse_positional( { "in", "out", "dcp", "pattern", "wp" } );
	auto result = options.parse(argc, argv);
	
	if (result.count("help") > 0)
	{
		usagePrint();
		exit(0);
	}

    if (result.count("in") > 0)
	{
        args.inputFile = result["in"].as<std::string>();
	}
	else
	{
        // printf("No input file\n");
        // usagePrint();
        // exit(0);
	}

    if(result.count("channel") > 0){
        args.channel = result["channel"].as<std::string>();
    }

	if (result.count("out") > 0)
	{
        args.outputFile = result["out"].as<std::string>();
	}
	else
	{
        // printf("No output file\n");
        // usagePrint();
        // exit(0);
	}

    if (!args.isMonochrome)
	{
		if (result.count("dcp") > 0)
		{
            args.dcpFile = result["dcp"].as<string>();
		}
		else
		{
			printf("No DCP file\n");
			usagePrint();
			exit(0);
		}

		//RGGB 1
		//BGGR 2
		//GBRG 3
		//GRBG 0
		if (result.count("pattern") > 0)
		{
			string str = result["pattern"].as<string>();
			if (str == "RGGB")
                args.bayerType = 1;
			else if (str == "BGGR")
                args.bayerType = 2;
			else if (str == "GBRG")
                args.bayerType = 3;
			else if (str == "GRBG")
                args.bayerType = 0;
			else
			{
				printf("Invalid CFA pattern\n");
				usagePrint();
				exit(0);
			}
		}
		else
		{
			printf("No CFA pattern\n");
			usagePrint();
			exit(0);
		}

		if (result.count("wp") > 0)
		{
			const auto v = result["wp"].as<std::vector<float>>();
			if (v.size() != 3)
			{
				printf("White point should contain 3 values\n");
				usagePrint();
				exit(0);
			}

			float max = *max_element(v.begin(), v.end());
			for (int i = 0; i < 3; i++)
			{
                args.wp[i] = v.at(i) / max;
			}
		}
		else
		{
			printf("No white point\n");
			usagePrint();
			exit(0);
		}
	}

	if (result.count("black") > 0)
	{
        args.blackLevel = result["black"].as<int>();
	}
	else
        args.blackLevel = 0;

	if (result.count("white") > 0)
	{
        args.whiteLevel = result["white"].as<int>();
	}

    if (result.count("in_dir") > 0)
    {
        args.in_dir = result["in_dir"].as<std::string>();
    }

    if (result.count("out_dir") > 0)
    {
        args.out_dir = result["out_dir"].as<std::string>();
    }

    if (result.count("mask") > 0)
    {
        args.mask = result["mask"].as<std::string>();
    }

    worker.compute(args);

	return 0;
}

