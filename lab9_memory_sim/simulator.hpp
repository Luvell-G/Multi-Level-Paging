#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <unordered_map>
#include <stdexcept>
#include <iomanip>
#include <string>
#include <algorithm>

enum Protection {
    READ_ONLY,
    READ_WRITE
};

enum AccessType {
    ACCESS_READ,
    ACCESS_WRITE
};

class SegmentationFault : public std::runtime_error {
public:
    explicit SegmentationFault(const std::string& msg) : std::runtime_error(msg) {}
};

class ProtectionViolation : public std::runtime_error {
public:
    explicit ProtectionViolation(const std::string& msg) : std::runtime_error(msg) {}
};

class PageFault : public std::runtime_error {
public:
    explicit PageFault(const std::string& msg) : std::runtime_error(msg) {}
};

struct Page {
    int frame_number;
    bool present;
    Protection protection;
    int last_access;

    Page(int frame = -1, bool pres = false, Protection prot = READ_WRITE, int access = 0)
        : frame_number(frame), present(pres), protection(prot), last_access(access) {}
};

struct Segment {
    int segment_id;
    int base_address;
    int limit;              // number of pages allowed
    Protection protection;
    int fault_count;

    Segment(int id = 0, int base = 0, int lim = 0, Protection prot = READ_WRITE)
        : segment_id(id), base_address(base), limit(lim), protection(prot), fault_count(0) {}
};

struct FrameRecord {
    int seg_id;
    int dir_id;
    int page_id;
};

class PhysicalMemory {
private:
    int num_frames;
    std::vector<bool> free_frames;
    std::queue<int> fifo_queue;
    std::list<int> lru_list;
    std::map<int, int> frame_to_page;
    bool use_lru;

public:
    PhysicalMemory(int frames = 0, bool lru = true);

    int allocateFrame(int pageNum, int time);
    void freeFrame(int frame);
    void touchFrame(int frame);
    int selectVictimFIFO();
    int selectVictimLRU();
    double utilization() const;
    int getNumFrames() const;
    bool isUsingLRU() const;
    bool isFree(int frame) const;
    void display() const;
};

class PageTable {
private:
    std::vector<Page> pages;
    int page_size;
    int directory_index;

public:
    PageTable(int numPages = 0, int pageSize = 256, int dirIdx = 0);

    int getFrameNumber(int pageNum, int time, AccessType accessType, PhysicalMemory& physMem);
    void setFrame(int pageNum, int frameNum, Protection prot, int current_time);
    Page& getPage(int pageNum);
    const Page& getPage(int pageNum) const;
    int getNumPages() const;
    int getPageSize() const;
    int getDirectoryIndex() const;
    void display() const;
};

class DirectoryTable {
private:
    std::vector<PageTable*> pageTables;
    int max_pages_per_table;

public:
    DirectoryTable(int maxPages = 4);
    ~DirectoryTable();

    PageTable* getPageTable(int dirNum, int pageSize);
    const std::vector<PageTable*>& getAllTables() const;
    void freeTables();
    int getNumTables() const;
};

class TLB {
private:
    std::unordered_map<std::string, int> cache;
    std::list<std::string> lruOrder;
    int maxSize;
    int hits;
    int total;

public:
    TLB(int size = 4);

    int get(int segNum, int dirNum, int pageNum);
    void put(int segNum, int dirNum, int pageNum, int frame);
    void remove(int segNum, int dirNum, int pageNum);
    void clear();
    double hitRate() const;
    int getHits() const;
    int getMisses() const;
    int getTotalLookups() const;
    int getMaxSize() const;
    void displayCache() const;
};

class SegmentTable {
private:
    std::vector<Segment> segments;
    std::map<int, DirectoryTable*> directoryTables;
    TLB tlb;
    PhysicalMemory physMem;
    std::unordered_map<int, FrameRecord> frameTable;

    int pageSize;
    int timeCounter;
    int totalLatency;
    int translationCount;
    int pageFaultCount;

    Segment* findSegment(int segNum);
    const Segment* findSegment(int segNum) const;
    std::string makeTLBKey(int segNum, int dirNum, int pageNum) const;
    int randomLatency() const;
    int handlePageFault(int segNum, int dirNum, int pageNum, Protection prot);
    void noteFault(int segNum);

public:
    SegmentTable(int tlbSize, int numFrames, bool use_lru, int pSize);
    ~SegmentTable();

    void addSegment(int id, int base, int limit, Protection prot);
    void removeSegment(int id);

    int translateAddress(int segNum, int dirNum, int pageNum, int offset, AccessType accessType);

    double getPageFaultRate() const;
    double getAverageLatency() const;
    int getPageSizeValue() const;
    int getSegmentCount() const;

    void displayStats() const;
    void printMemoryMap() const;
    void displayMemory() const;
    void displayTLB() const;
};

#endif