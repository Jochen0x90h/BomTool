#include "kicad.hpp"
#include <libzippp.h> // https://github.com/ctabin/libzippp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>


namespace fs = std::filesystem;
using namespace libzippp;


std::string clean(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return std::string(s.substr(1, s.size() - 2));
    return std::string(s);
}


/// @brief BOM Tool: Zip gerber files and create BOM and CPL files
///
/// Usage:
/// bomtool -n <name> -g <directory of gerber files> -p <path to pcb file> <output directory>
///
/// The path to pcb file sets defauls for name and gerber directory
///
int main(int argc, const char **argv) {
    if (argc < 2)
        return 1;

    std::string name;
    fs::path pcbPath;
    fs::path gerberDir;
    fs::path outDir;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-n") {
            // name
            ++i;
            name = argv[i];
        } else if (arg == "-g") {
            // gerber
            ++i;
            gerberDir = argv[i];
        } else if (arg == "-p") {
            // pcb
            ++i;
            pcbPath = argv[i];

            // set default name and gerber dir
            if (name.empty())
                name = pcbPath.stem().string();
            if (gerberDir.empty())
                gerberDir = pcbPath.parent_path() / "gerber";
        } else {
            outDir = arg;
        }
    }

    // zip gerber directory
    if (!gerberDir.empty()) {
        std::cout << "zip gerber" << std::endl;

        ZipArchive zip((outDir / (name + ".zip")).string());
        zip.open(ZipArchive::Write);

        // delete contents
        int count = zip.getEntriesCount();
        for (int i = 0; i < count; ++i) {
            auto entry = zip.getEntry(i);
            zip.deleteEntry(entry);
        }

        // add files
        fs::directory_iterator end;
        for (fs::directory_iterator it(gerberDir); it != end; ++it) {
            if (it->is_regular_file()) {
                // read file
                fs::path path = it->path();

                zip.addFile(path.filename().string(), path.string());
                /*std::ifstream file(path.string(), std::ios::binary | std::ios::ate);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::vector<char> buffer(size);
                if (file.read(buffer.data(), size)) {
                    zip.addData(path.filename().string(), buffer.data(), size);
                }*/
            }
        }
        zip.close();
    }

    if (!pcbPath.empty()) {

        // read .kicad_pcb file
        std::ifstream s(pcbPath.string());
        if (!s) {
            // error
            return 1;
        }
        kicad::Container file;
        kicad::readFile(s, file);
        s.close();

        // BOM file
        std::ofstream bom(outDir / (name + "-BOM.csv"));
        bom << "Comment,Designator,Footprint,LCSC PN" << std::endl;
        std::map<std::pair<std::string, std::string>, std::vector<std::string>> bomMap;

        // CPL file
        std::ofstream cpl(outDir / (name + "-CPL.csv"));
        cpl << "Designator,Mid X,Mid Y,Rotation,Layer" << std::endl;

        for (auto element1 : file.elements) {
            auto container1 = dynamic_cast<kicad::Container *>(element1);
            if (container1) {
                // check if it is a footprint
                if (container1->id == "footprint") {
                    auto footprint = clean(container1->getValue(0));

                    //std::cout << "footprint " << footprint << std::endl;

                    // get footprint properties
                    std::string x, y, rot;
                    std::string reference;
                    std::string value;
                    bool populate = true;
                    bool include = true;
                    for (auto element2 : container1->elements) {
                        auto container2 = dynamic_cast<kicad::Container *>(element2);
                        if (container2) {
                            if (container2->id == "at") {
                                auto at = container2;
                                x = at->getValue(0);
                                y = at->getValue(1);
                                rot = at->getValue(2, "0");
                            }
                            if (container2->id == "property") {
                                auto property = container2;
                                if (property->getValue(0) == "\"Reference\"")
                                    reference = clean(property->getValue(1));
                                if (property->getValue(0) == "\"Value\"")
                                    value = clean(property->getValue(1));
                            }
                            if (container2->id == "attr") {
                                auto attr = container2;
                                for (int i = 0; i < attr->elements.size(); ++i) {
                                    populate &= attr->getValue(i) != "dnp";
                                    include &= attr->getValue(i) != "exclude_from_bom";
                                }
                            }
                        }
                    }

                    //bool filter = false;
                    bool filter = true;

                    // remove library from footprint name
                    auto pos = footprint.find(':');
                    if (pos != std::string::npos)
                        footprint.erase(0, pos + 1);

                    // only add 0603 resistors and capacitors for now
                    //if (footprint == "Resistor_SMD:R_0603_1608Metric" || footprint == "Capacitor_SMD:C_0603_1608Metric") {
                    //    footprint = "0603";
                    //    filter = true;
                    //}

                    //if (populate && include) {
                    if (filter && populate && include) {
                        //std::cout << "add " << footprint << " reference " << reference << " value " << value << " at " << x << ' ' << y << ' ' << rot << std::endl;

                        bomMap[std::make_pair(value, footprint)].push_back(reference);

                        // write line to CPL file
                        cpl << reference << ',' << x << ",-" << y << ',' << rot << "," << "top" << std::endl;
                    } else {
                        //std::cout << "reject " << footprint << " reference " << reference << " value " << value << std::endl;
                    }
                }
            }
        }
        cpl.close();

        // print BOM
        for (auto &p : bomMap) {
            auto &value = p.first.first;
            auto &footprint = p.first.second;
            int count = p.second.size();
            std::cout << count << "x " << value << " (" << footprint << ')';
            for (auto &reference : p.second) std::cout << " " << reference;

            //std::cout << count << ", " << value << ", " << footprint;
            //for (auto &reference : p.second) std::cout << ", " << reference;
            std::cout << std::endl;
        }

        // write BOM
        for (auto &p : bomMap) {
            auto &value = p.first.first;
            auto &footprint = p.first.second;
            bom << value << ",\"";
            bool first = true;
            std::ranges::sort(p.second);
            for (auto &reference : p.second) {
                if (!first)
                    bom << ',';
                first = false;
                bom << reference;
            }
            bom << "\"," << footprint << ',' << std::endl;
        }
        bom.close();
    }

    return 0;
}
