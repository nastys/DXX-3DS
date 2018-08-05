/*
 *
 * Some simple physfs extensions
 *
 */


#if !defined(macintosh) && !defined(_MSC_VER)
#include <sys/param.h>
#endif
#if defined(__MACH__) && defined(__APPLE__)
#include <sys/mount.h>
#include <unistd.h>	// for chdir hack
#include <HIServices/Processes.h>
#endif

#include "physfsx.h"
#include "args.h"
#include "object.h"
#include "newdemo.h"

// Initialise PhysicsFS
void PHYSFSX_init(int argc, char *argv[])
{
	char hog[PATH_MAX];
	int ret = PHYSFS_init(argv[0]);
	if (ret != 1)
		Error("Failed to init PHYSFS....\n");
	PHYSFS_permitSymbolicLinks(1);

	// Add base directory
	PHYSFS_addToSearchPath(PHYSFS_getBaseDir(), 0);

	// Set write dir and init args
	PHYSFS_setWriteDir(PHYSFS_getBaseDir());
	InitArgs( argc,argv );

	// Add hog file
	memset(hog, '\x00', PATH_MAX);
	strcpy(hog, PHYSFS_getBaseDir());
	strcat(hog, "descent2.hog");
	PHYSFS_addToSearchPath(hog, 0);
}

// Add a searchpath, but that searchpath is relative to an existing searchpath
// It will add the first one it finds and return 1, if it doesn't find any it returns 0
int PHYSFSX_addRelToSearchPath(const char *relname, int add_to_end)
{
	char relname2[PATH_MAX], pathname[PATH_MAX];

	snprintf(relname2, sizeof(relname2), "%s", relname);
	PHYSFSEXT_locateCorrectCase(relname2);

	if (!PHYSFSX_getRealPath(relname2, pathname))
		return 0;

	return PHYSFS_addToSearchPath(pathname, add_to_end);
}

int PHYSFSX_removeRelFromSearchPath(const char *relname)
{
	char relname2[PATH_MAX], pathname[PATH_MAX];

	snprintf(relname2, sizeof(relname2), "%s", relname);
	PHYSFSEXT_locateCorrectCase(relname2);

	if (!PHYSFSX_getRealPath(relname2, pathname))
		return 0;

	return PHYSFS_removeFromSearchPath(pathname);
}

int PHYSFSX_fsize(const char *hogname)
{
	PHYSFS_file *fp;
	char hogname2[PATH_MAX];
	int size;

	snprintf(hogname2, sizeof(hogname2), "%s", hogname);
	PHYSFSEXT_locateCorrectCase(hogname2);

	fp = PHYSFS_openRead(hogname2);
	if (fp == NULL)
		return -1;

	size = PHYSFS_fileLength(fp);
	PHYSFS_close(fp);

	return size;
}

void PHYSFSX_listSearchPathContent()
{
	char **i, **list;

	con_printf(CON_DEBUG, "PHYSFS: Listing contents of Search Path.\n");
	list = PHYSFS_getSearchPath();
	for (i = list; *i != NULL; i++)
		con_printf(CON_DEBUG, "PHYSFS: [%s] is in the Search Path.\n", *i);
	PHYSFS_freeList(list);

	list = PHYSFS_enumerateFiles("");
	for (i = list; *i != NULL; i++)
		con_printf(CON_DEBUG, "PHYSFS: * We've got [%s].\n", *i);

	PHYSFS_freeList(list);
	
	con_printf(CON_DEBUG, "\n");
}

// checks which archives are supported by PhysFS. Return 0 if some essential (HOG) is not supported
int PHYSFSX_checkSupportedArchiveTypes()
{
	const PHYSFS_ArchiveInfo **i;
	int hog_sup = 0, mvl_sup = 0;

	con_printf(CON_DEBUG, "PHYSFS: Checking supported archive types.\n");
	for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++)
	{
		con_printf(CON_DEBUG, "PHYSFS: Supported archive: [%s], which is [%s].\n", (*i)->extension, (*i)->description);
		if (!d_stricmp((*i)->extension, "HOG"))
			hog_sup = 1;
		if (!d_stricmp((*i)->extension, "MVL"))
			mvl_sup = 1;
	}

	if (!hog_sup)
		con_printf(CON_CRITICAL, "PHYSFS: HOG not supported. The game will not work without!\n");
	if (!mvl_sup)
		con_printf(CON_URGENT, "PHYSFS: MVL not supported. Won't be able to play movies!\n");

	return hog_sup;
}

int PHYSFSX_getRealPath(const char *stdPath, char *realPath)
{
	const char *realDir = PHYSFS_getRealDir(stdPath);
	const char *sep = PHYSFS_getDirSeparator();
	char *p;
	
	if (!realDir)
	{
		realDir = PHYSFS_getWriteDir();
		if (!realDir)
			return 0;
	}
	
	strncpy(realPath, realDir, PATH_MAX - 1);
	if (strlen(realPath) >= strlen(sep))
	{
		p = realPath + strlen(realPath) - strlen(sep);
		if (strcmp(p, sep)) // no sep at end of realPath
			strncat(realPath, sep, PATH_MAX - 1 - strlen(realPath));
	}
	
	if (strlen(stdPath) >= 1)
		if (*stdPath == '/')
			stdPath++;
	
	while (*stdPath)
	{
		if (*stdPath == '/')
			strncat(realPath, sep, PATH_MAX - 1 - strlen(realPath));
		else
		{
			if (strlen(realPath) < PATH_MAX - 2)
			{
				p = realPath + strlen(realPath);
				p[0] = *stdPath;
				p[1] = '\0';
			}
		}
		stdPath++;
	}
	
	return 1;
}

// checks if path is already added to Searchpath. Returns 0 if yes, 1 if not.
int PHYSFSX_isNewPath(const char *path)
{
	int is_new_path = 1;
	char **i, **list;
	
	list = PHYSFS_getSearchPath();
	for (i = list; *i != NULL; i++)
	{
		if (!strcmp(path, *i))
		{
			is_new_path = 0;
		}
	}
	PHYSFS_freeList(list);
	
	return is_new_path;
}

int PHYSFSX_rename(const char *oldpath, const char *newpath)
{
	char old[PATH_MAX], n[PATH_MAX];
	
	PHYSFSX_getRealPath(oldpath, old);
	PHYSFSX_getRealPath(newpath, n);
	return (rename(old, n) == 0);
}

// Find files at path that have an extension listed in exts
// The extension list exts must be NULL-terminated, with each ext beginning with a '.'
char **PHYSFSX_findFiles(const char *path, const char *const *exts)
{
	char **list = PHYSFS_enumerateFiles(path);
	char **i, **j = list;
	const char *const *k;
	char *ext;
	
	if (list == NULL)
		return NULL;	// out of memory: not so good
	
	for (i = list; *i; i++)
	{
		ext = strrchr(*i, '.');
		if (ext)
			for (k = exts; *k != NULL && d_stricmp(ext, *k); k++) {}	// see if the file is of a type we want
		
		if (ext && *k)
			*j++ = *i;
		else
			free(*i);
	}

// aagallag: Horrible hack...
#define PLAYER_FNAME "player.plr"
	if (strcmp(exts[0], ".plr") == 0)
	{
		char* player_fname = malloc(strlen(PLAYER_FNAME)+1);
		memset(player_fname, '\x00', strlen(PLAYER_FNAME)+1);
		strcpy(player_fname, PLAYER_FNAME);
		if (PHYSFSX_exists(player_fname, 0))
			*j++ = player_fname;
	}
	
	*j = NULL;
	list = realloc(list, (j - list + 1)*sizeof(char *));	// save a bit of memory (or a lot?)
	return list;
}

// Same function as above but takes a real directory as second argument, only adding files originating from this directory.
// This can be used to further seperate files in search path but it must be made sure realpath is properly formatted.
char **PHYSFSX_findabsoluteFiles(const char *path, const char *realpath, const char *const *exts)
{
	char **list = PHYSFS_enumerateFiles(path);
	char **i, **j = list;
	const char *const *k;
	char *ext;
	
	if (list == NULL)
		return NULL;	// out of memory: not so good
	
	for (i = list; *i; i++)
	{
		ext = strrchr(*i, '.');
		if (ext)
			for (k = exts; *k != NULL && d_stricmp(ext, *k); k++) {}	// see if the file is of a type we want
		
		if (ext && *k && (!strcmp(PHYSFS_getRealDir(*i),realpath)))
			*j++ = *i;
		else
			free(*i);
	}
	
	*j = NULL;
	list = realloc(list, (j - list + 1)*sizeof(char *));	// save a bit of memory (or a lot?)
	return list;
}

#if 0
// returns -1 if error
// Gets bytes free in current write dir
PHYSFS_sint64 PHYSFSX_getFreeDiskSpace()
{
#if defined(__linux__) || (defined(__MACH__) && defined(__APPLE__))
	struct statfs sfs;
	
	if (!statfs(PHYSFS_getWriteDir(), &sfs))
		return (PHYSFS_sint64)(sfs.f_bavail * sfs.f_bsize);
	
	return -1;
#else
	return 0x7FFFFFFF;
#endif
}
#endif

#if 0
int PHYSFSX_exists(const char *filename, int ignorecase)
{
	char filename2[PATH_MAX];

	if (!ignorecase)
		return PHYSFS_exists(filename);

	snprintf(filename2, sizeof(filename2), "%s", filename);
	PHYSFSEXT_locateCorrectCase(filename2);

	return PHYSFS_exists(filename2);
}
#else
// PHYSFS_exists is broken on switch for some reason...
// So I have re-implemented my own solution
// Oh yeah, and I totally ignore the "ignorecase" arg
int PHYSFSX_exists(const char *filename, int ignorecase)
{
	char filename2[PATH_MAX];
	snprintf(filename2, sizeof(filename2), "%s", filename);
	if (ignorecase)
		PHYSFSEXT_locateCorrectCase(filename2);

	PHYSFS_file *fp;
	char **list = PHYSFS_enumerateFiles("");
	char **i = list;

	if (list == NULL){
		// out of memory: not so good
		Error("PHYSFS_enumerateFiles returned null...\n");
	}

	for (i = list; *i; i++)
	{
		if (strcmp(filename2, *i) == 0) {
			PHYSFS_freeList(list);
			return 1;
		}
	}
	PHYSFS_freeList(list);

	// aagallag: Hacky workaround
	// The enumerateFiles() function seems to only enumerate files
	// contained within descent.hog.  Let's just try and open
	// the file.
	fp = PHYSFS_openRead(filename2);
	if (!fp)
		return 0;
	PHYSFS_close(fp);
	return 1;
}
#endif

//Open a file for reading, set up a buffer
PHYSFS_file *PHYSFSX_openReadBuffered(const char *filename)
{
	PHYSFS_file *fp;
	PHYSFS_uint64 bufSize;
	char filename2[PATH_MAX];
	
	if (filename[0] == '\x01')
	{
		//FIXME: don't look in dir, only in hogfile
		filename++;
	}
	
	snprintf(filename2, sizeof(filename2), "%s", filename);
	PHYSFSEXT_locateCorrectCase(filename2);
	
	fp = PHYSFS_openRead(filename2);
	if (!fp)
		return NULL;
	
	bufSize = PHYSFS_fileLength(fp);
	while (!PHYSFS_setBuffer(fp, bufSize) && bufSize)
		bufSize /= 2;	// even if the error isn't memory full, for a 20MB file it'll only do this 8 times
	
	return fp;
}

//Open a file for writing, set up a buffer
PHYSFS_file *PHYSFSX_openWriteBuffered(const char *filename)
{
	PHYSFS_file *fp;
	PHYSFS_uint64 bufSize = 1024*1024;	// hmm, seems like an OK size.
	
	fp = PHYSFS_openWrite(filename);
	if (!fp)
		return NULL;
	
	while (!PHYSFS_setBuffer(fp, bufSize) && bufSize)
		bufSize /= 2;
	
	return fp;
}

/* 
 * Add archives to the game.
 * 1) archives from Sharepath/Data to extend/replace builtin game content
 * 2) archived demos
 */
void PHYSFSX_addArchiveContent()
{
	char **list = NULL;
	static const char *const archive_exts[] = { ".dxa", NULL };
	char *file[2];
	int i = 0, content_updated = 0;

	con_printf(CON_DEBUG, "PHYSFS: Adding archives to the game.\n");
	// find files in Searchpath ...
	list = PHYSFSX_findFiles("", archive_exts);
	// if found, add them...
	for (i = 0; list[i] != NULL; i++)
	{
		MALLOC(file[0], char, PATH_MAX);
		MALLOC(file[1], char, PATH_MAX);
		snprintf(file[0], sizeof(char)*PATH_MAX, "%s", list[i]);
		PHYSFSX_getRealPath(file[0],file[1]);
		if (PHYSFS_addToSearchPath(file[1], 0))
		{
			con_printf(CON_DEBUG, "PHYSFS: Added %s to Search Path\n",file[1]);
			content_updated = 1;
		}
		d_free(file[0]);
		d_free(file[1]);
	}
	PHYSFS_freeList(list);
	list = NULL;

#if PHYSFS_VER_MAJOR >= 2
	// find files in DEMO_DIR ...
	list = PHYSFSX_findFiles(DEMO_DIR, archive_exts);
	// if found, add them...
	for (i = 0; list[i] != NULL; i++)
	{
		MALLOC(file[0], char, PATH_MAX);
		MALLOC(file[1], char, PATH_MAX);
		snprintf(file[0], sizeof(char)*PATH_MAX, "%s%s", DEMO_DIR, list[i]);
		PHYSFSX_getRealPath(file[0],file[1]);
		if (PHYSFS_mount(file[1], DEMO_DIR, 0))
		{
			con_printf(CON_DEBUG, "PHYSFS: Added %s to %s\n",file[1], DEMO_DIR);
			content_updated = 1;
		}
		d_free(file[0]);
		d_free(file[1]);
	}

	PHYSFS_freeList(list);
	list = NULL;
#endif

	if (content_updated)
	{
		con_printf(CON_DEBUG, "Game content updated!\n");
		PHYSFSX_listSearchPathContent();
	}
}

// Removes content added above when quitting game
void PHYSFSX_removeArchiveContent()
{
	char **list = NULL;
	static const char *const archive_exts[] = { ".dxa", NULL };
	char *file[2];
	int i = 0;

	// find files in Searchpath ...
	list = PHYSFSX_findFiles("", archive_exts);
	// if found, remove them...
	for (i = 0; list[i] != NULL; i++)
	{
		MALLOC(file[0], char, PATH_MAX);
		MALLOC(file[1], char, PATH_MAX);
		snprintf(file[0], sizeof(char)*PATH_MAX, "%s", list[i]);
		PHYSFSX_getRealPath(file[0],file[1]);
		PHYSFS_removeFromSearchPath(file[1]);
		d_free(file[0]);
		d_free(file[1]);
	}
	PHYSFS_freeList(list);
	list = NULL;

	// find files in DEMO_DIR ...
	list = PHYSFSX_findFiles(DEMO_DIR, archive_exts);
	// if found, remove them...
	for (i = 0; list[i] != NULL; i++)
	{
		MALLOC(file[0], char, PATH_MAX);
		MALLOC(file[1], char, PATH_MAX);
		snprintf(file[0], sizeof(char)*PATH_MAX, "%s%s", DEMO_DIR, list[i]);
		PHYSFSX_getRealPath(file[0],file[1]);
		PHYSFS_removeFromSearchPath(file[1]);
		d_free(file[0]);
		d_free(file[1]);
	}
	PHYSFS_freeList(list);
	list = NULL;
}
