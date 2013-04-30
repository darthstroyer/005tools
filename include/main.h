/*
 * +--------------------------------------------------------------------+
 * |
 * | 005Tools by McHaggis
 * |
 * | Back up and restore 3DS/DSi/DS game saves from the command line.
 * | Designed to work with the R4i Save Dongle.
 * |
 * +--------------------------------------------------------------------+

    This file is part of 005Tools.

    005Tools is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    005Tools is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with 005Tools.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>

#include "hidapi.h"

// HID reports are 0x01 report ID followed by 0x40 of data
static const int REPORT_SIZE = 0x40;

struct HIDReport {
    unsigned char reportID;
    unsigned char data[REPORT_SIZE];
};

// This spawns our return buffers, filled by each read or write command
typedef std::vector<char> buffer_t;

class HIDDevice {
    public:
        hid_device *device;
        buffer_t firmware_data;
        std::string name;
        std::string version;
        bool found;
        bool has_card;

        buffer_t card_header;
        std::string card_title;
        std::string card_id;
        int card_size;
        int save_size;

        virtual void read(std::ostream &, int = -1)=0;
        virtual void write(std::istream &, int = -1)=0;
        virtual ~HIDDevice() {}
};

// Functions in tools.cpp
void chunk_data(std::istream data);
std::string get_key();
