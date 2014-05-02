/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#include <foedus/externalize/externalizable.hpp>
#include <foedus/savepoint/savepoint.hpp>
namespace foedus {
namespace savepoint {
Savepoint::Savepoint() {
}

ErrorStack Savepoint::load(tinyxml2::XMLElement* element) {
    EXTERNALIZE_LOAD_ELEMENT(element, current_epoch_);
    EXTERNALIZE_LOAD_ELEMENT(element, durable_epoch_);
    EXTERNALIZE_LOAD_ELEMENT(element, oldest_log_files_);
    EXTERNALIZE_LOAD_ELEMENT(element, oldest_log_files_offset_begin_);
    EXTERNALIZE_LOAD_ELEMENT(element, current_log_files_);
    EXTERNALIZE_LOAD_ELEMENT(element, current_log_files_offset_durable_);
    return RET_OK;
}

ErrorStack Savepoint::save(tinyxml2::XMLElement* element) const {
    CHECK_ERROR(insert_comment(element, "progress of the entire engine"));

    EXTERNALIZE_SAVE_ELEMENT(element, current_epoch_, "Current epoch of the entire engine.");
    EXTERNALIZE_SAVE_ELEMENT(element, durable_epoch_,
                             "Latest epoch whose logs were all flushed to disk");
    EXTERNALIZE_SAVE_ELEMENT(element, oldest_log_files_,
                             "Ordinal of the oldest active log file in each logger");
    EXTERNALIZE_SAVE_ELEMENT(element, oldest_log_files_offset_begin_,
                    "Indicates the inclusive beginning of active region in the oldest log file");
    EXTERNALIZE_SAVE_ELEMENT(element, current_log_files_,
                             "Indicates the log file each logger is currently appending to");
    EXTERNALIZE_SAVE_ELEMENT(element, current_log_files_offset_durable_,
                        "Indicates the exclusive end of durable region in the current log file");
    return RET_OK;
}

void Savepoint::populate_empty(log::LoggerId logger_count) {
    // Epoch-0 is reserved for invalid epoch value, so we start from ep-2.
    current_epoch_ = 2;  // The first epoch that might have transactions is ep-2.
    durable_epoch_ = 1;  // As there was no transaction in ep-1, this is valid
    oldest_log_files_.resize(logger_count, 0);
    oldest_log_files_offset_begin_.resize(logger_count, 0);
    current_log_files_.resize(logger_count, 0);
    current_log_files_offset_durable_.resize(logger_count, 0);
}

}  // namespace savepoint
}  // namespace foedus
