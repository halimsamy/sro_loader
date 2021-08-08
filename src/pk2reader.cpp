#include "pk2reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "shared_io.h"

//-----------------------------------------------------------------------------

int MakePathSlashWindows_1(int ch) {
  return ch == '/' ? '\\' : ch;
}

//-----------------------------------------------------------------------------

// I use different variations of this code depending on the program, so it's not going
// to be in a common file.
std::list<std::string> TokenizeString_1(const std::string &str, const std::string &delim) {
  // http://www.gamedev.net/community/forums/topic.asp?topic_id=381544#TokenizeString
  using namespace std;
  list<string> tokens;
  size_t p0 = 0, p1 = string::npos;
  while (p0 != string::npos) {
    p1 = str.find_first_of(delim, p0);
    if (p1 != p0) {
      string token = str.substr(p0, p1 - p0);
      tokens.push_back(token);
    }
    p0 = str.find_first_not_of(delim, p1);
  }
  return tokens;
}

//-----------------------------------------------------------------------------

void PK2Reader::Cache(std::string &base_name, PK2Entry &e) {
  m_cache[base_name] = e;
}

//-----------------------------------------------------------------------------

PK2Reader::PK2Reader() {
  m_root_offset = 0;
  m_file = 0;
  memset(&m_header, 0, sizeof(PK2Header));
  SetDecryptionKey();
}

//-----------------------------------------------------------------------------

PK2Reader::~PK2Reader() {
  Close();
}

//-----------------------------------------------------------------------------

size_t PK2Reader::GetCacheSize() {
  return m_cache.size();
}

//-----------------------------------------------------------------------------

void PK2Reader::ClearCache() {
  m_cache.clear();
}

//-----------------------------------------------------------------------------

std::string PK2Reader::GetError() {
  std::string e = m_error.str();
  m_error.str("");
  return e;
}

//-----------------------------------------------------------------------------

void PK2Reader::SetDecryptionKey(char *ascii_key, uint8_t ascii_key_length, char *base_key, uint8_t base_key_length) {
  if (ascii_key_length > 56) {
    ascii_key_length = 56;
  }

  uint8_t bf_key[56] = {0x00};

  uint8_t a_key[56] = {0x00};
  memcpy(a_key, ascii_key, ascii_key_length);

  uint8_t b_key[56] = {0x00};
  memcpy(b_key, base_key, base_key_length);

  for (int x = 0; x < ascii_key_length; ++x) {
    bf_key[x] = a_key[x] ^ b_key[x];
  }

  m_blowfish.Initialize(bf_key, ascii_key_length);
}

//-----------------------------------------------------------------------------

void PK2Reader::Close() {
  if (m_file) {
    fclose(m_file);
    m_file = 0;
  }

  m_cache.clear();
  m_root_offset = 0;
  m_error.str("");
  memset(&m_header, 0, sizeof(PK2Header));
}

//-----------------------------------------------------------------------------

bool PK2Reader::Open(const char *filename) {
  size_t read_count = 0;

  if (m_file != 0) {
    m_error << "There is already a PK2 opened.";
    return false;
  }

  fopen_s(&m_file, filename, "rb");
  if (m_file == 0) {
    m_error << "Could not open the file \"" << filename << "\".";
    return false;
  }

  read_count = fread(&m_header, 1, sizeof(PK2Header), m_file);
  if (read_count != sizeof(PK2Header)) {
    fclose(m_file);
    m_file = 0;
    m_error << "Could not read in the PK2Header.";
    return false;
  }

  char name[30] = {0};
  memcpy(name, "JoyMax File Manager!\n", 21);
  if (memcmp(name, m_header.name, 30) != 0) {
    fclose(m_file);
    m_file = 0;
    m_error << "Invalid PK2 name.";
    return false;
  }

  if (m_header.version != 0x01000002) {
    fclose(m_file);
    m_file = 0;
    m_error << "Invalid PK2 version.";
    return false;
  }

  m_root_offset = file_tell(m_file);

  if (m_header.encryption == 0) {
    return true;
  }

  uint8_t verify[16] = {0};
  m_blowfish.Encode("Joymax Pak File", 16, verify, 16);
  memset(verify + 3, 0, 13); // PK2s only store 1st 3 bytes

  if (memcmp(verify, m_header.verify, 16) != 0) {
    fclose(m_file);
    m_file = 0;
    m_error << "Invalid Blowfish key.";
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::GetEntries(PK2Entry &parent, std::list<PK2Entry> &entries) {
  if (m_file == 0) {
    m_error << "There is no PK2 loaded yet.";
    return false;
  }

  PK2EntryBlock block;
  size_t read_count = 0;

  if (parent.type != 1) {
    m_error << "Invalid entry type. Only folders are allowed.";
    return false;
  }

  if (parent.position < m_root_offset) {
    m_error << "Invalid seek index.";
    return false;
  }

  if (file_seek(m_file, parent.position, SEEK_SET) != 0) {
    m_error << "Invalid seek index.";
    return false;
  }

  while (true) {
    read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file);
    if (read_count != sizeof(PK2EntryBlock)) {
      m_error << "Could not read a PK2EntryBlock object";
      return false;
    }

    for (int x = 0; x < 20; ++x) {
      PK2Entry &e = block.entries[x];

      if (m_header.encryption) {
        m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));
      }

      // Protect against possible user seeking errors
      if (e.padding[0] != 0 || e.padding[1] != 0) {
        m_error << "The padding is not NULL. User seek error.";
        return false;
      }

      if (e.type == 1 || e.type == 2) {
        entries.push_back(e);
      }
    }

    if (block.entries[19].nextChain) {
      // More entries in the current directory
      if (file_seek(m_file, block.entries[19].nextChain, SEEK_SET) != 0) {
        m_error << "Invalid seek index for nextChain.";
        return false;
      }
    } else {
      // Out of the entries for the current directory
      break;
    }
  }

  return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::GetEntry(const char *pathname, PK2Entry &entry) {
  if (m_file == 0) {
    m_error << "There is no PK2 loaded yet.";
    return false;
  }

  std::string base_name = pathname;
  std::transform(base_name.begin(), base_name.end(), base_name.begin(), tolower);
  std::transform(base_name.begin(), base_name.end(), base_name.begin(), MakePathSlashWindows_1);

  std::list<std::string> tokens = TokenizeString_1(base_name, "\\");

  // Check the cache first so we can save some time on frequent accesses
  std::map<std::string, PK2Entry>::iterator itr = m_cache.find(base_name);
  if (itr != m_cache.end()) {
    entry = itr->second;
    return true;
  }

  PK2EntryBlock block;
  size_t read_count = 0;
  std::string name;

  if (entry.position == 0) {
    if (file_seek(m_file, m_root_offset, SEEK_SET) != 0) {
      m_error << "Invalid seek index.";
      return false;
    }
  } else {
    if (file_seek(m_file, entry.position, SEEK_SET) != 0) {
      m_error << "Invalid seek index.";
      return false;
    }
  }

  while (!tokens.empty()) {
    std::string path = tokens.front();
    tokens.pop_front();

    read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file);
    if (read_count != sizeof(PK2EntryBlock)) {
      m_error << "Could not read a PK2EntryBlock object";
      return false;
    }

    bool cycle = false;

    for (int x = 0; x < 20; ++x) {
      PK2Entry &e = block.entries[x];

      // I opt to decode entries as we process them rather than before hand to save 'some' processing
      // from extra entries we don't have to search.
      if (m_header.encryption) {
        m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));
      }

      // Protect against possible user seeking errors
      if (e.padding[0] != 0 || e.padding[1] != 0) {
        m_error << "The padding is not NULL. User seek error.";
        return false;
      }

      if (e.type == 0) {
        continue;
      }

      // Incurs some overhead in the long run, but the convenience it gives of not knowing exact
      // case is well worth it!
      name = e.name;
      std::transform(name.begin(), name.end(), name.begin(), tolower);

      if (name == path) {
        // We are at the end of the list of paths to find
        if (tokens.empty()) {
          entry = e;
          Cache(base_name, e);
          return true;
        } else {
          // We want to make sure we only search folders, otherwise
          // bugs could result.
          if (e.type == 1) {
            if (file_seek(m_file, e.position, SEEK_SET) != 0) {
              m_error << "Invalid seek index.";
              return false;
            }
            cycle = true;
            break;
          }

          m_error << "Invalid entry, files cannot have children!";

          // Invalid entry (files can't have children!)
          return false;
        }
      }
    }

    // We found a path entry, continue down the list
    if (cycle) {
      continue;
    }

    // More entries to search in the current directory
    if (block.entries[19].nextChain) {
      if (file_seek(m_file, block.entries[19].nextChain, SEEK_SET) != 0) {
        m_error << "Invalid seek index for nextChain.";
        return false;
      }
      tokens.push_front(path);
      continue;
    }

    // If we get here, what we looking for does not exist
    break;
  }

  m_error << "The entry does not exist";

  return false;
}

//-----------------------------------------------------------------------------

bool PK2Reader::ForEachEntryDo(bool (*UserFunc)(const std::string &, PK2EntryBlock &, void *), void *userdata) {
  if (m_file == 0) {
    m_error << "There is no PK2 loaded yet.";
    return false;
  }

  PK2EntryBlock block;
  size_t read_count = 0;

  if (file_seek(m_file, m_root_offset, SEEK_SET) != 0) {
    m_error << "Invalid seek index.";
    return false;
  }

  std::list<PK2Entry> folders;
  std::list<std::string> paths;

  std::string path;

  do {
    if (!folders.empty()) {
      PK2Entry e = folders.front();
      folders.pop_front();
      if (file_seek(m_file, e.position, SEEK_SET) != 0) {
        m_error << "Invalid seek index.";
        return false;
      }
    }

    if (!paths.empty()) {
      path = paths.front();
      paths.pop_front();
    }

    read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file);
    if (read_count != sizeof(PK2EntryBlock)) {
      m_error << "Could not read a PK2EntryBlock object";
      return false;
    }

    for (int x = 0; x < 20; ++x) {
      PK2Entry &e = block.entries[x];

      if (m_header.encryption) {
        m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));

        // Protect against possible user seeking errors
        if (e.padding[0] != 0 || e.padding[1] != 0) {
          m_error << "The padding is not NULL. User seek error.";
          return false;
        }
      }

      if (e.type == 1) {
        if (e.name[0] == '.' && (e.name[1] == 0 || (e.name[1] == '.' && e.name[2] == 0))) {
        } else {
          std::string cpath = path;
          if (cpath.empty()) {
            cpath = e.name;
          } else {
            cpath += "\\";
            cpath += e.name;
          }
          folders.push_back(e);
          paths.push_back(cpath);
        }
      }
    }

    if (block.entries[19].nextChain) {
      int64_t pos = block.entries[19].position;
      block.entries[19].position = block.entries[19].nextChain;
      folders.push_front(block.entries[19]);
      paths.push_front(path);
      block.entries[19].position = pos;
    }

    if ((*UserFunc)(path, block, userdata) == false) {
      break;
    }
  } while (!folders.empty());

  return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::ExtractToMemory(PK2Entry &entry, std::vector<uint8_t> &buffer) {
  if (entry.type != 2) {
    m_error << "The entry is not a file.";
    return false;
  }
  buffer.resize(entry.size);
  if (buffer.empty()) {
    return true;
  }
  file_seek(m_file, entry.position, SEEK_SET);
  size_t read_count = fread(&buffer[0], 1, entry.size, m_file);
  if (read_count != entry.size) {
    buffer.clear();
    m_error << "Could read all of the file data.";
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
