flam3 - cosmic recursive fractal flames
spot aka Scott Draves <readme@flam3.com>
see the file COPYING for the license covering this software.

This is free software to render fractal flames as described on
http://flam3.com.  Flam3-animate makes animations, and flam3-render
makes still images.  Flam3-genome creates and manipulates genomes
(parameter sets).  A C library is also installed.

Note: the following instructions are written for Linux users.  Windows
users may install the cygwin package to get the "env" command or set
the envars in your windows command prompt manually.  That means
instead of a command like

    env dtime=5 prefix=foo. in=test.flame flam3-animate

say

    set dtime=5
    set prefix=foo.
    set in=test.flame
    flam3-animate



As usual, to configure, build, and install:

    ./configure
    make
    sudo make install

This package depends on the expat xml library, the jpeg library, and
the png library.  If you don't have one already you can find it with
http://rpmfind.net.

To test it, run

    flam3-render < test.flam3

and it should produce 0000.jpg and 0001.jpg, one image for each
<flame> element in the parameter file.  To make an animation run

    flam3-animate < test.flam3

and it should produce 100 files named 0000.jpg through 0099.jpg that
interpolate between the two <flame> elements.

Both programs get their options through environment variables.  An
easy way to set them is to invoke the program via the env command,
which allows you to give name=value pairs.

envar		default		meaning
=====		=======		=======
prefix		""		prefix names of output files with this string.
first		0		time of first frame to render (animate only)
last		n-1		time of last frame to render (n=last time specified in the input file) (animate only)
time		NA		time of first and last frame (ie do one frame) (animate only)
frame		NA		synonym for "time" (animate only)
in		stdin		name of input file
out		NA		name of output file (bad idea if rending more than one, use prefix instead)
dtime		1		time between frames (animate only)
fields		0		if 1 then render fields, ie odd scanlines at time+0.5
nstrips		1		number of strips, ie render fractions of a frame at once (render only)
qs		1		quality scale, multiply quality of all frames by this
ss		1		size scale, multiply size (in pixels) of all frames by this
jpeg		NA		jpeg quality for compression, default is native jpeg default
format		jpg		"jpg" or "ppm" or "png"
pixel_aspect    1.0             aspect ratio of pixels (width over height), eg 0.90909 for NTSC
seed            random		integer seed for random numbers, defaults to time+pid
verbose		0		if non-zero then print progress meter on stderr
bits		64		also maybe 16 or 32, sets bit-width of internal buffers
image		filename	replace palette with png, jpg, or ppm image
tries		50		number of tries to make to find a good genome.
method		NA		method for crossover: alternate, interpolate, or union.
symmetry	NA		set symmetry of result.
clone		NA		clone input (this is an alternative to mutate).


for example:

    env dtime=5 prefix=foo. in=test.flam3 flam3-animate

means to render every 5th frame of parameter file foo.flam3, and store
the results in files named foo.XXXX.jpg.

the flam3-convert program reads from stdin the old format created by
the GIMP and writes to stdout the new xml format.

the flam3-genome program creates random parameter files. it also mutates,
rotates, and interpolates existing parameter files.  for example to
create 10 wholly new control points and render them at normal quality:

    env template=vidres.flam3 repeat=10 flam3-genome > new.flam3
    flam3-render < new.flam3

if you left out the "template=vidres.flam3" part then the size,
quality, etc parameters would be their default (small) values.  you
can set the symmetry group:

    env template=vidres.flam3 symmetry=3 flam3-genome > new3.flam3
    env template=vidres.flam3 symmetry=-2 flam3-genome > new-2.flam3
    flam3-render < new3.flam3
    flam3-render < new-2.flam3

Mutation is done by giving an input flame file to alter:

    env template=vidres.flam3 flam3-genome > parent.flam3
    env prefix=parent. flam3-render < parent.flam3
    env template=vidres.flam3 mutate=vidres.flam3 repeat=10 flam3-genome > mutation.flam3
    flam3-render < mutation.flam3

Normally one wouldn't use the same file for the template and the file
to mutate.  Crossover is handled similarly:

    env template=vidres.flam3 flam3-genome > parent0.flam3
    env prefix=parent0. flam3-render < parent0.flam3
    env template=vidres.flam3 flam3-genome > parent1.flam3
    env prefix=parent1. flam3-render < parent1.flam3
    env template=vidres.flam3 cross0=parent0.flam3 cross1=parent1.flam3 flam3-genome > crossover.flam3
    flam3-render < crossover.flam3

flam3-genome has 3 ways to produce parameter files for animation in
the style of electric sheep.  the highest level and most useful from
the command line is the sequence method.  it takes a collection of
control points and makes an animation that has each flame do fractal
rotation for 360 degrees, then make a smooth transition to the next.
for example:

    env sequence=test.flam3 nframes=20 flam3-genome > seq.flam3
    flam3-animate < seq.flam3

creates and renders a 60 frame animation.  there are two flames in
test.flam3, so the animation consists three stags: the first one
rotating, then a transition, then the second one rotating.  each stage
has 20 frames as specified on the command line.  but with only 20
frames to complete 360 degrees the shape will is moving quite quickly
so you will see "strobing" from the temporal subsamples used for
motion blur.  to eliminate them increase the number of batches by
editing test.flam3 and increasing it from 10 to 100.  if you want to
render only some fraction of a whole animation file, specify the begin
and end times:

    env begin=20 end=40 flam3-animate < seq.flam3

the other two methods are harder to use becaues they produce file that
are only good for one frame of animation.  the output consists of 3
control points, one for the time requested, one before and one after.
that allows proper motion blur.  for example:

    env template=vidres.flam3 flam3-genome > rotme.flam3
    env rotate=rotme.flam3 frame=10 nframes=20 flam3-genome > rot10.flam3
    env frame=10 flam3-animate < rot10.flam3

the file rot10.flam3 specifies the animation for just one frame, in
this case 10 out of 20 frames in the complete animation.  C1
continuous electric sheep genetic crossfades are created like this:

    env inter=test.flam3 frame=10 nframes=20 flam3-genome > inter10.flam3
    env frame=10 flam3-animate < inter10.flam3

A preview of image fractalization is available by setting the image
envar to the name of a png (alpha supported), jpg, or ppm format file.
Note this interface will change!  This image is used as a 2D palette
to paint the flame.  The input image must be 256x256 pixels.  For
example:

    env image=star.png flam3-render < test.flam3

    

--

The complete list of variations:

  linear
  sinusoidal
  spherical
  swirl
  horseshoe
  polar
  handkerchief
  heart
  disc
  spiral
  hyperbolic
  diamond
  ex
  julia
  bent
  waves
  fisheye
  popcorn
  exponential
  power
  cosine
  rings
  fan

see http://flam3.com/flame.pdf for descriptions & formulas for each of
these.

======================================

todo:

eliminate all static storage.

it would be good if interpolation from eg sym=4 to sym=-4 maintained
the 4ness throughout (?).

density estimation (make the filter kernel size (its area) inversely
proportional to the density).

support jitter of the control parameters in addition to temporal
sampling (batches).

make templates only override the attributes that are set in them.

======================================


changelog:

compile clean with gcc4.  fail gracefully if filter radius is too small.

06/27/05 in flam3_dimension, give up and return NaN if 90% or more of
	 the samples are being clipped.  release v2.6.

06/22/05 add envar to control the number of tries.

06/06/05 add new form of mutation that introduces post xforms.

05/20/05 fix memory trashing bug resulting from xform overflow.  put
	 regular xforms before symmetry in printed genomes.  enforce
	 weights non-negative & at least one xform.  remove
	 nan-protection from popcorn variation.  truncate xforms with
	 very small weight.

05/13/05 fix bug reported by erik reckase where fan variation could
         blow up to NaN because the domain of atan2 was not protected.
         remove protection from all atan2 calls and instead detect NaN
         results and replace them with noise.  count really large
         values as bad too to prevent blowing up to infinity.  enforce
         0<=hue<1, release v2.6b1.

05/05/05 report choices made during genome generation in notes
	 attribute.  flam3_dimension no longer hangs when most of the
	 attractor is outside the camera.  limit number of variations
	 produced by genetic operators to 5.  reduce rate of
	 interpolation method of crossover.

03/17/05 put cloning back in (found by James Rantanen).

03/08/05 change sawtooth variation (incompatible!).  add fan
	 variation.  rename sawtooth to rings.  release as v2.5.

03/01/05 fix rotation when nstrips > 1.  add flam3_dimension().  minor
	 bugfixes.  release as v2.4.

01/25/05 release as v2.3.

01/18/05 support post xforms (idea from eric nguyen).  support camera
	 rotation. 

12/28/04 release as v2.2.

12/27/04 preview implementation of image fractalization by adding a
         color coordinate.  changed how random/default color
         coordinates are selected (they alternate 0 and 1 instead of
         being distributed between 0 and 1).  WARNING: incompatible
         format change to samples argument of flam3_iterate.

12/20/04 allow mutation and crossover from files with multiple
         genomes.  a random genome is selected for each repetition.

12/11/04 fix bug printing out interpolated non-built-in palettes.
	 warn if any images sizes in animation do not match.

12/01/04 remove debugging code left in flam3-convert, thanks to Paul
	 Fishwick for reporting this.  add cosine variation.  add
	 sawtooth variation.  handle nstrips that do not divide the
	 height.  write partial results as strips are computed.  fix
	 old bug where in 32 bit mode one of the terms appeared to be
	 calculated at 16 bits.  release as v2.1.

10/28/04 fix docstring bug. release as v2.0.1.

10/28/04 renaming, cleanup, and modularization.  now exports
         libflam3.a and flam3.h, all names prefixed with flam3_.
         binaries named with flam3- prefix, genome files use flam3
	 suffix.  create and install a flam3.pc pkg-config file.
	 release as v2.0.

09/21/04 fix bug where code for integer rounding was left in 64-bit
	 floating point version.  round remaining time up so we do not
	 say ETA in 0 seconds.  do not use static allocation to hold
	 onto memory, just malloc and free what we need every frame.
	 enforce positive oversampling.  fix bug in hqi on sequences
	 of images of different sizes.  release as v1.15.

09/17/04 change name of envar to control jpeg compression quality from
	 "quality" to "jpeg".  check for bad nbatches values.  release
	 as v1.14.

08/23/04 add about 600 cmaps from Jackie, Pat Phillips, Classylady,
         and BTomchek.  use 64-bit (double) sized buffers.  remove
	 white_level. add new variations: exponential & power.  fix
	 bug where hue_rotation was left uninitialized.  add clone
	 option to pick-flame which just applies template to the input
	 without modifying genome.  random_control_point can now put
	 multiple variations in one xform.  remove altivec code because
	 it is incompatible with 64-bit buffers.  verbose (progress bar
	 on stderr) from Jon Morton.  control random number seeds via
	 seed envar.  support buffer sizes 16, 32, or 64 bits with
	 three versions of rect.c included into libifs.c.

03/28/04 fix bug interpolating between flames with different numbers
	 of xforms introduced by the new de/parsing.  add modified
	 version of the popcorn variation from apophysis.  fix small
	 bug in waves variation.  make distribution of variations
	 even.  add altivec code from R.J. Berry of warwick.ac.uk.
	 release as v1.13.

03/26/04 add wave variation.  allow negative variational coefs.  do
	 not truncate filter width to an integer.  add fisheye
	 variation.  make variations print by name instead of using a
	 vector, that is say spherical="1" instead of var="0 0 1" or
	 var1="2".  it should still read the old format.

03/07/04 fix bug printing out result of interpolating between
	 symmetries. release as v1.11.

03/03/04 add new means of crossover: a random point between the
	 parents as defined by linear interpolation.  in all kinds of
	 crossover, reset the color coordinates to make sure they are 
	 spread out well.  somehow lost part of the extra_attributes
	 patch, so put it in again.  add pixel_aspect_ratio envar.
	 decrease filter cutoff threshold.  the edges of the filter
	 were almost zeros, so making the filter smaller saves time
	 with almost no effect on the results.  do not print out the
	 attributes of control points that have default values.
	 release as v1.10. 

02/26/04 remove prefix argument to print_control_point, and add
	 extra_attributes.  allow any value for vibrancy parameter.
	 allow variation blending coefs to have any values.  do not
	 normalize them.  on windows nstrips is computated
	 automatically to fit within memory (leaves at least 20%
	 unused).  support png image format and if output is png then
	 compute & output alpha channel. release as v1.9.

02/01/04 add julia variation, and put bent variation back in.  change
         how colors are computed in presense of symmetry: xforms that
         come from a symmetry do not change the color index.  this
         prevents the colors from washing out.  since an xform may be
         interpolated between a symmetry and not, this this is a
         blending factor.  add more documentation.  add function to
         compute lyapunov coefs. allow control over symmetries
         produced by pick-flame.  release as v1.8.

07/20/03 cleanup, update documentation, release as v1.7.

07/15/03 fix bug in interpolation where in last frame when going from
	 non-symmetric to symmetric it left out some xforms.  drop
	 support for "cmap_inter".  add var1 as a abbreviation to var
	 in xml output, and do not print trailing 0s in var string.

07/09/03 change matrix interpolation to be linear rather than complex
	 log to avoid the discontinuity at 180 degrees.  this was
	 causing jumpiness in the C1 continuous algorithm.  this means
	 that rotation has to be handled with pick-flame.  put direct
	 support for symmetries in the de/parser to make control
	 points smaller and easier to understand.  support
	 combinations of bilateral & rotational symmetry.

06/22/03 bug in colormap interpolation.  release as v1.6.

06/20/03 fix some bugs, in particular remove sorting of the xforms
	 when control points are read.  they are only sorted when they
	 are generated.  updated dates on copyright notices.  added
	 time parameter to anim, shorthand for begin & end being the
	 same. added a fairly terrible hack to allow palettes to be
	 specified as blending of two basic palettes.  this requires
	 much less bandwidth than sending 256 rgb triples.  in
	 pick-flame change default to be enclosed xml output.

06/09/03 add C1 continuous interpolation to pick-flame (suggested by
	 Cassidy Curtis).  added new variations from Ultra Fractal
	 version by Mark Townsend.  added symmetry xforms.

06/02/03 add convert-flame which reads the old file format and writes 
	 the new one. release as v1.5.

03/22/03 fix bug in hqi & anim.  somewhere along the way (prolly jpeg)
	 nstrips was broken.  add qs and ss params to hqi.
	 discontinue strips for anim because the implementation is a
	 bit problematic (animating zoom??).  bump version to 1.4.

03/05/03 add pick-flame.c to the project, and extend it with mutation
	 and crossover capability.  add parse_control_points_from_file
	 (and use it). rename non-xml variants of de/parsers to *_old
	 and rename xml variants to remove _xml.  add
	 rotate_control_point.  bump version to 1.3.

02/10/03 increase CHOOSE_XFORM_GRAIN by 10x and use chars instead of
         ints.  remove extra -ljpeg from Makefile.  lose hack that
         ignored density xforms after interpolation.  what was that
         for??  it makes a difference, and just makes interpolation
         less smooth as far as i can tell.  bump version to 1.2.

01/17/03 release as v1.1.

01/16/03 support output in jpeg format; this is the default.  support
	 win32.

01/08/03 by default don't render the last frame so that animations
	 dove-tail and loop

01/06/03 fix how too many xforms are detected so that one more xform
	 is allowed.

12/22/02 first release as independent package.  release as v1.0.
