#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <string.h>
#include <limits.h>
#include "../inc/dvd_reader.h"
#include "../inc/ifo_read.h"
#include "../inc/ifo_print.h"
#include "../inc/dvd_udf.h"
#include "../inc/JukeFileApi.h"

#define MAXNAME 256
#ifndef PATH_MAX
#define PATH_MAX MAXNAME
#endif

/*Flag for verbose mode */
int verbose;
int aspect;


/* Structs to keep title set information in */

typedef struct {
  int		size_ifo;
  int		size_menu;
  int		size_bup;
  int		number_of_vob_files;
  int		size_vob[10];
} title_set_t;

typedef struct {
  int		number_of_title_sets;
  title_set_t  *title_set;
} title_set_info_t;


typedef struct {
  int		title;
  int		title_set;
  int		vts_title;
  int		chapters;
  int		aspect_ratio;
  int		angles;
  int		audio_tracks;
  int		audio_channels;
  int		sub_pictures;
} titles_t;

typedef struct {
  int		main_title_set;
  int		number_of_titles;
  titles_t	*titles;
} titles_info_t;

typedef struct {
  int		size_ifo;
  unsigned int	ifo_startlb;
  int		size_menu;
  unsigned int 	menu_startlb;
  int		size_bup;
  unsigned int  bup_startlb;
  int		number_of_vob_files;
  int		size_vob[10];
  unsigned int 	vob_startlb[10];
} DVDRip_title_set_t;

typedef struct {
  int		number_of_title_sets;
  DVDRip_title_set_t  *title_set;
} DVDRip_title_set_info_t;

struct Partition {
    int valid;
    char VolumeDesc[128];
    uint16_t Flags;
    uint16_t Number;
    char Contents[32];
    uint32_t AccessType;
    uint32_t Start;
    uint32_t Length;
};


#ifndef INVALID_SET_FILE_POINTER
#   define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif


void bsort_max_to_min(int sector[], int title[], int size);


int CheckSizeArray(const int size_array[], int reference, int target) {
	if ( (size_array[reference]/size_array[target] == 1) &&
	     ((size_array[reference] * 2 - size_array[target])/ size_array[target] == 1) &&
	     ((size_array[reference]%size_array[target] * 3) < size_array[reference]) ) {
		/* We have a dual DVD with two feature films - now lets see if they have the same amount of chapters*/
		return(1);
	} else {
		return(0);
	}
}


int CheckAudioSubChannels(int audio_audio_array[], int title_set_audio_array[],
			  int subpicture_sub_array[], int title_set_sub_array[],
			  int channels_channel_array[],int title_set_channel_array[],
			  int reference, int candidate, int title_sets) {

	int temp, i, found_audio, found_sub, found_channels;

	found_audio=0;
	temp = audio_audio_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( audio_audio_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_audio_array[i] ) {
			found_audio=1;
			break;
		}

	}

	found_sub=0;
	temp = subpicture_sub_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( subpicture_sub_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_sub_array[i] ) {
			found_sub=1;
			break;
		}

	}


	found_channels=0;
	temp = channels_channel_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( channels_channel_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_channel_array[i] ) {
			found_channels=1;
			break;
		}

	}


	return(found_audio + found_sub + found_channels);
}


void FreeSortArrays( int chapter_chapter_array[], int title_set_chapter_array[],
		int angle_angle_array[], int title_set_angle_array[],
		int subpicture_sub_array[], int title_set_sub_array[],
		int audio_audio_array[], int title_set_audio_array[],
		int size_size_array[], int title_set_size_array[],
		int channels_channel_array[], int title_set_channel_array[]) {


	free(chapter_chapter_array);
	free(title_set_chapter_array);

	free(angle_angle_array);
	free(title_set_angle_array);

	free(subpicture_sub_array);
	free(title_set_sub_array);

	free(audio_audio_array);
	free(title_set_audio_array);

	free(size_size_array);
	free(title_set_size_array);

	free(channels_channel_array);
	free(title_set_channel_array);
}


titles_info_t * DVDGetInfo(dvd_reader_t * _dvd) {

	/* title interation */
	int counter, i, f;

	/* Our guess */
	int candidate = 0;
	int multi = 0;
	int dual = 0;


	int titles;
	int title_sets;

	/* Arrays for chapter, angle, subpicture, audio, size, aspect, channels -  file_set relationship */

	/* Size == number_of_titles */
	int * chapter_chapter_array;
	int * title_set_chapter_array;

	int * angle_angle_array;
	int * title_set_angle_array;

	/* Size == number_of_title_sets */

	int * subpicture_sub_array;
	int * title_set_sub_array;

	int * audio_audio_array;
	int * title_set_audio_array;

	int * size_size_array;
	int * title_set_size_array;

	int * channels_channel_array;
	int * title_set_channel_array;

	/* Temp helpers */
	int channels;
	int temp;
	int found;
	int chapters_1;
	int chapters_2;
	int found_chapter;
	int number_of_multi;


	/*DVD handlers*/
	ifo_handle_t * vmg_ifo=NULL;
	dvd_file_t   *  vts_title_file=NULL;

	titles_info_t * titles_info=NULL;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}

	titles = vmg_ifo->tt_srpt->nr_of_srpts;
	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	if ((vmg_ifo->tt_srpt == 0) || (vmg_ifo->vts_atrt == 0)) {
		ifoClose(vmg_ifo);
		return(0);
	}


	/* Todo fix malloc check */
	titles_info = ( titles_info_t *)malloc(sizeof(titles_info_t));
	titles_info->titles = (titles_t *)malloc((titles)* sizeof(titles_t));

	titles_info->number_of_titles = titles;


	chapter_chapter_array = malloc(titles * sizeof(int));
	title_set_chapter_array = malloc(titles * sizeof(int));

	/*currently not used in the guessing */
	angle_angle_array = malloc(titles * sizeof(int));
	title_set_angle_array = malloc(titles * sizeof(int));


	subpicture_sub_array = malloc(title_sets * sizeof(int));
	title_set_sub_array = malloc(title_sets * sizeof(int));

	audio_audio_array = malloc(title_sets * sizeof(int));
	title_set_audio_array = malloc(title_sets * sizeof(int));

	size_size_array = malloc(title_sets * sizeof(int));
	title_set_size_array = malloc(title_sets * sizeof(int));

	channels_channel_array = malloc(title_sets * sizeof(int));
	title_set_channel_array = malloc(title_sets * sizeof(int));


	/* Interate over the titles nr_of_srpts */


	for (counter=0; counter < titles; counter++ )  {
		/* For titles_info */
		titles_info->titles[counter].title = counter + 1;
		titles_info->titles[counter].title_set = vmg_ifo->tt_srpt->title[counter].title_set_nr;
		titles_info->titles[counter].vts_title = vmg_ifo->tt_srpt->title[counter].vts_ttn;
		titles_info->titles[counter].chapters = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
		titles_info->titles[counter].angles = vmg_ifo->tt_srpt->title[counter].nr_of_angles;

		/* For main title*/
		chapter_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
		title_set_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
		angle_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_angles;
		title_set_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
	}

	/* Interate over vmg_nr_of_title_sets */

	for (counter=0; counter < title_sets ; counter++ )  {

		/* Picture*/
		subpicture_sub_array[counter] = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
		title_set_sub_array[counter] = counter + 1;


		/* Audio */
		audio_audio_array[counter] = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
		title_set_audio_array[counter] = counter + 1;

		channels=0;
		for  (i=0; i < audio_audio_array[counter]; i++) {
			if ( channels < vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1) {
				channels = vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1;
			}

		}
		channels_channel_array[counter] = channels;
		title_set_channel_array[counter] = counter + 1;

		/* For tiles_info */
		for (f=0; f < titles_info->number_of_titles ; f++ ) {
			if ( titles_info->titles[f].title_set == counter + 1 ) {
				titles_info->titles[f].aspect_ratio = vmg_ifo->vts_atrt->vts[counter].vtstt_vobs_video_attr.display_aspect_ratio;
				titles_info->titles[f].sub_pictures = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
				titles_info->titles[f].audio_tracks = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
				titles_info->titles[f].audio_channels = channels;
			}
		}

	}




	for (counter=0; counter < title_sets; counter++ ) {

		vts_title_file = DVDOpenFile(_dvd, counter + 1, DVD_READ_TITLE_VOBS);

		if (vts_title_file  != 0) {
			size_size_array[counter] = DVDFileSize(vts_title_file);
			DVDCloseFile(vts_title_file);
		} else {
			size_size_array[counter] = 0;
		}

		title_set_size_array[counter] = counter + 1;


	}


	/* Sort all arrays max to min */

	bsort_max_to_min(chapter_chapter_array, title_set_chapter_array, titles);
	bsort_max_to_min(angle_angle_array, title_set_angle_array, titles);
	bsort_max_to_min(subpicture_sub_array, title_set_sub_array, title_sets);
	bsort_max_to_min(audio_audio_array, title_set_audio_array, title_sets);
	bsort_max_to_min(size_size_array, title_set_size_array, title_sets);
	bsort_max_to_min(channels_channel_array, title_set_channel_array, title_sets);


	/* Check if the second biggest one actually can be a feature title */
	/* Here we will take do biggest/second and if that is bigger than one it's not a feauture title */
	/* Now this is simply not enough since we have to check that the diff between the two of them is small enough
	 to consider the second one a feature title we are doing two checks (biggest  + biggest - second) /second == 1
	 and biggest%second * 3 < biggest */

	if ( CheckSizeArray(size_size_array, 0, 1)  == 1 ) {
		/* We have a dual DVD with two feature films - now lets see if they have the same amount of chapters*/

		chapters_1 = 0;
		for (i=0 ; i < titles ; i++ ) {
			if (titles_info->titles[i].title_set == title_set_size_array[0] ) {
				if ( chapters_1 < titles_info->titles[i].chapters){
					chapters_1 = titles_info->titles[i].chapters;
				}
			}
		}

		chapters_2 = 0;
		for (i=0 ; i < titles ; i++ ) {
			if (titles_info->titles[i].title_set == title_set_size_array[1] ) {
				if ( chapters_2 < titles_info->titles[i].chapters){
					chapters_2 = titles_info->titles[i].chapters;
				}
			}
		}

		if (  vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio ==
			vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio) {
			/* In this case it's most likely so that we have a dual film but with different context
			They are with in the same size range and have the same aspect ratio
			I would guess that such a case is e.g. a DVD containing several episodes of a TV serie*/
			candidate = title_set_size_array[0];
			multi = 1;
		} else if ( chapters_1 == chapters_2  && vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio !=
			vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio){
			/* In this case we have (guess only) the same context - they have the same number of chapters but different aspect ratio and are in the same size range*/
			if ( vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
				candidate = title_set_size_array[0];
			} else if ( vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
				candidate = title_set_size_array[1];
			} else {
				/* Okay we didn't have the prefered aspect ratio - just make the biggest one a candidate */
				/* please send  report if this happens*/
				fprintf(stderr, "You have encountered a very special DVD, please send a bug report along with all IFO files from this title\n");
				candidate = title_set_size_array[0];
			}
			dual = 1;
		}
	} else {
		candidate = title_set_size_array[0];
	}


	/* Lets start checking audio,sub pictures and channels my guess is namly that a special suburb will put titles with a lot of
	 chapters just to make our backup hard */


	found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
				      subpicture_sub_array, title_set_sub_array,
				      channels_channel_array, title_set_channel_array,
				      0 , candidate, title_sets);


	/* Now lets see if we can find our candidate among the top most chapters */
	found_chapter=6;
	temp = chapter_chapter_array[0];
	for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
		if ( candidate == title_set_chapter_array[i] ) {
			found_chapter=i+1;
			break;
		}
	}

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);


	if (((found == 3) && (found_chapter == 1) && (dual == 0) && (multi == 0)) || ((found == 3) && (found_chapter < 3 ) && (dual == 1))) {

		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);

	}

	if (multi == 1) {
		for (i=0 ; i < title_sets ; ++i) {
			if (CheckSizeArray(size_size_array, 0, i + 1)  == 0) {
					break;
			}
		}
		number_of_multi = i;
		for (i = 0; i < number_of_multi; i++ ) {
			if (title_set_chapter_array[0] == i + 1) {
				candidate = title_set_chapter_array[0];
			}
		}

		found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
			      subpicture_sub_array, title_set_sub_array,
			      channels_channel_array, title_set_channel_array,
			      0 , candidate, title_sets);

		if (found == 3) {
			FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
					angle_angle_array, title_set_angle_array,
					subpicture_sub_array, title_set_sub_array,
					audio_audio_array, title_set_audio_array,
					size_size_array, title_set_size_array,
					channels_channel_array, title_set_channel_array);
			titles_info->main_title_set = candidate;
			return(titles_info);
		}
	}

	/* We have now come to that state that we more or less have given up :( giving you a good guess of the main feature film*/
	/*No matter what we will more or less only return the biggest VOB*/
	/* Lets see if we can find our biggest one - then we return that one */
	candidate = title_set_size_array[0];

	found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
				      subpicture_sub_array, title_set_sub_array,
				      channels_channel_array, title_set_channel_array,
				      0 , candidate, title_sets);

	/* Now lets see if we can find our candidate among the top most chapters */

	found_chapter=5;
	temp = chapter_chapter_array[0];
	for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
		if ( candidate == title_set_chapter_array[i] ) {
			found_chapter=i+1;
			break;
		}

	}

	/* Here we take chapters in to consideration*/
	if (found == 3) {
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);
	}

	/* Here we do but we lower the treshold for audio, sub and channels */

	if ((found > 1 ) && (found_chapter <= 4)) {
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);

		/* return it */
	} else {
		/* Here we give up and just return the biggest one :(*/
		/* Just return the biggest badest one*/
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);
	}


	/* Some radom thoughts about DVD guessing */
	/* We will now gather as much data about the DVD-Video as we can and
	then make a educated guess which one is the main feature film of it*/


	/* Make a tripple array with chapters, angles and title sets
	 - sort out dual title sets with a low number of chapters. Tradtionaly
	 the title set with most chapters is the main film. Number of angles is
	 keept as a reference point of low value*/

	/* Make a dual array with number of audio streams, sub picture streams
	 and title sets. Tradtionaly the main film has many audio streams
	 since it's supposed be synconised e.g. a English film syncronised/dubbed
	 in German. We are also keeping track of sub titles since it's also indication
	 of the main film*/

	/* Which title set is the biggest one - dual array with title sets and size
	 The biggest one is usally the main film*/

	/* Which title set is belonging to title 1 and how many chapters has it. Once
	 again tradtionaly title one is belonging to the main film*/

	/* Yes a lot of rant - but it helps me think - some sketch on paper or in the mind
	 I sketch in the comments - beside it will help you understand the code*/

	/* Okay lets see if the biggest one has most chapters, it also has more subtitles
	 and audio tracks than the second one and it's title one.
	 Done it must be the main film

	 Hmm the biggest one doesn't have the most chapters?

	 See if the second one has the same amount of chapters and is the biggest one
	 If so we probably have a 4:3 and 16:9 versions of film on the same disk

	 Now we fetch the 16:9 by default unless the forced to do 4:3
	 First check which one is which.
	 If the 16:9 is the biggest one and has the same or more subtile, audio streams
	 then we are happy unless we are in force 4:3 mode :(
	 The same goes in reverse if we are in force 4:3 mode


	 Hmm, in force 4:3 mode - now we check how much smaller than the biggest one it is
	 (or the reverse if we are in 16:9 mode)

	 Generally a reverse division should render in 1 and with a small modulo - like wise
	 a normal modulo should give us a high modulo

	 If we get more than one it's of cource a fake however if we get just one we still need to check
	 if we subtract the smaller one from the bigger one we should end up with a small number - hence we
	 need to multiply it more than 4 times to get it bigger than the biggest one. Now we know that the
	 two biggest once are really big and possibly carry the same film in differnet formats.

	 We will now return the prefered one either 16:9 or 4:3 but we will first check that the one
	 we return at lest has two or more audio tracks. We don't want it if the other one has a lot
	 more sound (we may end up with a film that only has 2ch Dolby Digital so we want to check for
	 6ch DTS or Dolby Digital. If the prefered one doesn't have those features but the other once has
	 we will return the other one.
	 */
}


int DVDGetTitleName(const char *device, char *title)
{
	/* Variables for filehandel and title string interaction */
	char psz_dvd[16];
	char buff[2048];
	HANDLE	filehandle;
	DWORD	Moved ;//, offset;
	LARGE_INTEGER li_seek;
	int i, last;

	 _snprintf_s( psz_dvd, 16, _TRUNCATE, "\\\\.\\%c:", device[0] );
	// fprintf(stderr,"device name %s\n",psz_dvd);
	/* Open DVD device */
/*
	if ( !(filehandle = open(device, O_RDONLY)) ) {
		fprintf(stderr, "Can't open secified device %s - check your DVD device\n", device);
		return(1);
	}
*/
	filehandle = CreateFile(psz_dvd,           // open MYFILE.TXT 
							GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_RANDOM_ACCESS, NULL );
 
	if (filehandle == INVALID_HANDLE_VALUE) 
	{ 
		fprintf(stderr, "Can't open secified device %s - check your DVD device\n", device);
		return(1);
	}
	/* Seek to title of first track, which is at (track_no * 32768) + 40 */
/*
	if ( 32808 != lseek(filehandle, 32808, SEEK_SET) ) {
		close(filehandle);
		fprintf(stderr, "Can't seek DVD device %s - check your DVD device\n", device);
		return(1);
	}
*/
	li_seek.QuadPart = (LONGLONG)(16*2048);

    li_seek.LowPart = SetFilePointer( filehandle,
                                      li_seek.LowPart,
                                      &li_seek.HighPart, FILE_BEGIN);
    if( (li_seek.LowPart == INVALID_SET_FILE_POINTER)
        && GetLastError() != NO_ERROR)
    {
		CloseHandle(filehandle);
		fprintf(stderr, "Can't seek DVD device %s - check your DVD device\n", device);
		return(1);        
    }



	/* Read the DVD-Video title */
/*
	if ( 32 != read(filehandle, title, 32)) {
		close(filehandle);
		fprintf(stderr, "Can't read title from DVD device %s\n", device);
		return(1);
	}
*/
	Moved = 0;
	if(!ReadFile(filehandle,buff,2048, &Moved,NULL))
	{
		CloseHandle(filehandle);
		fprintf(stderr, "Can't read title from DVD device %s\n", device);
		return(1);
	}
	
	memcpy(title, &buff[40],32);

	/* Terminate the title string */

	title[32] = '\0';
	

	
	/* Remove trailing white space */

	last = 32;
	for ( i = 0; i < 32; i++ ) {
		if ( title[i] != ' ' ) { last = i; }
	}

	title[last + 1] = '\0';
//	fprintf(stderr, "String %s\n", title);
	CloseHandle(filehandle);
	return(0);
}


void bsort_min_to_max(int sector[], int title[], int size){

	int temp_title, temp_sector, i, j;

	for ( i=0; i < size ; i++ ) {
	  for ( j=0; j < size ; j++ ) {
		if (sector[i] < sector[j]) {
			temp_sector = sector[i];
			temp_title = title[i];
			sector[i] = sector[j];
			title[i] = title[j];
			sector[j] = temp_sector;
			title[j] = temp_title;
		}
	  }
	}
}


void bsort_max_to_min(int sector[], int title[], int size){

	int temp_title, temp_sector, i, j;

	for ( i=0; i < size ; i++ ) {
	  for ( j=0; j < size ; j++ ) {
		if (sector[i] > sector[j]) {
			temp_sector = sector[i];
			temp_title = title[i];
			sector[i] = sector[j];
			title[i] = title[j];
			sector[j] = temp_sector;
			title[j] = temp_title;
		}
	  }
	}
}


void DVDFreeTitleSetInfo(title_set_info_t * title_set_info) {
	free(title_set_info->title_set);
	free(title_set_info);
}


void DVDFreeTitlesInfo(titles_info_t * titles_info) {
	free(titles_info->titles);
	free(titles_info);
}


title_set_info_t *DVDGetFileSet(dvd_reader_t * _dvd) {

	/* title interation */
	int title_sets, counter, i;


	/* DVD Video files */
	char	filename[MAXNAME];
	int	size;

	/*DVD ifo handler*/
	ifo_handle_t * 	vmg_ifo=NULL;

	/* The Title Set Info struct*/
	title_set_info_t * title_set_info;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}


	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);

	/* Todo fix malloc check */
	title_set_info = (title_set_info_t *)malloc(sizeof(title_set_info_t));
	title_set_info->title_set = (title_set_t *)malloc((title_sets + 1)* sizeof(title_set_t));

	title_set_info->number_of_title_sets = title_sets;


	/* Find VIDEO_TS.IFO is present - must be present since we did a ifo open 0*/

	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.IFO");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_ifo = size;
	} else {
		DVDFreeTitleSetInfo(title_set_info);
		return(0);
	}



	/* Find VIDEO_TS.VOB if present*/

	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.VOB");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_menu = size;
	} else {
		title_set_info->title_set[0].size_menu = 0 ;
	}

	/* Find VIDEO_TS.BUP if present */

	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.BUP");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_bup = size;
	} else {
		DVDFreeTitleSetInfo(title_set_info);
		return(0);
	}

	if (title_set_info->title_set[0].size_ifo != title_set_info->title_set[0].size_bup) {
		fprintf(stderr,"BUP and IFO size not the same be warened!\n");
	}


	/* Take care of the titles which we don't have in VMG */

	title_set_info->title_set[0].number_of_vob_files = 0;
	title_set_info->title_set[0].size_vob[0] = 0;


	if ( verbose > 0 ){
		fprintf(stderr,"\n\n\nFile sizes for Title set 0 VIDEO_TS.XXX\n");
		fprintf(stderr,"IFO = %d, MENU_VOB = %d, BUP = %d\n",title_set_info->title_set[0].size_ifo, title_set_info->title_set[0].size_menu, title_set_info->title_set[0].size_bup );

	}


	if ( title_sets >= 1 ) {
 		for (counter=0; counter < title_sets; counter++ ){

			if ( verbose > 1 ){
				fprintf(stderr,"At top of loop\n");
			}


			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.IFO",counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter + 1].size_ifo = size;
			} else {
				DVDFreeTitleSetInfo(title_set_info);
				return(0);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"After opening files\n");
			}


			/* Find VTS_XX_0.VOB if present*/

			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.VOB", counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter + 1].size_menu = size;
			} else {
				title_set_info->title_set[counter + 1].size_menu = 0 ;
			}


			if ( verbose > 1 ){
				fprintf(stderr,"After Menu VOB check\n");
			}


			/* Find all VTS_XX_[1 to 9].VOB files if they are present*/

			for( i = 0; i < 9; ++i ) {
				sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_%i.VOB", counter + 1, i + 1 );
				if(UDFFindFile(_dvd, filename, &size) == 0 ) {
					break;
				}
				title_set_info->title_set[counter + 1].size_vob[i] = size;
			}
			title_set_info->title_set[counter + 1].number_of_vob_files = i;

			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title VOB check\n");
			}


			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.BUP", counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter +1].size_bup = size;
			} else {
				DVDFreeTitleSetInfo(title_set_info);
				return(0);
			}

			if (title_set_info->title_set[counter +1].size_ifo != title_set_info->title_set[counter + 1].size_bup) {
				fprintf(stderr,"BUP and IFO size for fileset %d is not the same be warened!\n", counter + 1);
			}



			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title BUP check\n");
			}


			if ( verbose > 0 ) {
				fprintf(stderr,"\n\n\nFile sizes for Title set %d i.e.VTS_%02d_X.XXX\n", counter + 1, counter + 1);
				fprintf(stderr,"IFO: %d, MENU: %d\n", title_set_info->title_set[counter +1].size_ifo, title_set_info->title_set[counter +1].size_menu);
				for (i = 0; i < title_set_info->title_set[counter + 1].number_of_vob_files ; i++) {
					fprintf(stderr, "VOB %d is %d\n", i + 1, title_set_info->title_set[counter + 1].size_vob[i]);
				}
				fprintf(stderr,"BUP: %d\n",title_set_info->title_set[counter +1].size_bup);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"Bottom of loop \n");
			}
		}

        }

	/* Return the info */
	return(title_set_info);

}


void DVDFreeTitleSetInfo_image(DVDRip_title_set_info_t * title_set_info) {
	free(title_set_info->title_set);
	free(title_set_info);
}


DVDRip_title_set_info_t *DVDGetFileSet_image(dvd_reader_t * _dvd) {

	/* title interation */
	int title_sets, counter, i;

	/* startlb */
	unsigned int file_startlb = 0;

	/* DVD Video files */
	char	filename[MAXNAME];
	int	size;

	/*DVD ifo handler*/
	ifo_handle_t * 	vmg_ifo=NULL;

	/* The Title Set Info struct*/
	DVDRip_title_set_info_t * title_set_info;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}


	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);

	/* Todo fix malloc check */
	title_set_info = (DVDRip_title_set_info_t *)malloc(sizeof(DVDRip_title_set_info_t));
	title_set_info->title_set = (DVDRip_title_set_t *)malloc((title_sets + 1)* sizeof(DVDRip_title_set_t));

	title_set_info->number_of_title_sets = title_sets;


	/* Find VIDEO_TS.IFO is present - must be present since we did a ifo open 0*/

	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.IFO");

	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_ifo = size;
		title_set_info->title_set[0].ifo_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.IFO startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		DVDFreeTitleSetInfo_image(title_set_info);
		return(0);
	}



	/* Find VIDEO_TS.VOB if present*/
	file_startlb = 0;
	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.VOB");

	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_menu = size;
		title_set_info->title_set[0].menu_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.VOB startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		title_set_info->title_set[0].size_menu = 0 ;	
		title_set_info->title_set[0].menu_startlb = file_startlb;
	}

	/* Find VIDEO_TS.BUP if present */

	sprintf_s(filename,MAXNAME,"/VIDEO_TS/VIDEO_TS.BUP");
	file_startlb = 0;
	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_bup = size;
		title_set_info->title_set[0].bup_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.BUP startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		DVDFreeTitleSetInfo_image(title_set_info);
		return(0);
	}

	if (title_set_info->title_set[0].size_ifo != title_set_info->title_set[0].size_bup) {
		fprintf(stderr,"BUP and IFO size not the same be warened!\n");
	}


	/* Take care of the titles which we don't have in VMG */

	title_set_info->title_set[0].number_of_vob_files = 0;
	title_set_info->title_set[0].size_vob[0] = 0;


	if ( verbose > 0 ){
		fprintf(stderr,"\n\n\nFile sizes for Title set 0 VIDEO_TS.XXX\n");
		fprintf(stderr,"IFO startlb = %d size= %d , MENU_VOB = startlb = %d size= %d, BUP = startlb = %d size= %d\n",
		title_set_info->title_set[0].ifo_startlb,
		title_set_info->title_set[0].size_ifo, 
		title_set_info->title_set[0].menu_startlb, 
		title_set_info->title_set[0].size_menu, 
		title_set_info->title_set[0].bup_startlb,
		title_set_info->title_set[0].size_bup);
	}

	fprintf(stderr,"TILTE SET = (%d)\n", title_sets);
	if ( title_sets >= 1 ) {
 		for (counter=0; counter < title_sets; counter++ ){

			if ( verbose > 1 ){
				fprintf(stderr,"At top of loop\n");
			}


			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.IFO",counter + 1);
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter + 1].size_ifo = size;
				title_set_info->title_set[counter + 1].ifo_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.IFO startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				DVDFreeTitleSetInfo_image(title_set_info);
				return(0);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"After opening files\n");
			}


			/* Find VTS_XX_0.VOB if present*/

			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.VOB", counter + 1);
			
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter + 1].size_menu = size;
				title_set_info->title_set[counter + 1].menu_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.VOB startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				title_set_info->title_set[counter + 1].size_menu = 0 ;
				title_set_info->title_set[counter + 1].menu_startlb = file_startlb;
			}


			if ( verbose > 1 ){
				fprintf(stderr,"After Menu VOB check\n");
			}


			/* Find all VTS_XX_[1 to 9].VOB files if they are present*/

			for( i = 0; i < 9; ++i ) {
				sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_%i.VOB", counter + 1, i + 1 );
				file_startlb  = 0;
				if((file_startlb = UDFFindFile(_dvd, filename, &size)) == 0 ) {
					break;
				}
				title_set_info->title_set[counter + 1].size_vob[i] = size;
				title_set_info->title_set[counter + 1].vob_startlb[i] = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_%i.IFO startlb (%d) size (%d)\n", counter + 1, i+1, file_startlb, size); 
			}
			title_set_info->title_set[counter + 1].number_of_vob_files = i;

			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title VOB check\n");
			}


			sprintf_s(filename,MAXNAME,"/VIDEO_TS/VTS_%02i_0.BUP", counter + 1);
			
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter +1].size_bup = size;
				title_set_info->title_set[counter +1].bup_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.BUP startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				DVDFreeTitleSetInfo_image(title_set_info);
				return(0);
			}

			if (title_set_info->title_set[counter +1].size_ifo != title_set_info->title_set[counter + 1].size_bup) {
				fprintf(stderr,"BUP and IFO size for fileset %d is not the same be warened!\n", counter + 1);
			}



			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title BUP check\n");
			}


			if ( verbose > 0 ) {
				fprintf(stderr,"\n\n\nFile sizes for Title set %d i.e.VTS_%02d_X.XXX\n", counter + 1, counter + 1);
				fprintf(stderr,"IFO: startlb =%d size=%d, MENU: startlb =%d size=%d\n", 
					title_set_info->title_set[counter +1].ifo_startlb, 
					title_set_info->title_set[counter +1].size_ifo,
					title_set_info->title_set[counter +1].menu_startlb, 
					title_set_info->title_set[counter +1].size_menu);

				for (i = 0; i < title_set_info->title_set[counter + 1].number_of_vob_files ; i++) {
					fprintf(stderr, "VOB %d is startlb=%d size=%d\n", i + 1, 
						title_set_info->title_set[counter + 1].vob_startlb[i],
						title_set_info->title_set[counter + 1].size_vob[i]);
				}
				fprintf(stderr,"BUP: startlb=%d size=%d\n",
					title_set_info->title_set[counter +1].bup_startlb,
					title_set_info->title_set[counter +1].size_bup);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"Bottom of loop \n");
			}
		}

        }

	/* Return the info */
	return(title_set_info);

}


#define PLANE_AREA	0
#define TITLE_VOB	1
#define MENU_VOB	2
#define IFO		3
#define BUP		4

typedef struct _AddressEntry
{
	unsigned int startlb;
	unsigned int lbcount;
	unsigned int needKeySet;
	unsigned int type;
	unsigned short title;
	unsigned short index;
	struct _AddressEntry * next;
	struct _AddressEntry * prev;
} AddressEntry;

typedef struct _AddressMap
{
	unsigned int EntryCount;
	AddressEntry Head;
} AddressMap;

AddressEntry *
GenerateAddressEntry(
	unsigned int startlb, 
	unsigned int lbcount, 
	unsigned int needKey,
	unsigned int type,
	unsigned short title,
	unsigned short index
	)
{
	AddressEntry *entry;
 
	if((entry= (AddressEntry *)malloc( sizeof(AddressEntry) ) ) == NULL)
	{
		fprintf(stderr,"Out of Memory when making AddressEntry startlb=%d\n",startlb);
		return NULL;
	}
	
	entry->startlb = startlb;
	entry->lbcount = lbcount;
	entry->needKeySet = needKey;
	entry->type = type;
	entry->next = entry;
	entry->prev = entry;
	entry->title = title;
	entry->index = index;
	return entry;
}


int
InsertAddressEntry(AddressMap * Map, AddressEntry *entry)
{
	AddressEntry * p;
	AddressEntry * head = &(Map->Head);
	
	p = head->next;
	if(p == head){
		p->next = entry;
		p->prev = entry;
		entry->prev = p;
		entry->next = p;
	}else{
		while(p != head)
		{
			if(p->startlb >  entry->startlb)
			{
				p->prev->next = entry;
				entry->prev = p->prev;
				p->prev = entry;
				entry->next = p;
				break;
			}else if (p->startlb == entry->startlb){
				fprintf(stderr," Same Entry already exist! p->startlb=%d : entry->startlb=%d\n",p->startlb,entry->startlb);
				free(entry);
				return 0;
			} else{
				p = p->next;				
			}
		}
		
		if(p == head)
		{
			p->prev->next = entry;
			entry->prev = p->prev;
			p->prev = entry;
			entry->next = p;
		}			
	}
	
	return 1;
}

void PrintAddressMap(AddressMap * Map)
{
	AddressEntry * p, *q, *head;
	int count = 0;
	
	char * TypeStr[] = {"PLAN_AREA","TITLE_VOB","MENU_VOB","IFO","BUP"};
	
	head = &(Map->Head);
	p = head->next;

	fprintf(stderr,"Total entry %d\n",Map->EntryCount);

	while(p != head)
	{
		q = p;
		p = p->next;
		count ++;
		fprintf(stderr,"ADDRESS ENTRY (%d)\n",count);
		fprintf(stderr,"\t Slb=%d, blkC=%d, UseKey=%d,  Type= %s, Tittle= %d, Index=%d\n",
			q->startlb, q->lbcount,q->needKeySet, TypeStr[q->type],q->title, q->index); 
	}
}


void FreeAddressMap(AddressMap * Map)
{
	AddressEntry * p, *q, *head;
	head = &(Map->Head);
	p = head->next;
	while(p != head)
	{
		q = p;
		p = p->next;
		free(q);
	}

	free(Map);
}

int
AddUncoveredSpace(AddressMap * Map, unsigned int TotalDiscSize)
{
	AddressEntry * p, *q, *head, *entry;
	unsigned int startlb = 0;
	unsigned int blockcount = 0;	
	head = &(Map->Head);
	p = head->next;
	entry = NULL;

	while(p != head)
	{	
		q = p;
		p = p->next;
		if(q->startlb != startlb)
		{
			if(q->startlb >  startlb)
			{
				blockcount = q->startlb - startlb;
				if( (entry = GenerateAddressEntry(startlb,blockcount,0,PLANE_AREA,0,0)) == NULL)
				{
					return 0;
				}
				fprintf(stderr, " insert new address q->startlb (%d), startlb (%d)\n", q->startlb, startlb);	
				if(!InsertAddressEntry(Map,entry))
				{
					return 0;
				}
				Map->EntryCount ++;
				startlb = q->startlb + q->lbcount;
			}else if(q->startlb < startlb){
				fprintf(stderr, "Error Address Spaces are duplicated!!!\n");
				return 0;
			}
		}else {
			startlb = q->startlb + q->lbcount;
		}		
		
	}
	
	if(startlb != TotalDiscSize)
	{
		blockcount = TotalDiscSize - startlb;
		if( (entry = GenerateAddressEntry(startlb,blockcount,0,PLANE_AREA,0,0)) == NULL)
		{
			return 0;
		}
		
		fprintf(stderr, " insert new address totalDiscSize (%d), startlb (%d)\n", TotalDiscSize, startlb);	
		if(!InsertAddressEntry(Map,entry))
		{
			return 0;
		}
		Map->EntryCount ++;	
	}

	return 1;
}


AddressMap *
GenerateAddressMap(DVDRip_title_set_info_t * title_set_info, unsigned int TotalDiscSize)
{
	AddressMap 		*Map;
	AddressEntry 		*entry;
	DVDRip_title_set_t 	*title;
	unsigned int		blockcount; 
	unsigned int 		startlb;
	int CountOfVobFile = 0;	
	int loop = 0;
	int i,j;


	if( (Map = (AddressMap *)malloc( sizeof(AddressMap)) ) == NULL)
	{
		fprintf(stderr,"Out of Memory when making AddressMap n");
		return NULL;	
	}

	Map->EntryCount = 0;
	(Map->Head).prev = &(Map->Head);
	(Map->Head).next = &(Map->Head);	

	loop = title_set_info->number_of_title_sets + 1;
	for(i = 0; i < loop; i++)
	{	
		title = &(title_set_info->title_set[i]);
		/* Set IFO File */
		blockcount = ( (title->size_ifo/DVD_VIDEO_LB_LEN) + 
				((title->size_ifo % DVD_VIDEO_LB_LEN)?1:0) );
		fprintf(stderr,"IFO file title->ifo_startlb (%d) blockcount (%d)\n", title->ifo_startlb, blockcount);	
		if(blockcount !=0)
		{	
			if( (entry = GenerateAddressEntry(title->ifo_startlb,blockcount,0,IFO,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}

			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}	
		Map->EntryCount ++;
		/* Set MENU File */
		blockcount = ( (title->size_menu/DVD_VIDEO_LB_LEN) + 
				((title->size_menu % DVD_VIDEO_LB_LEN)?1:0) );			
		fprintf(stderr,"MENU file title->menu_startlb (%d) blockcount (%d)\n", title->menu_startlb, blockcount);	
		if(blockcount != 0)
		{	
			if( (entry = GenerateAddressEntry(title->menu_startlb,blockcount,1,MENU_VOB,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}
		

			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}	
		Map->EntryCount ++;	
		/* Set BUP  File */
		blockcount = ( (title->size_bup/DVD_VIDEO_LB_LEN) + 
				((title->size_bup % DVD_VIDEO_LB_LEN)?1:0) );			
		fprintf(stderr,"BUP file title->bup_startlb (%d) blockcount (%d)\n", title->bup_startlb, blockcount);
		if(blockcount != 0)
		{		
			if( (entry = GenerateAddressEntry(title->bup_startlb,blockcount,0,BUP,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}
		
			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}

		Map->EntryCount ++;
		
		
		/* Set TITLE File */
		CountOfVobFile = title->number_of_vob_files;
		if(CountOfVobFile > 0)
		{	
			blockcount = 0;
			startlb = title->vob_startlb[0];

			for(j = 0; j < CountOfVobFile; j++)
			{
				blockcount += ( (title->size_vob[j]/DVD_VIDEO_LB_LEN) + 
						((title->size_vob[j] % DVD_VIDEO_LB_LEN)?1:0) );
			}
			
			fprintf(stderr,"TITLE file startlb (%d) blockcount (%d)\n", startlb, blockcount);
			if(blockcount != 0)
			{		
				if( (entry = GenerateAddressEntry(startlb,blockcount,1,TITLE_VOB,i,1)) == NULL)
				{
					FreeAddressMap(Map);
					return NULL;
				}
		
				if(!InsertAddressEntry(Map,entry))
				{
					FreeAddressMap(Map);
					return NULL;
				}
			}
			Map->EntryCount ++;
		}
				
	}

	if(!AddUncoveredSpace(Map,TotalDiscSize))
	{
		FreeAddressMap(Map);
		return NULL;
	}
			
	return Map;	
}


typedef struct _PartInfo 
{
	unsigned short num;
	unsigned int start;
	unsigned int length; // size/2048
} PartInfo;

#define MAX_DATA_SIZE 	(1024*64)
#define MAX_BLK_SIZE	(MAX_DATA_SIZE/2048)
int DVDcopyUnscambledBlock(dvd_reader_t * dvd, HANDLE file, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	DWORD writingsize;
	DWORD writtensize;
	//int copyed = 0;

	char * buff = malloc(MAX_DATA_SIZE);
	//time_t t,g;
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
//	fprintf(stderr, "COPY Unscambled Address Area\n");
//	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d)\n", 
//		entry->startlb,	entry->lbcount, entry->type, entry->title);
	if(!buff){
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff,0,MAX_DATA_SIZE);

		//time(&t);
		if(!DVDReadBlocksUDFRaw( dvd, startlb, size, buff, 0))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);
			return 0;
		}
		//time(&g);

		//fprintf(stderr,"READ TIME: t(%d)\n",t);
		//fprintf(stderr,"READ TIME: g(%d)\n",g);
		

		//time(&t);
		/*
		if(write(file,buff,size*2048) != size*2048)
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		*/
		writingsize = size*2048;
		if((!WriteFile(file, buff, writingsize, &writtensize, NULL)) 
			|| (writingsize !=writingsize ))
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		//time(&g);
			
		//fprintf(stderr,"WRITE TIME: t(%d)\n",t);
		//fprintf(stderr,"WRITE TIME: g(%d)\n",g);
		
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}

int DVDcopyScambledBlock(dvd_reader_t * dvd, HANDLE file, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	DWORD writingsize;
	DWORD writtensize;
	//int copyed = 0;
	int csstitle = 0;
	//time_t t,g;

	char * buff = malloc(MAX_DATA_SIZE);
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
//	fprintf(stderr, "COPY Scambled Address Area\n");
//	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d) INDEX(%d)\n", 
//		entry->startlb,	entry->lbcount, entry->type, entry->title, entry->index);

	if(!buff) {
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;
	}

	if(entry->type == MENU_VOB)
	{
		csstitle =( (((int)entry->title) << 1) | 1 );
	}else{
		csstitle = ((int)entry->title) << 1;
	}
	
	/* set decrypte key */		
	if(GetCssTitle(dvd) != csstitle)
	{
		SetCssTitle(dvd, csstitle);
		DoDvdTitle(dvd, (int)entry->startlb);	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff, 0, MAX_DATA_SIZE);


		//time(&t);
		if(!DVDReadBlocksUDFRaw( dvd, startlb, size, buff, 1))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		//time(&g);

		//fprintf(stderr,"READ TIME: t(%d)\n",t);
		//fprintf(stderr,"READ TIME: g(%d)\n",g);
		
		//time(&t);
		/*
		if(write(file,buff,size*2048) != size*2048)
		{
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		*/
		writingsize = size*2048;
		if((!WriteFile(file, buff, writingsize, &writtensize, NULL)) 
			|| (writingsize !=writingsize ))
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		//time(&g);
		
		//fprintf(stderr,"WRITE TIME: t(%d)\n",t);
		//fprintf(stderr,"WRITE TIME: g(%d)\n", g);
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}

int DVDcopyDiscToISO(dvd_reader_t * _dvd, HANDLE file, AddressMap * Map)
{
	int CountOfAddressMap = 0;
	//int result;
	int Count = 0;
	AddressEntry *head, *p;

	CountOfAddressMap = Map->EntryCount;
	head = &(Map->Head);
	p = head->next;

	while(p != head)
	{
		Count++;
//		fprintf(stderr, "CopyAddress Entry(%d)\n",Count);

		switch(p->type)
		{
		case PLANE_AREA :
		case IFO :
		case BUP:
			if(!DVDcopyUnscambledBlock(_dvd, file, p))
			{
				return 0;
			}
			break;		
		case TITLE_VOB :
		case MENU_VOB :
			if(!DVDcopyScambledBlock(_dvd, file, p))
			{
				return 0;
			}
			break;		
		default:
			fprintf(stderr,"Error No Address type\n");
			return 0;
			break; 
		}
		p = p->next;
	}

	return 1;		
}

int DVDMirror_image(dvd_reader_t * _dvd, char * targetdir,char * title_name) {

	unsigned int i;
	DVDRip_title_set_info_t *title_set_info=NULL;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
	AddressMap		*Map = NULL;
//	int 			file;
	HANDLE			file;
	char 			targetname[PATH_MAX];
	time_t			g;
	
	title_set_info = DVDGetFileSet_image(_dvd);
	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}
	
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;
	
	
//	fprintf(stderr, "\n\nDVDMirror Image Address Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscSize %d\n",TotalDiscSize);

	if((Map = GenerateAddressMap(title_set_info, TotalDiscSize)) == NULL)
	{
		fprintf(stderr,"Cant Get AddressMap\n");
		DVDFreeTitleSetInfo_image(title_set_info);
		return(1);	
	} 		
	DVDFreeTitleSetInfo_image(title_set_info);	
	PrintAddressMap(Map);

	time(&g);
	sprintf_s(targetname,PATH_MAX,"%s/%s%d.ISO",targetdir,title_name,g);
	
/*
	if( (file = open(targetname, O_WRONLY|O_CREAT|O_LARGEFILE, 0644)) == -1)
	{
		fprintf(stderr, "Error openning %s\n",targetname);
		FreeAddressMap(Map);
		return(1);
	}	
*/

	file = CreateFile(targetname,           // create MYFILE.TXT 
							GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, CREATE_NEW,
                            FILE_FLAG_RANDOM_ACCESS, NULL ); 

	if (file == INVALID_HANDLE_VALUE) 
	{ 
		fprintf(stderr,"Error openning %s\n",targetname);
		FreeAddressMap(Map);
		return(1);
	}


	/* generate all keys */
	initAllCSSKeys(_dvd);
	fprintf(stderr, "dvd->css_state %d\n", SetCssState(_dvd, 2));

	
	/* read and copy */
	if(!DVDcopyDiscToISO(_dvd, file, Map))
	{
		fprintf(stderr, "DVDcopyDiscToISO Fail!\n");
	}
	
	FreeAddressMap(Map);
	CloseHandle(file);	
	
	return(0);
}

int DVDDisplayInfo(dvd_reader_t * _dvd, char * dvd) {


	int i, f;
	int chapters;
	int channels;
	char title_name[33]="";
	title_set_info_t * title_set_info=NULL;
	titles_info_t * titles_info=NULL;


	titles_info = DVDGetInfo(_dvd);
	if (!titles_info) {
		fprintf(stderr, "Guess work of main feature film faild\n");
		return(1);
	}

	title_set_info = DVDGetFileSet(_dvd);
	if (!title_set_info) {
		DVDFreeTitlesInfo(titles_info);
		return(1);
	}

	DVDGetTitleName(dvd,title_name);


	fprintf(stdout,"\n\n\nDVD-Video information of the DVD with tile %s\n\n", title_name);

	/* Print file structure */

	fprintf(stdout,"File Structure DVD\n");
	fprintf(stdout,"VIDEO_TS/\n");
	fprintf(stdout,"\tVIDEO_TS.IFO\t%i\n", title_set_info->title_set[0].size_ifo);

	if (title_set_info->title_set[0].size_menu != 0 ) {
		fprintf(stdout,"\tVIDEO_TS.VOB\t%i\n", title_set_info->title_set[0].size_menu);
	}

	fprintf(stdout,"\tVIDEO_TS.BUP\t%i\n", title_set_info->title_set[0].size_bup);

	for( i = 0 ; i < title_set_info->number_of_title_sets ; i++) {
		fprintf(stdout,"\tVTS_%02i_0.IFO\t%i\n", i + 1, title_set_info->title_set[i + 1].size_ifo);
		if (title_set_info->title_set[i + 1].size_menu != 0 ) {
			fprintf(stdout,"\tVTS_%02i_0.VOB\t%i\n", i + 1, title_set_info->title_set[i + 1].size_menu);
		}
		if (title_set_info->title_set[i + 1].number_of_vob_files != 0) {
			for( f = 0; f < title_set_info->title_set[i + 1].number_of_vob_files ; f++ ) {
				fprintf(stdout,"\tVTS_%02i_%i.VOB\t%i\n", i + 1, f + 1, title_set_info->title_set[i + 1].size_vob[f]);
			}
		}
		fprintf(stdout,"\tVTS_%02i_0.BUP\t%i\n", i + 1, title_set_info->title_set[i + 1].size_bup);
	}

	fprintf(stdout,"\n\nMain feature:\n");
	fprintf(stdout,"\tTitle set containing the main feature is  %d\n", titles_info->main_title_set);
	for (i=0; i < titles_info->number_of_titles ; i++ ) {
		if (titles_info->titles[i].title_set == titles_info->main_title_set) {
			if(titles_info->titles[i].aspect_ratio == 3) {
				fprintf(stdout,"\tThe aspect ratio of the main feature is 16:9\n");
			} else if (titles_info->titles[i].aspect_ratio == 0) {
				fprintf(stdout,"\tThe aspect ratio of the main feature is 4:3\n");
			} else {
				fprintf(stdout,"\tThe aspect ratio of the main feature is unknown\n");
			}
			fprintf(stdout,"\tThe main feature has %d angle(s)\n", titles_info->titles[i].angles);
			fprintf(stdout,"\tThe main feature has %d audio_track(s)\n", titles_info->titles[i].angles);
			fprintf(stdout,"\tThe main feature has %d subpicture channel(s)\n",titles_info->titles[i].sub_pictures);
			chapters=0;
			channels=0;

			for (f=0; f < titles_info->number_of_titles ; f++ ) {
                        	if ( titles_info->titles[i].title_set == titles_info->main_title_set ) {
                                	if(chapters < titles_info->titles[f].chapters) {
                                        	chapters = titles_info->titles[f].chapters;
					}
					if(channels < titles_info->titles[f].audio_channels) {
						channels = titles_info->titles[f].audio_channels;
					}
				}
			}
			fprintf(stdout,"\tThe main feature has a maximum of %d chapter(s) in on of it's titles\n", chapters);
			fprintf(stdout,"\tThe main feature has a maximum of %d audio channel(s) in on of it's titles\n", channels);
			break;
                }

        }

	fprintf(stdout,"\n\nTitle Sets:");
	for (f=0; f < title_set_info->number_of_title_sets ; f++ ) {
		fprintf(stdout,"\n\n\tTitle set %d\n", f + 1);
		for (i=0; i < titles_info->number_of_titles ; i++ ) {
			if (titles_info->titles[i].title_set == f + 1) {
				if(titles_info->titles[i].aspect_ratio == 3) {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is 16:9\n", f + 1);
				} else if (titles_info->titles[i].aspect_ratio == 0) {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is 4:3\n", f + 1);
				} else {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is unknown\n", f + 1);
				}
				fprintf(stdout,"\t\tTitle set %d has %d angle(s)\n", f + 1, titles_info->titles[i].angles);
				fprintf(stdout,"\t\tTitle set %d has %d audio_track(s)\n", f + 1, titles_info->titles[i].angles);
				fprintf(stdout,"\t\tTitle set %d has %d subpicture channel(s)\n", f + 1, titles_info->titles[i].sub_pictures);
				break;
			}
		}
		fprintf(stdout,"\n\t\tTitles included in title set %d is/are\n", f + 1);
		for (i=0; i < titles_info->number_of_titles ; i++ ) {
			if (titles_info->titles[i].title_set == f + 1) {
				fprintf(stdout,"\t\t\tTitle %d:\n", i + 1);
				fprintf(stdout,"\t\t\t\tTitle %d has %d chapter(s)\n", i + 1, titles_info->titles[i].chapters);
				fprintf(stdout,"\t\t\t\tTitle %d has %d audio channle(s)\n", i + 1, titles_info->titles[i].audio_channels);
			}
		}
	}
	DVDFreeTitlesInfo(titles_info);
	DVDFreeTitleSetInfo(title_set_info);

	return(0);
}


int Get_DVD_Size(dvd_reader_t * _dvd) {

	unsigned int i;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
		
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;

	if(i == 0) {
		fprintf(stderr, "Get_DVD_Size Error : Can't find partition\n");
		return 0;	
	}
	
	fprintf(stderr, "\n\nGet_DVD_Size  Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscBlockSize %d\n",(TotalDiscSize * 4));
	
	return (TotalDiscSize * 4);
}

int DVDcopyUnscambledBlockjuke(dvd_reader_t * dvd, int FD, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	//int copyed = 0;

	char * buff = malloc(MAX_DATA_SIZE);
	//time_t t,g;
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
//	fprintf(stderr, "COPY Unscambled Address Area\n");
//	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d)\n", 
//		entry->startlb,	entry->lbcount, entry->type, entry->title);
	if(!buff){
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff,0,MAX_DATA_SIZE);

		//time(&t);
		if(!DVDReadBlocksUDFRaw( dvd, startlb, size, buff, 0))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);
			return 0;
		}
		//time(&g);

		//fprintf(stderr,"READ TIME: t(%d)\n",t);
		//fprintf(stderr,"READ TIME: g(%d)\n",g);
		

		//time(&t);
		/*
		if(write(file,buff,size*2048) != size*2048)
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		*/
		if( JKBox_write(FD,buff,size) != size)
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}

		//time(&g);
			
		//fprintf(stderr,"WRITE TIME: t(%d)\n",t);
		//fprintf(stderr,"WRITE TIME: g(%d)\n",g);
		
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}

int DVDcopyScambledBlockjuke(dvd_reader_t * dvd, int FD, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	//int copyed = 0;
	int csstitle = 0;
	//time_t t,g;

	char * buff = malloc(MAX_DATA_SIZE);
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
//	fprintf(stderr, "COPY Scambled Address Area\n");
//	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d) INDEX(%d)\n", 
//		entry->startlb,	entry->lbcount, entry->type, entry->title, entry->index);

	if(!buff) {
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;
	}

	if(entry->type == MENU_VOB)
	{
		csstitle =( (((int)entry->title) << 1) | 1 );
	}else{
		csstitle = ((int)entry->title) << 1;
	}
	
	/* set decrypte key */		
	if(GetCssTitle(dvd) != csstitle)
	{
		SetCssTitle(dvd, csstitle);
		DoDvdTitle(dvd, (int)entry->startlb);	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff, 0, MAX_DATA_SIZE);


		//time(&t);
		if(!DVDReadBlocksUDFRaw( dvd, startlb, size, buff, 1))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		//time(&g);

		//fprintf(stderr,"READ TIME: t(%d)\n",t);
		//fprintf(stderr,"READ TIME: g(%d)\n",g);
		
		//time(&t);
		/*
		if(write(file,buff,size*2048) != size*2048)
		{
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		*/
		if( JKBox_write(FD,buff,size) != size)
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		//time(&g);
		
		//fprintf(stderr,"WRITE TIME: t(%d)\n",t);
		//fprintf(stderr,"WRITE TIME: g(%d)\n", g);
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}

int DVDcopyDiscToISOjuke(dvd_reader_t * _dvd, int FD, AddressMap * Map)
{
	int CountOfAddressMap = 0;
	//int result;
	int Count = 0;
	AddressEntry *head, *p;

	CountOfAddressMap = Map->EntryCount;
	head = &(Map->Head);
	p = head->next;

	while(p != head)
	{
		Count++;
//		fprintf(stderr, "CopyAddress Entry(%d)\n",Count);

		switch(p->type)
		{
		case PLANE_AREA :
		case IFO :
		case BUP:
			if(!DVDcopyUnscambledBlockjuke(_dvd, FD, p))
			{
				return 0;
			}
			break;		
		case TITLE_VOB :
		case MENU_VOB :
			if(!DVDcopyScambledBlockjuke(_dvd, FD, p))
			{
				return 0;
			}
			break;		
		default:
			fprintf(stderr,"Error No Address type\n");
			return 0;
			break; 
		}
		p = p->next;
	}

	return 1;		
}



int COPY_TO_DEV(dvd_reader_t * _dvd, int FD ) {

	unsigned int i;
	DVDRip_title_set_info_t *title_set_info=NULL;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
	AddressMap		*Map = NULL;
	//HANDLE 			file;
	
	
	title_set_info = DVDGetFileSet_image(_dvd);
	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}
	
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;
	
	
//	fprintf(stderr, "\n\nDVDMirror Image Address Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscBlockSize(per 2048) %d\n",TotalDiscSize);

	if((Map = GenerateAddressMap(title_set_info, TotalDiscSize)) == NULL)
	{
		fprintf(stderr,"Cant Get AddressMap\n");
		DVDFreeTitleSetInfo_image(title_set_info);
		return(1);	
	} 		
	DVDFreeTitleSetInfo_image(title_set_info);	
	PrintAddressMap(Map);

	
/*
	if( (file = open(targetdevfile, O_WRONLY)) == -1)
	{
		fprintf(stderr, "Error openning %s\n",targetdevfile);
		FreeAddressMap(Map);
		return(1);
	}	

	file = CreateFile(targetdevfile,           // create MYFILE.TXT 
							GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_RANDOM_ACCESS, NULL );

	if (file == INVALID_HANDLE_VALUE) 
	{ 
		fprintf(stderr,"Error openning %s\n",targetdevfile);
		FreeAddressMap(Map);
		return(1);
	}
*/


	/* generate all keys */
	initAllCSSKeys(_dvd);
	fprintf(stderr, "dvd->css_state %d\n", SetCssState(_dvd, 2));

	
	/* read and copy */
	if(!DVDcopyDiscToISOjuke(_dvd, FD, Map))
	{
		fprintf(stderr, "DVDcopyDiscToISO Fail!\n");
		FreeAddressMap(Map);
		return(1);
	}
	
	FreeAddressMap(Map);
	
	return(0);
}


int GetDVDSize( char * Sdevfile, int * size)
{
	dvd_reader_t * _dvd = NULL;
	unsigned int 		TotalDiscSize = 0;
	
	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDSize Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}	

	if(!(TotalDiscSize = Get_DVD_Size(_dvd)))
	{
		fprintf(stderr, "GetDVDSize Fail Get_DVD_Size\n");
		DVDClose(_dvd);
		return -1;
	}
	
	*size = TotalDiscSize;
	DVDClose(_dvd);
	return 0;
	
}

int GetDVDInfo( char * Sdevfile)
{
	dvd_reader_t * _dvd = NULL;
	
	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDSize Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}	

	DVDDisplayInfo(_dvd, Sdevfile);
	DVDClose(_dvd);
	return 0;
	
}


int SetDVDCopytoMedia( char * Sdevfile, int FD)
{
	dvd_reader_t * _dvd = NULL;
	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDCopytoMedia Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}
	
	if(COPY_TO_DEV(_dvd, FD) != 0)
	{
		fprintf(stderr, "GetDVDCopytoMedia Error : COPY_TO_DEV fail!\n");
		DVDClose(_dvd);	
		return -1;
	} 	
	DVDClose(_dvd);	
	return 0;
}


int FileDVDCopytoMedia( char * Sdevfile, char * Directory)
{
	dvd_reader_t * _dvd = NULL;
	char title[50];

	if( DVDGetTitleName(Sdevfile, title) != 0)
	{
		fprintf(stderr, "GetDVDTitleName fail get title %s\n", Sdevfile);
		return -1;
	}

	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDCopytoMedia Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}
	
	if(DVDMirror_image(_dvd, Directory,title) != 0)
	{
		fprintf(stderr, "GetDVDCopytoMedia Error : DVDMirror_image fail!\n");
		DVDClose(_dvd);	
		return -1;
	} 	
	DVDClose(_dvd);	
	return 0;
}


int GetDVDTitleName(char * Sdevfile, char * buf)
{
	if( DVDGetTitleName(Sdevfile, buf) != 0)
	{
		fprintf(stderr, "GetDVDTitleName fail get title %s\n", Sdevfile);
		return -1;
	}
	
	//fprintf(stderr, "Tilte name %s\n", buf);
	return 0;
}
