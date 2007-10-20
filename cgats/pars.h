#ifndef PARS_H
/* 
 * Simple ASCII file parsing object.
 * Used as a base for the CGATS.5 and IT8.7 family file I/O class
 * Version 2.01
 *
 * Author: Graeme W. Gill
 * Date:   20/12/95
 *
 * Copyright 1995, 1996, 2002 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licensed with a free use license:-
 * see the License.txt file in this directory for licensing details.
 */

#undef CGATS_DEBUG_MALLOC	/* Turns on partial support for filename and linenumber capture */

/* - - - - - - - - - - - - - - - - - - - - -  */

/* System interface objects. The defaults can be replaced */
/* for adaption to different system environments */

/* - - - - - - - - - - - - - - - - - - - - -  */

#ifdef CGATS_DEBUG_MALLOC

/* Heap allocator class interface definition */
#define CGATS_ALLOC_BASE																	\
	/* Public: */																			\
																							\
	void *(*dmalloc) (struct _cgatsAlloc *p, size_t size, char *file, int line);			\
	void *(*dcalloc) (struct _cgatsAlloc *p, size_t num, size_t size, char *file, int line);\
	void *(*drealloc)(struct _cgatsAlloc *p, void *ptr, size_t size, char *file, int line);	\
	void  (*dfree)   (struct _cgatsAlloc *p, void *ptr, char *file, int line);				\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _cgatsAlloc *p);														\

#if defined(_PARS_C_) || defined(_CGATS_C_)	|| defined(_PARSSTD_C_) /* only inside implimentation */
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#endif

#else /* !CGATS_DEBUG_MALLOC */

/* Heap allocator class interface definition */
#define CGATS_ALLOC_BASE																	\
	/* Public: */																			\
																							\
	void *(*malloc) (struct _cgatsAlloc *p, size_t size);									\
	void *(*calloc) (struct _cgatsAlloc *p, size_t num, size_t size);						\
	void *(*realloc)(struct _cgatsAlloc *p, void *ptr, size_t size);						\
	void  (*free)   (struct _cgatsAlloc *p, void *ptr);										\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _cgatsAlloc *p);														\

#endif /* !CGATS_DEBUG_MALLOC */

/* Common heap allocator interface class */
struct _cgatsAlloc {
	CGATS_ALLOC_BASE
}; typedef struct _cgatsAlloc cgatsAlloc;

/* - - - - - - - - - - - - - - - - - - - - -  */

/* Available when SEPARATE_STD is not defined: */
/* Implementation of heap class based on standard system malloc */
struct _cgatsAllocStd {
	CGATS_ALLOC_BASE
}; typedef struct _cgatsAllocStd cgatsAllocStd;

/* Create a standard alloc object */
cgatsAlloc *new_cgatsAllocStd(void);


/* - - - - - - - - - - - - - - - - - - - - -  */

/* File access class interface definition */
#define CGATS_FILE_BASE																		\
	/* Public: */																			\
																							\
	/* Set current position to offset. Return 0 on success, nz on failure. */				\
	int    (*seek) (struct _cgatsFile *p, long int offset);									\
																							\
	/* Read count items of size length. Return number of items successfully read. */ 		\
	size_t (*read) (struct _cgatsFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* Read a single character */													 		\
	int (*getch) (struct _cgatsFile *p);													\
																							\
	/* write count items of size length. Return number of items successfully written. */ 	\
	size_t (*write)(struct _cgatsFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* printf to the file */																\
	int (*printf)(struct _cgatsFile *p, const char *format, ...);							\
																							\
	/* flush all write data out to secondary storage. Return nz on failure. */				\
	int (*flush)(struct _cgatsFile *p);														\
																							\
	/* return the filename if known, NULL if not */											\
	char *(*fname)(struct _cgatsFile *p);													\
																							\
	/* we're done with the file object, close & free it */									\
	/* return nz on failure */																\
	int (*del)(struct _cgatsFile *p);														\


/* Common file interface class */
struct _cgatsFile {
	CGATS_FILE_BASE
}; typedef struct _cgatsFile cgatsFile;


/* - - - - - - - - - - - - - - - - - - - - -  */

/* Available when SEPARATE_STD is not defined: */

/* Implementation of file access class based on standard file I/O */
struct _cgatsFileStd {
	CGATS_FILE_BASE

	/* Private: */
	cgatsAlloc *al;		/* Heap allocator */
	int      del_al;	/* NZ if heap allocator should be deleted */
	FILE     *fp;
	int   doclose;		/* nz if free should close */
	char *filename;		/* NULL if not known */
}; typedef struct _cgatsFileStd cgatsFileStd;

/* Create given a file name */
cgatsFile *new_cgatsFileStd_name(char *name, char *mode);

/* Create given a (binary) FILE* */
cgatsFile *new_cgatsFileStd_fp(FILE *fp);

/* Create given a file name and allocator */
cgatsFile *new_cgatsFileStd_name_a(char *name, char *mode, cgatsAlloc *al);

/* Create given a (binary) FILE* and allocator */
cgatsFile *new_cgatsFileStd_fp_a(FILE *fp, cgatsAlloc *al);


/* - - - - - - - - - - - - - - - - - - - - -  */

struct _parse {
	/* Public Variables */
	int line;		/* Current line number */
	int token;		/* Current token number */

	/* Public Methods */
	void (*del)(struct _parse *p);				/* Delete the object */
	void (*reset_del)(struct _parse *p);		/* Reset the parsing delimiters */
	void (*add_del)(struct _parse *p,			/* Add to the parsing delimiters */
	    unsigned char *t, unsigned char *nr,
	    unsigned char *c, unsigned char *q);
	int (*read_line)(struct _parse *p);			/* Read in a line. Return 0 if read failed, */
												/* -1 on other error */
	char *(*get_token)(struct _parse *p);		/* Return a pointer to the next token, */
												/* NULL if no tokens. set errc NZ on other error */

	/* Private */
	cgatsAlloc *al;	/* Memory allocator */
	int del_al;		/* Flag to indicate we al->del() */
	cgatsFile *fp;	/* File we're dealing with */
	int ltflag;		/* Last terminator flag */
	int q;			/* Quote */
	char *b;		/* Line buffer */
	int bs;			/* Buffer size */
	int bo;			/* Next buffer offset */
	int to;			/* Token parsing offset into b */
	char *tb;		/* Token buffer */
	int tbs;		/* Token buffer size */
	unsigned char delf[256];		/* Parsing delimiter flags */
	/* Parsing flags */
#define PARS_TERM	0x01		/* Terminates a token */
#define PARS_SKIP	0x02		/* Character is not read */
#define PARS_COMM	0x04		/* Character starts a comment */
#define PARS_QUOTE	0x08		/* Character starts/ends a quoted string */

	char err[200];		/* Error message */
	int errc;			/* Error code */
}; typedef struct _parse parse;

/* Creator */
extern parse *new_parse_al(cgatsAlloc *al, cgatsFile *fp);	/* With allocator class */

/* Available when SEPARATE_STD is not defined: */
extern parse *new_parse(cgatsFile *fp);					/* Default allocator */

#define PARS_H
#endif /* PARS_H */
