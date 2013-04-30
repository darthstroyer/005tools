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

#include "main.h"
#include <vector>

short CHUNK_SIZE = 0x200;
std::map<std::string, int> chunks;
/* +--------------------------------------------------------------------+ */
void chunk_data (std::istream data) {
/* +--------------------------------------------------------------------+ */
/*
    1. Split file into chunks of 512 bytes
    2. Discard any containing only 0xFF
*/
    std::string chunk(CHUNK_SIZE, 0x00);
    data.read(&chunk[0], CHUNK_SIZE);

    for (short i=0; i<CHUNK_SIZE; i++) {
        if (chunk.at(i) != 0xFF)
            goto ADD;
    }

    return;

ADD:
    chunks[chunk]++;
    return;
}

/* +--------------------------------------------------------------------+ */
std::string get_key() {
/* +--------------------------------------------------------------------+ */
    std::string key;
    int max = 0;

    // 3. Iterate over the chunks map and find the key with the largest int
    for (auto &chunk : chunks) {
        if (chunk.second > max) {
            max = chunk.second;
            key = chunk.first;
        }
    }

    return key;
}
