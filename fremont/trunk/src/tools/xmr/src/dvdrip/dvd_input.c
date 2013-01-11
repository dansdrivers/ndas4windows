
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <io.h>

#include "dvd_udf.h"
#include "dvd_input.h"
#include "../inc/dvd_reader.h"
#include "../inc/dvdcss.h"






/////////////////////////////////////////////////////////////////////////
//		
//		DVD INPUT VARIABLE
//
/////////////////////////////////////////////////////////////////////////
/* For libdvdcss */
typedef struct dvdcss_s *dvdcss_handle;

typedef dvdcss_handle (*MyDVDcss_open)  (const char *);
typedef	int           (*MyDVDcss_close) (dvdcss_handle);
typedef	int           (*MyDVDcss_seek)  (dvdcss_handle, int, int);
typedef	int           (*MyDVDcss_title) (dvdcss_handle, int); 
typedef	int           (*MyDVDcss_read)  (dvdcss_handle, void *, int, int);
typedef	char *        (*MyDVDcss_error) (dvdcss_handle);

MyDVDcss_open	DVDcss_open;
MyDVDcss_close	DVDcss_close;
MyDVDcss_seek	DVDcss_seek;
MyDVDcss_title	DVDcss_title;
MyDVDcss_read	DVDcss_read;
MyDVDcss_error	DVDcss_error;

/* The DVDinput handle, add stuff here for new input methods. */
struct dvd_input_s {
  /* libdvdcss handle */
  dvdcss_handle dvdcss;
  
  /* dummy file input */
  int fd;
};

//////////////////////////////////////////////////////////////////////////////
//
//		DVD READ VARIABLE
//
///////////////////////////////////////////////////////////////////////////////
struct dvd_reader_s {
    /* Basic information. */
    int isImageFile;
  
    /* Hack for keeping track of the css status. 
     * 0: no css, 1: perhaps (need init of keys), 2: have done init */
    int css_state;
    int css_title; /* Last title that we have called DVDinpute_title for. */

    /* Information required for an image file. */
    dvd_input_t dev;

    /* Information required for a directory path drive. */
    char *path_root;
};

struct dvd_file_s {
    /* Basic information. */
    dvd_reader_t *dvd;
  
    /* Hack for selecting the right css title. */
    int css_title;

    /* Information required for an image file. */
    uint32_t lb_start;
    uint32_t seek_pos;

    /* Information required for a directory path drive. */
    size_t title_sizes[ 9 ];
    dvd_input_t title_devs[ 9 ];

    /* Calculated at open-time, size in blocks. */
    ssize_t filesize;
};



///////////////////////////////////////////////////////////////////////////////
//
//		DVD INPUT MODULE
//
///////////////////////////////////////////////////////////////////////////////
/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t css_open(const char *target)
{
  dvd_input_t dev;
  
  /* Allocate the handle structure */
  dev = (dvd_input_t) malloc(sizeof(dvd_input_t));
  if(dev == NULL) {
    fprintf(stderr, "Dvdmodule: Could not allocate memory.\n");
    return NULL;
  }
  
  /* Really open it with libdvdcss */
  dev->dvdcss = DVDcss_open(target);
  if(dev->dvdcss == 0) {
    fprintf(stderr, "Dvdmodule: Could not open device with libdvdcss.\n");
    free(dev);
    return NULL;
  }
  
  return dev;
}

/**
 * return the last error message
 */
static char *css_error(dvd_input_t dev)
{
  return DVDcss_error(dev->dvdcss);
}

/**
 * seek into the device.
 */
static int css_seek(dvd_input_t dev, int blocks, int flags)
{
  return DVDcss_seek(dev->dvdcss, blocks, flags);
}

/**
 * set the block for the begining of a new title (key).
 */
static int css_title(dvd_input_t dev, int block)
{
  return DVDcss_title(dev->dvdcss, block);
}

/**
 * read data from the device.
 */
static int css_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  return DVDcss_read(dev->dvdcss, buffer, blocks, flags);
}

/**
 * close the DVD device and clean up the library.
 */
static int css_close(dvd_input_t dev)
{
  int ret;

  ret = DVDcss_close(dev->dvdcss);

  if(ret < 0)
    return ret;

  free(dev);

  return 0;
}






/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t file_open(const char *target)
{
  dvd_input_t dev;
  errno_t err = 0;
  /* Allocate the library structure */
  dev = (dvd_input_t) malloc(sizeof(dvd_input_t));
  if(dev == NULL) {
    fprintf(stderr, "Dvdmodule: Could not allocate memory.\n");
    return NULL;
  }
  
  /* Open the device */
  err = _sopen_s(&dev->fd, target, O_RDONLY, _SH_DENYNO, (_S_IREAD | _S_IWRITE ) );
  if(err != 0) {
	perror("Dvdmodule: Could not open input");
    free(dev);
    return NULL;
  }
  
  return dev;
}

/**
 * return the last error message
 */
static char *file_error(dvd_input_t dev)
{
  /* use strerror(errno)? */
  return "unknown error";
}

/**
 * seek into the device.
 */
static int file_seek(dvd_input_t dev, int blocks, int flags)
{
  off_t pos;

  pos = _lseeki64(dev->fd, (off_t)(blocks *DVD_VIDEO_LB_LEN), SEEK_SET);
  if(pos < 0) {
      return (int)pos;
  }
  /* assert pos % DVD_VIDEO_LB_LEN == 0 */
  return (int) (pos / DVD_VIDEO_LB_LEN);
}

/**
 * set the block for the begining of a new title (key).
 */
static int file_title(dvd_input_t dev, int block)
{
  return -1;
}

/**
 * read data from the device.
 */
static int file_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  size_t len;
  ssize_t ret;
  
  len = (size_t)blocks * DVD_VIDEO_LB_LEN;
  
  while(len > 0) {
    
    ret = _read(dev->fd, buffer, len);
    
    if(ret < 0) {
      /* One of the reads failed, too bad.  We won't even bother
       * returning the reads that went ok, and as in the posix spec
       * the file postition is left unspecified after a failure. */
      return ret;
    }
    
    if(ret == 0) {
      /* Nothing more to read.  Return the whole blocks, if any, that we got.
	 and adjust the file possition back to the previous block boundary. */
      size_t bytes = (size_t)blocks * DVD_VIDEO_LB_LEN - len;
      off_t over_read = (bytes % DVD_VIDEO_LB_LEN);
      /*off_t pos =*/ _lseeki64(dev->fd, -(over_read*DVD_VIDEO_LB_LEN), SEEK_CUR);
      /* should have pos % 2048 == 0 */
      return (int) (bytes / DVD_VIDEO_LB_LEN);
    }
    
    len -= ret;
  }

  return blocks;
}

/**
 * close the DVD device and clean up.
 */
static int file_close(dvd_input_t dev)
{
  int ret;

  ret = _close(dev->fd);

  if(ret < 0)
    return ret;

  free(dev);

  return 0;
}


/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int DVDInputSetup(void)
{

	DVDcss_open = dvdcss_open;
	DVDcss_close = dvdcss_close;
    DVDcss_title = dvdcss_title;
    DVDcss_seek = dvdcss_seek;
    DVDcss_read = dvdcss_read;
    DVDcss_error = dvdcss_error;
      
	if(!DVDcss_open  || !DVDcss_close || !DVDcss_title || !DVDcss_seek
	      || !DVDcss_read || !DVDcss_error ) {
      fprintf(stderr,  "Dvdmodule: Can Open DvdCssLib"
	      "this shouldn't happen !\n");
    
		/* libdvdcss replacement functions */
		DVDinput_open  = file_open;
		DVDinput_close = file_close;
		DVDinput_seek  = file_seek;
		DVDinput_title = file_title;
		DVDinput_read  = file_read;
		DVDinput_error = file_error;
		return 0;
    }else{
		/* libdvdcss wraper functions */
		DVDinput_open  = css_open;
		DVDinput_close = css_close;
		DVDinput_seek  = css_seek;
		DVDinput_title = css_title;
		DVDinput_read  = css_read;
		DVDinput_error = css_error;
		return 1;
	}
}


////////////////////////////////////////////////////////////////////
//	
//		DVD READER MODULE
//
////////////////////////////////////////////////////////////////////
/* Loop over all titles and call dvdcss_title to crack the keys. */
int initAllCSSKeys( dvd_reader_t *dvd )
{
    time_t all_s, all_e;
    time_t t_s, t_e;
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    uint32_t start, len;
    int title;
	
    fprintf( stderr, "\n" );
    fprintf( stderr, "Dvdmodule: Attempting to retrieve all CSS keys\n" );
    fprintf( stderr, "Dvdmodule: This can take a _long_ time, "
	     "please be patient\n\n" );
	
    time(&all_s);
	
    for( title = 0; title < 100; title++ ) {
	time( &t_s);
	if( title == 0 ) {
	    sprintf_s( filename,MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VIDEO_TS.VOB" );
	} else {
	    sprintf_s( filename,MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 0 );
	}
	start = UDFFindFile( dvd, filename, &len );
	if( start != 0 && len != 0 ) {
	    /* Perform CSS key cracking for this title. */
	    fprintf( stderr, "Dvdmodule: Get key for %s at 0x%08x\n", 
		     filename, start );
	    if( DVDinput_title( dvd->dev, (int)start ) < 0 ) {
		fprintf( stderr, "Dvdmodule: Error cracking CSS key for %s (0x%08x)\n", filename, start);
	    }
	    time( &t_e);
	    fprintf( stderr, "Dvdmodule: Elapsed time %ld\n",  
		     (long int) t_e - t_s );
	}
	    
	if( title == 0 ) continue;
	    
	time( &t_s);
	sprintf_s( filename, MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 1 );
	start = UDFFindFile( dvd, filename, &len );
	if( start == 0 || len == 0 ) break;
	    
	/* Perform CSS key cracking for this title. */
	fprintf( stderr, "Dvdmodule: Get key for %s at 0x%08x\n", 
		 filename, start );
	if( DVDinput_title( dvd->dev, (int)start ) < 0 ) {
	    fprintf( stderr, "Dvdmodule: Error cracking CSS key for %s (0x%08x)!!\n", filename, start);
	}
	time( &t_e);
	fprintf( stderr, "Dvdmodule: Elapsed time %ld\n",  
		 (long int) t_e - t_s );
    }
    title--;
    
    fprintf( stderr, "Dvdmodule: Found %d VTS's\n", title );
    time(&all_e);
    fprintf( stderr, "Dvdmodule: Elapsed time %ld\n",  
	     (long int) all_e - all_s );
    
    return 0;
}



/**
 * Open a DVD image or block device file.
 */
static dvd_reader_t *DVDOpenImageFile( const char *location, int have_css )
{
    dvd_reader_t *dvd;
    dvd_input_t dev;
    
    dev = DVDinput_open( location );
    if( !dev ) {
	fprintf( stderr, "Dvdmodule: Can't open %s for reading\n", location );
	return 0;
    }

    dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
    if( !dvd ) return 0;
    dvd->isImageFile = 1;
    dvd->dev = dev;
    dvd->path_root = 0;
    
    if( have_css ) {
      /* Only if DVDCSS_METHOD = title, a bit if it's disc or if
       * DVDCSS_METHOD = key but region missmatch. Unfortunaly we
       * don't have that information. */
    
      dvd->css_state = 1; /* Need key init. */
    }
    
    return dvd;
}







dvd_reader_t *DVDOpen( const char *path )
{
    struct stat fileinfo;
    int ret, have_css;
	int b_file = 0;
    if( !path ) return 0;

	fprintf(stderr, "call DVDOpen\n");
    ret = stat( path, &fileinfo );
    if( ret < 0 ) {
		/* If we can't stat the file, give up */
		fprintf( stderr, "Dvdmodule: Can't stat %s\n", path );
		perror("");
    }
	fprintf(stderr, "sucess stat\n");

    /* Try to open libdvdcss or fall back to standard functions */
    have_css = DVDInputSetup();

	b_file = !path[0] || path[1] != ':' ||path[2];
    /* First check if this is a block/char device or a file*/
    if( (b_file == 0) ||( ret && ((fileinfo.st_mode & _S_IFMT ) & _S_IFREG ))  ) {
		/**
		* Block devices and regular files are assumed to be DVD-Video images.
		*/
		return DVDOpenImageFile( path, have_css );
    } else {
		return NULL;
	}
}

void DVDClose( dvd_reader_t *dvd )
{
    if( dvd ) {
        if( dvd->dev ) DVDinput_close( dvd->dev );
        if( dvd->path_root ) free( dvd->path_root );
        free( dvd );
        dvd = 0;
    }
}

/**
 * Open an unencrypted file on a DVD image file.
 */
static dvd_file_t *DVDOpenFileUDF( dvd_reader_t *dvd, char *filename )
{
    uint32_t start, len;
    dvd_file_t *dvd_file;

    start = UDFFindFile( dvd, filename, &len );
    if( !start ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = start;
    dvd_file->seek_pos = 0;
    memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
    memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
    dvd_file->filesize = len / DVD_VIDEO_LB_LEN;

    return dvd_file;
}

/**
 * Searches for <file> in directory <path>, ignoring case.
 * Returns 0 and full filename in <filename>.
 *     or -1 on file not found.
 *     or -2 on path not found.
 */



static dvd_file_t *DVDOpenVOBUDF( dvd_reader_t *dvd, int title, int menu )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    uint32_t start, len;
    dvd_file_t *dvd_file;

    if( title == 0 ) {
        sprintf_s( filename, MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VIDEO_TS.VOB" );
    } else {
        sprintf_s( filename, MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
    }
    start = UDFFindFile( dvd, filename, &len );
    if( start == 0 ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    /*Hack*/ dvd_file->css_title = title << 1 | menu;
    dvd_file->lb_start = start;
    dvd_file->seek_pos = 0;
    memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
    memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
    dvd_file->filesize = len / DVD_VIDEO_LB_LEN;

    /* Calculate the complete file size for every file in the VOBS */
    if( !menu ) {
        int cur;

        for( cur = 2; cur < 10; cur++ ) {
            sprintf_s( filename, MAX_UDF_FILE_NAME_LEN,"/VIDEO_TS/VTS_%02d_%d.VOB", title, cur );
            if( !UDFFindFile( dvd, filename, &len ) ) break;
            dvd_file->filesize += len / DVD_VIDEO_LB_LEN;
        }
    }
    
    if( dvd->css_state == 1 /* Need key init */ ) {
        initAllCSSKeys( dvd );
	dvd->css_state = 2;
    }
    /*    
    if( DVDinput_seek( dvd_file->dvd->dev, 
		       (int)start, DVDINPUT_SEEK_KEY ) < 0 ) {
        fprintf( stderr, "Dvdmodule: Error cracking CSS key for %s\n",
		 filename );
    }
    */
    
    return dvd_file;
}



dvd_file_t *DVDOpenFile( dvd_reader_t *dvd, int titlenum, 
			 dvd_read_domain_t domain )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];

    switch( domain ) {
    case DVD_READ_INFO_FILE:
        if( titlenum == 0 ) {
            sprintf_s( filename,MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VIDEO_TS.IFO" );
        } else {
            sprintf_s( filename,MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VTS_%02i_0.IFO", titlenum );
        }
        break;
    case DVD_READ_INFO_BACKUP_FILE:
        if( titlenum == 0 ) {
            sprintf_s( filename, MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VIDEO_TS.BUP" );
        } else {
            sprintf_s( filename, MAX_UDF_FILE_NAME_LEN, "/VIDEO_TS/VTS_%02i_0.BUP", titlenum );
        }
        break;
    case DVD_READ_MENU_VOBS:
        if( dvd->isImageFile ) {
            return DVDOpenVOBUDF( dvd, titlenum, 1 );
        } else {
            return 0;
        }
        break;
    case DVD_READ_TITLE_VOBS:
        if( titlenum == 0 ) return 0;
        if( dvd->isImageFile ) {
            return DVDOpenVOBUDF( dvd, titlenum, 0 );
        } else {
            return 0;
        }
        break;
    default:
        fprintf( stderr, "Dvdmodule: Invalid domain for file open.\n" );
        return 0;
    }
    
    if( dvd->isImageFile ) {
        return DVDOpenFileUDF( dvd, filename );
    } else {
        return 0;
    }
}

void DVDCloseFile( dvd_file_t *dvd_file )
{
    int i;

    if( dvd_file ) {
        if( dvd_file->dvd->isImageFile ) {
	    ;
	} else {
            for( i = 0; i < 9; ++i ) {
                if( dvd_file->title_devs[ i ] ) {
                    DVDinput_close( dvd_file->title_devs[i] );
                }
            }
        }

        free( dvd_file );
        dvd_file = 0;
    }
}

/* Internal, but used from dvd_udf.c */
int DVDReadBlocksUDFRaw( dvd_reader_t *device, uint32_t lb_number,
			 size_t block_count, unsigned char *data, 
			 int encrypted )
{
   int ret;

   if( !device->dev ) {
     	fprintf( stderr, "Dvdmodule: Fatal error in block read.\n" );
	return 0;
   }

   ret = DVDinput_seek( device->dev, (int) lb_number, DVDINPUT_NOFLAGS );
   if( ret != (int) lb_number ) {
     	fprintf( stderr, "Dvdmodule: Can't seek to block %u\n", lb_number );
	return 0;
   }

   return DVDinput_read( device->dev, (char *) data, 
			 (int) block_count, encrypted );
}

/* This is using a single input and starting from 'dvd_file->lb_start' offset.
 *
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read. */
static int DVDReadBlocksUDF( dvd_file_t *dvd_file, uint32_t offset,
			     size_t block_count, unsigned char *data,
			     int encrypted )
{
    return DVDReadBlocksUDFRaw( dvd_file->dvd, dvd_file->lb_start + offset,
				block_count, data, encrypted );
}

/* This is using possibly several inputs and starting from an offset of '0'.
 *
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read. */
static int DVDReadBlocksPath( dvd_file_t *dvd_file, unsigned int offset,
			      size_t block_count, unsigned char *data,
			      int encrypted )
{
    int i;
    int ret, ret2, off;

    ret = 0;
    ret2 = 0;
    for( i = 0; i < 9; ++i ) {
      if( !dvd_file->title_sizes[ i ] ) return 0; /* Past end of file */

        if( offset < dvd_file->title_sizes[ i ] ) {
            if( ( offset + block_count ) <= dvd_file->title_sizes[ i ] ) {
		off = DVDinput_seek( dvd_file->title_devs[ i ], 
				     (int)offset, DVDINPUT_NOFLAGS );
                if( off < 0 || off != (int)offset ) {
		    fprintf( stderr, "Dvdmodule: Can't seek to block %d\n", 
			     offset );
		    return off < 0 ? off : 0;
		}
                ret = DVDinput_read( dvd_file->title_devs[ i ], data,
				     (int)block_count, encrypted );
                break;
            } else {
                size_t part1_size = dvd_file->title_sizes[ i ] - offset;
		/* FIXME: Really needs to be a while loop.
                 * (This is only true if you try and read >1GB at a time) */
		
                /* Read part 1 */
                off = DVDinput_seek( dvd_file->title_devs[ i ], 
				     (int)offset, DVDINPUT_NOFLAGS );
                if( off < 0 || off != (int)offset ) {
		    fprintf( stderr, "Dvdmodule: Can't seek to block %d\n", 
			     offset );
		    return off < 0 ? off : 0;
		}
                ret = DVDinput_read( dvd_file->title_devs[ i ], data,
				     (int)part1_size, encrypted );
		if( ret < 0 ) return ret;
		/* FIXME: This is wrong if i is the last file in the set. 
                 * also error from this read will not show in ret. */
		
                /* Read part 2 */
                off = DVDinput_seek( dvd_file->title_devs[ i + 1 ], 
				     0, DVDINPUT_NOFLAGS );
                if( off < 0 || off != 0 ) {
		    fprintf( stderr, "Dvdmodule: Can't seek to block %d\n", 
			     0 );
		    return off < 0 ? off : 0;
		}
                ret2 = DVDinput_read( dvd_file->title_devs[ i + 1 ], 
				      data + ( part1_size
					       * (int64_t)DVD_VIDEO_LB_LEN ),
				      (int)(block_count - part1_size),
				      encrypted );
                if( ret2 < 0 ) return ret2;
		break;
            }
        } else {
            offset -= dvd_file->title_sizes[ i ];
        }
    }

    return ret + ret2;
}

/* This is broken reading more than 2Gb at a time is ssize_t is 32-bit. */
ssize_t DVDReadBlocks( dvd_file_t *dvd_file, int offset, 
		       size_t block_count, unsigned char *data )
{
    int ret;
    
    /* Hack, and it will still fail for multiple opens in a threaded app ! */
    if( dvd_file->dvd->css_title != dvd_file->css_title ) {
      dvd_file->dvd->css_title = dvd_file->css_title;
      if( dvd_file->dvd->isImageFile ) {
	DVDinput_title( dvd_file->dvd->dev, (int)dvd_file->lb_start );
      } else {
	DVDinput_title( dvd_file->title_devs[ 0 ], (int)dvd_file->lb_start );
      }
    }
    
    if( dvd_file->dvd->isImageFile ) {
	ret = DVDReadBlocksUDF( dvd_file, (uint32_t)offset, 
				block_count, data, DVDINPUT_READ_DECRYPT );
    } else {
	ret = DVDReadBlocksPath( dvd_file, (unsigned int)offset, 
				 block_count, data, DVDINPUT_READ_DECRYPT );
    }
    
    return (ssize_t)ret;
}

int32_t DVDFileSeek( dvd_file_t *dvd_file, int32_t offset )
{
   if( offset > dvd_file->filesize * DVD_VIDEO_LB_LEN ) {
       return -1;
   }
   dvd_file->seek_pos = (uint32_t) offset;
   return offset;
}

ssize_t DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
    unsigned char *secbuf;
    unsigned int numsec, seek_sector, seek_byte;
    int ret;
    
    seek_sector = dvd_file->seek_pos / DVD_VIDEO_LB_LEN;
    seek_byte   = dvd_file->seek_pos % DVD_VIDEO_LB_LEN;

    numsec = ( ( seek_byte + byte_size ) / DVD_VIDEO_LB_LEN ) + 1;
    secbuf = (unsigned char *) malloc( numsec * DVD_VIDEO_LB_LEN );
    if( !secbuf ) {
	fprintf( stderr, "Dvdmodule: Can't allocate memory " 
		 "for file read!\n" );
        return 0;
    }
    
    if( dvd_file->dvd->isImageFile ) {
	ret = DVDReadBlocksUDF( dvd_file, (uint32_t) seek_sector, 
				(size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
    } else {
	ret = DVDReadBlocksPath( dvd_file, seek_sector, 
				 (size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
    }

    if( ret != (int) numsec ) {
        free( secbuf );
        return ret < 0 ? ret : 0;
    }

    memcpy( data, &(secbuf[ seek_byte ]), byte_size );
    free( secbuf );

    dvd_file->seek_pos += byte_size;
    return byte_size;
}

ssize_t DVDFileSize( dvd_file_t *dvd_file )
{
    return dvd_file->filesize;
}


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
int	SetCssTitle(dvd_reader_t * device, int val)
{
	device->css_title = val;
	return device->css_title;
}

int GetCssTitle(dvd_reader_t * device)
{
	return device->css_title;
}

int SetCssState(dvd_reader_t * device, int val)
{
	device->css_state = val;
	return device->css_state;
}

int GetCssState(dvd_reader_t * device)
{
	return device->css_state;
}

int DoDvdTitle(dvd_reader_t * device, int startlb)
{
	return DVDinput_title(device->dev, startlb);
}
