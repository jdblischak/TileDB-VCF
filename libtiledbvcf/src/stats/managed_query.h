/**
 * @file   managed_query.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 *   This declares the managed query API
 */

#ifndef MANAGED_QUERY_H
#define MANAGED_QUERY_H

#include <stdexcept>  // for windows: error C2039: 'runtime_error': is not a member of 'std'
#include <unordered_set>

#include <tiledb/tiledb>

#include "array_buffers.h"
#include "utils/logger_public.h"

namespace tiledb::vcf {

using namespace tiledb;

class ManagedQuery {
 public:
  //===================================================================
  //= public non-static
  //===================================================================

  /**
   * @brief Construct a new Managed Query object
   *
   * @param array TileDB array
   * @param name Name of the array
   * @param sparse_layout Layout of read results for sparse arrays
   */
  ManagedQuery(
      std::shared_ptr<Array> array,
      std::string_view name = "unnamed",
      tiledb_layout_t sparse_layout = TILEDB_UNORDERED);

  ManagedQuery() = delete;
  ManagedQuery(const ManagedQuery&) = delete;
  ManagedQuery(ManagedQuery&&) = default;
  ~ManagedQuery() = default;

  /**
   * @brief Select columns names to query (dim and attr). If the
   * `if_not_empty` parameter is `true`, the column will be selected iff the
   * list of selected columns is empty. This prevents a `select_columns` call
   * from changing an empty list (all columns) to a subset of columns.
   *
   * @param names Vector of column names
   * @param if_not_empty Prevent changing an "empty" selection of all columns
   */
  void select_columns(
      const std::vector<std::string>& names, bool if_not_empty = false);

  /**
   * @brief Select dimension ranges to query.
   *
   * @tparam T Dimension type
   * @param dim Dimension name
   * @param ranges Vector of dimension ranges
   */
  template <typename T>
  void select_ranges(
      const std::string& dim, const std::vector<std::pair<T, T>>& ranges) {
    for (auto& [start, stop] : ranges) {
      subarray_->add_range(dim, start, stop);
      subarray_range_set_ = true;
    }
  }

  /**
   * @brief Select dimension points to query.
   *
   * @tparam T Dimension type
   * @param dim Dimension name
   * @param points Vector of dimension points
   */
  template <typename T>
  void select_points(const std::string& dim, const std::vector<T>& points) {
    for (auto& point : points) {
      subarray_->add_range(dim, point, point);
      subarray_range_set_ = true;
    }
  }

  /**
   * @brief Select dimension points to query.
   *
   * @tparam T Dimension type
   * @param dim Dimension name
   * @param points Vector of dimension points
   */
  template <typename T>
  void select_points(const std::string& dim, const std::span<T> points) {
    for (auto& point : points) {
      subarray_->add_range(dim, point, point);
      subarray_range_set_ = true;
    }
  }

  /**
   * @brief Select dimension point to query.
   *
   * @tparam T Dimension type
   * @param dim Dimension name
   * @param point Dimension points
   */
  template <typename T>
  void select_point(const std::string& dim, const T& point) {
    subarray_->add_range(dim, point, point);
    subarray_range_set_ = true;
  }

  /**
   * @brief Set a query condition.
   *
   * @param qc A TileDB QueryCondition
   */
  void set_condition(const QueryCondition& qc) {
    query_->set_condition(qc);
  }

  /**
   * @brief Set query result order (layout).
   *
   * @param layout A tiledb_layout_t constant
   */
  void set_layout(tiledb_layout_t layout) {
    query_->set_layout(layout);
  }

  /**
   * @brief Set column data for write query.
   *
   * @param column_name Column name
   * @param column_buffer Column buffer
   */
  void set_column_data(
      std::string column_name, std::shared_ptr<ColumnBuffer> column_buffer) {
    if (array_->schema().array_type() == TILEDB_SPARSE ||
        schema_->has_attribute(column_name)) {
      // Add data buffer
      auto data = column_buffer->data<std::byte>();
      query_->set_data_buffer(
          column_name, (void*)data.data(), data.size_bytes());

      // Add offsets buffer, if variable length
      if (column_buffer->is_var()) {
        auto offsets = column_buffer->offsets();
        query_->set_offsets_buffer(column_name, offsets.data(), offsets.size());
      }

      // Add validity buffer, if nullable
      if (column_buffer->is_nullable()) {
        query_->set_validity_buffer(
            column_name,
            column_buffer->validity().data(),
            column_buffer->validity().size());
      }
    }
  }

  /**
   * @brief Submit the read query.
   *
   */
  void submit();

  /**
   * @brief Submit the write query.
   *
   */
  void submit_write() {
    query_->submit();
  }

  /**
   * @brief Return the query status.
   *
   * @return Query::Status Query status
   */
  Query::Status status() {
    return query_->query_status();
  }

  /**
   * @brief Check if the query is complete.
   *
   * @return true Query status is COMPLETE
   */
  bool is_complete() {
    return query_->query_status() == Query::Status::COMPLETE;
  }

  /**
   * @brief Return true if the query result buffers hold all results from the
   * query. The return value is false if the query was incomplete.
   *
   * @return true The buffers hold all results from the query.
   */
  bool results_complete() {
    return is_complete() && results_complete_;
  }

  /**
   * @brief Returns the total number of cells read so far, including any
   * previous incomplete queries.
   *
   * @return size_t Total number of cells read
   */
  size_t total_num_cells() {
    return total_num_cells_;
  }

  /**
   * @brief Return a view of data in column `name`.
   *
   * @tparam T Data type
   * @param name Column name
   * @return std::span<T> Data view
   */
  template <typename T>
  std::span<T> data(const std::string& name) {
    check_column_name(name);
    return buffers_->at(name)->data<T>();
  }

  /**
   * @brief Return a vector of strings from the column `name`.
   *
   * @param name Column name
   * @return std::vector<std::string> Strings
   */
  std::vector<std::string> strings(const std::string& name) {
    check_column_name(name);
    return buffers_->at(name)->strings();
  }

  /**
   * @brief Return a string_view of the string at the provided cell index from
   * column `name`.
   *
   * @param name Column name
   * @param index Cell index
   * @return std::string_view String view
   */
  std::string_view string_view(const std::string& name, uint64_t index) {
    check_column_name(name);
    return buffers_->at(name)->string_view(index);
  }

  /**
   * @brief Return results from the query.
   *
   * @return std::shared_ptr<ArrayBuffers>
   */
  std::shared_ptr<ArrayBuffers> results();

  /**
   * @brief Get the schema of the array.
   *
   * @return std::shared_ptr<ArraySchema> Schema
   */
  std::shared_ptr<ArraySchema> schema() {
    return schema_;
  }

  /**
   * @brief Finalize the query.
   *
   */
  void finalize() {
    query_->finalize();
  }

 private:
  //===================================================================
  //= private non-static
  //===================================================================

  /**
   * @brief Check if column name is contained in the query results.
   *
   * @param name Column name
   */
  void check_column_name(const std::string& name) {
    if (!buffers_->contains(name)) {
      throw std::runtime_error(fmt::format(
          "[ManagedQuery] Column '{}' is not available in the query "
          "results.",
          name));
    }
  }

  // TileDB array being queried.
  std::shared_ptr<Array> array_;

  // Name displayed in log messages
  std::string name_;

  // Array schema
  std::shared_ptr<ArraySchema> schema_;

  // TileDB query being managed.
  std::unique_ptr<Query> query_;

  // TileDB subarray containing the ranges for slicing.
  std::unique_ptr<Subarray> subarray_;

  // True if a range has been added to the subarray
  bool subarray_range_set_ = false;

  // Set of column names to read (dim and attr). If empty, query all columns.
  std::vector<std::string> columns_;

  // Results in the buffers are complete (the query was never incomplete)
  bool results_complete_ = true;

  // Total number of cells read by the query
  size_t total_num_cells_ = 0;

  // A collection of ColumnBuffers attached to the query
  std::shared_ptr<ArrayBuffers> buffers_;

  // True if the query has been submitted and the results have not been read
  bool query_submitted_ = false;
};

};  // namespace tiledb::vcf

#endif
