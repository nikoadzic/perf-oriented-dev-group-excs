#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <list>
#include <new>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

constexpr std::size_t chunkSize = 256;
constexpr auto warmupDuration = std::chrono::seconds{2};
constexpr auto measureDuration = std::chrono::seconds{5};

enum class OperationKind {
  READ,
  WRITE,
  INSERT,
  ERASE,
};

enum class DataStructure {
  ARRAY,
  LINKED_LIST_SEQUENTIAL,
  LINKED_LIST_RANDOM,
  UNROLLED_LINKED_LIST,
};

struct ArrayAdapter {
  std::vector<std::vector<char>> &data;
  std::size_t &cursor;
  std::size_t elemBytes;

  std::vector<char> &current() { return data[cursor]; }
  const std::vector<char> &current() const { return data[cursor]; }

  std::size_t position() const { return cursor; }

  void advance() {
    cursor = (cursor + 1 == data.size()) ? 0 : cursor + 1;
  }

  void insert(char value) {
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                std::vector<char>(elemBytes, value));
  }

  void erase() {
    data.erase(data.begin() + static_cast<std::ptrdiff_t>(cursor));
    cursor = (cursor == data.size()) ? 0 : cursor;
  }
};

struct ListAdapter {
  std::list<std::vector<char>> &data;
  std::list<std::vector<char>>::iterator &cursor;
  std::size_t elemBytes;

  std::vector<char> &current() { return *cursor; }
  const std::vector<char> &current() const { return *cursor; }

  std::size_t position() const { return 0; }

  void advance() {
    ++cursor;
    if (cursor == data.end()) {
      cursor = data.begin();
    }
  }

  void insert(char value) {
    auto inserted = data.insert(cursor, std::vector<char>(elemBytes, value));
    cursor = inserted;
    advance();
  }

  void erase() {
    cursor = data.erase(cursor);
    if (cursor == data.end()) {
      cursor = data.begin();
    }
  }
};

struct UnrolledListAdapter {
  std::list<std::vector<char>> &data;
  std::list<std::vector<char>>::iterator &cursor;
  std::size_t elemBytes;
  std::size_t unrolledMaxChunkSize;
  std::size_t elementIdx;

  std::vector<char> &current() { return *cursor; }
  const std::vector<char> &current() const { return *cursor; }

  std::size_t position() const { return elementIdx; }

  void advance() {
    ++elementIdx;
    if (elementIdx >= cursor->size() / elemBytes) {
      elementIdx = 0;
      ++cursor;
      if (cursor == data.end()) {
        cursor = data.begin();
      }
    }
  }

  void insert(char value) {
    auto &chunk = *cursor;
    const std::size_t startByte = elementIdx * elemBytes;

    chunk.insert(chunk.begin() + static_cast<std::ptrdiff_t>(startByte),
                 elemBytes, value);

    if (chunk.size() / elemBytes > unrolledMaxChunkSize) {
      std::size_t totalElements = chunk.size() / elemBytes;
      std::size_t firstHalfElements = totalElements / 2;
      std::size_t secondHalfElements = totalElements - firstHalfElements;

      std::vector<char> nextChunk;
      nextChunk.reserve(secondHalfElements * elemBytes);
      auto splitIt = chunk.begin() +
                     static_cast<std::ptrdiff_t>(firstHalfElements * elemBytes);
      nextChunk.assign(std::make_move_iterator(splitIt),
                       std::make_move_iterator(chunk.end()));
      chunk.erase(splitIt, chunk.end());

      auto newChunkIt = data.insert(std::next(cursor), std::move(nextChunk));
      if (elementIdx >= firstHalfElements) {
        cursor = newChunkIt;
        elementIdx -= firstHalfElements;
      }
    }

    advance();
  }

  void erase() {
    auto &chunk = *cursor;
    const std::size_t startByte = elementIdx * elemBytes;

    chunk.erase(chunk.begin() + static_cast<std::ptrdiff_t>(startByte),
                chunk.begin() +
                    static_cast<std::ptrdiff_t>(startByte + elemBytes));

    if (chunk.empty()) {
      cursor = data.erase(cursor);
      elementIdx = 0;
      if (cursor == data.end()) {
        cursor = data.begin();
      }
    } else {
      if (elementIdx >= chunk.size() / elemBytes) {
        elementIdx = 0;
        ++cursor;
        if (cursor == data.end()) {
          cursor = data.begin();
        }
      }
    }
  }

  template <typename Mix>
  void step(std::uint64_t &seed, std::uint64_t &checksum, Mix &mix) {
    switch (mix.next()) {
    case OperationKind::READ: {
      const auto &chunk = current();
      const std::size_t startByte = elementIdx * elemBytes;
      for (std::size_t i = 0; i < elemBytes; ++i) {
        const std::uint64_t readValue =
            static_cast<unsigned char>(chunk[startByte + i]);
        seed = seed * 1'664'525u + 1'013'904'223u + readValue;
        checksum ^= seed ^ readValue;
      }
      advance();
      break;
    }
    case OperationKind::WRITE: {
      auto &chunk = current();
      const std::size_t startByte = elementIdx * elemBytes;
      for (std::size_t i = 0; i < elemBytes; ++i) {
        const std::uint64_t writeValue = (seed + checksum) & 0xFFu;
        chunk[startByte + i] = static_cast<char>(writeValue);
        seed = seed * 1'664'525u + 1'013'904'223u + writeValue;
        checksum ^= seed ^ writeValue;
      }
      advance();
      break;
    }
    case OperationKind::INSERT: {
      const auto insertValue = static_cast<char>((seed + checksum) & 0xFFu);
      insert(insertValue);
      seed = seed * 1'664'525u + 1'013'904'223u +
             static_cast<unsigned char>(insertValue);
      checksum ^= seed ^ static_cast<unsigned char>(insertValue);
      break;
    }
    case OperationKind::ERASE: {
      const std::uint64_t erasedValue =
          static_cast<unsigned char>(current()[elementIdx * elemBytes]);
      erase();
      seed = seed * 1'664'525u + 1'013'904'223u + erasedValue;
      checksum ^= seed ^ erasedValue;
      break;
    }
    }
  }
};

std::string_view dataStructureLabel(DataStructure value) {
  switch (value) {
  case DataStructure::ARRAY:
    return "array";
  case DataStructure::LINKED_LIST_SEQUENTIAL:
    return "linked_list_sequential";
  case DataStructure::LINKED_LIST_RANDOM:
    return "linked_list_random";
  case DataStructure::UNROLLED_LINKED_LIST:
    return "unrolled_linked_list";
  }
  return "unknown";
}

struct OperationMix {
  explicit OperationMix(std::size_t insDelPercentage) {
    insDelPeriod = (insDelPercentage == 0) ? 0 : (100 / insDelPercentage);
    counter = 0;
    nextRead = true;
    nextInsertion = true;
  }

  OperationKind next() {
    const bool doInsDel = (insDelPeriod != 0 && counter == insDelPeriod - 1);
    OperationKind kind;

    if (doInsDel) {
      kind = nextInsertion ? OperationKind::INSERT : OperationKind::ERASE;
      nextInsertion = !nextInsertion;
    } else {
      kind = nextRead ? OperationKind::READ : OperationKind::WRITE;
      nextRead = !nextRead;
    }

    if (insDelPeriod != 0) {
      counter = (counter + 1) % insDelPeriod;
    }
    return kind;
  }

  std::size_t insDelPeriod;
  std::size_t counter;
  bool nextRead;
  bool nextInsertion;
};

template <typename Adapter>
void performStep(Adapter &adapter, std::uint64_t &seed,
                        std::uint64_t &checksum, OperationMix &mix) {
  switch (mix.next()) {
  case OperationKind::READ: {
    const auto &element = adapter.current();
    const std::size_t position = adapter.position();
    for (char b : element) {
      const std::uint64_t readValue = static_cast<unsigned char>(b);
      seed = seed * 1'664'525u + 1'013'904'223u + readValue + position;
      checksum ^= seed ^ readValue;
    }
    adapter.advance();
    break;
  }
  case OperationKind::WRITE: {
    auto &element = adapter.current();
    const std::size_t position = adapter.position();
    for (char &b : element) {
      const std::uint64_t writeValue = (seed + checksum + position) & 0xFFu;
      b = static_cast<char>(writeValue);
      seed = seed * 1'664'525u + 1'013'904'223u + writeValue + position;
      checksum ^= seed ^ writeValue;
    }
    adapter.advance();
    break;
  }
  case OperationKind::INSERT: {
    const std::size_t position = adapter.position();
    const auto insertValue =
        static_cast<char>((seed + checksum + position) & 0xFFu);
    adapter.insert(insertValue);
    seed = seed * 1'664'525u + 1'013'904'223u +
           static_cast<unsigned char>(insertValue) + position;
    checksum ^= seed ^ static_cast<unsigned char>(insertValue);
    break;
  }
  case OperationKind::ERASE: {
    const std::size_t position = adapter.position();
    const std::uint64_t erasedValue =
        static_cast<unsigned char>(adapter.current()[0]);
    adapter.erase();
    seed = seed * 1'664'525u + 1'013'904'223u + erasedValue + position;
    checksum ^= seed ^ erasedValue;
    break;
  }
  }
}

void populateSequentialLinkedList(std::list<std::vector<char>> &listData,
                                  std::size_t elementCount,
                                  std::size_t elemBytes) {
  for (std::size_t i = 0; i < elementCount; ++i) {
    listData.emplace_back(elemBytes);
  }
}

void populateRandomLinkedList(std::list<std::vector<char>> &listData,
                              std::size_t elementCount, std::size_t elemBytes) {
  std::list<std::vector<char>> allocationList;
  std::vector<std::list<std::vector<char>>::iterator> nodes(elementCount);
  std::vector<std::size_t> allocationOrder(elementCount);
  std::iota(allocationOrder.begin(), allocationOrder.end(), 0);

  std::mt19937 rng(static_cast<unsigned>(std::rand()));
  std::shuffle(allocationOrder.begin(), allocationOrder.end(), rng);

  for (std::size_t logicalIndex : allocationOrder) {
    nodes[logicalIndex] =
        allocationList.emplace(allocationList.end(), elemBytes);
  }

  for (std::size_t logicalIndex = 0; logicalIndex < elementCount;
       ++logicalIndex) {
    listData.splice(listData.end(), allocationList, nodes[logicalIndex]);
  }
}

bool parseDataStructure(std::string_view text, DataStructure &value) {
  if (text == "array" || text == "ARRAY") {
    value = DataStructure::ARRAY;
    return true;
  }
  if (text == "linked_list_sequential" ||
      text == "linked_list_seq" || 
      text == "LINKED_LIST_SEQ" ||
      text == "LINKED_LIST_SEQUENTIAL") {
    value = DataStructure::LINKED_LIST_SEQUENTIAL;
    return true;
  }
  if (text == "linked_list_random" || 
    text == "linked_list_rand" || 
    text == "LINKED_LIST_RAND" || 
    text == "LINKED_LIST_RANDOM") {
    value = DataStructure::LINKED_LIST_RANDOM;
    return true;
  }
  if (text == "unrolled_linked_list" || text == "UNROLLED_LINKED_LIST") {
    value = DataStructure::UNROLLED_LINKED_LIST;
    return true;
  }
  return false;
}

void populateUnrolledLinkedList(std::list<std::vector<char>> &listData,
                                std::size_t elementCount,
                                std::size_t elemBytes,
                                std::size_t unrolledMaxChunkSize) {
  std::size_t remaining = elementCount;
  while (remaining > 0) {
    std::size_t currentChunkSize = std::min(remaining, unrolledMaxChunkSize);
    listData.emplace_back(currentChunkSize * elemBytes);
    remaining -= currentChunkSize;
  }
}

template <typename StepFn>
void runBenchmark(std::string_view label, std::size_t elementCount,
                  std::size_t elemBytes, std::size_t insDelPercent,
                  StepFn &&stepFn) {
  std::uint64_t checksum = 0;
  std::uint64_t seed = static_cast<std::uint64_t>(std::rand());
  OperationMix mix{insDelPercent};

  // Warmup
  const auto warmupEnd = std::chrono::steady_clock::now() + warmupDuration;
  while (std::chrono::steady_clock::now() < warmupEnd) {
    for (std::size_t i = 0; i < chunkSize; ++i) {
      stepFn(seed, checksum, mix);
    }
  }

  // Measurement
  std::size_t operations = 0;
  const auto measureStart = std::chrono::steady_clock::now();
  const auto measureEnd = measureStart + measureDuration;

  while (std::chrono::steady_clock::now() < measureEnd) {
    for (std::size_t i = 0; i < chunkSize; ++i) {
      stepFn(seed, checksum, mix);
      ++operations;
    }
  }
  const auto measureActualEnd = std::chrono::steady_clock::now();
  const double seconds =
      std::chrono::duration<double>(measureActualEnd - measureStart).count();

  std::cout << label << "," << elementCount << "," << elemBytes << ","
            << insDelPercent << ","
            << (static_cast<double>(operations) / seconds) << "," << checksum
            << std::endl;
}

int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) {
    std::cerr << "Usage: " << argv[0]
              << " <data-structure> <ins-del-percent> <element-size> "
                 "<element-count> [unrolled-max-chunk-size]\n";
    return 1;
  }

  DataStructure dataStructure{};
  if (!parseDataStructure(argv[1], dataStructure)) {
    std::cerr << "Invalid DataStructure argument: " << argv[1] << "\n";
    return 1;
  }

  const std::size_t insDelPercentValue = std::stoull(argv[2]);
  const std::size_t elemBytes = std::stoull(argv[3]);
  const std::size_t elementCountValue = std::stoull(argv[4]);
  const std::size_t unrolledMaxChunkSize =
      (argc == 6) ? std::stoull(argv[5]) : 64;

  // Seed the simple C RNG once for std::rand
  std::srand(static_cast<unsigned>(std::time(nullptr)));

  if (dataStructure == DataStructure::ARRAY) {
    std::vector<std::vector<char>> arrayData;
    arrayData.reserve(elementCountValue + 1);
    for (std::size_t i = 0; i < elementCountValue; ++i) {
      arrayData.emplace_back(elemBytes);
    }

    std::size_t cursor = 0;
    runBenchmark("array", elementCountValue, elemBytes, insDelPercentValue,
                 [&](std::uint64_t &seed, std::uint64_t &checksum,
                     OperationMix &mix) {
                   ArrayAdapter adapter{arrayData, cursor, elemBytes};
                   performStep(adapter, seed, checksum, mix);
                 });
  } else {
    std::list<std::vector<char>> listData;
    if (dataStructure == DataStructure::LINKED_LIST_SEQUENTIAL) {
      populateSequentialLinkedList(listData, elementCountValue, elemBytes);
    } else if (dataStructure == DataStructure::LINKED_LIST_RANDOM) {
      populateRandomLinkedList(listData, elementCountValue, elemBytes);
    } else {
      populateUnrolledLinkedList(listData, elementCountValue, elemBytes,
                                 unrolledMaxChunkSize);
    }

    auto cursor = listData.begin();
    if (dataStructure == DataStructure::UNROLLED_LINKED_LIST) {
      UnrolledListAdapter adapter{listData, cursor, elemBytes,
                                  unrolledMaxChunkSize, 0};
      runBenchmark(dataStructureLabel(dataStructure), elementCountValue,
                   elemBytes, insDelPercentValue,
                   [&](std::uint64_t &seed, std::uint64_t &checksum,
                       OperationMix &mix) {
                     adapter.step(seed, checksum, mix);
                   });
    } else {
      runBenchmark(dataStructureLabel(dataStructure), elementCountValue,
                   elemBytes, insDelPercentValue,
                   [&](std::uint64_t &seed, std::uint64_t &checksum,
                       OperationMix &mix) {
                     ListAdapter adapter{listData, cursor, elemBytes};
                     performStep(adapter, seed, checksum, mix);
                   });
    }
  }
}
