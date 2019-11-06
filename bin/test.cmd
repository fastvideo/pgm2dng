echo Creating uncompressed DNGs ...
pgm2dng.exe --in=..\Samples\A011_10250317_C116_8.pgm --out A011_10250317_C116_8.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797
pgm2dng.exe --in=..\Samples\A011_10250317_C116_10.pgm --out A011_10250317_C116_10.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797
pgm2dng.exe --in=..\Samples\A011_10250317_C116_12.pgm --out A011_10250317_C116_12.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797
pgm2dng.exe --in=..\Samples\A011_10250317_C116_14.pgm --out A011_10250317_C116_14.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797
pgm2dng.exe --in=..\Samples\A011_10250317_C116_16.pgm --out A011_10250317_C116_16.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797

echo Creating compressed DNGs ...
pgm2dng.exe --in=..\Samples\A011_10250317_C116_8.pgm --out A011_10250317_C116_8_comp.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797 --compress
pgm2dng.exe --in=..\Samples\A011_10250317_C116_10.pgm --out A011_10250317_C116_10_comp.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797 --compress
pgm2dng.exe --in=..\Samples\A011_10250317_C116_12.pgm --out A011_10250317_C116_12_comp.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797 --compress
pgm2dng.exe --in=..\Samples\A011_10250317_C116_14.pgm --out A011_10250317_C116_14_comp.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797 --compress
pgm2dng.exe --in=..\Samples\A011_10250317_C116_16.pgm --out A011_10250317_C116_16_comp.dng --dcp=..\Samples\BMPCC-4K-StdA-D65.dcp --pattern=GBRG  --wp=0.797595,1,0.831797 --compress