#pragma once

#ifndef SILKROAD_H_
#define SILKROAD_H_

#include <map>
#include <vector>
#include <string>

//-------------------------------------------------------------------------

// http://silkroadaddiction.com/forum2/ecsro-general-discussion/4429-custom-ecsro-division-info-file.html#post45290
/*
[1 Byte Locale]
[1 Byte Division count]
-----
[4 bytes Division name length, does not include the 00]
[Division name followed by 00]
[1 Byte IP count]
--
[4 bytes IP length, does not include the 00]
[IP followed by 00]
-- repeat for each IP in this division
----- repeat for each division
*/

//-------------------------------------------------------------------------

struct Division {
  std::string name;
  std::vector<std::string> addresses;
};

//-------------------------------------------------------------------------

struct DivisionInfo {
  unsigned char locale;
  std::vector<Division> divisions;
  DivisionInfo() { locale = 0; }
};

//-------------------------------------------------------------------------

struct SilkroadData {
  std::string path;
  int version;
  int gatePort;
  DivisionInfo divInfo;
  std::map<std::string, std::string> typeInfo;
  SilkroadData() {
    version = 0;
    gatePort = 0;
  }
};

//-------------------------------------------------------------------------

// Parses a stream of data into a DivisionInfo object
DivisionInfo ParseDivisionInfo(void *buffer, size_t size);

// Loads media.pk2 from the path at the index specified into a SilkroadData object
bool LoadData(std::string path, SilkroadData &obj);

// Removes whitespace at the start and end of a string
void TrimString(std::string &source);

//-------------------------------------------------------------------------

#endif
