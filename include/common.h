#ifndef _COMMON_H_
#define _COMMON_H_

#define SUCCESS 	0
#define FAILED	 	-1

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c);
int is_char_letter(char c);

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path);
int dir_exists(const char *path);
int unlink_secure(const char *path);
int mkdirs(const char* dir);
int copy_file(const char* input, const char* output);
int set_perms_directory(const char* inputdir, int mode);

/* Truncates s to at most max_bytes without leaving a partial UTF-8 sequence
 * at the end (also repairs a string that was already cut mid-sequence). */
void utf8_truncate(char* s, size_t max_bytes);

#endif
