#include "kicad.hpp"
#include <libzippp/libzippp.h> // https://github.com/ctabin/libzippp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>


namespace fs = std::filesystem;
using namespace libzippp;


// extract type of a component, e.g. "R" from "R1"
std::string getType(std::string_view reference) {
    size_t i = 0;
    while (i < reference.length()) {
        char ch = reference[i];
        if (ch >= '0' && ch <= '9')
            break;
        ++i;
    }
    return std::string(reference.substr(0, i));
}

// manufacturer
enum class Manufacturer {
    GENERIC,
    JLCPCB
};

struct Job {
    std::string name;
    bool gerber;
    bool bom;
    Manufacturer manufacturer;
    fs::path pcbPath;
};

struct BomKey {
    std::string type;
    std::string value;
    int voltage; // in mV
    std::string footprint;
    std::string manufacturer;
    std::string mpn;

    auto operator <=>(const BomKey& other) const noexcept = default;
};

struct BomValue {
    std::vector<std::string> references; // list of references (e.g. R1, R2, C1...)
    int padCount;
    bool throughHole;
    std::string description;
};

struct JlcBomKey {
    std::string type;
    std::string value;
    std::string footprintName;
    std::string lcscPn;

    auto operator <=>(const JlcBomKey& other) const noexcept = default;
};


/// @brief BOM Tool: Zip gerber files and create BOM and CPL files
///
/// Usage:
/// bom-tool <options> <path to .kicad_pcb file> <output directory>
/// Options:
///   -n Name for output files (optional, derived from .kicad_pcb file name if not given)
///   -g Export and zip gerber files (path to gerber and layers are read from the .kicad_pcb file)
///   -b Generate BOM and placement file
///   -j Generate for JLCPCB (oval holes alternate, BOM with LCSC PN, CPL)
///
/// Multiple pcb files can be processed in one go
int main(int argc, const char **argv) {
    if (argc < 2)
        return 1;

    std::list<Job> jobs;

    fs::path outDir;

    std::string name;
    bool gerber = false;
    bool bom = false;
    Manufacturer manufacturer = Manufacturer::GENERIC;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-n") {
            // set name of current job
            ++i;
            name = argv[i];
        } else if (arg == "-g") {
            // zip gerber
            gerber = true;
        } else if (arg == "-b") {
            // create generic bom (includes voltage)
            bom = true;
        } else if (arg == "-j") {
            // JLCPCB
            manufacturer = Manufacturer::JLCPCB;
        } else {
            if (gerber || bom) {
                // assume path to .kicad_pcb file: add job
                fs::path pcbPath = arg;
                if (name.empty())
                    name = pcbPath.stem().string();

                jobs.emplace_back(name, gerber, bom, manufacturer, pcbPath);

                // clear
                name.clear();
                gerber = false;
                bom = false;
            } else {
                // set output directory
                outDir = arg;
            }
        }
    }

    bool error = false;
    for (auto &job : jobs) {
        const char *manufacturers[] = {"Generic", "JLCPCB"};
        std::cout << "*** " << job.name << " for " << manufacturers[int(job.manufacturer)] << " ***" << std::endl;

        // read pcb (.kicad_pcb) file
        std::ifstream s(job.pcbPath.string());
        if (!s) {
            // error
            std::cout << "Error: Can't read file " << job.pcbPath.string() << std::endl;
            return 1;
        }
        kicad::Container file;
        kicad::readFile(s, file);
        s.close();

        // get last write time of pcb
        auto pcbTime = fs::last_write_time(job.pcbPath);

        // zip gerber directory
        if (job.gerber) {

            // get layers
            std::set<std::string> layers;
            {
                auto layerContainer = file.find("layers");
                if (layerContainer) {
                    for (auto layer : *layerContainer) {
                        layers.insert(layer->getString(0));
                    }
                }
            }

            // get gerber directory from pcb file (configured in the plot dialog)
            auto setup = file.find("setup");
            if (setup != nullptr) {
                auto plotParams = setup->find("pcbplotparams");
                if (plotParams != nullptr) {
                    // get gerber directory
                    auto gerberDir = fs::weakly_canonical(job.pcbPath.parent_path() / plotParams->findString("outputdirectory"));
                    if (fs::is_directory(gerberDir)) {
                        // get selected layers
                        auto selection = plotParams->findString("layerselection");
                        std::string selectedLayers;
                        uint32_t flags[2] = {};
                        int index = 0;
                        int length = selection.size();
                        for (int i = 2; i < length; ++i) {
                            char ch = selection[i];

                            // check for next filed
                            if (ch == '_') {
                                ++index;
                                if (index == 2)
                                    break;
                            }

                            int nibble = ch <= '9' ? ch - '0' : (ch - 'a' + 10);
                            flags[index] = (flags[index] << 4) | nibble;
                        }

                        // copper layers
                        if ((flags[1] & 1) != 0 && layers.contains("F.Cu"))
                            selectedLayers += "F.Cu,";
                        for (int i = 1; i < 31; ++i) {
                            if ((flags[1] >> i) & 1) {
                                std::string layer = "In" + std::to_string(i) + ".Cu";
                                if (layers.contains(layer)) {
                                    selectedLayers += layer;
                                    selectedLayers += ',';
                                }
                            }
                        }
                        if ((flags[1] & 0x80000000) && layers.contains("B.Cu"))
                            selectedLayers += "B.Cu,";

                        // other layers
                        static const char *layerNames[] = {
                            "F.Adhesive", "B.Adhesive", "F.Paste", "B.Paste",
                            "F.Silkscreen", "B.Silkscreen", "F.Mask", "B.Mask",
                            "User.Drawings", "User.Comments", "User.Eco1", "User.Eco2",
                            "Edge.Cuts", "Margin", "F.Courtyard", "B.Courtyard"
                            "F.Fab, B.Fab", "User.1", "User.2",
                            "User.3", "User.4", "User.5", "User.6",
                            "User.7", "User.8", "User.9"
                        };
                        for (int i = 0; i < 27; ++i) {
                            if ((flags[0] >> i) & 1) {
                                    if (layers.contains(layerNames[i])) {
                                        selectedLayers += layerNames[i];
                                        selectedLayers += ',';
                                    }
                                }
                        }
                        if (!selectedLayers.empty())
                            selectedLayers.resize(selectedLayers.size() - 1);

                        // export gerber
                        {
                            std::cout << "Export gerber" << std::endl;
                            std::string command = "kicad-cli pcb export gerbers -l " + selectedLayers + " --subtract-soldermask --output " + gerberDir.string() + ' ' + job.pcbPath.string();
                            int result = std::system(command.c_str());
                        }

                        // export drill
                        {
                            std::cout << "Export drill" << std::endl;
                            std::string command = "kicad-cli pcb export drill --excellon-separate-th";
                            if (job.manufacturer == Manufacturer::JLCPCB)
                                command += " --excellon-oval-format";
                            command += " --generate-map --map-format gerberx2 --output " + gerberDir.string() + ' ' + job.pcbPath.string();
                            int result = std::system(command.c_str());
                        }

                        // zip gerber
                        std::cout << "Zip gerber" << std::endl;
                        auto zipPath = outDir / (job.name + ".zip");

                        // create new zip
                        ZipArchive zip(zipPath.string());
                        if (zip.open(ZipArchive::New)) {
                            // add files
                            fs::directory_iterator end;
                            for (fs::directory_iterator it(gerberDir); it != end; ++it) {
                                if (it->is_regular_file()) {
                                    // read file
                                    fs::path path = it->path();

                                    // check last write time
                                    if (fs::last_write_time(path) < pcbTime) {
                                        std::cout << "Error: File is not up-to-date: " << path.string() << std::endl;
                                        error = true;
                                    }

                                    if (!zip.addFile(path.filename().string(), path.string())) {
                                        std::cout << "Error: Could add file to zip" << std::endl;
                                        error = true;
                                    }
                                }
                            }
                            zip.close();
                        } else {
                            std::cout << "Error: Could not write zip file: " << zipPath.string() << std::endl;
                            error = true;
                        }
                    } else {
                        std::cout << "Error: Gerber directory not found: " << gerberDir.string() << std::endl;
                        error = true;
                    }
                } else {
                    std::cout << "Error: Gerber directory configuration not found" << std::endl;
                    error = true;
                }
            } else {
                std::cout << "Error: Gerber directory configuration not found" << std::endl;
                error = true;
            }
        }

        if (job.bom && manufacturer == Manufacturer::GENERIC) {
            // open generic BOM file
            fs::path bomPath = outDir / (job.name + ".csv");
            std::ofstream bom(bomPath);
            if (bom.is_open()) {
                std::map<BomKey, BomValue> bomMap;

                //bom << "Count,Type,Value,Voltage,Footprint,SMD Pads,THT Pads,Manufacturer,MPN,Description" << std::endl;
                bom << "Count,Reference,Value,Voltage,Footprint,SMD Pads,THT Pads,Manufacturer,MPN,Description" << std::endl;

                for (auto element1 : file.elements) {
                    auto container1 = dynamic_cast<kicad::Container *>(element1);
                    if (container1) {
                        // check if it is a footprint
                        if (container1->id == "footprint") {
                            auto footprint = container1;

                            // get footprint name
                            auto footprintName = footprint->getString(0);

                            // remove library from footprint name
                            auto pos = footprintName.find(':');
                            if (pos != std::string::npos)
                                footprintName.erase(0, pos + 1);

                            // get footprint properties
                            //std::string type; // e.g. "R" or "C"
                            std::string reference; // e.g. "R1"
                            std::string value; // e.g. 100k
                            int voltage = 0;
                            //int padCount = 0;
                            std::set<std::string> padNames; // to detect duplicates
                            std::string manufacturer;
                            std::string mpn;
                            std::string description;
                            bool doNotPopulate = false;
                            bool excludeFromBom = false;
                            bool throughHole = false;
                            for (auto element2 : container1->elements) {
                                auto property = dynamic_cast<kicad::Container *>(element2);
                                if (property) {
                                    if (property->id == "property") {
                                        auto propertyName = property->getString(0);
                                        auto propertyValue = property->getString(1);
                                        if (propertyName == "Reference") {
                                            // reference, e.g. "R1"
                                            reference = propertyValue;
                                        } else if (propertyName == "Value") {
                                            // value, e.g. "100k"
                                            value = propertyValue;
                                        } else if (propertyName == "Voltage") {
                                            // operating voltage
                                            voltage = lround(std::stod(propertyValue) * 1000.0);
                                        } else if (propertyName == "Manufacturer") {
                                            manufacturer = propertyValue;
                                        } else if (propertyName == "MPN") {
                                            // manufacturer part number
                                            mpn = propertyValue;
                                        } else if (propertyName == "Description") {
                                            description = propertyValue;
                                        }
                                    }
                                    if (property->id == "attr") {
                                        doNotPopulate = property->contains("dnp");
                                        excludeFromBom = property->contains("exclude_from_bom");
                                        throughHole = property->contains("through_hole");
                                    }
                                    if (property->id == "pad") {
                                        auto padName = property->getString(0);
                                        padNames.insert(padName);
                                        //++padCount;
                                    }
                                }
                            }

                            if (!excludeFromBom) {
                                auto &v = bomMap[{getType(reference), value, voltage, footprintName, manufacturer, mpn}];
                                v.references.push_back(reference);
                                int padCount = padNames.size();
                                v.padCount = std::max(v.padCount, padCount);
                                v.throughHole = throughHole;
                                v.description = description;
                            }
                        }
                    }
                }

                // write BOM
                for (auto &p : bomMap) {
                    // count
                    bom << p.second.references.size() << ",";

                    // type
                    //bom << p.first.type << ",";

                    // references
                    bom << '"';
                    for (auto &reference : p.second.references) {
                        if (reference != p.second.references.front())
                            bom << ',';
                        bom << reference;
                    }
                    bom << "\",";

                    // value, voltage, footprint
                    bom << "\"" << p.first.value << "\","
                        << (p.first.voltage * 0.001) << ","
                        << p.first.footprint << ",";

                    // pad count
                    if (p.second.throughHole)
                        bom << ',';
                    bom << p.second.padCount << ",";
                    if (!p.second.throughHole)
                        bom << ',';

                    // manufactuer, part number, description
                    bom << "\"" << p.first.manufacturer << "\","
                        << p.first.mpn << ","
                        "\"" << p.second.description << "\"" << std::endl;
                }
                bom.close();
            } else {
                std::cout << "Error: Could not create BOM file in " << outDir.string() << std::endl;
                error = true;
            }
        }

        if (job.bom && manufacturer == Manufacturer::JLCPCB) {
            // open BOM file for JLCPCB
            fs::path bomPath = outDir / (job.name + "-BOM.csv");
            std::ofstream bom(bomPath);

            // open CPL file
            fs::path cplPath = outDir / (job.name + "-CPL.csv");
            std::ofstream cpl(cplPath);

            if (bom.is_open() && cpl.is_open()) {
                // map from part protperties (e.g. footprint) to list of references (e.g. R1, R2, R3...)
                std::map<JlcBomKey, std::vector<std::string>> bomMap;

                // set of used references to detect duplicates
                std::set<std::string> usedReferences;

                bom << "Comment,Designator,Footprint,LCSC PN" << std::endl;
                cpl << "Designator,Mid X,Mid Y,Rotation,Layer" << std::endl;
                for (auto element1 : file.elements) {
                    auto container1 = dynamic_cast<kicad::Container *>(element1);
                    if (container1) {
                        // check if it is a footprint
                        if (container1->id == "footprint") {
                            auto footprint = container1;

                            // get footprint name
                            auto footprintName = footprint->getString(0);
                            //std::cout << "Footprint: " << footprintName << std::endl;

                            // remove library from footprint name
                            auto pos = footprintName.find(':');
                            if (pos != std::string::npos)
                                footprintName.erase(0, pos + 1);

                            // get footprint properties
                            std::string x, y, rot;
                            std::string reference;
                            std::string value;
                            std::string lcscPn;
                            bool doNotPopulate = false;
                            bool excludeFromBom = false;
                            for (auto element2 : container1->elements) {
                                auto property = dynamic_cast<kicad::Container *>(element2);
                                if (property) {
                                    if (property->id == "at") {
                                        x = property->getString(0);
                                        y = property->getString(1);
                                        rot = property->getString(2, "0");
                                    }
                                    if (property->id == "property") {
                                        auto propertyName = property->getString(0);
                                        auto propertyValue = property->getString(1);
                                        if (propertyName == "Reference") {
                                            // reference (designator), e.g. "R1"
                                            reference = propertyValue;
                                            if (usedReferences.contains(reference)) {
                                                std::cout << "Error: Duplicate reference " << reference << std::endl;
                                                error = true;
                                            }
                                            usedReferences.insert(reference);
                                        } else if (propertyName == "Value") {
                                            // value, e.g. "100k"
                                            value = propertyValue;
                                        } else if (propertyName == "LCSC PN") {
                                            lcscPn = propertyValue;
                                        }
                                    }
                                    if (property->id == "attr") {
                                        doNotPopulate = property->contains("dnp");
                                        excludeFromBom = property->contains("exclude_from_bom");
                                    }
                                }
                            }

                            if (!doNotPopulate && !excludeFromBom) {
                                //std::cout << "add " << footprint << " reference " << reference << " value " << value << " at " << x << ' ' << y << ' ' << rot << std::endl;

                                bomMap[{getType(reference), value, footprintName, lcscPn}].push_back(reference);

                                // write line to CPL file
                                cpl << reference << ',' << x << ",-" << y << ',' << rot << "," << "top" << std::endl;
                            } else {
                                //std::cout << "reject " << footprint << " reference " << reference << " value " << value << std::endl;
                            }
                        }
                    }
                }
                cpl.close();

                // print BOM to std::cout
                /*for (auto &p : bomMap) {
                    int count = p.second.size();
                    std::cout << count << "x " << p.first.value << " (" << p.first.footprintName << ')';
                    for (auto &reference : p.second) std::cout << " " << reference;
                    std::cout << std::endl;
                }*/

                // write BOM
                std::cout << "Write BOM" << std::endl;
                for (auto &p : bomMap) {
                    // comment (use value)
                    bom << p.first.value;

                    // quoted list of references
                    bom << ",\"";
                    bool first = true;
                    std::ranges::sort(p.second);
                    for (auto &reference : p.second) {
                        if (!first)
                            bom << ',';
                        first = false;
                        bom << reference;
                    }
                    bom << "\",";

                    // footprint and LCSC PN
                    bom <<  p.first.footprintName << ',' << p.first.lcscPn << std::endl;
                }
                bom.close();
            } else {
                std::cout << "Error: Could not create BOM/CPL file in " << outDir.string() << std::endl;
                error = true;
            }
        }
    }

    std::cout << std::endl;
    if (error)
        std::cout << "!!! There were errors !!!" << std::endl;
    else
        std::cout << "*** OK ***" << std::endl;
    return 0;
}
