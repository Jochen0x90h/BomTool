#include "kicad.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>


namespace fs = std::filesystem;


std::string clean(std::string_view s) {
    return std::string(s.substr(1, s.size() - 2));
}


int main(int argc, const char **argv) {
    if (argc < 2)
        return 1;
    fs::path path = argv[1];

    std::ifstream s(path.string());
    if (!s) {
        // error
        return 1;
    }
    kicad::Container file;
    kicad::readFile(s, file);
    s.close();

    // BOM file
    std::ofstream bom(path.parent_path() / "BOM.csv");
    bom << "Comment,Designator,Footprint,LCSC Part #" << std::endl;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> bomMap;

    // CPL file
    std::ofstream cpl(path.parent_path() / "CPL.csv");
    cpl << "Designator,Mid X,Mid Y,Layer,Rotation" << std::endl;

    for (auto element1 : file.elements) {
        auto container1 = dynamic_cast<kicad::Container *>(element1);
        if (container1) {
            // check if it is a footprint
            if (container1->id == "footprint") {
                auto footprint = container1;
                auto name = clean(footprint->getValue(0));

                // only add 0603 resistors and capacitors for now
                if (name == "Resistor_SMD:R_0603_1608Metric" || name == "Capacitor_SMD:C_0603_1608Metric") {
                    name = "0603";

                    // get properties
                    std::string x, y, rot;
                    std::string reference;
                    std::string value;
                    for (auto element2 : footprint->elements) {
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
                        }
                    }

                    //std::cout << "footprint " << name << " reference " << reference << " value " << value << " at " << x << ' ' << y << ' ' << rot << std::endl;

                    bomMap[std::make_pair(value, name)].push_back(reference);

                    // write line to CPL file
                    cpl << reference << ',' << x << "mm,-" << y << "mm," << "Top" << ',' << rot << std::endl;
                }
            }
        }
    }
    cpl.close();

    // write BOM
    for (auto &p : bomMap) {
        auto &value = p.first.first;
        auto &name = p.first.second;
        bom << value << ",\"";
        bool first = true;
        for (auto &reference : p.second) {
            if (!first)
                bom << ',';
            first = false;
            bom << reference;
        }
        bom << "\"," << name << ',' << std::endl;
    }
    bom.close();

    return 0;
}
