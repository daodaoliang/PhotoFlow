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
#include <assert.h>
#include <string.h>

#include "../base/pf_mkstemp.hh"
#include "raster_image.hh"



PF::RasterImage::RasterImage( const std::string f ):
nref(1), file_name( f ),
image( NULL )
{
  if( file_name.empty() ) return;

  file_name_real = file_name;
  int ifd = open( file_name_real.c_str(), O_RDONLY );
  if( ifd < 0 ) {
    char* fullpath = strdup( file_name_real.c_str() );
    gchar* fname = g_path_get_basename( fullpath );
    gchar* fname2 = g_build_filename( PF::PhotoFlow::Instance().get_current_image_dir().c_str(),
        fname, NULL );
    std::cout<<"RasterImage::RasterImage(): file \""<<file_name_real<<"\" not found"<<std::endl;
    std::cout<<"                      trying with \""<<fname2<<"\""<<std::endl;
    ifd = open( fname2, O_RDONLY );
    if( ifd < 0 ) {
      std::cout<<"RasterImage::RasterImage(): \""<<file_name<<"\" not found"<<std::endl;
      return;
    } else {
      close(ifd);
    }
    file_name_real = fname2;
    g_free( fname );
    g_free( fname2 );
  } else {
    close(ifd);
  }
//#ifndef NDEBUG
  std::cout<<"RasterImage::RasterImage(): opening file \""<<file_name_real<<"\""<<std::endl;
//#endif

  // Create VipsImage from given file
#if VIPS_MAJOR_VERSION < 8 && VIPS_MINOR_VERSION < 40
  image = vips_image_new_from_file( file_name_real.c_str() );
#else
  image = vips_image_new_from_file( file_name_real.c_str(), NULL );
#endif
  if( !image ) {
    std::cout<<"RasterImage::RasterImage(): Failed to load "<<file_name<<std::endl;
    return;
  }

#ifndef NDEBUG
  std::cout<<"RasterImage::RasterImage(): # of bands="<<image->Bands<<std::endl;
  std::cout<<"RasterImage::RasterImage(): type="<<image->Type<<std::endl;
#endif

  int out_nbands = 0;
  if( (convert_colorspace(image->Type) == PF_COLORSPACE_GRAYSCALE) &&
      (image->Bands > 1) ) {
    out_nbands = 1;
  }
  if( (convert_colorspace(image->Type) == PF_COLORSPACE_RGB) &&
      (image->Bands > 3) ) {
    out_nbands = 3;
  }
  if( (convert_colorspace(image->Type) == PF_COLORSPACE_LAB) &&
      (image->Bands > 3) ) {
    out_nbands = 3;
  }
  if( (convert_colorspace(image->Type) == PF_COLORSPACE_CMYK) &&
      (image->Bands > 4) ) {
    out_nbands = 4;
  }

#ifndef NDEBUG
  std::cout<<"RasterImage::RasterImage(): out_nbands="<<out_nbands<<std::endl;
#endif
  if( out_nbands > 0 ) {
    VipsImage* out;
    if( vips_extract_band( image, &out, 0, "n", out_nbands, NULL ) ) {
      std::cout<<"RasterImage::RasterImage(): vips_extract_band() failed"<<std::endl;
      return;
    }
#ifndef NDEBUG
    std::cout<<"RasterImage::RasterImage(): # of output bands="<<out->Bands<<std::endl;
#endif

    PF_UNREF( image, "RasterImage::RasterImage(): image unref" );
    vips_image_init_fields( out,
        image->Xsize, image->Ysize,
        out_nbands, image->BandFmt,
        image->Coding,
        image->Type,
        1.0, 1.0);
    image = out;
  }
#ifndef NDEBUG
  std::cout<<"RasterImage::RasterImage(): # of output bands="<<image->Bands<<std::endl;
#endif

  // We make a copy of the original image to make sure that custom metadata is not deleted
  VipsImage* image_copy;
  if( vips_copy( image, &image_copy, NULL) ) {
    std::cout<<"RasterImage::RasterImage(): vips_copy() failed for image "<<file_name<<std::endl;
    PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_copy() failure." );
    return;
  }
  PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_copy()." );
  image = image_copy;

  // We read the EXIF data and store it in the image as a custom blob
  PF::exiv2_data_t* exiv2_buf = new PF::exiv2_data_t;
  try
  {
    Exiv2::ExifData::const_iterator pos;
    Exiv2::BasicIo::AutoPtr file (new Exiv2::FileIo (file_name_real));
    //std::unique_ptr<Exiv2::Image> image(Exiv2::ImageFactory::open(file));
    exiv2_buf->image = Exiv2::ImageFactory::open(file);
    assert(exiv2_buf->image.get() != 0);
    exiv2_buf->image->readMetadata();
    bool res = true;

    // EXIF metadata
    Exiv2::ExifData &exifData = exiv2_buf->image->exifData();
    if(!exifData.empty()) {
      Exiv2::ExifData::const_iterator orient_pos = exifData.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
      if( orient_pos != exifData.end() && orient_pos->count() == 1 && orient_pos->size() ) {
        PF::ExifOrientation orientation = (PF::ExifOrientation)orient_pos->toLong(0);
        VipsImage* temp = image;
        switch( orientation ) {
        case PF_EXIF_ORIENTATION_HFLIP:
          if( vips_flip( image, &temp, VIPS_DIRECTION_HORIZONTAL, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_ROT_180:
          if( vips_rot( image, &temp, VIPS_ANGLE_D180, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_VFLIP:
          if( vips_flip( image, &temp, VIPS_DIRECTION_VERTICAL, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_ROT_90_HFLIP:
          if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
          image = temp;
          if( vips_flip( image, &temp, VIPS_DIRECTION_HORIZONTAL, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_ROT_90:
          if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_ROT_90_VFLIP:
          if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
          image = temp;
          if( vips_flip( image, &temp, VIPS_DIRECTION_VERTICAL, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
          image = temp;
          break;
        case PF_EXIF_ORIENTATION_ROT_270:
          if( vips_rot( image, &temp, VIPS_ANGLE_D270, NULL ) ) {
            PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(270) failed." );
            image = NULL;
            return;
          }
          PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(270)." );
          image = temp;
          break;
        default:
          break;
        }

        Exiv2::XmpData& xmp_data = exiv2_buf->image->xmpData();
        exifData["Exif.Image.Orientation"] = static_cast<uint16_t> (PF_EXIF_ORIENTATION_NORMAL);
        xmp_data["Xmp.tiff.Orientation"] = static_cast<uint16_t> (PF_EXIF_ORIENTATION_NORMAL);
      }

      // Erase thumbnail data
      Exiv2::ExifThumb exifThumb(exifData);
      std::string thumbExt = exifThumb.extension();
      if(!thumbExt.empty()) {
        exifThumb.erase();
      }

    }


  }
  catch(Exiv2::AnyError &e)
  {
    std::string s(e.what());
    std::cerr << "[exiv2] " << file_name_real << ": " << s << std::endl;
    //return 1;
  }

  vips_image_set_blob( image, "exiv2-data",
      (VipsCallbackFn) exiv2_free, exiv2_buf, sizeof(PF::exiv2_data_t) );


  /*
  GExiv2Metadata* gexiv2_buf = gexiv2_metadata_new();
  gboolean gexiv2_success = gexiv2_metadata_open_path(gexiv2_buf, file_name_real.c_str(), NULL);
  if( gexiv2_success ) {
    orientation = gexiv2_metadata_get_orientation( gexiv2_buf );
    gexiv2_metadata_set_orientation( gexiv2_buf, GEXIV2_ORIENTATION_NORMAL );
    //std::cout<<"RasterImage::RasterImage(): setting gexiv2-data blob"<<std::endl;
    VipsImage* temp = image;
    switch( orientation ) {
    case GEXIV2_ORIENTATION_HFLIP:
      if( vips_flip( image, &temp, VIPS_DIRECTION_HORIZONTAL, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_ROT_180:
      if( vips_rot( image, &temp, VIPS_ANGLE_D180, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_VFLIP:
      if( vips_flip( image, &temp, VIPS_DIRECTION_VERTICAL, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_ROT_90_HFLIP:
      if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
      image = temp;
      if( vips_flip( image, &temp, VIPS_DIRECTION_HORIZONTAL, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_ROT_90:
      if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_ROT_90_VFLIP:
      if( vips_rot( image, &temp, VIPS_ANGLE_D90, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180) failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(180)." );
      image = temp;
      if( vips_flip( image, &temp, VIPS_DIRECTION_VERTICAL, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip() failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_flip()." );
      image = temp;
      break;
    case GEXIV2_ORIENTATION_ROT_270:
      if( vips_rot( image, &temp, VIPS_ANGLE_D270, NULL ) ) {
        PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(270) failed." );
        image = NULL;
        return;
      }
      PF_UNREF( image, "RasterImage::RasterImage(): image unref after vips_rot(270)." );
      image = temp;
      break;
    default:
      break;
    }
    image = temp;
    vips_image_set_blob( image, "gexiv2-data",
        (VipsCallbackFn) gexiv2_metadata_free, gexiv2_buf,
        sizeof(GExiv2Metadata) );
  }
  */


  PF::exif_read( &exif_data, file_name_real.c_str() );
  void* buf = malloc( sizeof(PF::exif_data_t) );
  if( !buf ) return;
  memcpy( buf, &exif_data, sizeof(PF::exif_data_t) );
  vips_image_set_blob( image, PF_META_EXIF_NAME,
      (VipsCallbackFn) PF::exif_free, buf,
      sizeof(PF::exif_data_t) );

#ifndef NDEBUG
  print_exif();
  print_exif( (PF::exif_data_t*)buf );

  {
    size_t bufsz;
    void* buf;
    if( !vips_image_get_blob( image, PF_META_EXIF_NAME,
        &buf,&bufsz ) ) {
      //std::cout<<"RasterImage::RasterImage(): exif_custom_data found in image("<<image<<")"<<std::endl;
    } else {
      std::cout<<"RasterImage::RasterImage(): exif_custom_data not found in image("<<image<<")"<<std::endl;
    }
  }
#endif

  pyramid.init( image );
}


PF::RasterImage::~RasterImage()
{
  if( image ) PF_UNREF( image, "RasterImage::~RasterImage() image" );
  std::cout<<"RasterImage::~RasterImage() called."<<std::endl;
}


VipsImage* PF::RasterImage::get_image(unsigned int& level)
{
#ifndef NDEBUG
  GType type = vips_image_get_typeof(image, PF_META_EXIF_NAME );
  if( type ) {
    std::cout<<"RasterImage::get_image(): exif_custom_data found in image("<<image<<")"<<std::endl;
    print_exif();
  } else std::cout<<"RasterImage::get_image(): exif_custom_data not found in image("<<image<<")"<<std::endl;
#endif

  /*
#ifdef DO_WARNINGS
#warning "RasterImage::get_image(): refreshing of exif metadata needed. This is not normal!"
#endif
  void* buf = malloc( sizeof(exif_data_t) );
  if( !buf ) return NULL;
  memcpy( buf, &exif_data, sizeof(exif_data_t) );
  vips_image_set_blob( image, PF_META_EXIF_NAME,
           (VipsCallbackFn) PF::exif_free, buf,
           sizeof(exif_data_t) );

#ifndef NDEBUG
 print_exif();
 GType type = vips_image_get_typeof(image, PF_META_EXIF_NAME );
 if( type ) std::cout<<"RasterImage::get_image(): exif_custom_data found in image("<<image<<") after set_blob"<<std::endl;
 else std::cout<<"RasterImage::get_image(): exif_custom_data not found in image("<<image<<") after set_blob"<<std::endl;
#endif
   */
  /*
  if( level == 0 ) {
    PF_REF( image, "RasterImage()::get_image(): level 0 ref");
    return image;
  }
   */
  PF::PyramidLevel* plevel = pyramid.get_level( level );
  if( plevel ) {
    return plevel->image;
  }
  return NULL;
}


void PF::RasterImage::print_exif(  PF::exif_data_t* data )
{
  std::cout<<"RasterImage: (data)"<<std::endl
      <<"      camera maker: "<<data->exif_maker<<std::endl
      <<"      model: "<<data->exif_model<<std::endl
      <<"      lens: "<<data->exif_lens<<std::endl;
}

void PF::RasterImage::print_exif()
{
  std::cout<<"RasterImage:"<<std::endl
      <<"      camera maker: "<<exif_data.exif_maker<<std::endl
      <<"      model: "<<exif_data.exif_model<<std::endl
      <<"      lens: "<<exif_data.exif_lens<<std::endl;
  size_t bufsz;
  PF::exif_data_t* buf;
  if( !vips_image_get_blob( image, PF_META_EXIF_NAME,
      (void**)&buf,&bufsz ) ) {
    if( bufsz == sizeof(PF::exif_data_t) ) {
      std::cout<<"RasterImage: (embedded)"<<std::endl
          <<"      camera maker: "<<buf->exif_maker<<std::endl
          <<"      model: "<<buf->exif_model<<std::endl
          <<"      lens: "<<buf->exif_lens<<std::endl;
    } else {
      std::cout<<"RasterImage: wrong exif_custom_data size in image("<<image<<") before set_blob"<<std::endl;
    }
  } else {
    std::cout<<"RasterImage: exif_custom_data not found in image("<<image<<") before set_blob"<<std::endl;
  }
}


void PF::RasterImage::print_icc()
{
  size_t bufsz;
  void* buf;
  if( !vips_image_get_blob( image, VIPS_META_ICC_NAME,
      (void**)&buf,&bufsz ) ) {
    std::cout<<"RasterImage: ICC profile found"<<std::endl;
  } else {
    std::cout<<"RasterImage: ICC profile not found"<<std::endl;
  }
}


void PF::RasterImage::print_icc( VipsImage* img)
{
  size_t bufsz;
  void* buf;
  if( !vips_image_get_blob( img, VIPS_META_ICC_NAME,
      (void**)&buf,&bufsz ) ) {
    std::cout<<"RasterImage: ICC profile found in img"<<std::endl;
  } else {
    std::cout<<"RasterImage: ICC profile not found in img"<<std::endl;
  }
}


std::map<Glib::ustring, PF::RasterImage*> PF::raster_images;


