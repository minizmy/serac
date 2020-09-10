// Copyright (c) 2019-2020, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include "common/boundary_condition_manager.hpp"

#include <algorithm>
#include <iterator>

#include "common/logger.hpp"

namespace serac {

void BoundaryConditionManager::addEssential(const std::set<int>& ess_bdr, serac::GeneralCoefficient ess_bdr_coef,
                                            FiniteElementState& state, const int component)
{
  auto num_attrs = state.mesh().bdr_attributes.Max();

  std::set<int> filtered_attrs;
  std::set_difference(ess_bdr.begin(), ess_bdr.end(), attrs_in_use_.begin(), attrs_in_use_.end(),
                      std::inserter(filtered_attrs, filtered_attrs.begin()));

  // Check if anything was removed
  if (filtered_attrs.size() < ess_bdr.size()) {
    SLIC_WARNING("Multiple definition of essential boundary! Using first definition given.");
  }

  BoundaryCondition bc(ess_bdr_coef, component, filtered_attrs, num_attrs);
  bc.setTrueDofs(state);
  ess_bdr_.emplace_back(std::move(bc));
  attrs_in_use_.insert(ess_bdr.begin(), ess_bdr.end());
  all_dofs_valid_ = false;
}

void BoundaryConditionManager::addNatural(const std::set<int>& nat_bdr, serac::GeneralCoefficient nat_bdr_coef,
                                          FiniteElementState& state, const int component)
{
  auto num_attrs = state.mesh().bdr_attributes.Max();
  nat_bdr_.emplace_back(nat_bdr_coef, component, nat_bdr, num_attrs);
  all_dofs_valid_ = false;
}

void BoundaryConditionManager::addGeneric(const std::set<int>& bdr_attr, serac::GeneralCoefficient bdr_coef,
                                          FiniteElementState& state, const std::string_view tag, const int component)
{
  auto num_attrs = state.mesh().bdr_attributes.Max();
  other_bdr_.emplace_back(bdr_coef, component, bdr_attr, num_attrs);
  other_bdr_.back().setTag(tag);
  all_dofs_valid_ = false;
}

void BoundaryConditionManager::addTrueDofs(const mfem::Array<int>& true_dofs, serac::GeneralCoefficient ess_bdr_coef,
                                           int component)
{
  ess_bdr_.emplace_back(ess_bdr_coef, component, true_dofs);
  all_dofs_valid_ = false;
}

void BoundaryConditionManager::updateAllDofs() const
{
  all_dofs_.DeleteAll();
  for (const auto& bc : ess_bdr_) {
    all_dofs_.Append(bc.getTrueDofs());
  }
  all_dofs_.Sort();
  all_dofs_.Unique();
  all_dofs_valid_ = true;
}

}  // namespace serac