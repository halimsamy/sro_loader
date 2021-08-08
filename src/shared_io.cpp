#include "shared_io.h"
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

//-----------------------------------------------------------------------------

int file_seek(FILE *file, int64_t offset, int orgin) {
#ifdef _WIN32
  return _fseeki64(file, offset, orgin);
#else
  return fseek(file, offset, orgin);
#endif
}

//-----------------------------------------------------------------------------

int64_t file_tell(FILE *file) {
#ifdef _WIN32
  return _ftelli64(file);
#else
  return ftell(file);
#endif
}

//-----------------------------------------------------------------------------

int file_remove(const char *filename) {
#ifdef _WIN32
  return DeleteFileA(filename);
#else
  return remove(filename);
#endif
}

//-----------------------------------------------------------------------------

std::vector<uint8_t> file_tovector(const char *filename) {
  std::vector<uint8_t> contents;
  FILE *infile = 0;
  fopen_s(&infile, filename, "rb");
  if (infile == 0) {
    return contents;
  }
  file_seek(infile, 0, SEEK_END);
  int64_t size = file_tell(infile);
  file_seek(infile, 0, SEEK_SET);
  try {
    contents.resize((size_t) size);
  }
  catch (std::exception &e) {
    e;
    fclose(infile);
    return contents;
  }
  size_t read_count = 0;
  int64_t index = 0;
  while (size
      && (read_count = fread(&contents[(size_t) index], 1, size > 0x7FFFFFF ? 0x7FFFFFF : (size_t) size, infile))) {
    index += read_count;
    size -= read_count;
  }
  fclose(infile);
  return contents;
}

//-----------------------------------------------------------------------------
