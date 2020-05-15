/* duplicates.c */

#include "hash.h"
#include "macros.h"
#include "table.h"

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* Globals */

char * PROGRAM_NAME = NULL;

/* Structures */

typedef struct {
    bool count;
    bool quiet;
} Options;


/* Functions */

void usage(int status) {
    fprintf(stderr, "Usage: %s paths...\n", PROGRAM_NAME);
    fprintf(stderr, "    -c     Only display total number of duplicates\n");
    fprintf(stderr, "    -q     Do not write anything (exit with 0 if duplicate found)\n");
    exit(status);
}

/**
 * Check if path is a directory.
 * @param       path        Path to check.
 * @return      true if Path is a directory, otherwise false.
 */
bool is_directory(const char *path) {
	struct stat s;

	// check if stat fails
	if (stat(path, &s) < 0)
		return false;
    return S_ISDIR(s.st_mode);
}

/**
 * Check if file is in table of checksums.
 *
 *  If quiet is true, then exit if file is in checksums table.
 *
 *  If count is false, then print duplicate association if file is in
 *  checksums table.
 *
 * @param       path        Path to file to check.
 * @param       checksums   Table of checksums.
 * @param       options     Options.
 * @return      0 if Path is not in checksums, otherwise 1.
 */
size_t check_file(const char *path, Table *checksums, Options *options) {
	char hex[BUFSIZ];
	if (hash_from_file(path, hex)){
		Value *v;
		v = table_search(checksums, hex);

		if (v != NULL && !options->count && !options->quiet){
			printf("%s is a duplicate of %s\n", path, v->string);
			return 1;
		}
		else if (v != NULL && (options->quiet || options->count))
			return 1; 
		else if (v == NULL){
			Value val;
			// use strdup since path is const
			val.string = strdup(path);
			table_insert(checksums, hex, val, STRING);
			// free val.string to avoid any memory leaks
			free(val.string);
			return 0;
		}
		else			
			return 0;
	}
	else
		return 0;
}

/**
 * Check all entries in directory (recursively).
 * @param       root        Path to directory to check.
 * @param       checksums   Table of checksums.
 * @param       options     Options.
 * @return      Number of duplicate entries in directory.
 */
size_t check_directory(const char *root, Table *checksums, Options *options) {
	int count = 0;
	
	DIR *dir = opendir(root);

	// realpath gets the full path of root
	char pathBuf[PATH_MAX];
	char *path = realpath(root, pathBuf);
	
	// check if opening the directory fails
	if (!dir){
		fprintf(stderr, "Unable to open directory on %s: %s\n", root, strerror(errno));
		return 0;
	}
	
	for (struct dirent *curr = readdir(dir); curr; curr = readdir(dir)){
		if (streq(curr->d_name, ".") || streq(curr->d_name, ".."))
			continue;
		// appending current entry to the path
		char currPath[PATH_MAX];
		sprintf(currPath, "%s/%s", root, curr->d_name);
		if (is_directory(curr->d_name)){
			// appending current dircetory to the path
			sprintf(currPath, "%s/%s", path, curr->d_name);
		}

		if (is_directory(currPath)){
			count += check_directory(currPath, checksums, options);
		}
		else
			count += check_file(currPath, checksums, options);
	}
	closedir(dir);
    return count;
}

/* Main Execution */

int main(int argc, char *argv[]) {
    /* Parse command line arguments */
	int argind = 1;
	PROGRAM_NAME = argv[0];

	Options *flags = malloc(sizeof(Options));
	flags->count = false;
	flags->quiet = false;

	if (argc == 1)
		return 0;

	while (argind < argc && strlen(argv[argind]) > 1 && argv[argind][0] == '-'){
		char *arg = argv[argind++];
		switch (arg[1]){
			case 'h':
				usage(0);
				break;
			case 'c':
				flags->count = true;
				break;
			case 'q':
				flags->quiet = true;
				break;
			default:
				break;
		}	
	}

	// store the agruments that arent flags into path array
	int num_non_flags = argc-argind;
	const char* path[num_non_flags];
	for (int i = 0; i < num_non_flags; i++){
		path[i] = argv[argind + i];
	}


    /* Check each argument */
	Table *files;
	files = table_create(argind);
	int count = 0;

	for (int i = 0; i < num_non_flags; i++){
		if (is_directory(path[i]))
			count += check_directory(path[i], files, flags);
		else
			count += check_file(path[i], files, flags);
	}


    /* Display count if necessary */

	// set return value based on whcih flags are set
	int return_val;
	if (flags->count)
		printf("%d\n", count);
	if (count == 0 && flags->quiet)
		return_val = 1;
	else
		return_val = 0;

	files = table_delete(files);
	free(flags);

    return return_val;
}

/* vim: set sts=4 sw=4 ts=8 expandtab ft=c: */
