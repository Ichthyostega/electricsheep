/*
 * main.c --
 *
 *      Main procedure
 *
 */

/*
 * Copyright (c) 1995 The Regents of the University of California.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

/*
 * Portions of this software Copyright (c) 1995 Brown University.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement
 * is hereby granted, provided that the above copyright notice and the
 * following two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL BROWN UNIVERSITY BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF BROWN
 * UNIVERSITY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * BROWN UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
 * BASIS, AND BROWN UNIVERSITY HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include "video.h"
#include "proto.h"
#ifndef NOCONTROLS
#include "ctrlbar.h"
#endif
#include <math.h>
#include <sys/types.h>
#include <signal.h>
#include <netinet/in.h>
#include <string.h> /* strtok */
#include "util.h"
#include "dither.h"

#ifdef MPEG_EXPORT
#include "mpeg_export.h"
#endif

/*
   Changes to make the code reentrant:
     Got rid of setjmp, longjmp
     deglobalized: EOF_flag, FilmState, curVidStream, bitOffset, bitLength,
     bitBuffer, sys_layer, input, seekValue, window, X Windows globals (to
     xinfo), curBits, ditherType, matched_depth, totNumFrames, realTimeStart

   Additional changes:
     Ability to play >1 movie (w/out CONTROLS)
     Make sure we do a full frame for each movie
     DISABLE_DITHER #ifdef to avoid compiling dithering code
     Changes to deal with non-MPEG streams
     Now deals with NO_DITHER, PPM_DITHER and noDisplayFlag==1
     CONTROLS version now can deal with >1 movie
   -lsh@cs.brown.edu (Loring Holden)
 */

/* Make Ordered be the default dither */
#define DEFAULT_ORDERED_DITHER

/* Define buffer length. */
#define BUF_LENGTH 80000

/* Function return type declarations */
void usage();

/* Forward declaration of functions in this file. */
#ifndef P
# ifdef __STDC__
#   define        P(s) s
# else
#   define P(s) ()
# endif
#endif 

#ifndef SIG_ONE_PARAM
void int_handler P((void ));
void bad_handler P((void ));
#else
void int_handler P((int signum));
void bad_handler P((int signum));
#endif
void usage P((char *s ));

#ifdef DCPREC
/* Declaration of global variable to hold DC precision */
int dcprec = 0;
#endif

/* Global file pointer to incoming data. */
FILE **input;
char **inputName;

/* Loop flag. */
int loopFlag = 1;

/* -1 means no limit, otherwise number of times to loop */
int nitersFlag = -1;

/* Shared memory flag. */
int shmemFlag = 0;

int unlinkFlag = 0;
int rootFlag = 0;


/* Quiet flag. */
#ifdef QUIET
int quietFlag = 1;
#else
int quietFlag = 1;
#endif

/* "Press return" flag, requires return for each new frame */
int requireKeypressFlag = 0;

/* Display image on screen? */
int noDisplayFlag = 0;

/* Seek Value. 
   0 means do not seek.
   N (N>0) means seek to N after the header is parsed
   N (N<0) means the seek has beeen done to offset N
*/

/* Framerate, -1: specified in stream (default)
               0: as fast as possible
               N (N>0): N frames/sec  
               */
int framerate = -1;

/* Flags/values to control Arbitrary start/stop frames. */
int partialFlag = 0, startFrame = -1, endFrame = -1;

/* Flag for gamma correction */
int gammaCorrectFlag = 0;
double gammaCorrect = 1.0;

/* Flag for chroma correction */
int chromaCorrectFlag = 0;
double chromaCorrect = 1.0;

/* Flag for high quality at the expense of speed */
#ifdef QUALITY
int qualityFlag = 1;
#else
int qualityFlag = 0;
#endif

/* no further error messages */
static BOOLEAN exiting=FALSE;

/* global variable for interrupt handlers */
VidStream **curVidStream;

/* Brown - put X specific variables in xinfo struct */
#define NUMMOVIES 15
XInfo xinfo[NUMMOVIES];
int numInput=0;

/*
 #define Color16DitherImage ColorDitherImage
 #define Color32DitherImage ColorDitherImage
*/

/*
 *--------------------------------------------------------------
 *
 * int_handler --
 *
 *        Handles Cntl-C interupts..
 *      (two different ones for different OSes)
 * Results:    None.
 * Side effects:   None.
 *--------------------------------------------------------------
 */
#ifndef SIG_ONE_PARAM
void
int_handler()
#else
void
int_handler(signum)
int signum;
#endif
{
  int i,displayClosed=0;
#ifdef MPEG_EXPORT
	if(export_data.export_it)
	{
		exporter_end();
	}
#endif

  if (!quietFlag && !exiting) {
    fprintf(stderr, "Interrupted!\n");
  }
  exiting = TRUE;
  for (i = 0;  i < numInput;  i++) {
    if (curVidStream[i] != NULL)
      DestroyVidStream(curVidStream[i], &xinfo[i]);
    if ((xinfo[i].display != NULL) && !displayClosed) {
      XCloseDisplay(xinfo[i].display);
      displayClosed=1;
    }
  }
  exit(1);
}


/*
 *--------------------------------------------------------------
 *
 * bad_handler --
 *
 *        Handles Seg faults/bus errors...
 *      (two different ones for different OSes)
 * Results:    None.
 * Side effects:   None.
 *
 *--------------------------------------------------------------
 */

#ifndef SIG_ONE_PARAM
void
  bad_handler()
#else
void
  bad_handler(signum)
int signum;
#endif
{
  if (!exiting) {
    fprintf(stderr, "Bad MPEG?  Giving up.\ntry 'mpeg_stat -verify' to see if the stream is valid.\n");
  }
  exit(1);
}


/*
 *--------------------------------------------------------------
 *
 * getposition --
 *
 *--------------------------------------------------------------
 */
void getposition(arg, xpos, ypos)
char *arg;
int *xpos, *ypos;
{
  char *pos;

  if ((pos = strtok(arg, "+-")) != NULL) {
    *xpos = atoi(pos);
    if ((pos = strtok(NULL, "+-")) != NULL) {
      *ypos = atoi(pos);
      return;
    }
  }
  if (!quietFlag) {
    fprintf(stderr, "Illegal position... Warning: argument ignored! (-position +x+y)\n");
  }
  return;
}


/*
 *--------------------------------------------------------------
 *
 * main --
 *
 *        Parses command line, starts decoding and displaying.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *--------------------------------------------------------------
 */

#ifndef __STDC__
void
#endif
main(argc, argv)
     int argc;
     char **argv;
{

  char *name;
  static VidStream **theStream;
  int mark;
  int i, mult, largy, y, lastStream, firstStream=-1, workToDo=TRUE;
  int doDisplay=0; /* Current movie is displaying on screen */ 
  long seekValue=0;/* holds value before it is put in vid_stream */
  int  owncmFlag=0;   /* holds value before it is put in xinfo  */
  BOOLEAN firstRead=FALSE;
  int ppm_width = -1,  ppm_height = -1, ppm_modulus = -1;

  mark = 1;
  argc--;

  input = (FILE **) malloc(NUMMOVIES*sizeof(FILE *));
  inputName = (char **) malloc(NUMMOVIES *sizeof(char *));
  theStream = (VidStream **) malloc(NUMMOVIES *sizeof(VidStream *));
  curVidStream = (VidStream **) malloc(NUMMOVIES *sizeof(VidStream *));
  for (i = 0; i < NUMMOVIES; i++) {
     input[i] = NULL;
     inputName[i] = "stdin";
     theStream[i] = NULL;
     curVidStream[i] = NULL;
     xinfo[i].hints.x = -1;
     xinfo[i].hints.y = -1;
     xinfo[i].ExistingWindow = 0;
  }
  name = (char *) "";

#ifndef DISABLE_DITHER
#ifndef DEFAULT_ORDERED_DITHER
  xinfo[0].ditherType = FULL_COLOR_DITHER;
#else
  xinfo[0].ditherType = ORDERED_DITHER;
#endif
#endif

  xinfo[0].ditherType = FULL_COLOR_DITHER;

  LUM_RANGE = 8;
  CR_RANGE = CB_RANGE = 4;
  noDisplayFlag = 0;

#ifdef SH_MEM
  shmemFlag = 1;
#endif

#ifdef MPEG_EXPORT
	exporter_init();
#endif

  while (argc) {
    if (strcmp(argv[mark], "-nop") == 0) {
      SetPFlag(TRUE);
      SetBFlag(TRUE);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-nob") == 0) {
      SetBFlag(TRUE);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-display") == 0) {
      name = argv[++mark];
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-position") == 0) {
      argc--; mark++;
      getposition(argv[mark], &xinfo[numInput].hints.x, &xinfo[numInput].hints.y);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-xid") == 0) {
      xinfo[numInput].ExistingWindow = atoi(argv[++mark]);
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-start") == 0) {
      if (argc < 2) usage(argv[0]);
      partialFlag = TRUE;
      if (seekValue != 0) {
	fprintf(stderr, "Cannot use -start with -seek (ignored)\n");
      } else {
	startFrame = atoi(argv[++mark]);
      }
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-seek") == 0) {
      if (argc < 2) usage(argv[0]);
      seekValue = atoi(argv[++mark]);
      if (startFrame != -1) startFrame = 0;
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-end") == 0) {
      if (argc < 2) usage(argv[0]);
      endFrame = atoi(argv[++mark]);
      partialFlag = TRUE;
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-gamma") == 0) {
      if (argc < 2) usage(argv[0]);
      gammaCorrectFlag = 1;
      sscanf(argv[++mark], "%lf", &gammaCorrect);
      if (gammaCorrect <= 0.0) {
        fprintf(stderr, "ERROR: Gamma correction must be greater than 0.\n");
        gammaCorrect = 1.0;
      }
      if (!quietFlag) {
        printf("Gamma Correction set to %4.2f.\n", gammaCorrect);
      }
      argc -= 2; mark++;
    } else if (strcmp(argv[mark], "-chroma") == 0) {
      if (argc < 2) usage(argv[0]);
      chromaCorrectFlag = 1;
      sscanf(argv[++mark], "%lf", &chromaCorrect);
      if (chromaCorrect <= 0.0) {
        fprintf(stderr, "ERROR: Chroma correction must be greater than 0.\n");
        chromaCorrect = 1.0;
      }
      if (!quietFlag) {
        printf("Chroma Correction set to %4.2f.\n",chromaCorrect);
      }
      argc -= 2; mark++;
#ifdef DCPREC
    } else if (strcmp(argv[mark], "-dc") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify dc precision after -dc flag");
        usage(argv[0]);
      }
      dcprec = atoi(argv[mark]) - 8;
      if ((dcprec > 3) || (dcprec < 0)) {
        perror("DC precision must be at least 8 and at most 11");
        usage(argv[0]);
      }
      argc--; mark++;
#endif
    } else if (strcmp(argv[mark], "-quality") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify on or off after -quality flag");
        usage(argv[0]);
      }
      if (strcmp(argv[mark], "on") == 0) {
        argc--; mark++;
        qualityFlag = 1;
      }
      else if (strcmp(argv[mark], "off") == 0) {
        argc--; mark++;
        qualityFlag = 0;
      }
      else {
        perror("Must specify on or off after -quality flag");
        usage(argv[0]);
      }
    } else if (strcmp(argv[mark], "-iters") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify an integer after -iters flag");
        usage(argv[0]);
      }
      nitersFlag = atoi(argv[mark]);
      argc--; mark++;
      if (0) {
        perror("Must specify on or off after -iters flag");
        usage(argv[0]);
      }
    } else if (strcmp(argv[mark], "-framerate") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify framerate after -framerate flag");
        usage(argv[0]);
      }
      framerate = atoi(argv[mark]);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-ppmwidth") == 0) {
      argc--; mark++;
      if (argc < 1) {
	perror("Must specify PPM file width after -ppmwidth flag");
	usage(argv[0]);
      }
      ppm_width = atoi(argv[mark]);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-ppmheight") == 0) {
      argc--; mark++;
      if (argc < 1) {
	perror("Must specify PPM file height after -ppmheight flag");
	usage(argv[0]);
      }
      ppm_height = atoi(argv[mark]);
      argc--; mark++;
    } else if (strcmp(argv[mark], "-ppmskip") == 0) {
      argc--; mark++;
      if (argc < 1) {
	perror("Must specify PPM file height after -ppmheight flag");
	usage(argv[0]);
      }
      ppm_modulus = atoi(argv[mark]);
      argc--; mark++;
#ifndef DISABLE_DITHER
    } else if (strcmp(argv[mark], "-dither") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify dither option after -dither flag");
        usage(argv[0]);
      }
      if (strcmp(argv[mark], "hybrid") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = HYBRID_DITHER;
      } else if (strcmp(argv[mark], "hybrid2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = HYBRID2_DITHER;
      } else if (strcmp(argv[mark], "fs4") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = FS4_DITHER;
      } else if (strcmp(argv[mark], "fs2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = FS2_DITHER;
      } else if (strcmp(argv[mark], "fs2fast") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = FS2FAST_DITHER;
      } else if (strcmp(argv[mark], "hybrid2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = HYBRID2_DITHER;
      } else if (strcmp(argv[mark], "2x2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = Twox2_DITHER;
      } else if ((strcmp(argv[mark], "gray256") == 0) ||
                 (strcmp(argv[mark], "grey256") == 0)) {
        argc--; mark++;
        xinfo[0].ditherType = GRAY256_DITHER;
      } else if ((strcmp(argv[mark], "gray") == 0) ||
                 (strcmp(argv[mark], "grey") == 0)) {
        argc--; mark++;
        xinfo[0].ditherType = GRAY_DITHER;
      } else if ((strcmp(argv[mark], "gray256x2") == 0) ||
                  (strcmp(argv[mark], "grey256x2") == 0)) {
        argc--; mark++;
        xinfo[0].ditherType = GRAY2562_DITHER;
      } else if ((strcmp(argv[mark], "gray") == 0) ||
                   (strcmp(argv[mark], "grey") == 0)) {
        argc--; mark++;
        xinfo[0].ditherType = GRAY_DITHER;
      } else if ((strcmp(argv[mark], "gray2") == 0) ||
                  (strcmp(argv[mark], "grey2") == 0)) {
        argc--; mark++;
        xinfo[0].ditherType = GRAY2_DITHER;
      } else if (strcmp(argv[mark], "color") == 0 ||
                 strcmp(argv[mark], "colour") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = FULL_COLOR_DITHER;
      } else if (strcmp(argv[mark], "color2") == 0 ||
                 strcmp(argv[mark], "colour2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = FULL_COLOR2_DITHER;
      } else if (strcmp(argv[mark], "none") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = NO_DITHER;
      } else if (strcmp(argv[mark], "ppm") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = PPM_DITHER;
      } else if (strcmp(argv[mark], "ordered") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = ORDERED_DITHER;
      } else if (strcmp(argv[mark], "ordered2") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = ORDERED2_DITHER;
      } else if (strcmp(argv[mark], "mbordered") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = MBORDERED_DITHER;
      } else if (strcmp(argv[mark], "mono") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = MONO_DITHER;
      } else if (strcmp(argv[mark], "threshold") == 0) {
        argc--; mark++;
        xinfo[0].ditherType = MONO_THRESHOLD;
      } else {
        perror("Illegal dither option.");
        usage(argv[0]);
      }
#endif
    } 
    else if (strcmp(argv[mark], "-eachstat") == 0) {
      argc--; mark++;
#ifdef ANALYSIS
      showEachFlag = 1;
#else
      fprintf(stderr, "To use -eachstat, recompile with -DANALYSIS in CFLAGS\n");
      exit(1);
#endif
    }
    else if (strcmp(argv[mark], "-shmem_off") == 0) {
      argc--; mark++;
      shmemFlag = 0;
    }
#ifdef QUIET
    else if (strcmp(argv[mark], "-quiet") == 0) { 
      argc--; mark++;
    }
    else if (strcmp(argv[mark], "-noisy") == 0) {
#else
    else if (strcmp(argv[mark], "-noisy") == 0) {
      argc--; mark++;
    }
    else if (strcmp(argv[mark], "-quiet") == 0) { 
#endif
      argc--; mark++;
      quietFlag = !quietFlag;
    }
    else if (strcmp(argv[mark], "-owncm") == 0) {
      argc--; mark++;
      owncmFlag = 1;
    }
    else if (strcmp(argv[mark], "-step") == 0) {
      argc--; mark++;
      requireKeypressFlag = 1;
    }
    else if (strcmp(argv[mark], "-loop") == 0) {
      argc--; mark++;
      loopFlag = 1;
    }
    else if (strcmp(argv[mark], "-unlink") == 0) {
      argc--; mark++;
      unlinkFlag = 1;
    }
    else if (strcmp(argv[mark], "-root") == 0) {
      argc--; mark++;
      rootFlag = 1;
    }
    else if (strcmp(argv[mark], "-no_display") == 0) {
      argc--; mark++;
      noDisplayFlag = 1;
      shmemFlag = 0;
    }
#ifdef MPEG_EXPORT
	else if(!strcmp(argv[mark], "-e"))
	{
  	    argc--; mark++;
 		if(argv[mark][0] >= '0' && argv[mark][0] <= '9')
 		{
 			exporter_set_vid_compressor(exporter_int_to_codec(atol(argv[mark])));
 			argc--; mark++;
 		}
		export_data.export_it = 1;
//		noDisplayFlag = 1;
// 	    shmemFlag = 0;
	}
#endif
    else if (strcmp(argv[mark], "-l_range") == 0) {
      argc--; mark++;
      LUM_RANGE = atoi(argv[mark]);
      if (LUM_RANGE < 1) {
        fprintf(stderr, "Illegal luminance range value: %d\n", LUM_RANGE);
        exit(1);
      }
      argc--; mark++;
    }
    else if (strcmp(argv[mark], "-cr_range") == 0) {
      argc--; mark++;
      CR_RANGE = atoi(argv[mark]);
      if (CR_RANGE < 1) {
        fprintf(stderr, "Illegal cr range value: %d\n", CR_RANGE);
        exit(1);
      }
      argc--; mark++;
    }
    else if (strcmp(argv[mark], "-cb_range") == 0) {
      argc--; mark++;
      CB_RANGE = atoi(argv[mark]);
      if (CB_RANGE < 1) {
        fprintf(stderr, "Illegal cb range value: %d\n", CB_RANGE);
        exit(1);
      }
      argc--; mark++;
    } 
#ifndef NOCONTROLS
    else if (strcmp(argv[mark], "-controls") == 0 || 
	     strcmp(argv[mark], "-controlbar") == 0 || 
	     strcmp(argv[mark], "-control_bar") == 0) {
      argc--; mark++;
      if (argc < 1) {
        perror("Must specify on, off, or none after -controls flag");
        usage(argv[0]);
      }
      if (strcmp(argv[mark], "on") == 0) {
        argc--; mark++;
        ControlShow = CTRLBAR_ON;
      }
      else if (strcmp(argv[mark], "off") == 0) {
        argc--; mark++;
        ControlShow = CTRLBAR_OFF;
      }
      else if (strcmp(argv[mark], "none") == 0) {
        argc--; mark++;
        ControlShow = CTRLBAR_NONE;
      }
      else {
        perror("Must specify on, off, or none after -controls flag");
        usage(argv[0]);
      }
    }
#endif /* !NOCONTROLS */
    else if ((strcmp(argv[mark], "-?") == 0) ||
               (strcmp(argv[mark], "-Help") == 0) ||
               (strcmp(argv[mark], "-help") == 0)) {
      usage(argv[0]);
    }
    else if (argv[mark][0] == '-' && argv[mark][1]==0) {
      fflush(stdout);
      if (numInput<NUMMOVIES) {
        input[numInput]=stdin;
        inputName[numInput++] = "stdin";
      } else {
          fprintf(stderr, "Can't load file %s - too many\n", "stdin");
      }
      argc--; mark++;
    }
    else if (argv[mark][0] == '-') {
      fprintf(stderr, "Un-recognized flag %s\n",argv[mark]);
      usage(argv[0]);
    }
    else {
      fflush(stdout);
      if (numInput<NUMMOVIES) {
        input[numInput]=fopen(argv[mark], "r");
	if (unlinkFlag)
	  unlink(argv[mark]);
        if (input[numInput] == NULL) {
          fprintf(stderr, "Could not open file %s\n", argv[mark]);
          usage(argv[0]);
        }
        inputName[numInput++] = argv[mark];
      } else {
          fprintf(stderr, "Can't load file %s - too many\n", argv[mark]);
      }
      argc--; mark++;
    }
  }

  lum_values = (int *) malloc(LUM_RANGE*sizeof(int));
  cr_values = (int *) malloc(CR_RANGE*sizeof(int));
  cb_values = (int *) malloc(CB_RANGE*sizeof(int));

  signal(SIGINT, int_handler);
#ifndef DEBUG
  signal(SIGSEGV, bad_handler);
  signal(SIGBUS,  bad_handler);
#endif
  if ((startFrame != -1) && (endFrame != -1) &&
      (endFrame < startFrame)) {
    usage(argv[0]);
  }

  init_tables();
  for (i = 0;  i < numInput;  i++) {
    xinfo[i].owncmFlag = owncmFlag;
    xinfo[i].display = NULL;       /* xinfo.ximage is set to null later */
    if (xinfo[i].hints.x == -1) {
      xinfo[i].hints.x = 200;
      xinfo[i].hints.y = 300;
    }
    xinfo[i].hints.width = 150;
    xinfo[i].hints.height = 150;
    xinfo[i].visual = NULL;
    xinfo[i].name = inputName[i];
    xinfo[i].cmap = 0;
    xinfo[i].gc = 0;
  }

#ifndef DISABLE_DITHER
  if (xinfo[0].ditherType == MONO_DITHER ||
      xinfo[0].ditherType == MONO_THRESHOLD)
    xinfo[0].depth= 1;

  switch (xinfo[0].ditherType) {
    
  case HYBRID_DITHER:
    InitColor();
    InitHybridDither();
    InitDisplay(name, &xinfo[0]);
    break;
    
  case HYBRID2_DITHER:
    InitColor();
    InitHybridErrorDither();
    InitDisplay(name, &xinfo[0]);
    break;
    
  case FS4_DITHER:
    InitColor();
    InitFS4Dither();
      InitDisplay(name, &xinfo[0]);
    break;
    
  case FS2_DITHER:
    InitColor();
    InitFS2Dither();
    InitDisplay(name, &xinfo[0]);
    break;
    
  case FS2FAST_DITHER:
    InitColor();
    InitFS2FastDither();
    InitDisplay(name, &xinfo[0]);
    break;
    
  case Twox2_DITHER:
    InitColor();
    Init2x2Dither();
    InitDisplay(name, &xinfo[0]);
    PostInit2x2Dither();
    break;

  case GRAY_DITHER:
  case GRAY2_DITHER:
    InitGrayDisplay(name, &xinfo[0]);
    break;

  case GRAY256_DITHER:
  case GRAY2562_DITHER:
    InitGray256Display(name, &xinfo[0]);
    break;

  case FULL_COLOR_DITHER:
  case FULL_COLOR2_DITHER:
    InitColorDisplay(name, &xinfo[0]);
    InitColorDither(xinfo[0].depth>=24);
#else
    InitColorDisplay(name, &xinfo[0]);
    InitColorDither(xinfo[0].depth>=24);
#endif
#ifndef DISABLE_DITHER
    break;

  case NO_DITHER:
    shmemFlag = 0;
    break;

  case PPM_DITHER:
    shmemFlag = 0;
    wpixel[0] = 0xff;
    wpixel[1] = 0xff00;
    wpixel[2] = 0xff0000;
    xinfo[0].depth = 24;
    InitColorDither(1);
    break;

  case ORDERED_DITHER:
    InitColor();
    InitOrderedDither();
    InitDisplay(name, &xinfo[0]);
    break;

  case MONO_DITHER:
  case MONO_THRESHOLD:
    InitMonoDisplay(name, &xinfo[0]);
    break;

  case ORDERED2_DITHER:
    InitColor();
    InitDisplay(name, &xinfo[0]);
    InitOrdered2Dither();
    break;

  case MBORDERED_DITHER:
    InitColor();
    InitDisplay(name, &xinfo[0]);
    InitMBOrderedDither();
    break;
  }
#endif


#ifdef SH_MEM
    if (shmemFlag && (xinfo[0].display != NULL)) {
      if (!XShmQueryExtension(xinfo[0].display)) {
        shmemFlag = 0;
        if (!quietFlag) {
          fprintf(stderr, "Shared memory not supported\n");
          fprintf(stderr, "Reverting to normal Xlib.\n");
        }
      }
    }
#endif

  InitCrop();

  y=300;
  largy=0;
  for (i=0;i<numInput;i++) {
    doDisplay=!noDisplayFlag;
#ifndef DISABLE_DITHER
    if ((xinfo[i].ditherType == NO_DITHER) ||
        (xinfo[i].ditherType == PPM_DITHER))
       doDisplay = FALSE;
#endif
    lastStream = i-1;
    while ((lastStream>=0) && (theStream[lastStream]==NULL)) {
       lastStream--;
    }
    if ((i != 0) && doDisplay) {
       if (lastStream > -1) {
         xinfo[i].hints.x =
            xinfo[lastStream].hints.x+10 + theStream[lastStream]->h_size;
         if (theStream[lastStream]->v_size>largy)
	   largy = theStream[lastStream]->v_size;
         if (xinfo[i].hints.x > DisplayWidth(xinfo[firstStream].display,
			       XDefaultScreen(xinfo[firstStream].display)) -80) {

		y += largy + 30;
		largy = 0;
		xinfo[i].hints.x = 0;
         }
         xinfo[i].hints.y = y;
         xinfo[i].visual = xinfo[firstStream].visual;
         xinfo[i].cmap = xinfo[firstStream].cmap;
         xinfo[i].gc = xinfo[firstStream].gc;
       }
       xinfo[i].display = xinfo[0].display;
       xinfo[i].depth = xinfo[0].depth;
       xinfo[i].ditherType = xinfo[0].ditherType;
       InitColorDisplay(name, &xinfo[i]);
    }
    curVidStream[i] = theStream[i] = NewVidStream((unsigned int) BUF_LENGTH);
    theStream[i]->ppm_width = ppm_width;
    theStream[i]->ppm_height = ppm_height;
    theStream[i]->ppm_modulus = ppm_modulus;
    theStream[i]->input = input[i];
    theStream[i]->seekValue = seekValue;
    theStream[i]->filename = inputName[i];
    theStream[i]->ditherType = xinfo[i].ditherType;
    theStream[i]->matched_depth = xinfo[i].depth;
    mark = quietFlag;
    quietFlag=1;
    if (mpegVidRsrc(0, theStream[i], 1, &xinfo[i])==NULL) {
       if (doDisplay) {
         XDestroyWindow(xinfo[i].display, xinfo[i].window);
       }	 
       /* stream has already been destroyed */
       curVidStream[i] = theStream[i]=NULL;
       fprintf(stderr, "Skipping movie %d, \"%s\" - not an MPEG stream\n",
	  i, inputName[i]);
       fclose(input[i]);
       if (i+1 == numInput) numInput--;
    } else if (firstStream == -1) firstStream=i;
    quietFlag = mark;

#ifndef DISABLE_DITHER
    if (IS_2x2_DITHER(xinfo[i].ditherType)) {
      mult = 2;
    }
    else {
      mult = 1;  
    }
#else
    mult = 1;
#endif

    if (!rootFlag && doDisplay && (theStream[i]!=NULL)) {
      ResizeDisplay((unsigned int) theStream[i]->h_size* mult,
                    (unsigned int) theStream[i]->v_size* mult,
		  &xinfo[i]);
    }
  }

  if (numInput > 1) {
    loopFlag = TRUE;
    framerate = 0;
  }

#ifndef NOCONTROLS
  if (xinfo[0].display == NULL) {
    ControlShow = CTRLBAR_NONE;  /* no display => no controls */
  }

  if (ControlShow != CTRLBAR_NONE) {
    MakeControlBar(&xinfo[0]);
    ControlBar(theStream, xinfo, numInput);
  }
  for (i = 0; i < numInput; i++) {
     if (theStream[i] != NULL) theStream[i]->realTimeStart = ReadSysClock();
  }
#else
  /* Start time for each movie - do after windows are mapped */
  for (i = 0; i < numInput; i++) {
     if (theStream[i] != NULL) theStream[i]->realTimeStart = ReadSysClock();
  }
#endif


#ifndef NOCONTROLS
  if (ControlShow == CTRLBAR_NONE) {
    while (TRUE) {
      for (i=0;i < numInput; i++) {
        while (theStream[i]->film_has_ended != TRUE) {
          mpegVidRsrc(0, theStream[i], 0, &xinfo[i]);
        }
        if (loopFlag && (-1 == nitersFlag || nitersFlag--)) {
          rewind(theStream[i]->input); 
          ResetVidStream(theStream[i]); /* Reinitialize vid_stream pointers */
          if (theStream[i]->seekValue < 0) {
            theStream[i]->seekValue = 0 - theStream[i]->seekValue;
          }
          mpegVidRsrc(0, theStream[i], 1, &xinfo[i]); /* Process start codes */
        } else if (doDisplay) break;
        else goto done;
      }
     }
   }
  else {
    ControlLoop(theStream, xinfo, numInput);
  }

done:
#ifdef MPEG_EXPORT
	if(export_data.export_it)
	{
		exporter_end();
	}
#endif


  mark=0;
  for (i=0;i < numInput; i++) { 
    DestroyVidStream(theStream[i], &xinfo[i]);
    if ((xinfo[i].display != NULL) && !mark) {
      XCloseDisplay(xinfo[i].display);
      mark=1;
    }
  }
  exit(0);
#else /* !NOCONTROLS */
  if (!numInput) {
     fprintf(stderr, "Must enter MPEG file to play\n");
     usage(argv[0]);
  }
  while (workToDo) {
     workToDo = FALSE;
     for (i = 0; i < numInput; i++) {
       if (theStream[i] != NULL) {
         mark = theStream[i]->totNumFrames;
         /* make sure we do a whole frame */
         while (mark == theStream[i]->totNumFrames) {
             mpegVidRsrc(0, theStream[i], 0, &xinfo[i]);
         }
         if (theStream[i]->film_has_ended) {
           if (loopFlag && (-1 == nitersFlag || nitersFlag--)) {
             clear_data_stream(theStream[i]);
             /* Reinitialize vid_stream pointers */
             ResetVidStream(theStream[i]);
             rewind(theStream[i]->input);
             if (theStream[i]->seekValue < 0) {
               theStream[i]->seekValue = 0 - theStream[i]->seekValue;
             }
#ifdef ANALYSIS 
             init_stats();
#endif
             /* Process start codes */
             if (mpegVidRsrc(0, theStream[i], 1, &xinfo[i])==NULL) {
	       /* print something sensible here,
		  but we only get here if the file is changed while we
		  are decoding, right?
		*/
	     }
           } /* loopFlag */
	 }   /* film_has_ended */
         workToDo = workToDo || (!theStream[i]->film_has_ended);
       } /* theStream[i]!=NULL */
     }   /* for (i.. */
  }      /* while workToDo */
  sleep(1000);
    /* freeze on the last frame */
#endif /* NOCONTROLS */
}
 

/*
 *--------------------------------------------------------------
 *
 * usage --
 *
 *        Print mpeg_play usage
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        exits with a return value -1
 *
 *--------------------------------------------------------------
 */

void
usage(s)
char *s;        /* program name */
{
    fprintf(stderr, "Usage:\n");
#ifndef NOCONTROLS
    fprintf(stderr, "mpeg_play [options] [filename]\n");
#else
    fprintf(stderr, "mpeg_play [[options] [filename]]  [[options] [filename]]  [..]\n");
#endif
    fprintf(stderr, "Options :\n");
   fprintf(stderr, "      [-display X_display]\t[-no_display]\n");
#ifndef DISABLE_DITHER
    fprintf(stderr, "      [-dither {ordered|ordered2|mbordered|fs4|fs2|fs2fast|hybrid|\n");
    fprintf(stderr, "                hybrid2|2x2|gray|gray256|color|color2|none|mono|threshold|ppm|\n");
    fprintf(stderr, "                gray2|gray256x2}]\n");
#endif
    fprintf(stderr, "      [-loop]\n");
    fprintf(stderr, "      [-start frame_num]\t[-end frame_num]\t[-seek file_offset]\n");
    fprintf(stderr, "      [-gamma gamma_correction_value]\t[-chroma chroma_correction_value]\n");
    fprintf(stderr, "      [-framerate num_frames_per_sec]  (0 means as fast as possible)\n");
    fprintf(stderr, "      [-position +x+y]\t[-quality {on|off}] (compiled default is ");
#ifdef QUALITY
    fprintf(stderr, "ON)\n");
#else
    fprintf(stderr, "OFF)\n");
#endif
#ifdef QUIET
    fprintf(stderr, "      [-noisy] (turns on all program output)\n");
#else
    fprintf(stderr, "      [-quiet] (turns off all program output)\n");
#endif
#ifndef NOCONTROLS
    fprintf(stderr, "      [-controls {on|off|none}] (default is on)\n");
#endif
    fprintf(stderr, "      [-?]\t[-help] for help (this message)\n");
    fprintf(stderr, "Rare options:\n");
    fprintf(stderr, "      [-nob]\t[-nop]\n");
#ifdef ANALYSIS
    fprintf(stderr, "      [-eachstat]\t[-owncm]\t[-shmem_off]\n");
#else
    fprintf(stderr, "      [-owncm]\t[-shmem_off]\n");
#endif
    fprintf(stderr, "      [-l_range num]\t[-cr_range num]\t[-cb_range num]\n");
/*    fprintf(stderr, "      [-xid xid]\n"); */
#ifdef DCPREC
    fprintf(stderr, "      [-dc {8|9|10|11}] (defaults to 8)\n");
#endif
    fprintf(stderr, "      with -dither ppm:\n");
    fprintf(stderr, "          [-ppmwidth num]\t[-ppmheight num]\t[-ppmskip num]\n");
#ifdef MPEG_EXPORT
	fprintf(stderr, "\n      [-e codec] EXPORT TO QUICKTIME MOVIE\n"
					"      codec is a number designating the codec:\n"
					"      0 - RGB uncompressed (pointless)\n"
					"      1 - JPEG quality 100\n"
					"      2 - YUV 4:2:2\n"
					"      3 - YUV 4:2:0 (default)\n");
#endif
    exit (-1);
}



/*
 *--------------------------------------------------------------
 *
 * DoDitherImage --
 *
 *      Called when image needs to be dithered. Selects correct
 *      dither routine based on info in xinfo[0].ditherType.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        None.
 *
 *--------------------------------------------------------------
 */

void
DoDitherImage(vid_stream)
VidStream *vid_stream;
{
 unsigned char *l=vid_stream->current->luminance,
               *Cr=vid_stream->current->Cr,
               *Cb=vid_stream->current->Cb,
               *disp=vid_stream->current->display;
 int h=(int) vid_stream->mb_height * 16;
 int w=(int) vid_stream->mb_width * 16;
 int ditherType=vid_stream->ditherType;
 int matched_depth=vid_stream->matched_depth;

#ifndef DISABLE_DITHER
  switch(ditherType) {
  case HYBRID_DITHER:
    HybridDitherImage(l, Cr, Cb, disp, h, w);
    break;

  case HYBRID2_DITHER:
    HybridErrorDitherImage(l, Cr, Cb, disp, h, w);
    break;

  case FS2FAST_DITHER:
    FS2FastDitherImage(l, Cr, Cb, disp, h, w);
    break;

  case FS2_DITHER:
    FS2DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case FS4_DITHER:
    FS4DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case Twox2_DITHER:
    Twox2DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case FULL_COLOR2_DITHER:
    if (matched_depth == 32)
      Twox2Color32DitherImage(l, Cr, Cb, disp, h, w);
    else
      Twox2Color16DitherImage(l, Cr, Cb, disp, h, w);
    break;
  case FULL_COLOR_DITHER:
    if (matched_depth >= 24)
#endif

      Color32DitherImage(l, Cr, Cb, disp, h, w);

#ifndef DISABLE_DITHER
    else
      Color16DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case GRAY_DITHER:
  case GRAY256_DITHER:
    if (matched_depth == 8) 
      GrayDitherImage(l, Cr, Cb, disp, h, w);
    else if (matched_depth == 16) 
      Gray16DitherImage(l, Cr, Cb, disp, h, w);
    else if (matched_depth == 32 || matched_depth == 24)
      Gray32DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case GRAY2_DITHER:
  case GRAY2562_DITHER:
    if (matched_depth == 8) 
      Gray2DitherImage(l, Cr, Cb, disp, h, w);
    else if (matched_depth == 16) 
      Gray216DitherImage(l, Cr, Cb, disp, h, w);
    else if (matched_depth == 32 || matched_depth == 24)
      Gray232DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case NO_DITHER:
    break;

  case PPM_DITHER:
    Color32DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case ORDERED_DITHER:
    OrderedDitherImage(l, Cr, Cb, disp, h, w);
    break;

  case MONO_DITHER:
    MonoDitherImage(l, Cr, Cb, disp, h, w);
    break;

  case MONO_THRESHOLD:
    MonoThresholdImage(l, Cr, Cb, disp, h, w);
    break;

  case ORDERED2_DITHER:
    Ordered2DitherImage(l, Cr, Cb, disp, h, w);
    break;

  case MBORDERED_DITHER:
    MBOrderedDitherImage(l, Cr, Cb, disp, h, w, vid_stream->ditherFlags);
    break;
  }
#endif
}
