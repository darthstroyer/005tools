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
#include <cstdlib>
#include <cstring>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>

/*
    Our class extends the generic class, so we can create other classes for other devices
    in the future.
*/
class R4iSaveDongle : public HIDDevice {
    private:
        // R4i SaveDongle Device IDs
        static const int
            VID_R4I = 0x04D8,
            PID_R4I = 0x003F,

        /* Number of read reports each command generates
           (device can only transfer 0x40 bytes per read) */
            CMD_FIRMWARE_REPORTS         = 3,
            CMD_GET_HEADER_REPORTS       = 10,
            CMD_START_TRANSFER_REPORTS   = 0,
            CMD_STOP_REPORTS             = 0,
            CMD_READ_DATA_REPORTS        = 8,
            CMD_WRITE_DATA_REPORTS       = 0,
            CMD_WRITE_LARGE_DATA_REPORTS = 0,
            CMD_DESCRIBE_CARD_REPORTS    = 0;

        // Reports that are sent to the HID device
        static unsigned char
            CMD_FIRMWARE[REPORT_SIZE],          // Get information aboute the R4i's firmware
            CMD_GET_HEADER[REPORT_SIZE],        // Get current ROM headers
            CMD_START_TRANSFER[REPORT_SIZE],    // Initiate save extraction process
            CMD_STOP[REPORT_SIZE],              // Abort current process
            CMD_READ_DATA[REPORT_SIZE],         // Read 512 bytes of data from the R4i
            CMD_WRITE_DATA[REPORT_SIZE],        // For saves less than 64kB
            CMD_WRITE_LARGE_DATA[REPORT_SIZE],  // For saves > 64kB
            CMD_DESCRIBE_CARD[REPORT_SIZE];     // Tell R4i how to write to the card

        /* The firmware version is repeated 3 times in the response,
           R4iSaveDongle.exe (v1.5) checks to see if all 3 bytes are the same */
        struct FirmwareReport {
            char unknown[2];
            unsigned char firmware_version1;
            char mcu_eeprom_data1[0x30];
            char unknown1[0x0F];
            unsigned char firmware_version2;
            char mcu_eeprom_data2[0x30];
            char unknown2[0x0F];
            unsigned char firmware_version3;
            char mcu_eeprom_data3[0x30];
            char unknown3[0x0F];
        };

        /* This is mostly empty data for 3DS cards (due to nobody releasing decryption methods).
           For DS(i) cards the structure looks like the normal header from 0x40 onwards
           see http://nocash.emubase.de/gbatek.htm#dscartridgeheader
           */
        struct CardInfo {
            char rom_id[0x04];              // What the R4i SD app uses to identify
            char unknown[0x3C];             // Probably don't care about this
            char title[12];                 // the game's title
            char id[6];                     // the game's ID (and publisher code)
            unsigned char unit_code[2];     // 0 for DS, 1 for DSi (I think)
            unsigned char rom_size;         // 2 ^ rom_size = size in bits
            unsigned char encryption_seed;  // not sure what this is for
            char dont_care[0x1EA];          // don't think any of this applies to us
            unsigned char save_desc1;       // not sure what this number signifies
            unsigned char save_desc2;       // not sure what this number signifies
            unsigned char save_size;        // 2 ^ save_size = kb (almost always)
            char unknown4[0x3D];            // back to not caring again
        };

        // Games that set NDS_BLOCK_WE_FLAG in R4iSaveDongle.exe
/*        static std::map<std::string, bool> set_nds_block_we_flag = {
            {"ADAE", 1}, // Pokemon Diamond US
            {"APAE", 1}, // Pokemon Pearl US
            {"CPUE", 1}  // Pokemon Platinum US
        };*/

        static const short
            CARD_NTR = 0,
            CARD_CTR = 1;

        char card_type;

        bool
            first_pass;
        bool
            in_transfer_mode,
            nds_block_we_flag;

        buffer_t send_command(unsigned char*, int);
        int detect_save_size();
        void transfer_init();
        void transfer_end();

    public:
        R4iSaveDongle();
        ~R4iSaveDongle();
        void read(std::ostream &, int);
        void write(std::istream &, int);
};

// Initialize the teports that are sent to the HID device
unsigned char
    R4iSaveDongle::CMD_FIRMWARE[REPORT_SIZE] = { 0xa0, 0x00, 0x00 },
    R4iSaveDongle::CMD_GET_HEADER[REPORT_SIZE] = { 0x22, 0x22, 0x00 },
    R4iSaveDongle::CMD_START_TRANSFER[REPORT_SIZE] = { 0x11, 0x11 },
    R4iSaveDongle::CMD_STOP[REPORT_SIZE] = { 0x2f, 0x2f, 0x00 },
    R4iSaveDongle::CMD_READ_DATA[REPORT_SIZE] = { 0x33, 0x33, 0x03, 0x00, 0x00, 0x00 },
    R4iSaveDongle::CMD_WRITE_DATA[REPORT_SIZE] = { 0x44, 0x44, 0x00, 0x0a },
    R4iSaveDongle::CMD_WRITE_LARGE_DATA[REPORT_SIZE] = { 0x64, 0x64, 0x00, 0x00 },
    R4iSaveDongle::CMD_DESCRIBE_CARD[REPORT_SIZE] = { 0x66, 0x66, 0x00, 0x00 , 0x00, 0x00, 0x00 };


/* +--------------------------------------------------------------------+
 *
 * ~R4iSaveDongle()
 * Destructor for the class, performs cleanup
 *
 * +--------------------------------------------------------------------+ */
R4iSaveDongle::~R4iSaveDongle() {
/* +--------------------------------------------------------------------+ */
    // Make sure the device clears any reports
    send_command(CMD_STOP, CMD_STOP_REPORTS);
}

/* +--------------------------------------------------------------------+
 *
 * R4iSaveDongle()
 * The main constructor for the class, does all the initialisation
 *
 * +--------------------------------------------------------------------+ */
R4iSaveDongle::R4iSaveDongle() {
/* +--------------------------------------------------------------------+ */
    buffer_t response;
    in_transfer_mode = false;

    // Get a handle on the dongle
    device = hid_open(VID_R4I, PID_R4I, NULL);
    name = "R4i Save Dongle";

    if (device == NULL) {
        found = false;
        return;
    }
    found = true;
    has_card = false;

    // Clear any crap that may have been left on the device from before
    send_command(CMD_STOP, CMD_STOP_REPORTS);

    // Get information about the device's firmware
    firmware_data = send_command(CMD_FIRMWARE, CMD_FIRMWARE_REPORTS);
    FirmwareReport *dev = reinterpret_cast<FirmwareReport *>(&firmware_data[0]);

    // All 3 reports for firmware info describe the firmware version and should be equal
    if (dev->firmware_version1 == dev->firmware_version2 && dev->firmware_version2 == dev->firmware_version3) {
        if (dev->firmware_version1 == 1)
            version = "1.0";
        else {
            std::ostringstream ss;
            ss << ((int) dev->firmware_version1 / 10 + 1) + (float(int(dev->firmware_version1) % 10) / 10);
            version = ss.str();
        }
    }
    else // Not sure this will ever happen, but the R4i software has something similar
        version = -1;

    card_header = send_command(CMD_GET_HEADER, CMD_GET_HEADER_REPORTS);
    CardInfo *info = reinterpret_cast<CardInfo *>(&card_header[0]);

    if (info->title[0] == 0 && info->save_desc1 == 0)
        return;

    has_card = true;

    // 3DS cards cannot be decrypted so we can't detected their titles
    if (info->title[0] != 0x00) {
        card_id = info->id;
        card_title = std::string(info->title).substr(0,12);
    }

    if (info->rom_size > 0)
        card_size = pow(2.0, int(info->rom_size));

    card_type = info->title[0] == 0x00 ? CARD_CTR : CARD_NTR;

    if (info->save_size > 0)
        // This seems to cover all my 3DS games and one of my 256kB DS games
        save_size = pow(2.0, int(info->save_size));
    else {
        // Not sure what save_desc1 and save_desc2 are for, but...
        if (info->save_desc1 == 0x62)
            // This is for Nintendogs (256kB), and... ?
            save_size = (info->save_desc2 == 0x16 ? 256 : 512) * 1024;
        else
            // We have to do it the old fasioned way (not currently working)
            save_size = 0;//detect_save_size();
    }

//    nds_block_we_flag = true;
}

/* +--------------------------------------------------------------------+
 *
 * (buffer_t) send_command()
 * Issues a command to the R4i dongle, and returns any reports generated
 *
 * +--------------------------------------------------------------------+ */
buffer_t R4iSaveDongle::send_command(unsigned char *cmd, int reports) {
/* +--------------------------------------------------------------------+ */
    HIDReport inrep = { 0x00 }, outrep;
    buffer_t retbuf;
    retbuf.reserve(REPORT_SIZE * reports);

    // First byte of the report is always 0x00 for us (the report id)
    memcpy(&inrep.data[0], &cmd[0], REPORT_SIZE);

    hid_write(device, &inrep.reportID, sizeof(inrep));

    int i = 0;
    while (i++ < reports) {
        hid_read(device, &outrep.data[0], sizeof(outrep.data));
        retbuf.insert(retbuf.end(), outrep.data, outrep.data+sizeof(outrep.data));
    }

    return retbuf;
}

/* +--------------------------------------------------------------------+
 *
 * void transfer_init ()
 * Gets the SD ready to send or receive data
 *
 * +--------------------------------------------------------------------+ */
void R4iSaveDongle::transfer_init() {
/* +--------------------------------------------------------------------+ */
    if (in_transfer_mode)
        return;

    // Need to tell the SD more information about the card
    CMD_DESCRIBE_CARD[2] = char(card_type); // 1 for a 3DS card, 0 for NDS/DSi
    CMD_DESCRIBE_CARD[3] = (save_size / 1024 >> 8) & 0xFF;
    CMD_DESCRIBE_CARD[4] = save_size < 1024 ? 0x01 : (save_size / 1024) & 0xFF;
    CMD_DESCRIBE_CARD[5] = !card_type && nds_block_we_flag ? 0x55 : 0x00;
    CMD_DESCRIBE_CARD[6] = !card_type && nds_block_we_flag ? 0xAA : 0x00;

    send_command(CMD_DESCRIBE_CARD, CMD_DESCRIBE_CARD_REPORTS);
    send_command(CMD_START_TRANSFER, CMD_START_TRANSFER_REPORTS);

    first_pass = true;
    in_transfer_mode = true;
}

/* +--------------------------------------------------------------------+
 *
 * void transfer_end ()
 * Finalizes the data transfer and resets various flags
 *
 * +--------------------------------------------------------------------+ */
void R4iSaveDongle::transfer_end() {
/* +--------------------------------------------------------------------+ */
    if (!in_transfer_mode)
        return;

    send_command(CMD_STOP, CMD_STOP_REPORTS);

    in_transfer_mode = false;
}

/* +--------------------------------------------------------------------+
 *
 * void write ()
 * Reads data from the passed stream and writes it to the card
 *
 * +--------------------------------------------------------------------+ */
void R4iSaveDongle::write(std::istream &data, int off = -1) {
/* +--------------------------------------------------------------------+ */
    short write_size = 0x20;
    bool big = save_size > 0xFFFF;

    if (off == -1)
        off = data.tellg();

    // Get the SD ready to receive the data
    transfer_init();

    CMD_WRITE_DATA[1] = big ? 0x00 : 0x44;
    CMD_WRITE_DATA[2] = card_type  && big ? 0x02 : (big ? 0x0A : 0x00);
    CMD_WRITE_DATA[3] = big ? (off >> 16) & 0xFF: 0x02;
    CMD_WRITE_DATA[4] = (off >> 8) & 0xFF;
    CMD_WRITE_DATA[5] = off & 0xFF;

    // The SD software does something along these lines:
    if (card_type || save_size > (512 * 1024)) {
        for (short i = 0; i < 3; i++) {
            int offset = (4 * i) + 6;
            CMD_WRITE_DATA[offset] = 0x02;
            CMD_WRITE_DATA[offset+1] = CMD_WRITE_DATA[3];
            CMD_WRITE_DATA[offset+2] = CMD_WRITE_DATA[4];
            CMD_WRITE_DATA[offset+3] = CMD_WRITE_DATA[5] + ((i+1)*32);
        }
        if (first_pass && off == 0) {
            CMD_WRITE_DATA[18] = 0xD8;
            CMD_WRITE_DATA[19] = CMD_WRITE_DATA[20] = CMD_WRITE_DATA[21] = 0;
            CMD_WRITE_DATA[22] = 0xFE;
            CMD_WRITE_DATA[23] = 0xFD;
            CMD_WRITE_DATA[24] = 0xFB;
            CMD_WRITE_DATA[25] = 0xF8;
        }
        else {
            CMD_WRITE_DATA[18] = 0;
            CMD_WRITE_DATA[19] = 0xA5;
            CMD_WRITE_DATA[20] = 0x5A;
            CMD_WRITE_DATA[25] = 0x55;
        }
    }

    if (big) {
        // The R4iSD.exe sends the data in 4×32B chunks
        for (int i=0; i < 4; i++) {
            CMD_WRITE_LARGE_DATA[2] = CMD_WRITE_LARGE_DATA[3] = i;

            data.read((char*)&CMD_WRITE_LARGE_DATA[4], write_size);

            // CMD_WRITE_DATA is sent after the actual data
            send_command(CMD_WRITE_LARGE_DATA, CMD_WRITE_LARGE_DATA_REPORTS);
            off += write_size;
        }
    }
    else // Send the data in 32B chunks
        data.read((char*)&CMD_WRITE_DATA[6], write_size);

    // CMD_WRITE_DATA is more like a commit for 3DS/big cards
    send_command(CMD_WRITE_DATA, CMD_WRITE_DATA_REPORTS);

    if (card_type && !first_pass && data.tellg() >= (16 * 1024))
        data.seekg(0, std::ios::end);

    if (off >= save_size) {
        if (card_type && first_pass) {
            data.seekg(0, std::ios::beg);
            first_pass = false;
        }
        else
            transfer_end();
    }

    return;
}

/* +--------------------------------------------------------------------+
 *
 * void read ()
 * Transfers the save data from device and writes it to a stream
 *
 * +--------------------------------------------------------------------+ */
void R4iSaveDongle::read(std::ostream &data, int off=-1) {
/* +--------------------------------------------------------------------+ */
    buffer_t response;
    bool big = save_size > 0xFFFF;

    if (off == -1)
        off = data.tellp();

    // Get the SD ready to receive the data
    transfer_init();

    // In the read command, 0x03 seems to signify that the following bytes are the offset
    CMD_READ_DATA[2] = big ? 0x03 : 0x00;
    CMD_READ_DATA[3] = big ? (off >> 16) & 0xFF : 0x03;
    CMD_READ_DATA[4] = (off >> 8) & 0xFF;

    // Read from card and write to file
    response = send_command(CMD_READ_DATA, CMD_READ_DATA_REPORTS);
    data.write((char *)&response[0], response.size());

    // Stop data transfer mode when we reach the end
    if (data.tellp() >= save_size)
       transfer_end();

    return;
}

/* +--------------------------------------------------------------------+
 *
 * int detect_save_size  ()
 * Reads and writes to the card using trial and error to detect size
 *
 * +--------------------------------------------------------------------+ */
int R4iSaveDongle::detect_save_size() {
    /*
        In order to detect a save size that is undetectable to the SD,
        we have to use trial and error methods, in the following order:

            1. Read a chunk of data from the card at an offset
            2. Write a specific chunk of data to the card at offset
            3. Read that chunk back and see if it matches what was written

        If successful:
            4. Write the original chunk of data back to the offset
            5. Increase the offset and repeat if we were successful

        else:
            4. Return the previous offset as the size or 0 if none

        This method doesn't seem to work well, I'm not sure why yet.
    */

    char buff[128] = "005Tools by McHaggis";
    std::fill(&buff[0], &buff[127], 0xFF);
    std::istringstream test_write (std::string(buff, 128), std::ios::binary);
    std::ostringstream test_read (std::ios::binary);
//    std::stringstream data(std::ios::binary);

    int offset = 0;
    for (int i = 26; i > 8; i--) { // test 512B to 32MB
        save_size = pow(2, i);
        offset = save_size - 512;

        // Make a backup of the data at the offset
//        read(data, offset);

        // Perform our test at the offset
//        write(test_write, offset);
        read(test_read, offset);

        // Compare data
        bool is_null = test_read.str().compare(0, 32, test_write.str(), 0, 32);

        if (is_null) {
            // Write the original data back
//            write(data, offset);

            // Reset everything
            test_write.seekg(0);
            test_read.str("");
            test_read.seekp(0);
//            data.str("");
//            data.seekp(0);
//            data.seekg(0);

            // Set offset for next test
//            offset = pow(2, i);
        }
        else {
            std::cout << "\nResult: " << i << std::endl;
            break;
        }
    }

    if (save_size == 1)
        save_size = 0;

    send_command(CMD_STOP, CMD_STOP_REPORTS);
    in_transfer_mode = false;

    return save_size;
}
