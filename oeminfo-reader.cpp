// oeminfo-reader.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

// Exécuter le programme : Ctrl+F5 ou menu Déboguer > Exécuter sans débogage
// Déboguer le programme : F5 ou menu Déboguer > Démarrer le débogage

// Astuces pour bien démarrer : 
//   1. Utilisez la fenêtre Explorateur de solutions pour ajouter des fichiers et les gérer.
//   2. Utilisez la fenêtre Team Explorer pour vous connecter au contrôle de code source.
//   3. Utilisez la fenêtre Sortie pour voir la sortie de la génération et d'autres messages.
//   4. Utilisez la fenêtre Liste d'erreurs pour voir les erreurs.
//   5. Accédez à Projet > Ajouter un nouvel élément pour créer des fichiers de code, ou à Projet > Ajouter un élément existant pour ajouter des fichiers de code existants au projet.
//   6. Pour rouvrir ce projet plus tard, accédez à Fichier > Ouvrir > Projet et sélectionnez le fichier .sln.


#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <algorithm>

char* optarg = NULL;
int optind = 1;

int getopt(int argc, char* const argv[], const char* optstring)
{
    if ((optind >= argc) || (argv[optind][0] != '-') || (argv[optind][0] == 0))
    {
        return -1;
    }

    int opt = argv[optind][1];
    const char* p = strchr(optstring, opt);

    if (p == NULL)
    {
        return '?';
    }
    if (p[1] == ':')
    {
        optind++;
        if (optind >= argc)
        {
            return '?';
        }
        optarg = argv[optind];
        optind++;
    }
    return opt;
}



std::map<int, std::map<int, std::string>> elements = {
    {6, {
        {0x12, "Region"},
        {0x43, "Root Type (info)"},
        {0x44, "rescue Version"},
        {0x4a, "16 byte string 0 terminated"},
        {0x4e, "Rom Version"},
        {0x58, "Alternate ROM Version?"},
        {0x5e, "OEMINFO_VENDER_AND_COUNTRY_NAME_COTA"}, // Taken from fastboot logs
        {0x5b, "Hardware Version Customizeable"},
        {0x5c, "USB Switch?"}, // Guessed from fastboot logs
        {0x61, "Hardware Version"},
        {0x62, "PRF?"},
        {0x65, "Rom Version Customizeable"},
        {0x67, "CN or CDMA info 0x67"},
        {0x68, "CN or CDMA info 0x68"},
        {0x6a, "CN or CDMA info 0x6a"},
        {0x6b, "CN or CDMA info 0x6b"},
        {0x6f, "Software Version"},
        {0x73, "Oeminfo Gamma"}, // From fastboot, but who knows what it actually is, has to do with hisifb_write_gm_to_reserved_mem and the display panel
        {0x76, "pos_delivery constant"},
        {0x8b, "Unknown SHA256 1"},
        {0x85, "3rd_recovery constant"},
        {0x8c, "Software Version as CSV"},
        {0x8d, "Unknown SHA256 2"},
        {0x96, "Unknown SHA256 3"},
        {0xa6, "Update Token"},
        {0xa9, "Some kind of json changelog"},
        {0xb4, "cust version"},
        {0xb6, "preload version"},
        {0xba, "system version"},
        {0x15f, "Logo Boot"}, // Can be overridden in product, version, vendor or system partitions
        {0x160, "Logo Battery Empty"},
        {0x161, "Logo Battery Charge"},
    }},
    {8, {
        {0x5c, "Userlock"},
        {0x5d, "System Lock State"},
        {0x28, "Version number"},
        {0x33, "Software Version as CSV"},
        {0x35, "semicolon separated text containing device identifiers, possibly used in bootloader code generation"},
        {0x3f, "update token"},
        {0x50, "cust version"},
        {0x52, "preload version"},
        {0x56, "system version"},
        {0x5ec, "build number"},
        {0x5ee, "model number"},
        {0xc, "system security data"},
        {0x1197, "Logo Battery Charge"},
        {0x1196, "Logo Battery Empty"},
        {0x1196, "Logo additional (custom format)"},
        {0x1195, "Logo Google"},
    }}
};


struct ProductInfo {
    // From oeminfo
    std::string board;
    std::string infostr = "";
    std::string region;
    std::string model;
    std::string marketname = "";

    // result of the parse
    std::string version;
    std::string baseband;
    std::string device;

    // TODO
    std::string brand;
};

ProductInfo unpackOEM(std::ifstream& input) {
    std::vector<char> HW_Version(8);
    std::vector<char> ROM_Version(32);
    std::vector<char> HW_Region(16);
    std::vector<char> SW_Version(128);
    std::vector<char> MarketingName(32);
    std::vector<char> Model(128);
    ProductInfo product_info = {};


    std::vector<char> binary((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    size_t content_length = binary.size();
    size_t content_startbyte = 0;

    if (content_length != 67108864) {
        std::cout << "Wrong filesize";
        return product_info;
    }

    while (content_startbyte < content_length) {
        char header[8];
        uint32_t version_number, id, type, data_len, age;
        std::memcpy(header, binary.data() + content_startbyte, 8);
        std::memcpy(&version_number, binary.data() + content_startbyte + 8, sizeof(version_number));
        std::memcpy(&id, binary.data() + content_startbyte + 12, sizeof(id));
        std::memcpy(&type, binary.data() + content_startbyte + 16, sizeof(type));
        std::memcpy(&data_len, binary.data() + content_startbyte + 20, sizeof(data_len));
        std::memcpy(&age, binary.data() + content_startbyte + 24, sizeof(age));

        if (std::memcmp(header, "OEM_INFO", 8) == 0) {
            static uint32_t version = 0;
            if (version == 0) {
                version = version_number;
            }
            if (version != version_number) {
                std::cout << "version number changed during parsing! wtf";
                return product_info;
            }
            if (version_number == 8) {
                // Handle version 8 specific logic
                std::cout << "oeminfo version 8:";
            }
            if (version_number == 6) {
                // Handle version 6 specific logic
                std::cout << "oeminfo version 6:";
            }
            if (id == 0x61) {
                std::memcpy(HW_Version.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            if (id == 0x12) {
                std::memcpy(HW_Region.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            // 0x4e or 0x61
            if (id == 0x4e) {
                std::memcpy(SW_Version.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            if (id == 0x61) {
                std::memcpy(ROM_Version.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            if (id == 0x81) {
                std::memcpy(MarketingName.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            if (id == 0x5b) {
                std::memcpy(Model.data(), binary.data() + content_startbyte + 0x200, data_len);
            }
            std::string fileout = std::to_string(id) + "-" + std::to_string(type) + "-" + std::to_string(age) + "-" + std::to_string(content_startbyte);
            std::cout << "hdr:" << std::string(header, 8) << " age:" << std::hex << age << " id:" << std::hex << id << " " << elements[version][id] << std::endl;
        }
        content_startbyte += 0x400; // Move to the next header
    }



    std::string temp;

    temp = std::string(SW_Version.begin(), SW_Version.end());
    product_info.infostr = temp.substr(0, temp.find('\0'));
    temp = std::string(HW_Region.begin(), HW_Region.end());
    product_info.region = temp.substr(0, temp.find('\0'));

    // Extract the board (i.e. "POT-L21")
    temp = std::string(HW_Version.begin(), HW_Version.end());
    product_info.board = temp.substr(0, temp.find('\0'));

    // Extract the model (POT-LX1)
    temp = std::string(Model.begin(), Model.end());
    product_info.model = temp.substr(0, temp.find('\0'));

    temp = std::string(MarketingName.begin(), MarketingName.end());
    product_info.marketname = temp.substr(0, temp.find('\0'));

    // Extract the full description
    std::string tempm;
    std::istringstream iss(product_info.infostr);
    std::getline(iss, tempm, ' ');

    // Extract the version (i.e. "9.1.0.311").
    std::getline(iss, product_info.version, '(');

    // Remove trailing whitespace.
    if (!product_info.version.empty() && product_info.version.back() == ')') {
        product_info.version.pop_back();
    }

    // Extract the baseband (i.e. "C432E3R4P1")
    std::getline(iss, product_info.baseband, ')');

    // Extract the brand
    product_info.brand = "HUAWEI";


    // Extract the device (i.e. "HWPOT-H")
    std::istringstream iss1(product_info.model);
    std::string tempmodel;
    std::getline(iss1, tempmodel, '-');
    product_info.device = "HW" + tempmodel + "-H";

    /*
**** OEMINFO ****
  Info String (Rom Version) = POT-LX1 10.0.0.238(C432E3R4P1)
  Board = POT-L21
  Region = hw/eu
  Model = POT-LX1
  MarketingName = HUAWEI P smart 2019
 **** Extract ****
  Device = HWPOT-H
  Version = 10.0.0.238
  BaseBand = C432E3R4P1
*/

    std::cout << " **** OEMINFO **** " << std::endl;
    std::cout << "  Info String (Rom Version) = " << product_info.infostr << std::endl;
    std::cout << "  Board = " << product_info.board << std::endl;
    std::cout << "  Region = " << product_info.region << std::endl;
    std::cout << "  Model = " << product_info.model << std::endl;
    std::cout << "  MarketingName = " << product_info.marketname << std::endl;

    std::cout << " **** Extract **** " << std::endl;
    std::cout << "  Device = " << product_info.device << std::endl;
    std::cout << "  Version = " << product_info.version << std::endl;
    std::cout << "  BaseBand = " << product_info.baseband << std::endl;

    return product_info;
}

void help(const std::string& script) {
    std::cout << script << " -a extract -i <inputfile> -r <replace_inputfile>  -t <type 0x00>" << std::endl;
    std::exit(0);
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "error: no subcommand" << std::endl;
        help(argv[0]);
    }

    std::string inputFile;
    std::string outputFile;
    std::string action;

    int opt;
    while ((opt = getopt(argc, argv, "i:a:")) != -1) {
        switch (opt) {
        case 'i':
            inputFile = optarg;
            break;

        case 'a':
            action = optarg;
            break;
        default:
            help(argv[0]);
        }
    }

    std::ifstream input(inputFile, std::ios::binary);
    if (!input) {
        std::cerr << "Error opening input file." << std::endl;
        return 1;
    }

    if (action == "extract") {
        ProductInfo product_info = unpackOEM(input);
    }
    else {
        std::cerr << "Unknown action: " << action << std::endl;
        help(argv[0]);
    }

    return 0;
}


