#include "util.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
die(const char *fmt, ...)
{
	int saved_errno = errno;
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt && *fmt) {
		size_t len = strlen(fmt);

		if (fmt[len - 1] == ':')
			fprintf(stderr, " %s", strerror(saved_errno));
	}

	fputc('\n', stderr);
	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	p = calloc(nmemb, size);
	if (!p)
		die("calloc:");

	return p;
}