flame - cosmic recursive fractal flames
Scott Draves <readme@flam3.com>
http://flam3.com
see the file COPYING for the license covering this software.

free software to render fractal flames.  anim-flame makes animations,
and hqi-flame makes still images.

Note: the following instructions are written for Linux users.  On
windows the programs lack the "-flame" suffix.  You will also either
have to install the cygwin package to get the "env" command or set the
envars in your windows command prompt m anually.  That means instead
of a command like

    env dtime=5 prefix=foo. in=test.flame anim-flame

say

    set dtime=5
    set prefix=foo.
    set in=test.flame
    anim



As usual, to configure, build, and install:

    ./configure
    make
    sudo make install

This package depends on the expat xml library, and the jpeg library.
If you don't have them already you can find it with
http://rpmfind.net.

To test it, run

    hqi-flame < test.flame

and it should produce 0000.jpg and 0001.jpg, one image for each
<flame> element in the parameter file.  To make an animation run

    anim-flame < test.flame

and it should produce 100 files named 0000.jpg through 0099.jpg that
interpolate between the two <flame> elements.

both programs get their options through environment variables.  an
easy way to set them is to invoke the program via the env command,
which allows you to give name=value pairs.

envar		default		meaning
=====		=======		=======
prefix		""		prefix names of output files with this string.
first		0		time of first frame to render (anim only)
last		n-1		time of last frame to render (n=last time specified in the input file) (anim only)
time		NA		time of first and last frame (ie do one frame) (anim only)
frame		NA		synonym for "time" (anim only)
in		stdin		name of input file
out		NA		name of output file (bad idea if rending more than one, use prefix instead)
dtime		1		time between frames (anim only)
fields		0		if 1 then render fields, ie odd scanlines at time+0.5
nstrips		1		number of strips, ie render fractions of a frame at once (hqi only)
qs		1		quality scale, multiply quality of all frames by this
ss		1		size scale, multiply size (in pixels) of all frames by this
quality		NA		jpeg quality for compression, default is native jpeg default
format		jpg		"jpg" or "ppm" or "png"
pixel_aspect    1.0             aspect ratio of pixels (width over height), eg 0.90909 for NTSC


for example:

    env dtime=5 prefix=foo. in=test.flame anim-flame

means to render every 5th frame of parameter file foo.flame, and store
the results in files named foo.XXXX.jpg.

the convert program reads from stdin the old format created by the
GIMP and writes to stdout the new xml format.

the pick program creates random parameter files. it also mutates,
rotates, and interpolates existing parameter files.  for example to
create 10 wholly new control points and render them at normal quality:

    env template=vidres.flame repeat=10 pick-flame > new.flame
    hqi-flame < new.flame

if you left out the "template=vidres.flame" part then the size,
quality, etc parameters would be their default (small) values.  you
can set the symmetry group:

    env template=vidres.flame symmetry=3 pick-flame > new3.flame
    env template=vidres.flame symmetry=-2 pick-flame > new-2.flame
    hqi-flame < new3.flame
    hqi-flame < new-2.flame

Mutation is done by giving an input flame file to alter:

    env template=vidres.flame pick-flame > parent.flame
    env prefix=parent. hqi-flame < parent.flame
    env template=vidres.flame mutate=vidres.flame repeat=10 pick-flame > mutation.flame
    hqi-flame < mutation.flame

Normally one wouldn't use the same file for the template and the file
to mutate.  Crossover is handled similarly, but crossover is not a
random process so it doesn't help to create more than one child that
way:

    env template=vidres.flame pick-flame > parent0.flame
    env prefix=parent0. hqi-flame < parent0.flame
    env template=vidres.flame pick-flame > parent1.flame
    env prefix=parent1. hqi-flame < parent1.flame
    env template=vidres.flame cross0=parent0.flame cross1=parent1.flame pick-flame > crossover.flame
    hqi-flame < crossover.flame

pick has 3 ways to produce parameter files for animation in the style
of electric sheep.  the highest level and most useful from the command
line is the sequence method.  it takes a collection of control points
and makes an animation that has each flame do fractal rotation for 360
degrees, then make a smooth transition to the next.  for example:

    env sequence=test.flame nframes=20 pick-flame > seq.flame
    anim-flame < seq.flame

creates and renders a 60 frame animation.  there are two flames in
test.flame, so the animation consists three stags: the first one
rotating, then a transition, then the second one rotating.  each stage
has 20 frames as specified on the command line.  but with only 20
frames to complete 360 degrees the shape will is moving quite quickly
so you will see "strobing" from the temporal subsamples used for
motion blur.  to eliminate them increase the number of batches by
editing test.flame and increasing it from 10 to 100.  if you want to
render only some fraction of a whole animation file, specify the begin
and end times:

    env begin=20 end=40 anim-flame < seq.flame

the other two methods are harder to use becaues they produce file that
are only good for one frame of animation.  the output consists of 3
control points, one for the time requested, one before and one after.
that allows proper motion blur.  for example:

    env template=vidres.flame pick-flame > rotme.flame
    env rotate=rotme.flame frame=10 nframes=20 pick-flame > rot10.flame
    env frame=10 anim-flame < rot10.flame

the file rot10.flame specifies the animation for just one frame, in
this case 10 out of 20 frames in the complete animation.  C1
continuous electric sheep genetic crossfades are created like this:

    env inter=test.flame frame=10 nframes=20 pick-flame > inter10.flame
    env frame=10 anim-flame < inter10.flame



======================================

todo:

it would be good if interpolation from eg sym=4 to sym=-4 maintained
the 4ness throughout.

density estimation (make the filter kernel size (its area) inversely
proportional to the density).

use 64-bit arithmetic and jitter the control parameters instead of
temporal sampling.

make templates only override the attributes that are set in them.

======================================


changelog:

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
	 with almost no effect on the results.  don't print out the
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
