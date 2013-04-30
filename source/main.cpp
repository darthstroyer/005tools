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

#define APPVERSION "0.1c"
#define APPNAME "005Tools"
#define APPCMD "005tools"

/* +--------------------------------------------------------------------+ */

#include <clocale>
#include <iomanip>
#include <typeinfo>
#include <array>
#include "main.h"
#include "r4isd.h"

#ifdef __linux__
  #include <csignal>
  #include <termios.h>
  #include <unistd.h>
#else
  #include <windows.h>
#endif

using namespace std;
/* +--------------------------------------------------------------------+ */

/* +-Macro Functions----------------------------------------------------+ */
#ifdef __linux__
  #define CURSOR_ON() system("setterm -cursor on");
  #define CURSOR_OFF() system("setterm -cursor off");

  termios oldt;

  #define HIDE_INPUT() \
    tcgetattr(STDIN_FILENO, &oldt); \
    termios newt = oldt; \
    newt.c_lflag &= ~ECHO; \
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  #define SHOW_INPUT() \
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#else
  // I'll get to these later
  #define CURSOR_ON()
  #define CURSOR_OFF()
  #define SHOW_INPUT()
  #define HIDE_INPUT()
#endif

/* +-Command Line Arguments---------------------------------------------+ */

// Our HID device
HIDDevice *dev;

// Commands
const int ARG_INFO     = 1;
const int ARG_DOWNLOAD = 2;
const int ARG_UPLOAD   = 3;
const int ARG_ERASE    = 4;

int arg_passed;
string arg_filename;

// Command line options
string rom_header_output;
string firmware_output;
int override_save_size = 0; // Allow the user to override the detected save size
bool output_key = false;

struct cmd_opt {
    string short_name;
    string description;
    string value_desc;
    bool value_required;
    bool specified;
    string value;
};

map<string, cmd_opt> opts_in = {
    { "--help",           { "-?", "Shows this help", "", NULL } },
    { "--output-header",  { "-h", "Save game header from device to FILE", "FILE", true } },
    { "--output-firmware",{ "-f", "Save firmware information from device to FILE", "FILE", true } },
    { "--key-file",       { "-k", "Specifies the encryption key file to either save to or use", "FILE", true } },
    { "--save-size",      { "-s", "Override detected save size with BYTES", "BYTES", true } },
};

struct command {
    string description;
    string file1;
    string file2;
};
map<string, command> cmd_in = {
    { "info", { "Display information about the currently inserted game card" } },
    { "download", { "Downloads the currently inserted game card's save data and writes it to <filename>" } },
    { "upload", { "Overwrites the currently inserted game card's save data with data from <filename>" } },
    { "erase", { "Erase the save data stored on the currently inserted game card" } }
};

/* +-Functions----------------------------------------------------------+ */
void device_ops();
void draw_progress();
void handle_sigint();
int write_save_data();
int read_save_data();

/* +--------------------------------------------------------------------+
 *
 * int draw_progress ()
 * Draw a progress bar to stdout
 *
 * +--------------------------------------------------------------------+ */
void draw_progress(double total, double current) {
/* +--------------------------------------------------------------------+ */
    int width = 40;
    double fraction = current / total;
    int chars = round(fraction * width);

    printf("%4.0f%% [",fraction*100);

    int i=0;
    for (; i < chars-1; i++)
        cout << "=";

    cout << ">";

    for (; i < width-1; i++)
        cout << " ";

    cout << "] ";
#ifdef __linux__
    printf("%'u / %'u bytes", (int)current, (int)total);
#else
    printf("%u / %u bytes", (int)current, (int)total);
#endif

#ifdef __linux__
    cout << endl;
#else
    cout << " ";
#endif

    if (current != total)
        cout << "\b\r";
}

/* +--------------------------------------------------------------------+
 *
 * device_ops()
 * Start device operations
 *
 * +--------------------------------------------------------------------+ */
void device_ops() {
/* +--------------------------------------------------------------------+ */
    // Initialize the HID API
    hid_init();
    dev = new R4iSaveDongle;

    if(!dev->found) {
        // Could try a different device here, such as the NDS Backup Adapter Plus
        cout << "Device not found (protip: make sure the device is plugged in, and permissions are set)\n";
        return;
    }

    // We found our device, let's give some confirmation
    cout << dev->name << " v" << dev->version << " found." << "\n";

    // Output firmware data to file for debugging, if requested
    if (firmware_output.length() > 0) {
        fstream firmware_file (firmware_output, ios::out | ios::binary);
        if (firmware_file.is_open()) {
            firmware_file.write(&dev->firmware_data[0], dev->firmware_data.size());
            firmware_file.close();
        }
        else
            cerr << "Unable to open " << firmware_output << " for writing, check your permissions.\n";
    }

    // Output card header to file for debugging, if requested
    if (rom_header_output.length() > 0) {
        fstream rom_header_file (rom_header_output, ios::out | ios::binary);
        if (rom_header_file.is_open()) {
            rom_header_file.write(&dev->card_header[0], dev->card_header.size());
            rom_header_file.close();
        }
        else
            cerr << "Unable to open " << rom_header_output << " for writing, check your permissions.\n";
    }

    // Can't continue without a card to work with
    if (!dev->has_card) {
        cerr << "No card inserted!\n";
        return;
    }

    // Dump some info about the game
    if (dev->card_title.size() > 0) {
        cout << "Detected game: " << dev->card_id << " " << dev->card_title;
        cout << " (" << dev->card_size << "Mb" << ")\n";
    }
    else // only neimod can decrypt the 3DS headers :-/
        cout << "Detected unknown CTR-005 game (encrypted 3DS card)\n";

    // Output game save size
    cout << "Save game size: ";

    if (override_save_size > 0)
        dev->save_size = override_save_size;

    if (dev->save_size > 0) {
        if (dev->save_size > 1048576)
            cout << (dev->save_size / 1048576.00) << "MB";
        else
            cout << (dev->save_size / 1024.00) << "kB";

        cout << (override_save_size > 0 ? " (user defined)\n" : "\n");
    }
    else {
        cout << "(unknown)\n";
    }

    // If all we're doing is outputting info, let's get out now.
    if (arg_passed == ARG_INFO)
        return;

    // Most other commands need us to know the save size
    if (arg_passed != ARG_UPLOAD && dev->save_size <= 0) {
        cerr << "Unable to detect save size (override with --save-size=BYTES).\n";
        return;
    }

    cout << endl;

    if (arg_passed == ARG_ERASE) {
        char buff[128];
        std::fill(&buff[0], &buff[0] + 128, 0xFF);

        // We create an istrstream that can behave like istream and
        // fill it with 128 bytes of zeros before passing to dev->write()
        std::istringstream is (std::string(buff, 128), ios::binary);

        int i=0, written=0;
        do {
            dev->write(is, written);
            written = ++i * is.tellg();
            is.seekg(0); // Reset the position in the stream for the next iteration
            draw_progress(dev->save_size, written);
        }
        while (written < dev->save_size);

        cout << "\nData successfully written to game card.\n";
        return;
    }

    fstream file (arg_filename, ios::binary | (arg_passed == ARG_DOWNLOAD ? ios::out : ios::in));

    if (arg_passed == ARG_DOWNLOAD) {
        do {
            dev->read(file);
            draw_progress(dev->save_size, file.tellp());
        }
        while (file.tellp() < dev->save_size);
        cout << "\nData successfully downloaded to " << arg_filename << ".";
    }
    else if (arg_passed == ARG_UPLOAD) {
        // Get the file size by subtracting the beginning position from the end
        long file_size = 1 + file.seekg(-1, ios::end).tellg() - file.seekg(0, ios::beg).tellg();

        // We can guess the save size from the passed file's size
        if (dev->save_size <= 0)
            dev->save_size = file_size;
        else if (dev->save_size != (file_size)) {
            cerr << "Mismatched file size is " << file_size
                 << " bytes, save size is " << dev->save_size << " bytes.\n";

            return;
        }

        do {
            dev->write(file);
            draw_progress(dev->save_size, file.tellg());
        }
        while (file.tellg() < (dev->save_size));
        cout << "\nData successfully written to game card.";
    }

    cout << endl;
}

/* +--------------------------------------------------------------------+
 *
 * handle_sigint()
 * Handler for Ctrl+C, need to clean up devices here
 *
 * +--------------------------------------------------------------------+ */
void handle_sigint(int s) {
/* +--------------------------------------------------------------------+ */
    // Make sure the cursor is back on and input is shown
    CURSOR_ON();
    SHOW_INPUT();

    // Let the device clean itself up
    delete dev;

    // Stick a couple of newlines in for good measure
    cout << "\n" << endl;
    exit(1);
}

/* +--------------------------------------------------------------------+ */
int main(int argc, char *argv[]) {
/* +--------------------------------------------------------------------+ */
    bool cmd_set = false;
    setlocale(LC_ALL, "");
    cout << "\n" << APPNAME << " v" << APPVERSION << " by McHaggis\n";

    CURSOR_OFF();
    HIDE_INPUT();

    vector<string> args(argv, argv+argc);

    // Display help if no arguments...
    if (argc < 2)
        goto help;

    for (size_t i=1; i<args.size(); i++) {
        string &arg = args[i];

        // If first char is -, it's an option
        if (arg.substr(0,1) == "-") {
            istringstream iss(arg);
            string opt_name;
            string opt_val;
            getline(iss, opt_name, '=');
            getline(iss, opt_val);

            // See if it matches our options
            if (!opts_in.count(opt_name)) {
                // Otherwise see if it maps to a short name
                for (auto &opt_long : opts_in) {
                    if (opt_long.second.short_name == opt_name) {
                        opt_name = opt_long.first;
                        break;
                    }
                }
                // Check again, exit if no match
                if (!opts_in.count(opt_name)) {
                    cerr << "ERROR: '" << opt_name << "' is not a valid option for this application.\n" << endl;
                    goto error;
                }
            }

            if (opt_name == "--help")
                goto help;
            else {
                cmd_opt &opt = opts_in[opt_name];
                opt.specified = true;
                if (opt_val.size() == 0) {
                    if (i == args.size() -1) {
                        cerr << "ERROR: No value specified for option '" << opt_name << "'.\n" << endl;
                        goto error;
                    }
                    else
                        opt.value = args.at(++i);
                }
                else
                    opt.value = opt_val;

                cout << opts_in["--save-size"].value << endl;
            }
        }
        else if (!cmd_set) {
            if (arg == "info")
                arg_passed = ARG_INFO;
            else if (arg == "download")
                arg_passed = ARG_DOWNLOAD;
            else if (arg == "upload")
                arg_passed = ARG_UPLOAD;
            else if (arg == "erase")
                arg_passed = ARG_ERASE;
            else {
                cerr << "ERROR: '" << arg << "' is not a valid command for this application.\n" << endl;
                goto error;
            }

            cmd_set = true;
        }
        else if (arg_passed == ARG_DOWNLOAD || arg_passed == ARG_UPLOAD) {
            arg_filename = arg;
        }
    }

    if ((arg_passed == ARG_DOWNLOAD || arg_passed == ARG_UPLOAD) && arg_filename == "") {
        cerr << "ERROR: <filename> is required for upload and download operations.\n" << endl;
        goto error;
    }

#ifdef __linux__
    // Setup SIGINT handler
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = handle_sigint;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
#endif

    // Finally, start dicking around with the device
    device_ops();
    CURSOR_ON();
    SHOW_INPUT();

    cout << endl;

    return 1;

help:
    // Display help if no arguments
    cout << "Usage: " << APPCMD << " [OPTIONS] <command> <filename>\n\n"
         << "The following commands are available:\n\n";

    for (auto &cmd : cmd_in) {
        cout << "\t" << setw(30) << left
             << cmd.first
             << cmd.second.description << endl;
    }

    cout << "\nThe following options are available:\n\n";

    for (auto &opt : opts_in) {
        cout << "\t" << setw(30) << left
             << (opt.second.short_name + ", " +  opt.first + " " + opt.second.value_desc)
             << opt.second.description << endl;
    }

    cout << "\nPlease make sure permissions are appropriately set for your device (or use su, if you don't care)\n";

    cout << "\n";

error:
    CURSOR_ON();
    SHOW_INPUT();
    return -1;
}

