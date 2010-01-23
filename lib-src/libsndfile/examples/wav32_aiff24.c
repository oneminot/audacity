/*
** Copyright (C) 1999-2002 Erik de Castro Lopo <erikd@zip.com.au>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#include	<sndfile.h>

#define	 BUFFER_LEN      1024

static double	data [BUFFER_LEN] ;
	
static
void    copy_data (SNDFILE *outfile, SNDFILE *infile, unsigned int len, double normfactor)
{	unsigned int	readcount, k ;

	readcount = len ;
	while (readcount == len)
	{	readcount = sf_read_double (infile, data, len) ;
		for (k = 0 ; k < readcount ; k++)
			data [k] *= normfactor ;
		sf_write_double (outfile, data, readcount) ;
		} ;

	return ;
} /* copy_data */

static
void	print_usage (char *progname)
{	
	fprintf (stderr, "\nConverts a 32 bit floating point WAV file into a 24 bit PCM AIFF file.\n") ;
	fprintf (stderr, "        Usage : %s <input file> <output file>\n\n", progname) ;
} /* print_usage */

int
main (int argc, char *argv[])
{	char 		*progname, *infilename, *outfilename ;
	SNDFILE	 	*infile, *outfile ;
	SF_INFO	 	sfinfo ;
	double		normfactor ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;
		
	if (argc != 3)
	{	print_usage (progname) ;
		return  1 ;
		} ;
		
	infilename = argv [1] ;
	outfilename = argv [2] ;
		
	if (! strcmp (infilename, outfilename))
	{	fprintf (stderr, "Error : Input and output filenames are the same.\n\n") ;
		print_usage (progname) ;
		return  1 ;
		} ;
		
	if (! (infile = sf_open (infilename, SFM_READ, &sfinfo)))
	{	fprintf (stderr, "Not able to open input file %s.\n", infilename) ;
		sf_perror (NULL) ;
		return  1 ;
		} ;
		
	if (sfinfo.format != (SF_FORMAT_WAV | SF_FORMAT_FLOAT))
	{	fprintf (stderr, "Error : Input file %s is not a 32 bit floating point WAV file.\n", infilename) ;
		return  1 ;
		} ;
	
	sfinfo.format = (SF_FORMAT_AIFF | SF_FORMAT_PCM_24) ;
	
	sf_command (infile, SFC_CALC_SIGNAL_MAX, &normfactor, sizeof (normfactor)) ;

	if (normfactor < 1.0 && normfactor > 0.0)
		normfactor = ((double) 0x400000) ;
	else
		normfactor = 1.0 ;
	
	fprintf (stderr, "normfactor : %g\n", normfactor) ;
	
	if (! (outfile = sf_open (outfilename, SFM_WRITE, &sfinfo)))
	{	fprintf (stderr, "Not able to open output file %s.\n", outfilename) ;
		return  1 ;
		} ;
		
	copy_data (outfile, infile, BUFFER_LEN / sfinfo.channels, normfactor) ;
		
	sf_close (infile) ;
	sf_close (outfile) ;
	
	return 0 ;
} /* main */

