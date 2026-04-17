#include "simulator.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <random>
#include <chrono>
#include <sstream>

Protection parseProtection(const std::string& s) {
    return (s == "RO" || s == "READ_ONLY") ? READ_ONLY : READ_WRITE;
}

AccessType parseAccessType(const std::string& s) {
    return (s == "W" || s == "WRITE" || s == "1") ? ACCESS_WRITE : ACCESS_READ;
}

void processBatchFile(SegmentTable& st, const std::string& filename) {
    std::ifstream file(filename);
    std::ofstream log("batch_results.txt");

    if (!file) {
        std::cout << "Could not open batch file: " << filename << "\n";
        return;
    }

    int faults = 0;
    int translations = 0;
    int segNum, dirNum, pageNum, offset;
    std::string access;

    while (file >> segNum >> dirNum >> pageNum >> offset >> access) {
        try {
            int addr = st.translateAddress(segNum, dirNum, pageNum, offset, parseAccessType(access));
            log << "Address (" << segNum << "," << dirNum << "," << pageNum
                << "," << offset << "," << access << "): Physical=" << addr << "\n";
        } catch (const std::exception& e) {
            faults++;
            log << "Address (" << segNum << "," << dirNum << "," << pageNum
                << "," << offset << "," << access << "): " << e.what() << "\n";
        }
        translations++;
    }

    if (translations > 0) {
        log << "Fault Rate: " << static_cast<double>(faults) / translations * 100.0 << "%\n";
    }

    std::cout << "Batch results logged to batch_results.txt\n";
}

void generateRandomAddresses(SegmentTable& st, int num, double validRatio, const std::string& logFile) {
    std::ofstream log(logFile);
    std::mt19937 gen(static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()
    ));

    int faults = 0;
    int pageSize = st.getPageSizeValue();

    for (int i = 0; i < num; ++i) {
        bool valid = (std::generate_canonical<double, 10>(gen) < validRatio);

        int segNum = valid ? (gen() % std::max(1, st.getSegmentCount())) : (st.getSegmentCount() + (gen() % 5));
        int dirNum = valid ? (gen() % 3) : (5 + (gen() % 5));
        int pageNum = valid ? (gen() % 5) : (8 + (gen() % 10));
        int offset = valid ? (gen() % pageSize) : (pageSize + (gen() % 300));
        AccessType access = (gen() % 2) ? ACCESS_WRITE : ACCESS_READ;

        log << "Address " << i << ": ("
            << segNum << "," << dirNum << "," << pageNum << "," << offset << ","
            << (access == ACCESS_READ ? "Read" : "Write") << ") ";

        try {
            int addr = st.translateAddress(segNum, dirNum, pageNum, offset, access);
            log << "Physical=" << addr << "\n";
        } catch (const std::exception& e) {
            faults++;
            log << e.what() << "\n";
        }
    }

    if (num > 0) {
        log << "Page Fault Rate: " << static_cast<double>(faults) / num * 100.0 << "%\n";
    }
}

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    int numFrames = 10;
    int tlbSize = 4;
    int pageSize = 1000;
    bool use_lru = true;
    std::string batchFile;

    for (int i = 1; i < argc; i += 2) {
        std::string arg = argv[i];
        if (i + 1 >= argc) break;

        if (arg == "--frames") numFrames = std::stoi(argv[i + 1]);
        else if (arg == "--tlb") tlbSize = std::stoi(argv[i + 1]);
        else if (arg == "--pagesize") pageSize = std::stoi(argv[i + 1]);
        else if (arg == "--replace") use_lru = (std::string(argv[i + 1]) == "lru");
        else if (arg == "--batch") batchFile = argv[i + 1];
    }

    SegmentTable segmentTable(tlbSize, numFrames, use_lru, pageSize);

    segmentTable.addSegment(0, 0, 5, READ_WRITE);
    segmentTable.addSegment(1, 5000, 3, READ_ONLY);

    if (!batchFile.empty()) {
        processBatchFile(segmentTable, batchFile);
        segmentTable.displayStats();
        return 0;
    }

    segmentTable.printMemoryMap();

    std::cout << "\nCommands:\n";
    std::cout << "  add <id> <base> <limit> <prot>\n";
    std::cout << "  remove <id>\n";
    std::cout << "  translate <seg> <dir> <page> <offset> <R/W>\n";
    std::cout << "  random <num> <validRatio>\n";
    std::cout << "  tlb\n";
    std::cout << "  stats\n";
    std::cout << "  map\n";
    std::cout << "  quit\n\n";

    std::string command;
    std::cout << "Next command: ";

    while (std::cin >> command) {
        try {
            if (command == "add") {
                int id, base, limit;
                std::string prot;
                std::cin >> id >> base >> limit >> prot;
                segmentTable.addSegment(id, base, limit, parseProtection(prot));
                std::cout << "Segment " << id << " added\n";
            } else if (command == "remove") {
                int id;
                std::cin >> id;
                segmentTable.removeSegment(id);
                std::cout << "Segment " << id << " removed\n";
            } else if (command == "translate") {
                int segNum, dirNum, pageNum, offset;
                std::string access;
                std::cin >> segNum >> dirNum >> pageNum >> offset >> access;
                int addr = segmentTable.translateAddress(segNum, dirNum, pageNum, offset, parseAccessType(access));
                std::cout << "Physical Address: " << addr << "\n";
            } else if (command == "random") {
                int num;
                double ratio;
                std::cin >> num >> ratio;
                generateRandomAddresses(segmentTable, num, ratio, "random_results.txt");
                std::cout << "Results logged to random_results.txt\n";
            } else if (command == "tlb") {
                segmentTable.displayTLB();
            } else if (command == "stats") {
                segmentTable.displayStats();
            } else if (command == "map") {
                segmentTable.printMemoryMap();
            } else if (command == "quit") {
                break;
            } else {
                std::cout << "Unknown command\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }

        std::cout << "\nNext command: ";
    }

    return 0;
}