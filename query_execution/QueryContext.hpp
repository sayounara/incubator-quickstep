/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

#ifndef QUICKSTEP_QUERY_EXECUTION_QUERY_CONTEXT_HPP_
#define QUICKSTEP_QUERY_EXECUTION_QUERY_CONTEXT_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "catalog/CatalogTypedefs.hpp"
#include "expressions/predicate/Predicate.hpp"
#include "expressions/scalar/Scalar.hpp"
#include "expressions/table_generator/GeneratorFunctionHandle.hpp"
#include "storage/AggregationOperationState.hpp"
#include "storage/HashTable.hpp"
#include "storage/InsertDestination.hpp"
#include "storage/WindowAggregationOperationState.hpp"
#include "types/containers/Tuple.hpp"
#include "utility/BloomFilter.hpp"
#include "utility/Macros.hpp"
#include "utility/SortConfiguration.hpp"

#include "glog/logging.h"

#include "tmb/id_typedefs.h"

namespace tmb { class MessageBus; }

namespace quickstep {

class CatalogDatabaseLite;
class StorageManager;

namespace serialization { class QueryContext; }

/** \addtogroup QueryExecution
 *  @{
 */

/**
 * @brief The QueryContext stores stateful execution info per query.
 **/
class QueryContext {
 public:
  /**
   * @brief A unique identifier for an AggregationOperationState per query.
   **/
  typedef std::uint32_t aggregation_state_id;

  /**
   * @brief A unique identifier for a BloomFilter per query.
   **/
  typedef std::uint32_t bloom_filter_id;

  /**
   * @brief A unique identifier for a GeneratorFunctionHandle per query.
   **/
  typedef std::uint32_t generator_function_id;

  /**
   * @brief A unique identifier for an InsertDestination per query.
   *
   * @note A negative value indicates a nonexistent InsertDestination.
   **/
  typedef std::int32_t insert_destination_id;
  static constexpr insert_destination_id kInvalidInsertDestinationId = static_cast<insert_destination_id>(-1);

  /**
   * @brief A unique identifier for a JoinHashTable per query.
   **/
  typedef std::uint32_t join_hash_table_id;

  /**
   * @brief A unique identifier for a Predicate per query.
   *
   * @note A negative value indicates a null Predicate.
   **/
  typedef std::int32_t predicate_id;
  static constexpr predicate_id kInvalidPredicateId = static_cast<predicate_id>(-1);

  /**
   * @brief A unique identifier for a group of Scalars per query.
   *
   * @note A negative value indicates a nonexistent ScalarGroup.
   **/
  typedef std::int32_t scalar_group_id;
  static constexpr scalar_group_id kInvalidScalarGroupId = static_cast<scalar_group_id>(-1);

  /**
   * @brief A unique identifier for a SortConfiguration per query.
   **/
  typedef std::uint32_t sort_config_id;

  /**
   * @brief A unique identifier for a Tuple to be inserted per query.
   **/
  typedef std::uint32_t tuple_id;

  /**
   * @brief A unique identifier for a group of UpdateAssignments per query.
   **/
  typedef std::uint32_t update_group_id;

  /**
   * @brief A unique identifier for a window aggregation state.
   **/
  typedef std::uint32_t window_aggregation_state_id;

  /**
   * @brief Constructor.
   *
   * @param proto A serialized Protocol Buffer representation of a
   *        QueryContext, originally generated by the optimizer.
   * @param database The Database to resolve relation and attribute references
   *        in.
   * @param storage_manager The StorageManager to use.
   * @param scheduler_client_id The TMB client ID of the scheduler thread.
   * @param bus A pointer to the TMB.
   **/
  QueryContext(const serialization::QueryContext &proto,
               const CatalogDatabaseLite &database,
               StorageManager *storage_manager,
               const tmb::client_id scheduler_client_id,
               tmb::MessageBus *bus);

  ~QueryContext() {}

  /**
   * @brief Check whether a serialization::QueryContext is fully-formed and
   *        all parts are valid.
   *
   * @param proto A serialized Protocol Buffer representation of a QueryContext,
   *        originally generated by the optimizer.
   * @param database The Database to resolve relation and attribute references
   *        in.
   * @return Whether proto is fully-formed and valid.
   **/
  static bool ProtoIsValid(const serialization::QueryContext &proto,
                           const CatalogDatabaseLite &database);

  /**
   * @brief Whether the given AggregationOperationState id is valid.
   *
   * @param id The AggregationOperationState id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidAggregationStateId(const aggregation_state_id id) const {
    return id < aggregation_states_.size();
  }

  /**
   * @brief Get the AggregationOperationState.
   *
   * @param id The AggregationOperationState id in the query.
   *
   * @return The AggregationOperationState, alreadly created in the constructor.
   **/
  inline AggregationOperationState* getAggregationState(const aggregation_state_id id) {
    DCHECK_LT(id, aggregation_states_.size());
    DCHECK(aggregation_states_[id]);
    return aggregation_states_[id].get();
  }

  /**
   * @brief Release the given AggregationOperationState.
   *
   * @param id The id of the AggregationOperationState to release.
   *
   * @return The AggregationOperationState, alreadly created in the constructor.
   **/
  inline AggregationOperationState* releaseAggregationState(const aggregation_state_id id) {
    DCHECK_LT(id, aggregation_states_.size());
    DCHECK(aggregation_states_[id]);
    return aggregation_states_[id].release();
  }

  /**
   * @brief Destroy the given aggregation state.
   *
   * @param id The ID of the AggregationOperationState to destroy.
   **/
  inline void destroyAggregationState(const aggregation_state_id id) {
    DCHECK_LT(id, aggregation_states_.size());
    DCHECK(aggregation_states_[id]);
    aggregation_states_[id].reset(nullptr);
  }

  /**
   * @brief Whether the given BloomFilter id is valid.
   *
   * @param id The BloomFilter id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidBloomFilterId(const bloom_filter_id id) const {
    return id < bloom_filters_.size();
  }

  /**
   * @brief Get a mutable reference to the BloomFilter.
   *
   * @param id The BloomFilter id.
   *
   * @return The BloomFilter, already created in the constructor.
   **/
  inline BloomFilter* getBloomFilterMutable(const bloom_filter_id id) {
    DCHECK_LT(id, bloom_filters_.size());
    return bloom_filters_[id].get();
  }

  /**
   * @brief Get a constant pointer to the BloomFilter.
   *
   * @param id The BloomFilter id.
   *
   * @return The constant pointer to BloomFilter that is
   *         already created in the constructor.
   **/
  inline const BloomFilter* getBloomFilter(const bloom_filter_id id) const {
    DCHECK_LT(id, bloom_filters_.size());
    return bloom_filters_[id].get();
  }

  /**
   * @brief Destory the given BloomFilter.
   *
   * @param id The id of the BloomFilter to destroy.
   **/
  inline void destroyBloomFilter(const bloom_filter_id id) {
    DCHECK_LT(id, bloom_filters_.size());
    bloom_filters_[id].reset();
  }

  /**
   * @brief Whether the given GeneratorFunctionHandle id is valid.
   *
   * @param id The GeneratorFunctionHandle id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidGeneratorFunctionId(const generator_function_id id) const {
    return id < generator_functions_.size();
  }

  /**
   * @brief Get the GeneratorFunctionHandle.
   *
   * @param id The GeneratorFunctionHandle id in the query.
   *
   * @return The GeneratorFunctionHandle, alreadly created in the constructor.
   **/
  inline const GeneratorFunctionHandle& getGeneratorFunctionHandle(
      const generator_function_id id) {
    DCHECK_LT(static_cast<std::size_t>(id), generator_functions_.size());
    return *generator_functions_[id];
  }

  /**
   * @brief Whether the given InsertDestination id is valid.
   *
   * @param id The InsertDestination id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidInsertDestinationId(const insert_destination_id id) const {
    return id != kInvalidInsertDestinationId
        && id >= 0
        && static_cast<std::size_t>(id) < insert_destinations_.size();
  }

  /**
   * @brief Get the InsertDestination.
   *
   * @param id The InsertDestination id in the query.
   *
   * @return The InsertDestination, alreadly created in the constructor.
   **/
  inline InsertDestination* getInsertDestination(const insert_destination_id id) {
    DCHECK_GE(id, 0);
    DCHECK_LT(static_cast<std::size_t>(id), insert_destinations_.size());
    return insert_destinations_[id].get();
  }

  /**
   * @brief Destory the given InsertDestination.
   *
   * @param id The id of the InsertDestination to destroy.
   **/
  inline void destroyInsertDestination(const insert_destination_id id) {
    DCHECK_GE(id, 0);
    DCHECK_LT(static_cast<std::size_t>(id), insert_destinations_.size());
    insert_destinations_[id].reset();
  }

  /**
   * @brief Whether the given JoinHashTable id is valid.
   *
   * @param id The JoinHashTable id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidJoinHashTableId(const join_hash_table_id id) const {
    return id < join_hash_tables_.size();
  }

  /**
   * @brief Get the JoinHashTable.
   *
   * @param id The JoinHashTable id in the query.
   *
   * @return The JoinHashTable, already created in the constructor.
   **/
  inline JoinHashTable* getJoinHashTable(const join_hash_table_id id) {
    DCHECK_LT(id, join_hash_tables_.size());
    return join_hash_tables_[id].get();
  }

  /**
   * @brief Destory the given JoinHashTable.
   *
   * @param id The id of the JoinHashTable to destroy.
   **/
  inline void destroyJoinHashTable(const join_hash_table_id id) {
    DCHECK_LT(id, join_hash_tables_.size());
    join_hash_tables_[id].reset();
  }

  /**
   * @brief Whether the given Predicate id is valid or no predicate.
   *
   * @param id The Predicate id.
   *
   * @return True if valid or no predicate, otherwise false.
   **/
  bool isValidPredicate(const predicate_id id) const {
    return (id == kInvalidPredicateId)  // No predicate.
        || (id >= 0 && static_cast<std::size_t>(id) < predicates_.size());
  }

  /**
   * @brief Get the const Predicate.
   *
   * @param id The Predicate id in the query.
   *
   * @return The const Predicate (alreadly created in the constructor), or
   *         nullptr for the given invalid id.
   **/
  inline const Predicate* getPredicate(const predicate_id id) {
    if (id == kInvalidPredicateId) {
      return nullptr;
    }

    DCHECK_GE(id, 0);
    DCHECK_LT(static_cast<std::size_t>(id), predicates_.size());
    return predicates_[id].get();
  }

  /**
   * @brief Whether the given Scalar group id is valid.
   *
   * @param id The Scalar group id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidScalarGroupId(const scalar_group_id id) const {
    return id != kInvalidScalarGroupId
        && id >= 0
        && static_cast<std::size_t>(id) < scalar_groups_.size();
  }

  /**
   * @brief Get the group of Scalars.
   *
   * @param id The Scalar group id in the query.
   *
   * @return The reference to the Scalar group, alreadly created in the
   *         constructor.
   **/
  inline const std::vector<std::unique_ptr<const Scalar>>& getScalarGroup(const scalar_group_id id) {
    DCHECK_GE(id, 0);
    DCHECK_LT(static_cast<std::size_t>(id), scalar_groups_.size());
    return scalar_groups_[id];
  }

 /**
   * @brief Whether the given SortConfiguration id is valid.
   *
   * @param id The SortConfiguration id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidSortConfigId(const sort_config_id id) const {
    return id < sort_configs_.size();
  }

  /**
   * @brief Get the SortConfiguration.
   *
   * @param id The SortConfiguration id in the query.
   *
   * @return The SortConfiguration, alreadly created in the constructor.
   **/
  inline const SortConfiguration& getSortConfig(const sort_config_id id) {
    DCHECK_LT(id, sort_configs_.size());
    return *sort_configs_[id];
  }

  /**
   * @brief Whether the given Tuple id is valid.
   *
   * @param id The Tuple id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidTupleId(const tuple_id id) const {
    return id < tuples_.size();
  }

  /**
   * @brief Release the ownership of the Tuple referenced by the id.
   *
   * @note Each id should use only once.
   *
   * @param id The Tuple id in the query.
   *
   * @return The Tuple, alreadly created in the constructor.
   **/
  inline Tuple* releaseTuple(const tuple_id id) {
    DCHECK_LT(id, tuples_.size());
    DCHECK(tuples_[id]);
    return tuples_[id].release();
  }

  /**
   * @brief Whether the given update assignments group id is valid.
   *
   * @param id The group id of the update assignments.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidUpdateGroupId(const update_group_id id) const {
    return static_cast<std::size_t>(id) < update_groups_.size();
  }

  /**
   * @brief Get the group of the update assignments for UpdateWorkOrder.
   *
   * @param id The group id of the update assignments in the query.
   *
   * @return The reference to the update assignments group, alreadly created in the
   *         constructor.
   **/
  inline const std::unordered_map<attribute_id, std::unique_ptr<const Scalar>>& getUpdateGroup(
      const update_group_id id) {
    DCHECK_LT(static_cast<std::size_t>(id), update_groups_.size());
    DCHECK(!update_groups_[id].empty());
    return update_groups_[id];
  }

  /**
   * @brief Whether the given WindowAggregationOperationState id is valid.
   *
   * @param id The WindowAggregationOperationState id.
   *
   * @return True if valid, otherwise false.
   **/
  bool isValidWindowAggregationStateId(const window_aggregation_state_id id) const {
    return id < window_aggregation_states_.size();
  }

  /**
   * @brief Get the WindowAggregationOperationState.
   *
   * @param id The WindowAggregationOperationState id in the query.
   *
   * @return The WindowAggregationOperationState, already created in the
   *         constructor.
   **/
  inline WindowAggregationOperationState* getWindowAggregationState(
      const window_aggregation_state_id id) {
    DCHECK_LT(id, window_aggregation_states_.size());
    DCHECK(window_aggregation_states_[id]);
    return window_aggregation_states_[id].get();
  }

  /**
   * @brief Release the given WindowAggregationOperationState.
   *
   * @param id The id of the WindowAggregationOperationState to destroy.
   *
   * @return The WindowAggregationOperationState, already created in the
   *         constructor.
   **/
  inline WindowAggregationOperationState* releaseWindowAggregationState(
      const window_aggregation_state_id id) {
    DCHECK_LT(id, window_aggregation_states_.size());
    DCHECK(window_aggregation_states_[id]);
    return window_aggregation_states_[id].release();
  }

 private:
  std::vector<std::unique_ptr<AggregationOperationState>> aggregation_states_;
  std::vector<std::unique_ptr<BloomFilter>> bloom_filters_;
  std::vector<std::unique_ptr<const GeneratorFunctionHandle>> generator_functions_;
  std::vector<std::unique_ptr<InsertDestination>> insert_destinations_;
  std::vector<std::unique_ptr<JoinHashTable>> join_hash_tables_;
  std::vector<std::unique_ptr<const Predicate>> predicates_;
  std::vector<std::vector<std::unique_ptr<const Scalar>>> scalar_groups_;
  std::vector<std::unique_ptr<const SortConfiguration>> sort_configs_;
  std::vector<std::unique_ptr<Tuple>> tuples_;
  std::vector<std::unordered_map<attribute_id, std::unique_ptr<const Scalar>>> update_groups_;
  std::vector<std::unique_ptr<WindowAggregationOperationState>> window_aggregation_states_;

  DISALLOW_COPY_AND_ASSIGN(QueryContext);
};

/** @} */

}  // namespace quickstep

#endif  // QUICKSTEP_QUERY_EXECUTION_QUERY_CONTEXT_HPP_
