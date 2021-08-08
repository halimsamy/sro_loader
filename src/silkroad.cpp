#include "silkroad.h"
#include <windows.h>
#include "pk2reader.h"
#include "BlowFish.h"
#include <algorithm>

//-------------------------------------------------------------------------

// Tokenizes a string into a vector
static std::vector<std::string> TokenizeString(const std::string &str, const std::string &delim) {
  // http://www.gamedev.net/community/forums/topic.asp?topic_id=381544#TokenizeString
  using namespace std;
  vector<string> tokens;
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

//-------------------------------------------------------------------------

// Parses a stream of data into a DivisionInfo object
DivisionInfo ParseDivisionInfo(void *buffer_, size_t size) {
  DivisionInfo info;
  LPBYTE buffer = (LPBYTE) buffer_;
  DWORD index = 0;
  info.locale = buffer[index++];
  if (index > size)
    throw (std::exception("Invalid data format."));
  BYTE divCount = buffer[index++];
  if (index > size)
    throw (std::exception("Invalid data format."));
  for (BYTE x = 0; x < divCount; ++x) {
    Division tmpDiv;
    DWORD nameLength = *((LPDWORD) (buffer + index));
    index += 4;
    if (index > size)
      throw (std::exception("Invalid data format."));
    tmpDiv.name = (char *) (buffer + index);
    index += (nameLength + 1);
    if (index > size)
      throw (std::exception("Invalid data format."));
    BYTE ipCount = buffer[index++];
    if (index > size)
      throw (std::exception("Invalid data format."));
    for (BYTE y = 0; y < ipCount; ++y) {
      DWORD ipLength = *((LPDWORD) (buffer + index));
      index += 4;
      if (index > size)
        throw (std::exception("Invalid data format."));
      std::string ip = (char *) (buffer + index);
      tmpDiv.addresses.push_back(ip);
      index += (ipLength + 1);
      if (index > size)
        throw (std::exception("Invalid data format."));
    }
    info.divisions.push_back(tmpDiv);
  }
  return info;
}

//-------------------------------------------------------------------------

// Loads media.pk2 from the path at the index specified into a SilkroadData object
bool LoadData(std::string path, SilkroadData &obj) {
  PK2Reader reader;
  std::string s = path;
  if (s[s.size() - 1] != '\\' && s[s.size() - 1] != '/') {
    s += "\\";
  }
  s += "media.pk2";

  // Try to open the PK2
  if (reader.Open(s.c_str()) == false) {
    reader.Close();
    char er[1024] = {0};
    _snprintf_s(er, 1023, "The PK2Reader could not load %s due to an error: ", s.c_str(), reader.GetError().c_str());
    MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
    return false;
  }

  PK2Entry entry_type_txt = {0};
  if (reader.GetEntry("type.txt", entry_type_txt) == false) {
    char er[1024] = {0};
    _snprintf_s(er, 1023, "The PK2Reader could not find %s", "type.txt");
    MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
    return false;
  }

  PK2Entry entry_sv_t = {0};
  if (reader.GetEntry("SV.T", entry_sv_t) == false) {
    char er[1024] = {0};
    _snprintf_s(er, 1023, "The PK2Reader could not find %s", "SV.T");
    MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
    return false;
  }

  PK2Entry entry_gateport_txt = {0};
  if (reader.GetEntry("GATEPORT.TXT", entry_gateport_txt) == false) {
    char er[1024] = {0};
    _snprintf_s(er, 1023, "The PK2Reader could not find %s", "GATEPORT.TXT");
    MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
    return false;
  }

  PK2Entry entry_divisioninfo_txt = {0};
  if (reader.GetEntry("DIVISIONINFO.TXT", entry_divisioninfo_txt) == false) {
    char er[1024] = {0};
    _snprintf_s(er, 1023, "The PK2Reader could not find %s", "DIVISIONINFO.TXT");
    MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
    return false;
  }

  // Type.txt parser
  {
    PK2Entry &entry = entry_type_txt;

    std::vector<uint8_t> buffer;
    if (reader.ExtractToMemory(entry, buffer) == false) {
      char er[1024] = {0};
      _snprintf_s(er, 1023, "The PK2Reader could not load %s", "type.txt");
      MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
      return false;
    }
    buffer.push_back(0);

    std::vector<std::string> tokens = TokenizeString((char *) &buffer[0], "\n=");
    std::string key;
    std::string data;
    for (size_t x = 0; x < tokens.size(); ++x) {
      if ((x + 1) % 2 == 1) {
        key = tokens[x];
      } else {
        data = tokens[x];
        TrimString(data);
        TrimString(key);
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        data.erase(data.begin());
        data.erase(data.end() - 1);
        obj.typeInfo[key] = data;
      }
    }
  }

  // SV.T parser
  {
    PK2Entry &entry = entry_sv_t;

    std::vector<uint8_t> buffer;
    if (reader.ExtractToMemory(entry, buffer) == false) {
      char er[1024] = {0};
      _snprintf_s(er, 1023, "The PK2Reader could not load %s", "SV.T");
      MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
      return false;
    }

    Blowfish bf;
    bf.Initialize((void *) "SILKROADVERSION", 8);
    bf.Decode(&buffer[4], 8, &buffer[4], buffer.size() - 4);
    obj.version = atoi((char *) &buffer[4]);
  }

  // GATEPORT.TXT parser
  {
    PK2Entry &entry = entry_gateport_txt;

    std::vector<uint8_t> buffer;
    if (reader.ExtractToMemory(entry, buffer) == false) {
      char er[1024] = {0};
      _snprintf_s(er, 1023, "The PK2Reader could not load %s", "GATEPORT.TXT");
      MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
      return false;
    }
    buffer.push_back(0);
    obj.gatePort = atoi((char *) &buffer[0]);
  }

  // DIVISIONINFO.TXT parser
  {
    PK2Entry &entry = entry_divisioninfo_txt;

    std::vector<uint8_t> buffer;
    if (reader.ExtractToMemory(entry, buffer) == false) {
      char er[1024] = {0};
      _snprintf_s(er, 1023, "The PK2Reader could not load %s", "DIVISIONINFO.TXT");
      MessageBoxA(0, er, "Fatal Error", MB_ICONERROR);
      return false;
    }

    try {
      obj.divInfo = ParseDivisionInfo(&buffer[0], buffer.size());
    }
    catch (std::exception &e) {
      UNREFERENCED_PARAMETER(e);
      MessageBoxA(0, "There was an error parsing the divisions file.", "Fatal Error", MB_ICONERROR);
      return false;
    }
  }
  obj.path = path;
  reader.Close();
  return true;
}

//-------------------------------------------------------------------------

// Removes whitespace at the start and end of a string
void TrimString(std::string &source) {
  while (!source.empty() && isspace(source[0])) {
    source.erase(source.begin());
  }
  while (!source.empty() && isspace(source[source.size() - 1])) {
    source.erase(source.end() - 1);
  }
}

//-------------------------------------------------------------------------
