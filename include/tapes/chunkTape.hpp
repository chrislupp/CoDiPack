/**
 * CoDiPack, a Code Differentiation Package
 *
 * Copyright (C) 2015 Chair for Scientific Computing, TU Kaiserslautern
 *
 * This file is part of CoDiPack.
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
 * Authors: TODO
 */

#pragma once

#include <iostream>
#include <cstddef>
#include <tuple>

#include "activeReal.hpp"
#include "chunk.hpp"
#include "chunkVector.hpp"
#include "tapeInterface.hpp"

namespace codi {

  template <typename IndexType>
  struct ExpressionCounter {

    typedef IndexType Position;

    IndexType count;

    inline Position getPosition() {
      return count;
    }

    inline void reset(const Position& pos) {
      count = pos;
    }
  };

  template <typename Real, typename IndexType>
  class ChunkTape : public TapeInterface<Real, IndexType> {
  public:

    typedef Chunk2< Real, IndexType> DataChunk;
    typedef ChunkVector<DataChunk, ExpressionCounter<IndexType> > DataChunkVector;

    typedef Chunk1<OperationInt> OperatorChunk;
    typedef ChunkVector<OperatorChunk, DataChunkVector> OperatorChunkVector;

    typedef typename OperatorChunkVector::Position Position;

  private:

    ExpressionCounter<IndexType> expressionCount;
    DataChunkVector data;
    OperatorChunkVector operators;
    Real* adjoints;
    size_t adjointsSize;

    bool active;

  public:
    ChunkTape() :
      expressionCount(),
      data(DefaultChunkSize, expressionCount),
      operators(DefaultChunkSize, data),
      adjoints(NULL),
      active(false){
    }

    void setDataChunkSize(const size_t& dataChunkSize) {
      data.setChunkSize(dataChunkSize);
    }

    void setOperatorChunkSize(const size_t& opChunkSize) {
      operators.setChunkSize(opChunkSize);
    }

    void resize(const size_t& dataSize, const size_t& opSize) {
      data.resize(dataSize);
      operators.resize(opSize);
    }

    void resizeAdjoints(const size_t& size) {
      size_t oldSize = adjointsSize;
      adjointsSize = size;

      adjoints = (Real*)realloc(adjoints, sizeof(Real) * adjointsSize);

      for(size_t i = oldSize; i < adjointsSize; ++i) {
        adjoints[i] = 0.0;
      }
    }

    void allocateAdjoints() {
      resizeAdjoints(expressionCount + 1);
    }

    template<typename Rhs>
    inline void store(Real& lhsValue, IndexType& lhsIndex, const Rhs& rhs) {
      Real gradient; /* This value will not be used */

      ENABLE_CHECK (OptTapeActivity, active){
        data.reserveItems(ExpressionTraits<Rhs>::maxActiveVariables);
        operators.reserveItems(1); // operators needs a reserve bevor the data items for the operator are pushed
        /* first store the size of the current stack position and evaluate the
         rhs expression. If there was an active variable on the rhs, update
         the index of the lhs */
        size_t startSize = data.getChunkPosition();
        rhs.calcGradient(gradient);
        size_t activeVariables = data.getChunkPosition() - startSize;
        if(0 == activeVariables) {
          lhsIndex = 0;
        } else {
          operators.setDataAndMove(std::make_tuple((OperationInt)activeVariables));
          lhsIndex = ++expressionCount.count;
        }
      }
      /* now set the value of the lhs */
      lhsValue = rhs.getValue();
    }

    inline void store(Real& value, IndexType& lhsIndex, const ActiveReal<Real, SimpleTape<Real, IndexType> >& rhs) {
      ENABLE_CHECK (OptTapeActivity, active){
        lhsIndex = rhs.getGradientData();
      }
      value = rhs.getValue();
    }

    inline void store(Real& value, IndexType& lhsIndex, const typename TypeTraits<Real>::PassiveReal& rhs) {
      ENABLE_CHECK (OptTapeActivity, active){
        lhsIndex = 0;
      }
      value = rhs;
    }

    inline void pushJacobi(Real& /*gradient*/, const Real& /*value*/, const IndexType& index) {
      if(0 != index) {
        data.setDataAndMove(std::make_tuple(1.0, index));
      }
    }

    inline void pushJacobi(Real& /*gradient*/, const Real& jacobi, const Real& /*value*/, const IndexType& index) {
      if(0 != index) {
        ENABLE_CHECK(OptJacobiIsZero, 0.0 != jacobi) {
          data.setDataAndMove(std::make_tuple(jacobi, index));
        }
      }
    }

    inline void initGradientData(Real& /*value*/, IndexType& index) {
      index = 0;
    }

    inline void destroyGradientData(Real& /*value*/, IndexType& index) {
      /* nothing to do */
    }


    void setGradient(IndexType& index, const Real& gradient) {
      if(0 != index) {
        this->gradient(index) = gradient;
      }
    }

    inline Real getGradient(const IndexType& index) const {
      if(adjointsSize <= index) {
        return Real();
      } else {
        return adjoints[index];
      }
    }

    inline Real& gradient(IndexType& index) {
      if(adjointsSize <= index) {
        resizeAdjoints(index + 1);
      }

      return adjoints[index];
    }

    inline Position getPosition() {
      return operators.getPosition();
    }

    inline void clearAdjoints(){
      for(size_t i = 0; i <= operators.getUsedSize(); ++i) {
        adjoints.data[i] = 0.0;
      }
    }

    inline void reset(const Position& pos) {
      for(size_t i = pos.inner.inner; i <= expressionCount.count; ++i) {
        adjoints[i] = 0.0;
      }

      // reset will be done iterativly through the vectors
      operators.reset(pos);
    }

    inline void reset() {
      reset(Position());
    }

    inline void evaluate(const size_t& startAdjPos, const size_t& endAdjPos, size_t& opPos, OperationInt* &operators, size_t& dataPos, Real* &jacobies, IndexType* &indices) {
      size_t adjPos = startAdjPos;

      while(adjPos > endAdjPos) {
        const Real& adj = adjoints[adjPos];
        --adjPos;
        --opPos;
        const OperationInt& activeVariables = operators[opPos];
        ENABLE_CHECK(OptZeroAdjoint, adj != 0){
          for(OperationInt curVar = 0; curVar < activeVariables; ++curVar) {
            --dataPos;
            adjoints[indices[dataPos]] += adj * jacobies[dataPos];

          }
        } else {
          dataPos -= activeVariables;
        }
      }
    }

    inline void evaluateOp(const typename OperatorChunkVector::Position& start, const typename OperatorChunkVector::Position& end) {
      OperationInt* operatorData;
      size_t dataPos = start.data;
      typename DataChunkVector::Position curInnerPos = start.inner;
      for(size_t curChunk = start.chunk; curChunk > end.chunk; --curChunk) {
        std::tie(operatorData) = operators.getDataAtPosition(curChunk, 0);

        typename DataChunkVector::Position endInnerPos = operators.getInnerPosition(curChunk);
        evaluate(curInnerPos, endInnerPos, dataPos, operatorData);

        curInnerPos = endInnerPos;

        dataPos = operators.getChunkUsedData(curChunk - 1);
      }

      // Iterate over the reminder also covers the case if the start chunk and end chunk are the same
      std::tie(operatorData) = operators.getDataAtPosition(end.chunk, 0);
      evaluate(curInnerPos, end.inner, dataPos, operatorData);
    }

    inline void evaluate(const typename DataChunkVector::Position& start, const typename DataChunkVector::Position& end, size_t& opPos, OperationInt* &operatorData) {
      Real* jacobiData;
      IndexType* indexData;
      size_t dataPos = start.data;
      typename ExpressionCounter<IndexType>::Position curInnerPos = start.inner;
      for(size_t curChunk = start.chunk; curChunk > end.chunk; --curChunk) {
        std::tie(jacobiData, indexData) = data.getDataAtPosition(curChunk, 0);

        typename ExpressionCounter<IndexType>::Position endInnerPos = data.getInnerPosition(curChunk);
        evaluate(curInnerPos, endInnerPos, opPos, operatorData, dataPos, jacobiData, indexData);

        curInnerPos = endInnerPos;

        dataPos = data.getChunkUsedData(curChunk - 1);
      }

      // Iterate over the reminder also covers the case if the start chunk and end chunk are the same
      std::tie(jacobiData, indexData) = data.getDataAtPosition(end.chunk, 0);
      evaluate(curInnerPos, end.inner, opPos, operatorData, dataPos, jacobiData, indexData);
    }

    void evaluate(const Position& start, const Position& end) {
      if(adjointsSize <= expressionCount.count) {
        resizeAdjoints(expressionCount.count + 1);
      }

      evaluateOp(start, end);
    }

    void evaluate() {
      evaluate(getPosition(), Position());
    }

    inline void registerInput(ActiveReal<Real, ChunkTape<Real, IndexType> >& value) {
      operators.reserveItems(1);
      operators.setDataAndMove(std::make_tuple(0));

      value.getGradientData() = ++expressionCount.count;
    }

    inline void registerOutput(ActiveReal<Real, ChunkTape<Real, IndexType> >& /*value*/) {
      /* do nothing */
    }
    inline void setActive(){
      active = true;
    }

    inline void setPassive(){
      active = false;
    }

    inline bool isActive(){
      return active;
    }

  };
}
