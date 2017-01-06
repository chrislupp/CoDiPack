/*
 * CoDiPack, a Code Differentiation Package
 *
 * Copyright (C) 2015 Chair for Scientific Computing (SciComp), TU Kaiserslautern
 * Homepage: http://www.scicomp.uni-kl.de
 * Contact:  Prof. Nicolas R. Gauger (codi@scicomp.uni-kl.de)
 *
 * Lead developers: Max Sagebaum, Tim Albring (SciComp, TU Kaiserslautern)
 *
 * This file is part of CoDiPack (http://www.scicomp.uni-kl.de/software/codi).
 *
 * CoDiPack is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * CoDiPack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU
 * General Public License along with CoDiPack.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Max Sagebaum, Tim Albring, (SciComp, TU Kaiserslautern)
 */

#pragma once

#include "../../configure.h"
#include "../../expressionHandle.hpp"
#include "../../typeTraits.hpp"

#include "handleFactoryInterface.hpp"

/**
 * @brief Global namespace for CoDiPack - Code Differentiation Package
 */
namespace codi {

  /**
   * @brief A factory for function handles, that use static objects to store the data for the function call.
   *
   * The static data of the expression is stored in a static object and a pointer to that
   * static object is returned as the handle.
   *
   * @tparam          Real  A calculation type that supports all mathematical operations.
   * @tparam     IndexType  An integer type that is used to identify the AD objects.
   * @tparam GradientValue  A value type that supports add and scaling operations.
   */
  template<typename Real, typename IndexType, typename GradientValue=Real>
  struct StaticObjectHandleFactory
    // final : public HandleFactoryInterface</* handle type */, Real, IndexType, GradientValue>
  {

    /** @brief The passive value of the Real type */
    typedef typename TypeTraits<Real>::PassiveReal PassiveReal;

    /**
     * @brief The type for handle that is used by this factory
     *
     * The handle is just a pointer to a static object.
     */
    typedef const ExpressionHandle<Real*, Real, IndexType>* Handle;

    /**
     * @brief Create the handle for the given tape and the given expression.
     *
     * @return The pointer to the static object
     *
     * @tparam Expr  The expression that performs the evaluation of the reverse AD operations.
     * @tparam Tape  The tape that is performing the reverse AD evaluation.
     */
    template<typename Expr, typename Tape>
    static CODI_INLINE Handle createHandle() {

      return ExpressionHandleStore<Real*, Real, IndexType, Expr>::getHandle();
    }

    /**
     * @brief The evaluation of the handle, that was created by this factory.
     *
     * The data from the static object is read and used to call the function.
     *
     * @tparam Tape  The tape that is performing the reverse AD evaluation.
     */
    template<typename Tape>
    static CODI_INLINE void callHandle(Handle handle, const GradientValue& adj, const StatementInt& passiveActives, size_t& indexPos, IndexType* &indices, size_t& constantPos, PassiveReal* &constants, Real* primalVector, GradientValue* adjoints) {

      Tape::evaluateHandle(handle->adjointFunc, handle->maxActiveVariables, handle->maxConstantVariables, adj, passiveActives, indexPos, indices, constantPos, constants, primalVector, adjoints);
    }
  };
}
