#pragma once

#ifndef PK2READER_H_
#define PK2READER_H_

//-----------------------------------------------------------------------------

#include "shared_types.h"
#include "blowfish.h"
#include <vector>
#include <map>
#include <list>
#include <sstream>
#include <string>

//-----------------------------------------------------------------------------

#pragma pack(push, 1)

struct PK2Header {
  char name[30]; // PK2 internal name
  uint32_t version; // PK2 version
  uint8_t encryption; // does the PK2 have encryption?
  uint8_t verify[16]; // used to test the blowfish key
  uint8_t reserved[205]; // unused
};

struct PK2Entry {
  uint8_t type; // files are 2, folders are 1, null entries are 0
  char name[81]; // entry name
  uint64_t accessTime; // Windows time format
  uint64_t createTime; // Windows time format
  uint64_t modifyTime; // Windows time format
  int64_t position; // position of data for files, position of children for folders
  uint32_t size; // size of files
  int64_t nextChain; // next chain in the current directory
  uint8_t padding[2]; // So blowfish can be used directly on the structure
};

struct PK2EntryBlock {
  PK2Entry entries[20]; // each block is 20 contiguous entries
};

#pragma pack(pop)

//-----------------------------------------------------------------------------

class PK2Reader {
private:
  FILE *m_file;
  PK2Header m_header;
  int64_t m_root_offset;
  Blowfish m_blowfish;
  std::stringstream m_error;
  std::map<std::string, PK2Entry> m_cache;

private:
  PK2Reader &operator=(const PK2Reader &rhs);
  PK2Reader(const PK2Reader &rhs);
  void Cache(std::string &base_name, PK2Entry &e);

public:
  PK2Reader();
  ~PK2Reader();

  // Returns how many cache entries there are. Each entry is 128 bytes of data. There
  // is also additional overhead of the string containing the full path entry name.
  size_t GetCacheSize();

  // Clears all cached entries.
  void ClearCache();

  // Returns the error if a function returns false.
  std::string GetError();

  // Sets the decryption key used for the PK2. If there is no encryption used, do not
  // call this function. Otherwise, the default variables contain keys for official SRO.
  void SetDecryptionKey(char *ascii_key = "169841",
                        uint8_t ascii_key_length = 6,
                        char *base_key = "\x03\xF8\xE4\x44\x88\x99\x3F\x64\xFE\x35",
                        uint8_t base_key_length = 10);

  // Opens/Closes a PK2 file. There is no overhead for these functions and the file
  // remains open until Close is explicitly called or the PK2Reader object is destroyed.
  bool Open(const char *filename);
  void Close();

  // Returns true of an entry was found with the 'pathname' using 'entry' as the parent. If
  // you want to search from the root, make sure entry is a zero'ed out object.
  bool GetEntry(const char *pathname, PK2Entry &entry);

  // Returns true if a list of entries exists at the 'parent'. This will return the
  // "current directory" of the direct child of the parent. Children of any entries
  // in this list must be 'explored' manually.
  bool GetEntries(PK2Entry &parent, std::list<PK2Entry> &entries);

  // Loops through all PK2 entries and passes them to a user function. Returns true
  // if all entries were processed or false if there was an error along the way.
  // Blocks of 20 are returned instead of individually to allow for better
  // efficiency and flexibility when implementing more complicated logic (like defragment).
  bool ForEachEntryDo(bool (*UserFunc)(const std::string &, PK2EntryBlock &, void *), void *userdata);

  // Extracts the current entry to memory. Returns true on success and false on failure.
  // Users are advised to use on common buffer to reduce the need for frequent memory
  // reallocations on the vector side.
  bool ExtractToMemory(PK2Entry &entry, std::vector<uint8_t> &buffer);
};

//-----------------------------------------------------------------------------

#endif
