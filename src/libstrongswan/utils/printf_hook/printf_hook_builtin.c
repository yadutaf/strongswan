/*
 * Copyright (C) 2013 Martin Willi
 * Copyright (C) 2013 revosec AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/**
 * Copyright (C) 2002-2006 H. Peter Anvin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "printf_hook.h"

#include <utils/utils.h>
#include <utils/debug.h>
#include <collections/hashtable.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define PRINTF_BUF_LEN 8192
#define ARGS_MAX 3

typedef struct private_printf_hook_t private_printf_hook_t;
typedef struct printf_hook_handler_t printf_hook_handler_t;

/**
 * private data of printf_hook
 */
struct private_printf_hook_t {

	/**
	 * public functions
	 */
	printf_hook_t public;
};

/**
 * struct with information about a registered handler
 */
struct printf_hook_handler_t {

	/**
	 * callback function
	 */
	printf_hook_function_t hook;

	/**
	 * number of arguments
	 */
	int numargs;

	/**
	 * types of the arguments
	 */
	int argtypes[ARGS_MAX];
};

/**
 * Data to pass to a printf hook.
 */
struct printf_hook_data_t {

	/**
	 * Output buffer
	 */
	char *q;

	/**
	 * Remaining bytes in q
	 */
	size_t n;
};

/**
 * Registered hooks (char => printf_hook_handler_t)
 */
static hashtable_t *hooks;

/**
 * builtin-printf variant of print_in_hook()
 */
size_t print_in_hook(printf_hook_data_t *data, char *fmt, ...)
{
	int written;
	va_list args;

	va_start(args, fmt);
	written = builtin_vsnprintf(data->q, data->n, fmt, args);
	va_end(args);

	if (written > data->n)
	{
		written = data->n;
	}
	data->q += written;
	data->n += written;
	return written;
}

METHOD(printf_hook_t, add_handler, void,
	private_printf_hook_t *this, char spec, printf_hook_function_t hook, ...)
{
	int i = -1;
	bool failed = FALSE;
	printf_hook_handler_t *handler;
	printf_hook_argtype_t argtype;
	va_list args;

	INIT(handler,
		.hook = hook,
	);

	va_start(args, hook);
	while (!failed)
	{
		argtype = va_arg(args, printf_hook_argtype_t);

		if (argtype == PRINTF_HOOK_ARGTYPE_END)
		{
			break;
		}
		if (++i >= countof(handler->argtypes))
		{
			DBG1(DBG_LIB, "Too many arguments for printf hook with "
				 "specifier '%c', not registered!", spec);
			failed = TRUE;
			break;
		}
		handler->argtypes[i] = argtype;
	}
	va_end(args);

	handler->numargs = i + 1;
	if (!failed && handler->numargs > 0)
	{
		free(hooks->put(hooks, (void*)(uintptr_t)spec, handler));
	}
	else
	{
		free(handler);
	}
}

/**
 * Printf format modifier flags
 */
typedef enum {
	FL_ZERO		= 0x01,
	FL_MINUS	= 0x02,
	FL_PLUS		= 0x04,
	FL_TICK		= 0x08,
	FL_SPACE	= 0x10,
	FL_HASH		= 0x20,
	FL_SIGNED	= 0x40,
	FL_UPPER	= 0x80,
} bpf_flag_t;

/**
 * Size of format string arguments
 */
typedef enum {
	RNK_CHAR		= -2,
	RNK_SHORT		= -1,
	RNK_INT			=  0,
	RNK_LONG		=  1,
	RNK_LONGLONG	=  2,

	RNK_INTMAX		= RNK_LONGLONG,
	RNK_SIZE_T		= RNK_LONG,
	RNK_PTRDIFF_T	= RNK_LONG,

	RNK_MIN			= RNK_CHAR,
	RNK_MAX			= RNK_LONGLONG,
} bpf_rank_t;

/**
 * Printf specifier Parser state
 */
typedef enum {
	/* Ground state */
	ST_NORMAL,
	/* Special flags */
	ST_FLAGS,
	/* Field width */
	ST_WIDTH,
	/* Field precision */
	ST_PREC,
	/* Length or conversion modifiers */
	ST_MODIFIERS,
} bpf_state_t;

#define EMIT(x) ({ if (o<n){*q++ = (x);} o++; })

/**
 * Write an integer argument to q, using flags, base, width and precision
 */
static size_t format_int(char *q, size_t n, uintmax_t val, bpf_flag_t flags,
						 int base, int width, int prec)
{
	static const char lcdigits[] = "0123456789abcdef";
	static const char ucdigits[] = "0123456789ABCDEF";
	char *qq;
	size_t o = 0, oo;
	const char *digits;
	uintmax_t tmpval;
	int minus = 0;
	int ndigits = 0, nchars;
	int tickskip, b4tick;

	/* Select type of digits */
	digits = (flags & FL_UPPER) ? ucdigits : lcdigits;

	/* If signed, separate out the minus */
	if (flags & FL_SIGNED && (intmax_t) val < 0)
	{
		minus = 1;
		val = (uintmax_t) (-(intmax_t) val);
	}

	/* Count the number of digits needed.  This returns zero for 0. */
	tmpval = val;
	while (tmpval)
	{
		tmpval /= base;
		ndigits++;
	}

	/* Adjust ndigits for size of output */
	if (flags & FL_HASH && base == 8)
	{
		if (prec < ndigits + 1)
		{
			prec = ndigits + 1;
		}
	}

	if (ndigits < prec)
	{
		/* Mandatory number padding */
		ndigits = prec;
	}
	else if (val == 0)
	{
		/* Zero still requires space */
		ndigits = 1;
	}

	/* For ', figure out what the skip should be */
	if (flags & FL_TICK)
	{
		if (base == 16)
		{
			tickskip = 4;
		}
		else
		{
			tickskip = 3;
		}
	}
	else
	{
		/* No tick marks */
		tickskip = ndigits;
	}

	/* Tick marks aren't digits, but generated by the number converter */
	ndigits += (ndigits - 1) / tickskip;

	/* Now compute the number of nondigits */
	nchars = ndigits;

	if (minus || (flags & (FL_PLUS | FL_SPACE)))
	{
		/* Need space for sign */
		nchars++;
	}
	if ((flags & FL_HASH) && base == 16)
	{
		/* Add 0x for hex */
		nchars += 2;
	}

	/* Emit early space padding */
	if (!(flags & (FL_MINUS | FL_ZERO)) && width > nchars)
	{
		while (width > nchars)
		{
			EMIT(' ');
			width--;
		}
	}

	/* Emit nondigits */
	if (minus)
	{
		EMIT('-');
	}
	else if (flags & FL_PLUS)
	{
		EMIT('+');
	}
	else if (flags & FL_SPACE)
	{
		EMIT(' ');
	}

	if ((flags & FL_HASH) && base == 16)
	{
		EMIT('0');
		EMIT((flags & FL_UPPER) ? 'X' : 'x');
	}

	/* Emit zero padding */
	if ((flags & (FL_MINUS | FL_ZERO)) == FL_ZERO && width > ndigits)
	{
		while (width > nchars)
		{
			EMIT('0');
			width--;
		}
	}

	/* Generate the number.  This is done from right to left. */
	/* Advance the pointer to end of number */
	q += ndigits;
	o += ndigits;
	/* Temporary values */
	qq = q;
	oo = o;

	b4tick = tickskip;
	while (ndigits > 0)
	{
		if (!b4tick--)
		{
			qq--;
			oo--;
			ndigits--;
			if (oo < n)
			{
				*qq = '_';
			}
			b4tick = tickskip - 1;
		}
		qq--;
		oo--;
		ndigits--;
		if (oo < n)
		{
			*qq = digits[val % base];
		}
		val /= base;
	}

	/* Emit late space padding */
	while ((flags & FL_MINUS) && width > nchars)
	{
		EMIT(' ');
		width--;
	}

	return o;
}

int builtin_vsnprintf(char *buffer, size_t n, const char *format, va_list ap)
{
	const char *p = format;
	char ch;
	char *q = buffer;
	/* Number of characters output */
	size_t o = 0;
	uintmax_t val = 0;
	/* Default rank */
	int rank = RNK_INT;
	int width = 0;
	int prec = -1;
	int base;
	size_t sz;
	bpf_flag_t flags = 0;
	bpf_state_t state = ST_NORMAL;
	/* %s string argument */
	const char *sarg;
	/* %c char argument */
	char carg;
	/* String length */
	int slen;

	while ((ch = *p++))
	{
		switch (state)
		{
			case ST_NORMAL:
			{
				if (ch == '%')
				{
					state = ST_FLAGS;
					flags = 0;
					rank = RNK_INT;
					width = 0;
					prec = -1;
				}
				else
				{
					EMIT(ch);
				}
				break;
			}
			case ST_FLAGS:
			{
				switch (ch)
				{
					case '-':
						flags |= FL_MINUS;
						break;
					case '+':
						flags |= FL_PLUS;
						break;
					case '\'':
						flags |= FL_TICK;
						break;
					case ' ':
						flags |= FL_SPACE;
						break;
					case '#':
						flags |= FL_HASH;
						break;
					case '0':
						flags |= FL_ZERO;
						break;
					default:
						state = ST_WIDTH;
						/* Process this character again */
						p--;
						break;
				}
				break;
			}
			case ST_WIDTH:
			{
				if (ch >= '0' && ch <= '9')
				{
					width = width * 10 + (ch - '0');
				}
				else if (ch == '*')
				{
					width = va_arg(ap, int);
					if (width < 0)
					{
						width = -width;
						flags |= FL_MINUS;
					}
				}
				else if (ch == '.')
				{
					/* Precision given */
					prec = 0;
					state = ST_PREC;
				}
				else
				{
					state = ST_MODIFIERS;
					/* Process this character again */
					p--;
				}
				break;
			}
			case ST_PREC:
			{
				if (ch >= '0' && ch <= '9')
				{
					prec = prec * 10 + (ch - '0');
				}
				else if (ch == '*')
				{
					prec = va_arg(ap, int);
					if (prec < 0)
					{
						prec = -1;
					}
				}
				else
				{
					state = ST_MODIFIERS;
					/* Process this character again */
					p--;
				}
				break;
			}
			case ST_MODIFIERS:
			{
				switch (ch)
				{
					/* Length modifiers - nonterminal sequences */
					case 'h':
						rank--;
						break;
					case 'l':
						rank++;
						break;
					case 'j':
						rank = RNK_INTMAX;
						break;
					case 'z':
						rank = RNK_SIZE_T;
						break;
					case 't':
						rank = RNK_PTRDIFF_T;
						break;
					case 'L':
					case 'q':
						rank += 2;
						break;
					default:
					{
						/* Output modifiers - terminal sequences */

						/* Next state will be normal */
						state = ST_NORMAL;

						/* Canonicalize rank */
						if (rank < RNK_MIN)
						{
							rank = RNK_MIN;
						}
						else if (rank > RNK_MAX)
						{
							rank = RNK_MAX;
						}

						switch (ch)
						{
							case 'P':
							{
								/* Upper case pointer */
								flags |= FL_UPPER;
								/* fall through */
							}
							case 'p':
							{
								/* Pointer */
								base = 16;
								prec = (CHAR_BIT*sizeof(void *)+3)/4;
								flags |= FL_HASH;
								val = (uintmax_t)(uintptr_t)
									va_arg(ap, void *);
								goto is_integer;
							}
							case 'd':
							case 'i':
							{
								/* Signed decimal output */
								base = 10;
								flags |= FL_SIGNED;
								switch (rank)
								{
									case RNK_CHAR:
										/* Yes, all these casts are
										   needed... */
										val = (uintmax_t)(intmax_t)(signed char)
												va_arg(ap, signed int);
										break;
									case RNK_SHORT:
										val = (uintmax_t)(intmax_t)(signed short)
												va_arg(ap, signed int);
										break;
									case RNK_INT:
										val = (uintmax_t)(intmax_t)
												va_arg(ap, signed int);
										break;
									case RNK_LONG:
										val = (uintmax_t)(intmax_t)
												va_arg(ap, signed long);
										break;
									case RNK_LONGLONG:
										val = (uintmax_t)(intmax_t)
												va_arg(ap, signed long long);
										break;
								}
								goto is_integer;
							}
							case 'o':
							{
								/* Octal */
								base = 8;
								goto is_unsigned;
							}
							case 'u':
							{
								/* Unsigned decimal */
								base = 10;
								goto is_unsigned;
							}
							case 'X':
							{
								/* Upper case hexadecimal */
								flags |= FL_UPPER;
								/* fall through */
							}
							case 'x':
							{
								/* Hexadecimal */
								base = 16;
								goto is_unsigned;
							}
							is_unsigned:
							{
								switch (rank) {
									case RNK_CHAR:
										val = (uintmax_t)(unsigned char)
												va_arg(ap, unsigned int);
										break;
									case RNK_SHORT:
										val = (uintmax_t)(unsigned short)
												va_arg(ap, unsigned int);
										break;
									case RNK_INT:
										val = (uintmax_t)
												va_arg(ap, unsigned int);
										break;
									case RNK_LONG:
										val = (uintmax_t)
												va_arg(ap, unsigned long);
										break;
									case RNK_LONGLONG:
										val = (uintmax_t)
												va_arg(ap, unsigned long long);
										break;
								}
								goto is_integer;
							}
							is_integer:
							{
								sz = format_int(q, (o < n) ? n - o : 0,
												val, flags, base, width, prec);
								q += sz;
								o += sz;
								break;
							}
							case 'c':
							{
								/* Character */
								carg = (char)va_arg(ap, int);
								sarg = &carg;
								slen = 1;
								goto is_string;
							}
							case 's':
							{
								/* String */
								sarg = va_arg(ap, const char *);
								sarg = sarg ? sarg : "(null)";
								slen = strlen(sarg);
								goto is_string;
							}
							case 'm':
							{
								/* glibc error string */
								sarg = strerror(errno);
								slen = strlen(sarg);
								goto is_string;
							}
							is_string:
							{
								char sch;
								int i;

								if (prec != -1 && slen > prec)
								{
									slen = prec;
								}

								if (width > slen && !(flags & FL_MINUS))
								{
									char pad = (flags & FL_ZERO) ? '0' : ' ';
									while (width > slen)
									{
										EMIT(pad);
										width--;
									}
								}
								for (i = slen; i; i--)
								{
									sch = *sarg++;
									EMIT(sch);
								}
								if (width > slen && (flags & FL_MINUS))
								{
									while (width > slen)
									{
										EMIT(' ');
										width--;
									}
								}
								break;
							}
							case 'n':
							{
								/* Output the number of characters written */
								switch (rank)
								{
									case RNK_CHAR:
										*va_arg(ap, signed char *) = o;
										break;
									case RNK_SHORT:
										*va_arg(ap, signed short *) = o;
										break;
									case RNK_INT:
										*va_arg(ap, signed int *) = o;
										break;
									case RNK_LONG:
										*va_arg(ap, signed long *) = o;
										break;
									case RNK_LONGLONG:
										*va_arg(ap, signed long long *) = o;
										break;
								}
								break;
							}
							default:
							{
								printf_hook_handler_t *handler;

								handler = hooks->get(hooks, (void*)(uintptr_t)ch);
								if (handler)
								{
									const void *args[ARGS_MAX];
									int i, iargs[ARGS_MAX];
									void *pargs[ARGS_MAX];
									printf_hook_spec_t spec =  {
										.hash = flags & FL_HASH,
										.plus = flags & FL_PLUS,
										.minus = flags & FL_MINUS,
										.width = width,
									};
									printf_hook_data_t data = {
										.q = q,
										.n = (o < n) ? n - o : 0,
									};

									for (i = 0; i < handler->numargs; i++)
									{
										switch (handler->argtypes[i])
										{
											case PRINTF_HOOK_ARGTYPE_INT:
												iargs[i] = va_arg(ap, int);
												args[i] = &iargs[i];
												break;
											case PRINTF_HOOK_ARGTYPE_POINTER:
												pargs[i] = va_arg(ap, void*);
												args[i] = &pargs[i];
												break;
										}
									}
									sz = handler->hook(&data, &spec, args);
									q += sz;
									o += sz;
								}
								else
								{
									/* Anything else, including % */
									EMIT(ch);
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	/* Null-terminate the string */
	if (o < n)
	{
		/* No overflow */
		*q = '\0';
	}
	else if (n > 0)
	{
		/* Overflow - terminate at end of buffer */
		buffer[n - 1] = '\0';
	}
	return o;
}

int builtin_printf(const char *format, ...)
{
	int written;
	va_list args;

	va_start(args, format);
	written = builtin_vprintf(format, args);
	va_end(args);

	return written;
}

int builtin_fprintf(FILE *stream, const char *format, ...)
{
	int written;
	va_list args;

	va_start(args, format);
	written = builtin_vfprintf(stream, format, args);
	va_end(args);

	return written;
}

int builtin_sprintf(char *str, const char *format, ...)
{
	int written;
	va_list args;

	va_start(args, format);
	written = builtin_vsnprintf(str, ~(size_t)0, format, args);
	va_end(args);

	return written;
}

int builtin_snprintf(char *str, size_t size, const char *format, ...)
{
	int written;
	va_list args;

	va_start(args, format);
	written = builtin_vsnprintf(str, size, format, args);
	va_end(args);

	return written;
}

int builtin_asprintf(char **str, const char *format, ...)
{
	int written;
	va_list args;

	va_start(args, format);
	written = builtin_vasprintf(str, format, args);
	va_end(args);

	return written;
}

int builtin_vprintf(const char *format, va_list ap)
{
	return builtin_vfprintf(stdout, format, ap);
}

int builtin_vfprintf(FILE *stream, const char *format, va_list ap)
{
	char buf[PRINTF_BUF_LEN];
	int len;

	len = builtin_vsnprintf(buf, sizeof(buf), format, ap);
	return fwrite(buf, 1, len, stream);
}

int builtin_vsprintf(char *str, const char *format, va_list ap)
{
	return builtin_vsnprintf(str, ~(size_t)0, format, ap);
}

int builtin_vasprintf(char **str, const char *format, va_list ap)
{
	char buf[PRINTF_BUF_LEN];
	int len;

	len = builtin_vsnprintf(buf, sizeof(buf), format, ap);
	*str = strdup(buf);
	return len;
}

METHOD(printf_hook_t, destroy, void,
	private_printf_hook_t *this)
{
	enumerator_t *enumerator;
	printf_hook_handler_t *handler;

	enumerator = hooks->create_enumerator(hooks);
	while (enumerator->enumerate(enumerator, NULL, &handler))
	{
		free(handler);
	}
	enumerator->destroy(enumerator);

	hooks->destroy(hooks);

	free(this);
}

/*
 * see header file
 */
printf_hook_t *printf_hook_create()
{
	private_printf_hook_t *this;

	INIT(this,
		.public = {
			.add_handler = _add_handler,
			.destroy = _destroy,
		},
	);

	hooks = hashtable_create(hashtable_hash_ptr, hashtable_equals_ptr, 8);

	return &this->public;
}
