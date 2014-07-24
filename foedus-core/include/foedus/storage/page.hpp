/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_STORAGE_PAGE_HPP_
#define FOEDUS_STORAGE_PAGE_HPP_

#include <cstring>

#include "foedus/assert_nd.hpp"
#include "foedus/compiler.hpp"
#include "foedus/cxx11.hpp"
#include "foedus/epoch.hpp"
#include "foedus/assorted/atomic_fences.hpp"
#include "foedus/storage/fwd.hpp"
#include "foedus/storage/storage_id.hpp"
#include "foedus/thread/thread_id.hpp"

namespace foedus {
namespace storage {
/**
 * @brief The following 1-byte value is stored in the common page header.
 * @ingroup STORAGE
 * @details
 * These values are stored in snapshot pages too, so we must keep values' compatibility.
 * Specify values for that reason.
 */
enum PageType {
  kUnknownPageType = 0,
  kArrayPageType = 1,
  kMasstreeIntermediatePageType = 2,
  kMasstreeBorderPageType = 3,
  kSequentialPageType = 4,
  kSequentialRootPageType = 5,
  kHashRootPageType = 6,
  kHashBinPageType = 7,
  kHashDataPageType = 8,
};

// some of them are 64bit uint, so can't use enum.
const uint64_t  kPageVersionLockedBit    = (1ULL << 63);
const uint64_t  kPageVersionInsertingBit = (1ULL << 62);
const uint64_t  kPageVersionSplittingBit = (1ULL << 61);
const uint64_t  kPageVersionDeletedBit   = (1ULL << 60);
const uint64_t  kPageVersionHasFosterChildBit    = (1ULL << 59);
const uint64_t  kPageVersionIsBorderBit  = (1ULL << 58);
const uint64_t  kPageVersionIsSupremumBit  = (1ULL << 57);
const uint64_t  kPageVersionInsertionCounterMask  = 0x01F8000000000000ULL;
const uint8_t   kPageVersionInsertionCounterShifts = 51;
const uint64_t  kPageVersionSplitCounterMask      = 0x0007FFFE00000000ULL;
const uint8_t   kPageVersionSplitCounterShifts    = 33;
const uint32_t  kPageVersionKeyCountMask          = 0xFFFF0000U;
const uint8_t   kPageVersionKeyCountShifts        = 16;
const uint32_t  kPageVersionLayerMask             = 0x0000FF00U;
const uint8_t   kPageVersionLayerShifts           = 8;

const uint64_t  kPageVersionUnlockMask =
  (kPageVersionHasFosterChildBit |
  kPageVersionIsBorderBit |
  kPageVersionKeyCountMask |
  kPageVersionLayerMask);

/**
 * @brief 64bit in-page version counter and also locking mechanism.
 * @ingroup STORAGE
 * @details
 * Each page has this in the header.
 * \li bit-0: locked
 * \li bit-1: inserting
 * \li bit-2: splitting
 * \li bit-3: (not used) deleted
 * \li bit-4: has_foster_child
 * \li bit-5: is_border
 * \li bit-6: is_high_fence_supremum
 * \li bit-[7,13): insert counter
 * \li bit-[13,31): split counter (do we need this much..?)
 * \li bit-31: unused
 * \li bit-[32,48): \e physical key count (those keys might be deleted)
 * \li bit-[48,56): layer (not a mutable property, placed here just to save space)
 * \li bit-[56,64): unused
 * Unlike [YANDONG12], this is 64bit to also contain a key count.
 * We maintain key count and permutation differently from [YANDONG12].
 *
 * This object is a POD.
 * All methods are inlined except stream.
 */
struct PageVersion CXX11_FINAL {
  PageVersion() ALWAYS_INLINE : data_(0) {}
  explicit PageVersion(uint64_t data) ALWAYS_INLINE : data_(data) {}

  /** used only when we create a new page. don't call this for an existing page (if locked..!) */
  void    initialize(
    bool locked,
    bool has_foster_child,
    bool is_border,
    bool is_high_fence_supremum,
    uint8_t layer) {
    data_ = 0;
    if (locked) {
      data_ |= kPageVersionLockedBit;
    }
    if (has_foster_child) {
      data_ |= kPageVersionHasFosterChildBit;
    }
    if (is_border) {
      data_ |= kPageVersionIsBorderBit;
    }
    if (is_high_fence_supremum) {
      data_ |= kPageVersionIsSupremumBit;
    }
    data_ |= (static_cast<uint64_t>(layer) << kPageVersionLayerShifts);
  }

  void    set_data(uint64_t data) ALWAYS_INLINE { data_ = data; }

  bool    is_locked() const ALWAYS_INLINE { return data_ & kPageVersionLockedBit; }
  bool    is_inserting() const ALWAYS_INLINE { return data_ & kPageVersionInsertingBit; }
  bool    is_splitting() const ALWAYS_INLINE { return data_ & kPageVersionSplittingBit; }
  bool    is_deleted() const ALWAYS_INLINE { return data_ & kPageVersionDeletedBit; }
  bool    has_foster_child() const ALWAYS_INLINE { return data_ & kPageVersionHasFosterChildBit; }
  bool    is_border() const ALWAYS_INLINE { return data_ & kPageVersionIsBorderBit; }
  bool    is_high_fence_supremum() const ALWAYS_INLINE { return data_ & kPageVersionIsSupremumBit; }
  uint32_t  get_insert_counter() const ALWAYS_INLINE {
    return (data_ & kPageVersionInsertionCounterMask) >> kPageVersionInsertionCounterShifts;
  }
  uint32_t  get_split_counter() const ALWAYS_INLINE {
    return (data_ & kPageVersionSplitCounterMask) >> kPageVersionSplitCounterShifts;
  }
  uint16_t  get_key_count() const ALWAYS_INLINE {
    return (data_ & kPageVersionKeyCountMask) >> kPageVersionKeyCountShifts;
  }

  /** Layer-0 stores the first 8 byte slice, Layer-1 next 8 byte... */
  uint8_t   get_layer() const ALWAYS_INLINE {
    return (data_ & kPageVersionLayerMask) >> kPageVersionLayerShifts;
  }

  void      set_inserting() ALWAYS_INLINE {
    ASSERT_ND(is_locked());
    data_ |= kPageVersionInsertingBit;
  }
  void      set_inserting_and_increment_key_count() ALWAYS_INLINE {
    set_inserting();
    data_ += (1ULL << kPageVersionKeyCountShifts);
  }
  void      increment_key_count() ALWAYS_INLINE {
    ASSERT_ND(is_locked());
    data_ += (1ULL << kPageVersionKeyCountShifts);
  }
  void      set_key_count(uint8_t key_count) ALWAYS_INLINE {
    ASSERT_ND(is_locked());
    data_ = (data_ & (~static_cast<uint64_t>(kPageVersionKeyCountMask)))
            | (static_cast<uint64_t>(key_count) << kPageVersionKeyCountShifts);
    ASSERT_ND(get_key_count() == key_count);
  }

  void      set_splitting() ALWAYS_INLINE {
    ASSERT_ND(is_locked());
    data_ |= kPageVersionSplittingBit;
  }
  void      set_has_foster_child(bool has) ALWAYS_INLINE {
    ASSERT_ND(is_locked());
    if (has) {
      data_ |= kPageVersionHasFosterChildBit;
    } else {
      data_ &= ~kPageVersionHasFosterChildBit;
    }
  }

  /**
  * @brief Spins until we observe a non-inserting and non-splitting version.
  * @return version of this page that wasn't during modification.
  */
  PageVersion stable_version() const ALWAYS_INLINE;

  /**
  * @brief Locks the page, spinning if necessary.
  * @details
  * After taking lock, you might want to additionally set inserting/splitting bits.
  * Those can be done just as a usual write once you get a lock.
  */
  void lock_version() ALWAYS_INLINE;

  /**
  * @brief Unlocks the given page version, assuming the caller has locked it.
  * @pre page_version_ & kPageVersionLockedBit (we must have locked it)
  * @pre this thread locked it (can't check it, but this is the rule)
  * @details
  * This method also takes fences before/after unlock to make it safe.
  */
  void unlock_version() ALWAYS_INLINE;

  friend std::ostream& operator<<(std::ostream& o, const PageVersion& v);

  uint64_t data_;
};
STATIC_SIZE_CHECK(sizeof(PageVersion), 8)

/**
 * @brief Just a marker to denote that a memory region represents a data page.
 * @ingroup STORAGE
 * @details
 * Each storage page class contains this at the beginning to declare common properties.
 * In other words, we can always reinterpret_cast a page pointer to this object and
 * get/set basic properties.
 */
struct PageHeader CXX11_FINAL {
  /**
   * @brief Page ID of this page.
   * @details
   * If this page is a snapshot page, it stores SnapshotPagePointer.
   * If this page is a volatile page, it stores VolatilePagePointer (only numa_node/offset matters).
   */
  uint64_t      page_id_;     // +8 -> 8

  /**
   * ID of the storage this page belongs to.
   */
  StorageId     storage_id_;  // +4 -> 12

  /**
   * Checksum of the content of this page to detect corrupted pages.
   * Changes only when this page becomes a snapshot page.
   */
  Checksum      checksum_;    // +4 -> 16

  /** Actually of PageType. */
  uint8_t       page_type_;   // +1 -> 17

  /**
   * Whether this page image is of a snapshot page.
   * This is one of the properties that don't have permanent meaning.
   */
  bool          snapshot_;    // +1 -> 18

  /**
   * Whether this page is a root page, which exists only one per storage and has no parent
   * pointer. (some storage types looks like having multiple root pages, but actually
   * it has only one root-of-root even in that case.)
   */
  bool          root_;        // +1 -> 19

  /**
   * Which node's thread has updated this page most recently.
   * This is one of the properties that don't have permanent meaning.
   * We don't maintain this property transactionally. This is used only as a statistics for
   * partitioning.
   */
  thread::ThreadGroupId stat_latest_modifier_;  // +1 -> 20

  /** Same above. When the modification happened. We use this to keep hot volatile pages. */
  Epoch         stat_latest_modify_epoch_;  // +4 -> 24

  /**
   * Used in several storage types as concurrency control mechanism for the page.
   */
  PageVersion   page_version_;   // +8  -> 32

  // No instantiation.
  PageHeader() CXX11_FUNC_DELETE;
  PageHeader(const PageHeader& other) CXX11_FUNC_DELETE;
  PageHeader& operator=(const PageHeader& other) CXX11_FUNC_DELETE;

  PageType get_page_type() const { return static_cast<PageType>(page_type_); }

  inline void init_volatile(
    VolatilePagePointer page_id,
    StorageId storage_id,
    PageType page_type,
    bool root) ALWAYS_INLINE {
    page_id_ = page_id.word;
    storage_id_ = storage_id;
    checksum_ = 0;
    page_type_ = page_type;
    snapshot_ = false;
    root_ = root;
    stat_latest_modifier_ = 0;
    stat_latest_modify_epoch_ = Epoch();
    page_version_.data_ = 0;
  }

  inline void init_snapshot(
    SnapshotPagePointer page_id,
    StorageId storage_id,
    PageType page_type,
    bool root) ALWAYS_INLINE {
    page_id_ = page_id;
    storage_id_ = storage_id;
    checksum_ = 0;
    page_type_ = page_type;
    snapshot_ = true;
    root_ = root;
    stat_latest_modifier_ = 0;
    stat_latest_modify_epoch_ = Epoch();
    page_version_.data_ = 0;
  }
};

/**
 * @brief Just a marker to denote that the memory region represents a data page.
 * @ingroup STORAGE
 * @details
 * We don't instantiate this object nor derive from this. This is just a marker.
 * Because derived page objects have more header properties and even the data_ is layed out
 * differently. We thus make everything private to prevent misuse.
 * @attention Remember, anyway we don't have RTTI for data pages. They are just byte arrays used
 * with reinterpret_cast.
 */
struct Page CXX11_FINAL {
  /** At least the basic header exists in all pages. */
  PageHeader&  get_header()              { return header_; }
  const PageHeader&  get_header() const  { return header_; }

 private:
  PageHeader  header_;
  char        data_[kPageSize - sizeof(PageHeader)];

  // No instantiation.

  Page() CXX11_FUNC_DELETE;
  Page(const Page& other) CXX11_FUNC_DELETE;
  Page& operator=(const Page& other) CXX11_FUNC_DELETE;
};

/**
 * @brief Callback interface to initialize a volatile page.
 * @ingroup STORAGE
 * @details
 * This is used when a method might initialize a volatile page (eg following a page pointer).
 * Page initialization depends on page type and some of them needs additional parameters,
 * so we made it this object. One virtual function call overhead, but more flexible and no
 * need for lambda.
 */
struct VolatilePageInitializer {
  VolatilePageInitializer(StorageId storage_id, PageType page_type, bool root)
    : storage_id_(storage_id), page_type_(page_type), root_(root) {
  }
  // no virtual destructor for better performance. make sure derived class doesn't need
  // any explicit destruction.

  inline void initialize(Page* page, VolatilePagePointer page_id) const ALWAYS_INLINE {
    std::memset(page, 0, kPageSize);
    page->get_header().init_volatile(page_id, storage_id_, page_type_, root_);
    initialize_more(page);
  }

  /** Implement this in derived class to do additional initialization. */
  virtual void initialize_more(Page* page) const = 0;

  const StorageId storage_id_;
  const PageType  page_type_;
  const bool      root_;
};

/**
 * @brief Empty implementation of VolatilePageInitializer.
 * @ingroup STORAGE
 * @details
 * Use this if new page is never created (tolerate_null_page).
 */
struct DummyVolatilePageInitializer CXX11_FINAL : public VolatilePageInitializer {
  DummyVolatilePageInitializer()
    : VolatilePageInitializer(0, kUnknownPageType, true) {
  }
  void initialize_more(Page* /*page*/) const CXX11_OVERRIDE {}
};

const DummyVolatilePageInitializer kDummyPageInitializer;



inline PageVersion PageVersion::stable_version() const {
  assorted::memory_fence_acquire();
  SPINLOCK_WHILE(true) {
    uint64_t ver = data_;
    if ((ver & (kPageVersionInsertingBit | kPageVersionSplittingBit)) == 0) {
      return PageVersion(ver);
    } else {
      assorted::memory_fence_acquire();
    }
  }
}

inline void PageVersion::lock_version() {
  SPINLOCK_WHILE(true) {
    uint64_t ver = data_;
    if (ver & kPageVersionLockedBit) {
      continue;
    }
    uint64_t new_ver = ver | kPageVersionLockedBit;
    if (assorted::raw_atomic_compare_exchange_strong<uint64_t>(&data_, &ver, new_ver)) {
      ASSERT_ND(data_ & kPageVersionLockedBit);
      return;
    }
  }
}

inline void PageVersion::unlock_version() {
  uint64_t page_version = data_;
  ASSERT_ND(page_version & kPageVersionLockedBit);
  uint64_t base = page_version & kPageVersionUnlockMask;
  uint64_t insertion_counter = page_version & kPageVersionInsertionCounterMask;
  if (page_version & kPageVersionInsertingBit) {
    insertion_counter += (1ULL << kPageVersionInsertionCounterShifts);
  }
  uint64_t split_counter = page_version & kPageVersionSplitCounterMask;
  if (page_version & kPageVersionSplittingBit) {
    split_counter += (1ULL << kPageVersionSplitCounterShifts);
  }
  ASSERT_ND((insertion_counter & split_counter) == 0);
  assorted::memory_fence_release();
  data_ = base | insertion_counter | split_counter;
  assorted::memory_fence_release();
}

}  // namespace storage
}  // namespace foedus
#endif  // FOEDUS_STORAGE_PAGE_HPP_
