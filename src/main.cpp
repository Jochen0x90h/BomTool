#include "kicad.hpp"
#include <libzippp/libzippp.h> // https://github.com/ctabin/libzippp
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>


namespace fs = std::filesystem;
using namespace libzippp;


// remove quotes from string
std::string clean(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return std::string(s.substr(1, s.size() - 2));
    return std::string(s);
}

struct Job {
    std::string name;
    bool gerber = true;
    bool bom = false;
    fs::path pcbPath;
};


/// @brief BOM Tool: Zip gerber files and create BOM and CPL files
///
/// Usage:
/// bomtool -n <name> <option> <path to .kicad_pcb file> <output directory>
/// Options:
///   -g Only gerber (gerber directory is determined from pcb file)
///   -b BOM and gerber
///
/// Multiple pcb files can be processed in one go
int main(int argc, const char **argv) {
    if (argc < 2)
        return 1;

    std::list<Job> jobs;

    // create first job
    Job *job = &jobs.emplace_back();

    fs::path outDir;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-n") {
            // set name of current job
            ++i;
            job->name = argv[i];
        } else if (arg == "-g") {
            // only gerber
            ++i;
            job->pcbPath = argv[i];

            // add next job
            job = &jobs.emplace_back();
        } else if (arg == "-b") {
            // pcb and bom
            ++i;
            job->bom = true;
            job->pcbPath = argv[i];

            // add next job
            job = &jobs.emplace_back();
        } else {
            outDir = arg;
        }
    }

    // remove last job which is empty
    auto last = jobs.end();
    --last;
    jobs.erase(last);

    bool error = false;
    for (auto &job : jobs) {
        // set default name if it is empty
        if (job.name.empty())
            job.name = job.pcbPath.stem().string();

        std::cout << "*** " << job.name << " ***" << std::endl;

        // read pcb (.kicad_pcb) file
        std::ifstream s(job.pcbPath.string());
        if (!s) {
            // error
            return 1;
        }
        kicad::Container file;
        kicad::readFile(s, file);
        s.close();

        // get last write time of pcb
        auto pcbTime = fs::last_write_time(job.pcbPath);

        // zip gerber directory
        if (job.gerber) {
            // find gerber directory
            auto setup = file.find("setup");
            if (setup != nullptr) {
                auto plotParams = setup->find("pcbplotparams");
                auto gerberDir = fs::weakly_canonical(job.pcbPath.parent_path() / clean(plotParams->findValue("outputdirectory")));

                if (fs::is_directory(gerberDir)) {
                    std::cout << "zip gerber" << std::endl;
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
            }
        }

        if (job.bom) {
            // open BOM file
            fs::path bomPath = outDir / (job.name + "-BOM.csv");
            std::ofstream bom(bomPath);

            // open CPL file
            fs::path cplPath = outDir / (job.name + "-CPL.csv");
            std::ofstream cpl(cplPath);

            if (bom.is_open() && cpl.is_open()) {
                std::map<std::pair<std::string, std::string>, std::vector<std::string>> bomMap;

                bom << "Comment,Designator,Footprint,LCSC PN" << std::endl;
                cpl << "Designator,Mid X,Mid Y,Rotation,Layer" << std::endl;
                for (auto element1 : file.elements) {
                    auto container1 = dynamic_cast<kicad::Container *>(element1);
                    if (container1) {
                        // check if it is a footprint
                        if (container1->id == "footprint") {
                            auto footprint = container1;

                            // get footprint name
                            auto footprintName = clean(footprint->getValue(0));
                            //std::cout << "Footprint: " << footprintName << std::endl;

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
                            auto pos = footprintName.find(':');
                            if (pos != std::string::npos)
                                footprintName.erase(0, pos + 1);

                            // only add 0603 resistors and capacitors for now
                            //if (footprint == "Resistor_SMD:R_0603_1608Metric" || footprint == "Capacitor_SMD:C_0603_1608Metric") {
                            //    footprint = "0603";
                            //    filter = true;
                            //}

                            //if (populate && include) {
                            if (filter && populate && include) {
                                //std::cout << "add " << footprint << " reference " << reference << " value " << value << " at " << x << ' ' << y << ' ' << rot << std::endl;

                                bomMap[std::make_pair(value, footprintName)].push_back(reference);

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
                    auto &footprintName = p.first.second;
                    int count = p.second.size();
                    std::cout << count << "x " << value << " (" << footprintName << ')';
                    for (auto &reference : p.second) std::cout << " " << reference;

                    //std::cout << count << ", " << value << ", " << footprintName;
                    //for (auto &reference : p.second) std::cout << ", " << reference;
                    std::cout << std::endl;
                }

                // write BOM
                for (auto &p : bomMap) {
                    auto &value = p.first.first;
                    auto &footprintName = p.first.second;
                    bom << value << ",\"";
                    bool first = true;
                    std::ranges::sort(p.second);
                    for (auto &reference : p.second) {
                        if (!first)
                            bom << ',';
                        first = false;
                        bom << reference;
                    }
                    bom << "\"," << footprintName << ',' << std::endl;
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
