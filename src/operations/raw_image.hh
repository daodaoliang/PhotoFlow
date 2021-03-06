/* 
 */

/*

    Copyright (C) 2014 Ferrero Andrea

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.


 */

/*

    These files are distributed with PhotoFlow - http://aferrero2707.github.io/PhotoFlow/

 */

#ifndef PF_RAW_IMAGE_H
#define PF_RAW_IMAGE_H

#include <string>

#include <vips/vips.h>

#include <glibmm.h>

#include "../base/operation.hh"
#include "../base/processor.hh"
#include "../base/imagepyramid.hh"
#include "../base/photoflow.hh"
#include "../base/exif_data.hh"
#include "../base/array2d.hh"

#include "fast_demosaic.hh"

//#define PF_USE_LIBRAW
#define PF_USE_RAWSPEED
//#define PF_USE_DCRAW_RT

#ifdef PF_USE_LIBRAW
#include <libraw/libraw.h>

typedef libraw_data_t dcraw_data_t;
#endif

#ifdef PF_USE_RAWSPEED
#include "rawspeed/src/librawspeed/RawSpeed-API.h"

struct dcraw_iparams_t
{
  unsigned int filters;
  int xtrans_uncropped[6][6];
  int xtrans[6][6];
};

struct dcraw_color_data_t
{
  unsigned black;
  unsigned cblack[4];
  float maximum;
  float cam_mul[4];
  float cam_xyz[4][3];
  double ca_fitparams[3][2][16];
};

struct dcraw_sizes_data_t
{
  unsigned short int raw_height, raw_width;
  unsigned short int height, width;
  unsigned short int top_margin, left_margin;
  int flip;
};

struct dcraw_data_t
{
  dcraw_iparams_t idata;
	dcraw_color_data_t color;
	dcraw_sizes_data_t sizes;
};

class ImageMatrices
{

public:
    double rgb_cam[3][3] = {};
    double cam_rgb[3][3] = {};
    double xyz_cam[3][3] = {};
    double cam_xyz[3][3] = {};
};



#endif


namespace PF 
{

bool check_xtrans( unsigned filters );

  class RawImage
  {
    int nref;
    std::string file_name;
    std::string file_name_real;
		std::string cache_file_name;
		std::string cache_file_name2;

    dcraw_data_t dcraw_data;
    dcraw_data_t* pdata;

    int iwidth, iheight, crop_x, crop_y;

	  float c_black[4];

	  ImageMatrices imatrices;

#ifdef PF_USE_RAWSPEED
		rawspeed::CameraMetaData *meta;
#endif

		// VipsImages storing the raw data, one band for the pixel values and a nother for the colors
    VipsImage* image;
		// VipsImage storing the dark frame data (if available)
    VipsImage* df_image;
		// VipsImage storing the flat field data (if available)
    VipsImage* ff_image;
		// Demosaiced image
    VipsImage* demo_image;
    PF::ProcessorBase* fast_demosaic;

    int* raw_hist;

    exif_data_t exif_data;

    Array2D<float> rawData;  // holds preprocessed pixel values, rowData[i][j] corresponds to the ith row and jth column
    //RawMatrix rawData;  // holds preprocessed pixel values, rowData[i][j] corresponds to the ith row and jth column
    // Result of CA auto-correction
    double fitparams[3][2][16];

    PF::ImagePyramid pyramid;

    int LinEqSolve(int nDim, double* pfMatr, double* pfVect, double* pfSolution);
    void CA_correct_RT();

    bool load_rawspeed();
    bool load_rawtherapee();

  public:
    RawImage( const std::string name );
    ~RawImage();

    void ref() { nref += 1; }
    void unref() { nref -= 1; }
    int get_nref() { return nref; }

    static void inverse33 (const double (*coeff)[3], double (*icoeff)[3]);

    bool is_xtrans() { return check_xtrans( dcraw_data.idata.filters ); }

    std::string get_file_name() { return file_name_real; }

    unsigned int BL(const int row, const int col)
    {
      return ((((row+dcraw_data.sizes.top_margin) & 1) << 1) + ((col+dcraw_data.sizes.left_margin) & 1));
    }

    unsigned FC (unsigned row, unsigned col) const
    {
      //return( dcraw_data.idata.filters >> ((((row+dcraw_data.sizes.top_margin) << 1 & 14) +
      //    ((col+dcraw_data.sizes.left_margin) & 1)) << 1) & 3 );
      return( dcraw_data.idata.filters >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3 );
    }

    unsigned FC_xtrans (unsigned row, unsigned col) const
    {
      return( dcraw_data.idata.xtrans[(row)%6][(col)%6] );
    }

    VipsImage* get_image(unsigned int& level);

    void print_exif( PF::exif_data_t* data );
    void print_exif();
  };

	extern std::map<Glib::ustring, PF::RawImage*> raw_images;

}

#endif
