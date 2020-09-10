// Copyright (c) 2019-2020, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file boundary_condition_manager.hpp
 *
 * @brief This file contains the declaration of the boundary condition manager class
 */

#ifndef BOUNDARY_CONDITION_MANAGER
#define BOUNDARY_CONDITION_MANAGER

#include <memory>
#include <set>

#include "common/boundary_condition.hpp"
#include "common/finite_element_state.hpp"
#include "common/serac_types.hpp"

namespace serac {

/**
 * @brief A "view" for filtering a container
 * @note Will be made obsolete by C++20
 * @see std::ranges::views::filter
 */
template <typename Iter, typename Pred>
class FilterView {
public:
  /**
   * @brief An iterator over a filtered view
   */
  class FilterViewIterator {
  public:
    /**
     * @brief Constructs a new iterator object
     * @param[in] curr The element in the container that should be initially "pointed to"
     * @param[in] end The element "one past the end" of the container
     * @param[in] pred The predicate to filter with
     */
    FilterViewIterator(Iter curr, Iter end, const Pred& pred) : curr_(curr), end_(end), pred_(pred) {}

    /**
     * @brief Advances the pointed-to container element to the next element that
     * satisfies the predicate
     */
    FilterViewIterator& operator++()
    {
      // Move forward once to advance the element, then continue
      // advancing until a predicate-satisfying element is found
      ++curr_;
      while ((curr_ != end_) && (!pred_(*curr_))) {
        ++curr_;
      }
      return *this;
    }

    /**
     * @brief Dereferences the iterator
     * @return A non-owning reference to the pointed-to element
     */
    const auto& operator*() const { return *curr_; }

    /**
     * @brief Comparison operation, checks for iterator inequality
     */
    bool operator!=(const FilterViewIterator& other) { return curr_ != other.curr_; }

  private:
    /**
     * @brief The currently pointed to element
     */
    Iter curr_;

    /**
     * @brief One past the last element of the container, used for bounds checking
     */
    Iter end_;

    /**
     * @brief A reference for the predicate to filter with
     */
    const Pred& pred_;
  };

  /**
   * @brief Constructs a new lazily-evaluated filtering view over a container
   * @param[in] begin The begin() iterator to the container
   * @param[in] end The end() iterator to the container
   * @param[in] pred The predicate for the filter
   */
  FilterView(Iter begin, Iter end, Pred&& pred) : begin_(begin), end_(end), pred_(std::move(pred))
  {
    // Skip to the first element that meets the predicate, making sure not to deref the "end" iterator
    while ((begin_ != end_) && (!pred_(*begin_))) {
      ++begin_;
    }
  }

  /**
   * @brief Returns the first filtered element, i.e., the first element in the
   * underlying container that satisfies the predicate
   */
  FilterViewIterator       begin() { return FilterViewIterator(begin_, end_, pred_); }
  const FilterViewIterator begin() const { return FilterViewIterator(begin_, end_, pred_); }

  /**
   * @brief Returns one past the end of the container, primarily for bounds-checking
   */
  FilterViewIterator       end() { return FilterViewIterator(end_, end_, pred_); }
  const FilterViewIterator end() const { return FilterViewIterator(end_, end_, pred_); }

private:
  /**
   * @brief begin() iterator to the underlying container
   */
  Iter begin_;

  /**
   * @brief end() iterator to the underlying container
   */
  Iter end_;

  /**
   * @brief Predicate to filter with
   */
  Pred pred_;
};

// Deduction guide - iterator and lambda types must be deduced, so
// this mitigates a "builder" function
template <class Iter, class Pred>
FilterView(Iter, Iter, Pred &&) -> FilterView<Iter, Pred>;

class BoundaryConditionManager {
public:
  /**
   * @brief Set the essential boundary conditions from a list of boundary markers and a coefficient
   *
   * @param[in] ess_bdr The set of essential BC attributes
   * @param[in] ess_bdr_coef The essential BC value coefficient
   * @param[in] state The finite element state to which the BC should be applied
   * @param[in] component The component to set (-1 implies all components are set)
   */
  void addEssential(const std::set<int>& ess_bdr, serac::GeneralCoefficient ess_bdr_coef, FiniteElementState& state,
                    const int component = -1);

  /**
   * @brief Set the natural boundary conditions from a list of boundary markers and a coefficient
   *
   * @param[in] nat_bdr The set of mesh attributes denoting a natural boundary
   * @param[in] nat_bdr_coef The coefficient defining the natural boundary function
   * @param[in] state The finite element state to which the BC should be applied
   * @param[in] component The component to set (-1 implies all components are set)
   */
  void addNatural(const std::set<int>& nat_bdr, serac::GeneralCoefficient nat_bdr_coef, FiniteElementState& state,
                  const int component = -1);

  /**
   * @brief Set a generic boundary condition from a list of boundary markers and a coefficient
   *
   * @param[in] bdr_attr The set of mesh attributes denoting a natural boundary
   * @param[in] bdr_coef The coefficient defining the natural boundary function
   * @param[in] state The finite element state to which the BC should be applied
   * @param[in] tag The tag for the generic boundary condition, for identification purposes
   * @param[in] component The component to set (-1 implies all components are set)
   */
  void addGeneric(const std::set<int>& bdr_attr, serac::GeneralCoefficient bdr_coef, FiniteElementState& state,
                  const std::string_view tag, const int component = -1);

  /**
   * @brief Set a list of true degrees of freedom from a coefficient
   *
   * @param[in] true_dofs The true degrees of freedom to set with a Dirichlet condition
   * @param[in] ess_bdr_coef The coefficient that evaluates to the Dirichlet condition
   * @param[in] component The component to set (-1 implies all components are set)
   */
  void addTrueDofs(const mfem::Array<int>& true_dofs, serac::GeneralCoefficient ess_bdr_coef, int component = -1);

  /**
   * @brief Returns all the degrees of freedom associated with all the essential BCs
   * @return A const reference to the list of DOF indices, without duplicates and sorted
   */
  const mfem::Array<int>& allDofs() const
  {
    if (!all_dofs_valid_) {
      updateAllDofs();
    }
    return all_dofs_;
  }

  /**
   * @brief Eliminates all essential BCs from a matrix
   * @param[inout] matrix The matrix to eliminate from, will be modified
   * @return The eliminated matrix entries
   * @note The sum of the eliminated matrix and the modified parameter is
   * equal to the initial state of the parameter
   */
  std::unique_ptr<mfem::HypreParMatrix> eliminateAllFromMatrix(mfem::HypreParMatrix& matrix) const
  {
    return std::unique_ptr<mfem::HypreParMatrix>(matrix.EliminateRowsCols(allDofs()));
  }

  /**
   * @brief Accessor for the essential BC objects
   */
  std::vector<BoundaryCondition>& essentials() { return ess_bdr_; }
  /**
   * @brief Accessor for the natural BC objects
   */
  std::vector<BoundaryCondition>& naturals() { return nat_bdr_; }
  /**
   * @brief Accessor for the generic BC objects
   */
  std::vector<BoundaryCondition>& generics() { return other_bdr_; }

  /**
   * @brief Accessor for the essential BC objects
   */
  const std::vector<BoundaryCondition>& essentials() const { return ess_bdr_; }
  /**
   * @brief Accessor for the natural BC objects
   */
  const std::vector<BoundaryCondition>& naturals() const { return nat_bdr_; }
  /**
   * @brief Accessor for the generic BC objects
   */
  const std::vector<BoundaryCondition>& generics() const { return other_bdr_; }

  const auto genericsWithTag(const std::string& tag)
  {
    return FilterView(other_bdr_.begin(), other_bdr_.end(), [tag](const auto& bc) { return bc.tag() == tag; });
  }

private:
  /**
   * @brief Updates the "cached" list of all DOF indices
   */
  void updateAllDofs() const;

  /**
   * @brief The vector of essential boundary conditions
   */
  std::vector<BoundaryCondition> ess_bdr_;

  /**
   * @brief The vector of natural boundary conditions
   */
  std::vector<BoundaryCondition> nat_bdr_;

  /**
   * @brief The vector of generic (not Dirichlet or Neumann) boundary conditions
   */
  std::vector<BoundaryCondition> other_bdr_;

  /**
   * @brief The set of boundary attributes associated with
   * already-registered BCs
   * @see https://mfem.org/mesh-formats/
   */
  std::set<int> attrs_in_use_;

  /**
   * @brief The set of true DOF indices corresponding
   * to all registered BCs
   */
  mutable mfem::Array<int> all_dofs_;

  /**
   * @brief Whether the set of stored total DOFs is valid
   */
  mutable bool all_dofs_valid_ = false;
};

}  // namespace serac

#endif