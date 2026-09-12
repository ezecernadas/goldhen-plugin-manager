#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>

#include "types.h"
#include "common.h"

#define TMP_BUFF_SIZE 0x20000

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c)
{
	if (c >= '0' && c <= '9')
		return SUCCESS;
	return FAILED;
}

int is_char_letter(char c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
		return SUCCESS;
	return FAILED;
}

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path)
{
    if (access(path, F_OK) == 0) {
    	return SUCCESS;
    }
    
	return FAILED;
}

int dir_exists(const char *path)
{
	if (access(path, W_OK) == 0)
	{
		return SUCCESS;
	}

	return FAILED;
}

int unlink_secure(const char *path)
{   
    if(file_exists(path)==SUCCESS)
    {
        chmod(path, 0777);
		return remove(path);
    }
    return FAILED;
}

/*
* Creates all the directories in the provided path. (can include a filename)
* (directory must end with '/')
*/
int mkdirs(const char* dir)
{
    char path[256];
    snprintf(path, sizeof(path), "%s", dir);

    char* ptr = strrchr(path, '/');
    *ptr = 0;
    ptr = path;
    ptr++;
    while (*ptr)
    {
        while (*ptr && *ptr != '/')
            ptr++;

        char last = *ptr;
        *ptr = 0;

        if (file_exists(path) == FAILED)
        {
            if (mkdir(path, 0777) < 0)
                return FAILED;
            else
                chmod(path, 0777);
        }
        
        *ptr++ = last;
        if (last == 0)
            break;

    }

    return SUCCESS;
}

int copy_file(const char* input, const char* output)
{
    size_t read, written;
    FILE *fd, *fd2;

    if (mkdirs(output) != SUCCESS)
        return FAILED;

    if((fd = fopen(input, "rb")) == NULL)
        return FAILED;

    if((fd2 = fopen(output, "wb")) == NULL)
    {
        fclose(fd);
        return FAILED;
    }

    char* buffer = malloc(TMP_BUFF_SIZE);

    if (!buffer)
    {
        fclose(fd);
        fclose(fd2);
        return FAILED;
    }

    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, fd);
        written = fwrite(buffer, 1, read, fd2);
    }
    while ((read == written) && (read == TMP_BUFF_SIZE));

    free(buffer);
    fclose(fd);
    fclose(fd2);
    chmod(output, 0777);

    return (read - written);
}

void utf8_truncate(char* s, size_t max_bytes)
{
	size_t len, cut, lead, expected;
	unsigned char c;

	if (!s)
		return;

	len = strlen(s);
	cut = (len < max_bytes) ? len : max_bytes;

	// Walk back over continuation bytes (10xxxxxx) to find the lead byte of
	// the code point that contains position cut-1.
	lead = cut;
	while (lead > 0 && ((unsigned char)s[lead - 1] & 0xC0) == 0x80 && cut - lead < 3)
		lead--;

	if (lead == 0 || ((unsigned char)s[lead - 1] & 0xC0) == 0x80)
	{
		// No lead byte: a stray continuation run, drop it entirely.
		s[lead] = '\0';
		return;
	}

	lead--;
	c = (unsigned char)s[lead];
	if (c < 0x80)
		expected = 1;
	else if ((c & 0xE0) == 0xC0)
		expected = 2;
	else if ((c & 0xF0) == 0xE0)
		expected = 3;
	else if ((c & 0xF8) == 0xF0)
		expected = 4;
	else
		expected = 1;

	// Drop the last code point when it would be cut in half.
	if (lead + expected > cut)
		cut = lead;

	s[cut] = '\0';
}

int set_perms_directory(const char* inputdir, int mode)
{
	DIR *d;
	struct dirent *dir;

	d = opendir(inputdir);
	if (!d)
		return FAILED;

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			char dataPath[256] = {0};
			snprintf(dataPath, sizeof(dataPath), "%s" "%s", inputdir, dir->d_name);
			chmod(dataPath, mode);
		}
	}
	closedir(d);

    return SUCCESS;
}
