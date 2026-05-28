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

struct UnrolledCursor {
  std::list<std::vector<char>>::iterator chunk_it;
  std::size_t element_idx;
};

void performArrayStep(std::vector<std::vector<char>> &arrayData,
                      std::size_t &cursor, std::uint64_t &seed,
                      std::uint64_t &checksum, OperationMix &mix,
                      std::size_t elemBytes) {
  switch (mix.next()) {
  case OperationKind::READ: {
    const auto &element = arrayData[cursor];
    for (char b : element) {
      const std::uint64_t readValue = static_cast<unsigned char>(b);
      seed = seed * 1'664'525u + 1'013'904'223u + readValue + cursor;
      checksum ^= seed ^ readValue;
    }
    cursor = (cursor + 1 == arrayData.size()) ? 0 : cursor + 1;
    break;
  }
  case OperationKind::WRITE: {
    auto &element = arrayData[cursor];
    for (char &b : element) {
      const std::uint64_t writeValue = (seed + checksum + cursor) & 0xFFu;
      b = static_cast<char>(writeValue);
      seed = seed * 1'664'525u + 1'013'904'223u + writeValue + cursor;
      checksum ^= seed ^ writeValue;
    }
    cursor = (cursor + 1 == arrayData.size()) ? 0 : cursor + 1;
    break;
  }
  case OperationKind::INSERT: {
    const auto insertValue =
        static_cast<char>((seed + checksum + cursor) & 0xFFu);
    arrayData.insert(arrayData.begin() + static_cast<std::ptrdiff_t>(cursor),
                     std::vector<char>(elemBytes, insertValue));
    seed = seed * 1'664'525u + 1'013'904'223u +
           static_cast<unsigned char>(insertValue) + cursor;
    checksum ^= seed ^ static_cast<unsigned char>(insertValue);
    cursor = (cursor + 1 == arrayData.size()) ? 0 : cursor + 1;
    break;
  }

  // TODO: access all bytes
  case OperationKind::ERASE: {
    const std::size_t eraseIndex = cursor;
    const std::uint64_t erasedValue =
        static_cast<unsigned char>(arrayData[cursor][0]);
    arrayData.erase(arrayData.begin() + static_cast<std::ptrdiff_t>(cursor));
    seed = seed * 1'664'525u + 1'013'904'223u + erasedValue + eraseIndex;
    checksum ^= seed ^ erasedValue;
    cursor = (eraseIndex == arrayData.size()) ? 0 : eraseIndex;
    break;
  }
  }
}

void performListStep(std::list<std::vector<char>> &listData,
                     std::list<std::vector<char>>::iterator &cursor,
                     std::uint64_t &seed, std::uint64_t &checksum,
                     OperationMix &mix, std::size_t elemBytes) {
  switch (mix.next()) {
  case OperationKind::READ: {
    const auto &element = *cursor;
    for (char b : element) {
      const std::uint64_t readValue = static_cast<unsigned char>(b);
      seed = seed * 1'664'525u + 1'013'904'223u + readValue;
      checksum ^= seed ^ readValue;
    }
    ++cursor;
    if (cursor == listData.end()) {
      cursor = listData.begin();
    }
    break;
  }
  case OperationKind::WRITE: {
    auto &element = *cursor;
    for (char &b : element) {
      const std::uint64_t writeValue = (seed + checksum) & 0xFFu;
      b = static_cast<char>(writeValue);
      seed = seed * 1'664'525u + 1'013'904'223u + writeValue;
      checksum ^= seed ^ writeValue;
    }
    ++cursor;
    if (cursor == listData.end()) {
      cursor = listData.begin();
    }
    break;
  }
  case OperationKind::INSERT: {
    const auto insertValue = static_cast<char>((seed + checksum) & 0xFFu);
    auto inserted =
        listData.insert(cursor, std::vector<char>(elemBytes, insertValue));
    ++inserted;
    cursor = inserted;
    if (cursor == listData.end()) {
      cursor = listData.begin();
    }
    seed = seed * 1'664'525u + 1'013'904'223u +
           static_cast<unsigned char>(insertValue);
    checksum ^= seed ^ static_cast<unsigned char>(insertValue);
    break;
  }

  // TODO: access all bytes
  case OperationKind::ERASE: {
    const std::uint64_t erasedValue = static_cast<unsigned char>((*cursor)[0]);
    cursor = listData.erase(cursor);
    if (cursor == listData.end()) {
      cursor = listData.begin();
    }
    seed = seed * 1'664'525u + 1'013'904'223u + erasedValue;
    checksum ^= seed ^ erasedValue;
    break;
  }
  }
}

void performUnrolledListStep(std::list<std::vector<char>> &listData,
                             UnrolledCursor &cursor, std::uint64_t &seed,
                             std::uint64_t &checksum, OperationMix &mix,
                             std::size_t elemBytes,
                             std::size_t unrolledMaxChunkSize) {
  switch (mix.next()) {
  case OperationKind::READ: {
    // Access the element at cursor.element_idx within the current chunk.
    auto &chunk = *cursor.chunk_it;
    std::size_t startByte = cursor.element_idx * elemBytes;
    for (std::size_t i = 0; i < elemBytes; ++i) {
      const std::uint64_t readValue =
          static_cast<unsigned char>(chunk[startByte + i]);
      seed = seed * 1'664'525u + 1'013'904'223u + readValue;
      checksum ^= seed ^ readValue;
    }

    // Advance the cursor. If we reach the end of the current chunk,
    // move to the beginning of the next chunk (wrapping around if necessary).
    cursor.element_idx++;
    if (cursor.element_idx >= chunk.size() / elemBytes) {
      cursor.element_idx = 0;
      cursor.chunk_it++;
      if (cursor.chunk_it == listData.end()) {
        cursor.chunk_it = listData.begin();
      }
    }
    break;
  }
  case OperationKind::WRITE: {
    // Access the element at cursor.element_idx within the current chunk.
    auto &chunk = *cursor.chunk_it;
    std::size_t startByte = cursor.element_idx * elemBytes;
    for (std::size_t i = 0; i < elemBytes; ++i) {
      const std::uint64_t writeValue = (seed + checksum) & 0xFFu;
      chunk[startByte + i] = static_cast<char>(writeValue);
      seed = seed * 1'664'525u + 1'013'904'223u + writeValue;
      checksum ^= seed ^ writeValue;
    }

    // Advance the cursor identically to the READ case.
    cursor.element_idx++;
    if (cursor.element_idx >= chunk.size() / elemBytes) {
      cursor.element_idx = 0;
      cursor.chunk_it++;
      if (cursor.chunk_it == listData.end()) {
        cursor.chunk_it = listData.begin();
      }
    }
    break;
  }
  case OperationKind::INSERT: {
    const auto insertValue = static_cast<char>((seed + checksum) & 0xFFu);
    auto &chunk = *cursor.chunk_it;
    std::size_t startByte = cursor.element_idx * elemBytes;

    // Insert the new element (elemBytes bytes) into the current vector chunk.
    chunk.insert(chunk.begin() + startByte, elemBytes, insertValue);

    seed = seed * 1'664'525u + 1'013'904'223u +
           static_cast<unsigned char>(insertValue);
    checksum ^= seed ^ static_cast<unsigned char>(insertValue);

    // If the chunk exceeds unrolledMaxChunkSize, split it in half.
    if (chunk.size() / elemBytes > unrolledMaxChunkSize) {
      std::size_t totalElements = chunk.size() / elemBytes;
      std::size_t firstHalfElements = totalElements / 2;
      std::size_t secondHalfElements = totalElements - firstHalfElements;

      // Create a new chunk and move the second half of elements into it.
      std::vector<char> nextChunk;
      nextChunk.reserve(secondHalfElements * elemBytes);
      auto splitIt = chunk.begin() + firstHalfElements * elemBytes;
      nextChunk.assign(std::make_move_iterator(splitIt),
                       std::make_move_iterator(chunk.end()));
      chunk.erase(splitIt, chunk.end());

      // Insert the new chunk into the std::list.
      auto newChunkIt =
          listData.insert(std::next(cursor.chunk_it), std::move(nextChunk));

      // Update the cursor if the element it points to has moved to the new chunk.
      if (cursor.element_idx >= firstHalfElements) {
        cursor.chunk_it = newChunkIt;
        cursor.element_idx -= firstHalfElements;
      }
    }

    // To mimic standard list behavior, the cursor is updated to point to
    // the element AFTER the one we just inserted.
    cursor.element_idx++;
    if (cursor.element_idx >= cursor.chunk_it->size() / elemBytes) {
      cursor.element_idx = 0;
      cursor.chunk_it++;
      if (cursor.chunk_it == listData.end()) {
        cursor.chunk_it = listData.begin();
      }
    }
    break;
  }
  case OperationKind::ERASE: {
    auto &chunk = *cursor.chunk_it;
    std::size_t startByte = cursor.element_idx * elemBytes;
    const std::uint64_t erasedValue =
        static_cast<unsigned char>(chunk[startByte]);

    // Remove the element from the current vector chunk.
    chunk.erase(chunk.begin() + startByte,
                chunk.begin() + startByte + elemBytes);

    seed = seed * 1'664'525u + 1'013'904'223u + erasedValue;
    checksum ^= seed ^ erasedValue;

    // If the chunk becomes completely empty, remove it from the list.
    if (chunk.empty()) {
      cursor.chunk_it = listData.erase(cursor.chunk_it);
      cursor.element_idx = 0;
      if (cursor.chunk_it == listData.end()) {
        cursor.chunk_it = listData.begin();
      }
    } else {
      // If we erased the last element in a chunk, wrap to the next chunk.
      if (cursor.element_idx >= chunk.size() / elemBytes) {
        cursor.element_idx = 0;
        cursor.chunk_it++;
        if (cursor.chunk_it == listData.end()) {
          cursor.chunk_it = listData.begin();
        }
      }
    }
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

void populateUnrolledLinkedList(std::list<std::vector<char>> &listData,
                                std::size_t elementCount, std::size_t elemBytes,
                                std::size_t unrolledMaxChunkSize) {
  std::size_t remaining = elementCount;
  while (remaining > 0) {
    std::size_t currentChunkSize = std::min(remaining, unrolledMaxChunkSize);
    listData.emplace_back(currentChunkSize * elemBytes);
    remaining -= currentChunkSize;
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
  if (text == "unrolled_linked_list" ||
      text == "unrolled_list" ||
      text == "UNROLLED_LINKED_LIST") {
    value = DataStructure::UNROLLED_LINKED_LIST;
    return true;
  }
  return false;
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
  const std::size_t unrolledMaxChunkSize = (argc == 6) ? std::stoull(argv[5]) : 64;

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
                   performArrayStep(arrayData, cursor, seed, checksum, mix,
                                    elemBytes);
                 });
  } else if (dataStructure == DataStructure::UNROLLED_LINKED_LIST) {
    std::list<std::vector<char>> listData;
    populateUnrolledLinkedList(listData, elementCountValue, elemBytes,
                               unrolledMaxChunkSize);

    UnrolledCursor cursor{listData.begin(), 0};
    runBenchmark(dataStructureLabel(dataStructure), elementCountValue,
                 elemBytes, insDelPercentValue,
                 [&](std::uint64_t &seed, std::uint64_t &checksum,
                     OperationMix &mix) {
                   performUnrolledListStep(listData, cursor, seed, checksum, mix,
                                           elemBytes, unrolledMaxChunkSize);
                 });
  } else {
    std::list<std::vector<char>> listData;
    if (dataStructure == DataStructure::LINKED_LIST_SEQUENTIAL) {
      populateSequentialLinkedList(listData, elementCountValue, elemBytes);
    } else {
      populateRandomLinkedList(listData, elementCountValue, elemBytes);
    }

    auto cursor = listData.begin();
    runBenchmark(dataStructureLabel(dataStructure), elementCountValue,
                 elemBytes, insDelPercentValue,
                 [&](std::uint64_t &seed, std::uint64_t &checksum,
                     OperationMix &mix) {
                   performListStep(listData, cursor, seed, checksum, mix,
                                   elemBytes);
                 });
  }
}
