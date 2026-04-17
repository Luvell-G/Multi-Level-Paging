#include "simulator.hpp"

#include <cstdlib>

// ---------------- PhysicalMemory ----------------

PhysicalMemory::PhysicalMemory(int frames, bool lru)
    : num_frames(frames), free_frames(frames, true), use_lru(lru) {}

int PhysicalMemory::allocateFrame(int pageNum, int /*time*/) {
    for (int i = 0; i < num_frames; ++i) {
        if (free_frames[i]) {
            free_frames[i] = false;
            if (use_lru) {
                lru_list.push_back(i);
            } else {
                fifo_queue.push(i);
            }
            frame_to_page[i] = pageNum;
            return i;
        }
    }

    int frame = use_lru ? selectVictimLRU() : selectVictimFIFO();
    if (frame == -1) {
        throw PageFault("No frame available for replacement");
    }

    frame_to_page[frame] = pageNum;
    return frame;
}

void PhysicalMemory::freeFrame(int frame) {
    if (frame >= 0 && frame < num_frames) {
        free_frames[frame] = true;
        frame_to_page.erase(frame);

        if (use_lru) {
            lru_list.remove(frame);
        } else {
            std::queue<int> temp;
            while (!fifo_queue.empty()) {
                if (fifo_queue.front() != frame) {
                    temp.push(fifo_queue.front());
                }
                fifo_queue.pop();
            }
            fifo_queue = temp;
        }
    }
}

void PhysicalMemory::touchFrame(int frame) {
    if (!use_lru || frame < 0 || frame >= num_frames || free_frames[frame]) {
        return;
    }

    auto it = std::find(lru_list.begin(), lru_list.end(), frame);
    if (it != lru_list.end()) {
        lru_list.erase(it);
        lru_list.push_back(frame);
    }
}

int PhysicalMemory::selectVictimFIFO() {
    while (!fifo_queue.empty()) {
        int frame = fifo_queue.front();
        fifo_queue.pop();
        if (!free_frames[frame]) {
            fifo_queue.push(frame);
            return frame;
        }
    }
    return -1;
}

int PhysicalMemory::selectVictimLRU() {
    while (!lru_list.empty()) {
        int frame = lru_list.front();
        lru_list.pop_front();
        if (!free_frames[frame]) {
            lru_list.push_back(frame);
            return frame;
        }
    }
    return -1;
}

double PhysicalMemory::utilization() const {
    if (num_frames == 0) return 0.0;
    int used = static_cast<int>(std::count(free_frames.begin(), free_frames.end(), false));
    return static_cast<double>(used) / num_frames * 100.0;
}

int PhysicalMemory::getNumFrames() const {
    return num_frames;
}

bool PhysicalMemory::isUsingLRU() const {
    return use_lru;
}

bool PhysicalMemory::isFree(int frame) const {
    if (frame < 0 || frame >= num_frames) return true;
    return free_frames[frame];
}

void PhysicalMemory::display() const {
    std::cout << "Physical Memory Frames:\n";
    for (int i = 0; i < num_frames; ++i) {
        std::cout << "  Frame " << i << ": "
                  << (free_frames[i] ? "FREE" : "ALLOCATED") << "\n";
    }
}

// ---------------- PageTable ----------------

PageTable::PageTable(int numPages, int pageSize, int dirIdx)
    : pages(numPages), page_size(pageSize), directory_index(dirIdx) {}

int PageTable::getFrameNumber(int pageNum, int time, AccessType accessType, PhysicalMemory& physMem) {
    if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
        throw PageFault("Invalid page number " + std::to_string(pageNum));
    }

    if (!pages[pageNum].present) {
        int frame = physMem.allocateFrame(pageNum, time);
        pages[pageNum].frame_number = frame;
        pages[pageNum].present = true;
        pages[pageNum].last_access = time;
        return frame;
    }

    if (accessType == ACCESS_WRITE && pages[pageNum].protection == READ_ONLY) {
        throw ProtectionViolation("Cannot write to read-only page " + std::to_string(pageNum));
    }

    pages[pageNum].last_access = time;
    physMem.touchFrame(pages[pageNum].frame_number);
    return pages[pageNum].frame_number;
}

void PageTable::setFrame(int pageNum, int frameNum, Protection prot, int current_time) {
    if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
        throw std::out_of_range("Invalid page index");
    }
    pages[pageNum].frame_number = frameNum;
    pages[pageNum].present = true;
    pages[pageNum].protection = prot;
    pages[pageNum].last_access = current_time;
}

Page& PageTable::getPage(int pageNum) {
    if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
        throw std::out_of_range("Invalid page index");
    }
    return pages[pageNum];
}

const Page& PageTable::getPage(int pageNum) const {
    if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
        throw std::out_of_range("Invalid page index");
    }
    return pages[pageNum];
}

int PageTable::getNumPages() const {
    return static_cast<int>(pages.size());
}

int PageTable::getPageSize() const {
    return page_size;
}

int PageTable::getDirectoryIndex() const {
    return directory_index;
}

void PageTable::display() const {
    std::cout << "    Directory " << directory_index << ":\n";
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const Page& p = pages[i];
        std::cout << "      Page " << i
                  << " | Frame=" << p.frame_number
                  << " | Present=" << (p.present ? "Yes" : "No")
                  << " | Prot=" << (p.protection == READ_ONLY ? "RO" : "RW")
                  << " | LastAccess=" << p.last_access << "\n";
    }
}

// ---------------- DirectoryTable ----------------

DirectoryTable::DirectoryTable(int maxPages) : max_pages_per_table(maxPages) {}

DirectoryTable::~DirectoryTable() {
    freeTables();
}

PageTable* DirectoryTable::getPageTable(int dirNum, int pageSize) {
    while (dirNum >= static_cast<int>(pageTables.size())) {
        pageTables.push_back(new PageTable(max_pages_per_table, pageSize, static_cast<int>(pageTables.size())));
    }
    return pageTables[dirNum];
}

const std::vector<PageTable*>& DirectoryTable::getAllTables() const {
    return pageTables;
}

void DirectoryTable::freeTables() {
    for (PageTable* pt : pageTables) {
        delete pt;
    }
    pageTables.clear();
}

int DirectoryTable::getNumTables() const {
    return static_cast<int>(pageTables.size());
}

// ---------------- TLB ----------------

TLB::TLB(int size) : maxSize(size), hits(0), total(0) {}

int TLB::get(int segNum, int dirNum, int pageNum) {
    std::string key = std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);
    total++;

    auto it = cache.find(key);
    if (it != cache.end()) {
        hits++;
        lruOrder.remove(key);
        lruOrder.push_back(key);
        return it->second;
    }
    return -1;
}

void TLB::put(int segNum, int dirNum, int pageNum, int frame) {
    std::string key = std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);

    auto existing = cache.find(key);
    if (existing != cache.end()) {
        lruOrder.remove(key);
        cache[key] = frame;
        lruOrder.push_back(key);
        return;
    }

    if (static_cast<int>(cache.size()) >= maxSize && !lruOrder.empty()) {
        cache.erase(lruOrder.front());
        lruOrder.pop_front();
    }

    cache[key] = frame;
    lruOrder.push_back(key);
}

void TLB::remove(int segNum, int dirNum, int pageNum) {
    std::string key = std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);
    cache.erase(key);
    lruOrder.remove(key);
}

void TLB::clear() {
    cache.clear();
    lruOrder.clear();
    hits = 0;
    total = 0;
}

double TLB::hitRate() const {
    return total > 0 ? static_cast<double>(hits) / total * 100.0 : 0.0;
}

int TLB::getHits() const {
    return hits;
}

int TLB::getMisses() const {
    return total - hits;
}

int TLB::getTotalLookups() const {
    return total;
}

int TLB::getMaxSize() const {
    return maxSize;
}

void TLB::displayCache() const {
    std::cout << "TLB Contents (LRU Order):\n";
    for (const auto& key : lruOrder) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            std::cout << "  " << key << " -> Frame " << it->second << "\n";
        }
    }
    std::cout << "TLB Hit Rate: " << hitRate() << "%\n";
}

// ---------------- SegmentTable ----------------

SegmentTable::SegmentTable(int tlbSize, int numFrames, bool use_lru, int pSize)
    : tlb(tlbSize),
      physMem(numFrames, use_lru),
      pageSize(pSize),
      timeCounter(0),
      totalLatency(0),
      translationCount(0),
      pageFaultCount(0) {}

SegmentTable::~SegmentTable() {
    for (auto& pair : directoryTables) {
        delete pair.second;
    }
}

Segment* SegmentTable::findSegment(int segNum) {
    for (auto& seg : segments) {
        if (seg.segment_id == segNum) return &seg;
    }
    return nullptr;
}

const Segment* SegmentTable::findSegment(int segNum) const {
    for (const auto& seg : segments) {
        if (seg.segment_id == segNum) return &seg;
    }
    return nullptr;
}

std::string SegmentTable::makeTLBKey(int segNum, int dirNum, int pageNum) const {
    return std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);
}

int SegmentTable::randomLatency() const {
    return 1 + (std::rand() % 10);
}

void SegmentTable::noteFault(int segNum) {
    Segment* seg = findSegment(segNum);
    if (seg) {
        seg->fault_count++;
    }
    pageFaultCount++;
}

int SegmentTable::handlePageFault(int segNum, int dirNum, int pageNum, Protection prot) {
    noteFault(segNum);

    int frame = physMem.allocateFrame(pageNum, timeCounter);
    if (frameTable.find(frame) != frameTable.end()) {
        FrameRecord victim = frameTable[frame];

        auto it = directoryTables.find(victim.seg_id);
        if (it != directoryTables.end()) {
            PageTable* victimPT = it->second->getPageTable(victim.dir_id, pageSize);
            Page& victimPage = victimPT->getPage(victim.page_id);
            victimPage.present = false;
            victimPage.frame_number = -1;
            tlb.remove(victim.seg_id, victim.dir_id, victim.page_id);
        }
    }

    frameTable[frame] = {segNum, dirNum, pageNum};
    return frame;
}

void SegmentTable::addSegment(int id, int base, int limit, Protection prot) {
    if (findSegment(id) != nullptr) {
        throw std::runtime_error("Segment " + std::to_string(id) + " already exists");
    }

    segments.emplace_back(id, base, limit, prot);
    directoryTables[id] = new DirectoryTable(limit > 0 ? limit : 1);
}

void SegmentTable::removeSegment(int id) {
    Segment* seg = findSegment(id);
    auto dtIt = directoryTables.find(id);

    if (seg == nullptr || dtIt == directoryTables.end()) {
        throw std::runtime_error("Cannot remove invalid segment " + std::to_string(id));
    }

    DirectoryTable* dt = dtIt->second;
    for (PageTable* pt : dt->getAllTables()) {
        if (pt == nullptr) continue;
        for (int i = 0; i < pt->getNumPages(); ++i) {
            Page& page = pt->getPage(i);
            if (page.present) {
                physMem.freeFrame(page.frame_number);
                frameTable.erase(page.frame_number);
                tlb.remove(id, pt->getDirectoryIndex(), i);
                page.present = false;
                page.frame_number = -1;
            }
        }
    }

    delete dt;
    directoryTables.erase(id);

    for (auto it = segments.begin(); it != segments.end(); ++it) {
        if (it->segment_id == id) {
            segments.erase(it);
            break;
        }
    }
}

int SegmentTable::translateAddress(int segNum, int dirNum, int pageNum, int offset, AccessType accessType) {
    int latency = randomLatency();
    totalLatency += latency;
    translationCount++;

    Segment* segment = findSegment(segNum);
    if (segment == nullptr) {
        noteFault(segNum);
        throw SegmentationFault("Invalid segment " + std::to_string(segNum));
    }

    if (accessType == ACCESS_WRITE && segment->protection == READ_ONLY) {
        noteFault(segNum);
        throw ProtectionViolation("Cannot write to read-only segment " + std::to_string(segNum));
    }

    if (dirNum < 0) {
        noteFault(segNum);
        throw PageFault("Invalid dirNum " + std::to_string(dirNum) + ", must be >= 0");
    }

    auto dtIt = directoryTables.find(segNum);
    if (dtIt == directoryTables.end()) {
        noteFault(segNum);
        throw SegmentationFault("Missing directory table for segment " + std::to_string(segNum));
    }

    DirectoryTable* dirTable = dtIt->second;
    PageTable* pageTable = dirTable->getPageTable(dirNum, pageSize);

    if (pageNum < 0 || pageNum >= segment->limit) {
        noteFault(segNum);
        throw PageFault("Invalid pageNum " + std::to_string(pageNum) +
                        ", limit is " + std::to_string(segment->limit));
    }

    if (offset < 0 || offset >= pageTable->getPageSize()) {
        noteFault(segNum);
        throw PageFault("Invalid offset " + std::to_string(offset) +
                        ", page size is " + std::to_string(pageTable->getPageSize()));
    }

    int frame = tlb.get(segNum, dirNum, pageNum);
    if (frame != -1) {
        return segment->base_address + frame * pageTable->getPageSize() + offset;
    }

    Page& page = pageTable->getPage(pageNum);

    if (!page.present) {
        int newFrame = handlePageFault(segNum, dirNum, pageNum, segment->protection);
        pageTable->setFrame(pageNum, newFrame, segment->protection, timeCounter);
        frame = newFrame;
    } else {
        if (accessType == ACCESS_WRITE && page.protection == READ_ONLY) {
            noteFault(segNum);
            throw ProtectionViolation("Cannot write to read-only page " + std::to_string(pageNum));
        }
        frame = pageTable->getFrameNumber(pageNum, timeCounter, accessType, physMem);
    }

    tlb.put(segNum, dirNum, pageNum, frame);
    timeCounter++;

    return segment->base_address + frame * pageTable->getPageSize() + offset;
}

double SegmentTable::getPageFaultRate() const {
    if (translationCount == 0) return 0.0;
    return static_cast<double>(pageFaultCount) / translationCount * 100.0;
}

double SegmentTable::getAverageLatency() const {
    if (translationCount == 0) return 0.0;
    return static_cast<double>(totalLatency) / translationCount;
}

int SegmentTable::getPageSizeValue() const {
    return pageSize;
}

int SegmentTable::getSegmentCount() const {
    return static_cast<int>(segments.size());
}

void SegmentTable::displayStats() const {
    std::cout << "\nPage Fault Statistics:\n";
    for (const auto& seg : segments) {
        std::cout << "Segment " << seg.segment_id << ": " << seg.fault_count << " faults\n";
        if (translationCount > 0 && seg.fault_count > 0.2 * translationCount) {
            std::cout << "Suggestion: Increase limit for Segment "
                      << seg.segment_id << " to reduce faults\n";
        }
    }

    std::cout << "Replacement Policy: " << (physMem.isUsingLRU() ? "LRU" : "FIFO") << "\n";
    std::cout << "Average Translation Latency: " << getAverageLatency() << "\n";
    std::cout << "Page Fault Rate: " << getPageFaultRate() << "%\n";
    std::cout << "Physical Memory Utilization: " << physMem.utilization() << "%\n";
    std::cout << "TLB Size: " << tlb.getMaxSize() << "\n";
    tlb.displayCache();
}

void SegmentTable::printMemoryMap() const {
    std::cout << "\nMemory Map:\n";
    for (const auto& seg : segments) {
        std::cout << "Segment " << seg.segment_id
                  << ": Base=" << seg.base_address
                  << ", Limit=" << seg.limit
                  << ", Protection=" << (seg.protection == READ_ONLY ? "RO" : "RW")
                  << ", Faults=" << seg.fault_count << "\n";

        auto dtIt = directoryTables.find(seg.segment_id);
        if (dtIt == directoryTables.end()) continue;

        const auto& tables = dtIt->second->getAllTables();
        for (PageTable* pt : tables) {
            if (pt) pt->display();
        }
    }

    std::cout << "\nFrame Status:\n";
    physMem.display();
}

void SegmentTable::displayMemory() const {
    physMem.display();
}

void SegmentTable::displayTLB() const {
    tlb.displayCache();
}