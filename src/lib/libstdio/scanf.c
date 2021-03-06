/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#include	"sfstdio.h"

/*	Read input from stdin using a given format
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int scanf(const char* form, ...)
#else
int scanf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
	reg Sfio_t*	sf;
#if __STD_C
	va_start(args,form);
#else
	reg char*	form;	/* scanning format */
	va_start(args);
	form = va_arg(args,char*);
#endif

	if(!form || !(sf = _sfstream(stdin)))
		return -1;

	if((rv = sfvscanf(sf,form,args)) <= 0)
		_stdseterr(stdin,sf);

	va_end(args);

	return rv;
}
