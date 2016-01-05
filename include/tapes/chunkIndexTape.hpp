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

#include <iostream>
#include <cstddef>
#include <tuple>

#include "../activeReal.hpp"
#include "chunk.hpp"
#include "chunkVector.hpp"
#include "indexHandler.hpp"
#include "externalFunctions.hpp"
#include "reverseTapeInterface.hpp"

/**
 * @brief Global namespace for CoDiPack - Code Differentiation Package
 */
namespace codi {

  /**
   * @brief Helper struct to define the nested chunk vectors for the ChunkIndexTape.
   *
   * See ChunkIndexTape for details.
   */
  template <typename Real, typename IndexType>
  struct ChunkIndexTapeTypes {
    /** @brief The data for each statement. */
    typedef Chunk2<StatementInt, IndexType> StatementChunk;
    /** @brief The chunk vector for the statement data. */
    typedef ChunkVector<StatementChunk, EmptyChunkVector> StatementChunkVector;

    /** @brief The data for the jacobies of each statement */
    typedef Chunk2< Real, IndexType> DataChunk;
    /** @brief The chunk vector for the jacobi data. */
    typedef ChunkVector<DataChunk, StatementChunkVector> DataChunkVector;

    /** @brief The data for the external functions. */
    typedef Chunk2<ExternalFunction,typename DataChunkVector::Position> ExternalFunctionChunk;
    /** @brief The chunk vector for the external  function data. */
    typedef ChunkVector<ExternalFunctionChunk, DataChunkVector> ExternalFunctionChunkVector;

    /** @brief The position for all the different data vectors. */
    typedef typename ExternalFunctionChunkVector::Position Position;

  };

  /**
   * @brief A tape which grows if more space is needed.
   *
   * The ChunkIndexTape implements a fully featured ReverseTapeInterface in a most
   * user friendly fashion. The storage vectors of the tape are grown if the
   * tape runs out of space.
   *
   * This is handled by a nested definition of multiple ChunkVectors which hold
   * the different data vectors. The current implementation uses 3 ChunkVectors
   * and one terminator. The relation is
   *
   * externalFunctions -> jacobiData -> statements
   *
   * The size of the tape can be set with the resize function,
   * the tape will allocate enough chunks such that the given data requirements will fit into the chunks.
   *
   * The tape also uses the index manager IndexHandler to reuse the indices that are deleted.
   * That means that ActiveReal's which use this tape need to be copied by usual means and deleted after
   * the are no longer used. No c-like memory operations like memset and memcpy should be applied
   * to these types.
   *
   * @tparam      Real  The floating point type used in the ActiveReal.
   * @tparam IndexType  The type for the indexing of the adjoint variables.
   */
  template <typename Real, typename IndexType>
  class ChunkIndexTape : public ReverseTapeInterface<Real, IndexType, ChunkIndexTape<Real, IndexType>, typename ChunkIndexTapeTypes<Real, IndexType>::Position > {
  public:

    /** @brief The data for each statement. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::StatementChunk StatementChunk;
    /** @brief The chunk vector for the statement data. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::StatementChunkVector StatementChunkVector;

    /** @brief The data for the jacobies of each statement */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::DataChunk DataChunk;
    /** @brief The chunk vector for the jacobi data. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::DataChunkVector DataChunkVector;

    /** @brief The data for the external functions. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::ExternalFunctionChunk ExternalFunctionChunk;
    /** @brief The chunk vector for the external  function data. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::ExternalFunctionChunkVector ExternalFunctionChunkVector;

    /** @brief The position for all the different data vectors. */
    typedef typename ChunkIndexTapeTypes<Real, IndexType>::Position Position;


  private:

    /** @brief The counter for the current expression. */
    EmptyChunkVector emptyVector;
    /** @brief The data for the statements. */
    StatementChunkVector statements;
    /** @brief The data for the jacobies of each statements. */
    DataChunkVector data;
    /** @brief The data for the external functions. */
    ExternalFunctionChunkVector externalFunctions;

    /**
     * @brief The adjoint vector.
     *
     * The size of the adjoint vector is set according to the requested positions.
     * But the positions should not be greater than the current expression counter.
     */
    Real* adjoints;

    /** @brief The current size of the adjoint vector. */
    IndexType adjointsSize;

    /**
     * @brief Determines if statements are recorded or ignored.
     */
    bool active;

    /**
     * @brief The index handler for the tape.
     *
     * It stores the deleted indices are reuses them.
     */
    IndexHandler<IndexType> indexHandler;

  public:
    /**
     * @brief Creates a tape with the default chunk sizes for the data, statements and
     * external functions defined in the configuration.
     */
    ChunkIndexTape() :
      emptyVector(),
      statements(DefaultChunkSize, emptyVector),
      data(DefaultChunkSize, statements),
      externalFunctions(1000, data),
      adjoints(NULL),
      adjointsSize(0),
      active(false){
    }

    /**
     * @brief Set the size of the jacobi data chunks.
     *
     * @param[in] dataChunkSize The new size for the jacobi data chunks.
     */
    void setDataChunkSize(const size_t& dataChunkSize) {
      data.setChunkSize(dataChunkSize);
    }

    /**
     * @brief Set the size of the statement data chunks.
     *
     * @param[in] statementChunkSize The new size for the statement data chunks.
     */
    void setStatementChunkSize(const size_t& statementChunkSize) {
      statements.setChunkSize(statementChunkSize);
    }

    /**
     * @brief Set the size of the external function data chunks.
     *
     * @param[in] extChunkSize The new size for the external function data chunks.
     */
    void setExternalFunctionChunkSize(const size_t& extChunkSize) {
      externalFunctions.setChunkSize(extChunkSize);
    }

    /**
     * @brief Set the size of the adjoint vector.
     *
     * @param[in] adjointsSize The new size for the adjoint vector.
     */
    void setAdjointsSize(const size_t& adjointsSize) {
      resizeAdjoints(adjointsSize);
    }

    /**
     * @brief Return the number of used statements.
     *
     * @return The number of used statements.
     */
    size_t getUsedStatementsSize() {
      return statements.getDataSize();
    }

    /**
     * @brief Return the number of used data entries.
     * @return The number of used data entries.
     */
    size_t getUsedDataEntriesSize() {
      return data.getDataSize();
    }

    /**
     * @brief Get the current size of the adjoint vector.
     *
     * @return The size of the current adjoint vector.
     */
    size_t getAdjointsSize() {
      return indexHandler.getMaximumGlobalIndex() + 1;
    }

    /**
     * @brief Set the size of the jacobi and statement data.
     *
     * The tape will allocate enough chunks such that the given data
     * sizes will fit into the chunk vectors.
     *
     * @param[in]      dataSize  The new size of the jacobi data.
     * @param[in] statementSize  The new size of the statement data.
     */
    void resize(const size_t& dataSize, const size_t& statementSize) {
      data.resize(dataSize);
      statements.resize(statementSize);
    }

private:
    /**
     * @brief Helper function: Sets the adjoint vector to a new size.
     *
     * @param[in] size The new size for the adjoint vector.
     */
    void resizeAdjoints(const IndexType& size) {
      IndexType oldSize = adjointsSize;
      adjointsSize = size;

      adjoints = (Real*)realloc(adjoints, sizeof(Real) * (size_t)adjointsSize);

      for(IndexType i = oldSize; i < adjointsSize; ++i) {
        adjoints[i] = 0.0;
      }
    }

public:
    /**
     * @brief Allocate the adjoint vector such that it fits the current number of statements.
     */
    void allocateAdjoints() {
      //TODO: Tim fragen of er das brauch
      resizeAdjoints(indexHandler.getMaximumGlobalIndex() + 1);
    }

    /**
     * @brief Store the jacobies of the statement on the tape.
     *
     * The jacobies and the indices of the rhs expression are
     * stored on the tape. Also the number of active variables
     * is stored in the statement vector.
     *
     * The gradient data of the lhs will get a new index.
     * The primal value of the lhs is set to the primal value of the rhs.
     *
     * @param[out]   lhsValue    The primal value of the lhs. This value is set to the value
     *                           of the right hand side.
     * @param[out]   lhsIndex    The gradient data of the lhs. The index will be updated.
     * @param[in]         rhs    The right hand side expression of the assignment.
     *
     * @tparam Rhs The expression on the rhs of the statement.
     */
    template<typename Rhs>
    inline void store(Real& lhsValue, IndexType& lhsIndex, const Rhs& rhs) {
      void* null = NULL;
      ENABLE_CHECK (OptTapeActivity, active){
        statements.reserveItems(1); // statements needs a reserve before the data items for the statement are pushed
        data.reserveItems(ExpressionTraits<Rhs>::maxActiveVariables);
        /* first store the size of the current stack position and evaluate the
         rhs expression. If there was an active variable on the rhs, update
         the index of the lhs */
        size_t startSize = data.getChunkPosition();
        rhs.template calcGradient<void*>(null);
        size_t activeVariables = data.getChunkPosition() - startSize;
        if(0 == activeVariables) {
          indexHandler.freeIndex(lhsIndex);
        } else {
          indexHandler.checkIndex(lhsIndex);

          statements.setDataAndMove(std::make_tuple((StatementInt)activeVariables, lhsIndex));
        }
      } else {
        indexHandler.freeIndex(lhsIndex);
      }
      /* now set the value of the lhs */
      lhsValue = rhs.getValue();
    }

    /**
     * @brief Optimization for the copy operation just copies the index of the rhs.
     *
     * No data is stored in this method.
     *
     * The primal value of the lhs is set to the primal value of the rhs.
     *
     * @param[out]   lhsValue    The primal value of the lhs. This value is set to the value
     *                           of the right hand side.
     * @param[out]   lhsIndex    The gradient data of the lhs. The index will be set to the index of the rhs.
     * @param[in]         rhs    The right hand side expression of the assignment.
     */
    inline void store(Real& lhsValue, IndexType& lhsIndex, const ActiveReal<Real, ChunkIndexTape<Real, IndexType> >& rhs) {
      ENABLE_CHECK (OptTapeActivity, active){
        if(0 != rhs.getGradientData()) {
          indexHandler.checkIndex(lhsIndex);

          statements.reserveItems(1); // statements needs a reserve before the data items for the statement are pushed
          data.reserveItems(1);
          data.setDataAndMove(std::make_tuple(1.0, rhs.getGradientData()));
          statements.setDataAndMove(std::make_tuple((StatementInt)1, lhsIndex));
        } else {
          indexHandler.freeIndex(lhsIndex);
        }
      } else {
        indexHandler.freeIndex(lhsIndex);
      }
      lhsValue = rhs.getValue();
    }

    /**
     * @brief Optimization for a passive value on the rhs. The lhs index is set to zero.
     *
     * No data is stored in this method.
     *
     * The primal value of the lhs is set to the primal value of the rhs.
     *
     * @param[out]   lhsValue    The primal value of the lhs. This value is set to the value
     *                           of the right hand side.
     * @param[out]   lhsIndex    The gradient data of the lhs. The index will be set to zero.
     * @param[in]         rhs    The right hand side expression of the assignment.
     */
    inline void store(Real& lhsValue, IndexType& lhsIndex, const typename TypeTraits<Real>::PassiveReal& rhs) {
      indexHandler.freeIndex(lhsIndex);
      lhsValue = rhs;
    }

    /**
     * @brief Manual store routine.
     *
     * Use this routine to add a statement if the corresponding jacobi entries will be manually pushed onto the tape.
     *
     * The Jacobi entries must be pushed immediately after calling this routine using pushJacobi.
     *
     * @param[out]   lhsIndex    The gradient data of the lhs.
     * @param[in]        size    The number of Jacobi entries.
     */
    inline void store(IndexType& lhsIndex, StatementInt size) {
      ENABLE_CHECK (OptTapeActivity, active){
        data.reserveItems(size);
        statements.reserveItems(1); // statements needs a reserve before the data items for the statement are pushed
        indexHandler.checkIndex(lhsIndex);
        statements.setDataAndMove(std::make_tuple(size, lhsIndex));
      }
    }

    /**
     * @brief Stores the jacobi with the value 1.0 on the tape if the index is active.
     *
     * @param[in]   data Not used in this implementation.
     * @param[in]  value Not used in this implementation.
     * @param[in]  index Used to check if the variable is active.
     *
     * @tparam Data  The type of the data for the tape.
     */
    template<typename Data>
    inline void pushJacobi(Data& data, const Real& value, const IndexType& index) {
      CODI_UNUSED(data);
      CODI_UNUSED(value);
      if(0 != index) {
        this->data.setDataAndMove(std::make_tuple(1.0, index));
      }
    }

    /**
     * @brief Stores the jacobi on the tape if the index is active.
     *
     * @param[in]   data Not used in this implementation.
     * @param[in] jacobi Stored on the tape if the variable is active.
     * @param[in]  value Not used in this implementation.
     * @param[in]  index Used to check if the variable is active.
     *
     * @tparam Data  The type of the data for the tape.
     */
    template<typename Data>
    inline void pushJacobi(Data& data, const Real& jacobi, const Real& value, const IndexType& index) {
      CODI_UNUSED(data);
      CODI_UNUSED(value);
      if(0 != index) {
        ENABLE_CHECK(OptIgnoreInvalidJacobies, isfinite(jacobi)) {
          ENABLE_CHECK(OptJacobiIsZero, 0.0 != jacobi) {
            this->data.setDataAndMove(std::make_tuple(jacobi, index));
          }
        }
      }
    }

    /**
     * @brief Set the index to zero.
     * @param[in] value Not used in this implementation.
     * @param[out] index The index of the active type.
     */
    inline void initGradientData(Real& value, IndexType& index) {
      CODI_UNUSED(value);
      index = 0;
    }

    /**
     * @brief Frees the index.
     *
     * @param[in] value Not used in this implementation.
     * @param[in] index The index is given to the index handler.
     */
    inline void destroyGradientData(Real& value, IndexType& index) {
      CODI_UNUSED(value);

      indexHandler.freeIndex(index);
    }


    /**
     * @brief Set the gradient value of the corresponding index.
     *
     * If the index 0 is the inactive indicator and is ignored.
     *
     * @param[in]    index  The index of the active type.
     * @param[in] gradient  The new value for the gradient.
     */
    void setGradient(IndexType& index, const Real& gradient) {
      if(0 != index) {
        this->gradient(index) = gradient;
      }
    }

    /**
     * @brief Get the gradient value of the corresponding index.
     *
     * @param[in] index The index of the active type.
     * @return The gradient value corresponding to the given index.
     */
    inline Real getGradient(const IndexType& index) const {
      if(adjointsSize <= index) {
        return Real();
      } else {
        return adjoints[index];
      }
    }

    /**
     * @brief Get a reference to the gradient value of the corresponding index.
     *
     * An index of 0 will raise an assert exception.
     *
     * @param[in] index The index of the active type.
     * @return The reference to the gradient data.
     */
    inline Real& gradient(IndexType& index) {
      assert(0 != index);

      //TODO: Add error when index is bigger than expression count
      if(adjointsSize <= index) {
        resizeAdjoints(index + 1);
      }

      return adjoints[index];
    }

    /**
     * @brief Get the current position of the tape.
     *
     * The position can be used to reset the tape to that position or to
     * evaluate only parts of the tape.
     * @return The current position of the tape.
     */
    inline Position getPosition() {
      return externalFunctions.getPosition();
    }

    /**
     * @brief Sets all adjoint/gradients to zero.
     */
    inline void clearAdjoints(){
      for(IndexType i = 0; i <= indexHandler.getMaximumGlobalIndex(); ++i) {
        adjoints[i] = 0.0;
      }
    }

    /**
     * @brief Does nothing because the indices are not connected to the positions.
     *
     * @param[in] start Not used
     * @param[in] end Not used
     */
    inline void clearAdjoints(const Position& start, const Position& end){
      CODI_UNUSED(start);
      CODI_UNUSED(end);
    }

    /**
     * @brief Reset the tape to the given position.
     *
     * @param[in] pos Reset the state of the tape to the given position.
     */
    inline void reset(const Position& pos) {
      clearAdjoints();

      externalFunctions.forEach(externalFunctions.getPosition(), pos, popExternalFunction);

      // reset will be done iteratively through the vectors
      externalFunctions.reset(pos);

      indexHandler.reset();
    }

    /**
     * @brief Reset the tape to its initial state.
     */
    inline void reset() {
      reset(Position());
    }

  private:

    /**
     * @brief Implementation of the AD stack evaluation.
     *
     * It has to hold startAdjPos >= endAdjPos.
     *
     * @param[inout]           stmtPos The starting point in the expression evaluation. The index is decremented.
     * @param[in]           endStmtPos The ending point in the expression evaluation.
     * @param[in]    numberOfArguments The pointer to the number of arguments of the statement.
     * @param[in]           lhsIndices The pointer the indices of the lhs.
     * @param[inout]           dataPos The current position in the jacobi and index vector. This value is used in the next invocation of this method..
     * @param[in]             jacobies The pointer to the jacobies of the rhs arguments.
     * @param[in]              indices The pointer the indices of the rhs arguments.
     */
    inline void evaluateExpressions(size_t& stmtPos, const size_t& endStmtPos, StatementInt* &numberOfArguments, IndexType* lhsIndices, size_t& dataPos, Real* &jacobies, IndexType* &indices) {

      while(stmtPos > endStmtPos) {
        --stmtPos;
        const IndexType& lhsIndex = lhsIndices[stmtPos];
        const Real adj = adjoints[lhsIndex];
        adjoints[lhsIndex] = 0.0;
        const StatementInt& activeVariables = numberOfArguments[stmtPos];
        ENABLE_CHECK(OptZeroAdjoint, adj != 0){
          for(StatementInt curVar = 0; curVar < activeVariables; ++curVar) {
            --dataPos;
            adjoints[indices[dataPos]] += adj * jacobies[dataPos];

          }
        } else {
          dataPos -= activeVariables;
        }
      }
    }

    /**
     * @brief Evaluate a part of the statement vector.
     *
     * It has to hold start >= end.
     *
     * The function calls the evaluation method for the jacobi vector.
     *
     * @param[in]      start The starting point for the statement vector.
     * @param[in]        end The ending point for the statement vector.
     * @param[inout] stmtPos The index position for the statement arrays.
     * @param[in]   jacobies The statement data for the jacobies of the rhs.
     * @param[in]    indices The statement data for the indices of the rhs.
     */
    inline void evaluateStmt(const typename StatementChunkVector::Position& start, const typename StatementChunkVector::Position& end, size_t& stmtPos, Real* &jacobies, IndexType* &indices) {
      StatementInt* numberOfArgumentsData;
      IndexType* lhsIndexData;

      size_t dataPos = start.data;
      for(size_t curChunk = start.chunk; curChunk > end.chunk; --curChunk) {
        std::tie(numberOfArgumentsData, lhsIndexData) = statements.getDataAtPosition(curChunk, 0);

        evaluateExpressions(dataPos, 0, numberOfArgumentsData, lhsIndexData, stmtPos, jacobies, indices);

        dataPos = statements.getChunkUsedData(curChunk - 1);
      }

      // Iterate over the reminder also covers the case if the start chunk and end chunk are the same
      std::tie(numberOfArgumentsData, lhsIndexData) = statements.getDataAtPosition(end.chunk, 0);
      evaluateExpressions(dataPos, end.data, numberOfArgumentsData, lhsIndexData, stmtPos, jacobies, indices);
    }

    /**
     * @brief Evaluate a part of the jacobi vector.
     *
     * It has to hold start >= end.
     *
     * It calls the evaluation method for the expression counter.
     *
     * @param[in]       start The starting point for the jacobi vector.
     * @param[in]         end The ending point for the jacobi vector.
     */
    inline void evaluateData(const typename DataChunkVector::Position& start, const typename DataChunkVector::Position& end) {
      Real* jacobiData;
      IndexType* indexData;

      size_t dataPos = start.data;
      typename StatementChunkVector::Position curInnerPos = start.inner;
      for(size_t curChunk = start.chunk; curChunk > end.chunk; --curChunk) {
        std::tie(jacobiData, indexData) = data.getDataAtPosition(curChunk, 0);

        typename StatementChunkVector::Position endInnerPos = data.getInnerPosition(curChunk);
        evaluateStmt(curInnerPos, endInnerPos, dataPos, jacobiData, indexData);

        curInnerPos = endInnerPos;
        dataPos = data.getChunkUsedData(curChunk - 1);
      }

      // Iterate over the reminder also covers the case if the start chunk and end chunk are the same
      std::tie(jacobiData, indexData) = data.getDataAtPosition(end.chunk, 0);
      evaluateStmt(curInnerPos, end.inner, dataPos, jacobiData, indexData);
    }

    /*
     * Function object for the evaluation of the external functions.
     *
     * It stores the last position for the statement vector. With this
     * position it evaluates the statement vector to the position
     * where the external function was added and then calls the
     * external function.
     */
    struct ExtFuncEvaluator {
      typename DataChunkVector::Position curInnerPos;
      ExternalFunction* extFunc;
      typename DataChunkVector::Position* endInnerPos;

      ChunkIndexTape<Real, IndexType>& tape;

      ExtFuncEvaluator(typename DataChunkVector::Position curInnerPos, ChunkIndexTape<Real, IndexType>& tape) :
        curInnerPos(curInnerPos),
        extFunc(NULL),
        endInnerPos(NULL),
        tape(tape){}

      void operator () (typename ExternalFunctionChunk::DataPointer& data) {
        std::tie(extFunc, endInnerPos) = data;

        // always evaluate the stack to the point of the external function
        tape.evaluateData(curInnerPos, *endInnerPos);

        extFunc->evaluate();

        curInnerPos = *endInnerPos;
      }
    };

    /**
     * @brief Evaluate a part of the external function vector.
     *
     * It has to hold start >= end.
     *
     * It calls the evaluation method for the statement vector.
     *
     * @param[in]       start The starting point for the external function vector.
     * @param[in]         end The ending point for the external function vector.
     */
    void evaluateExtFunc(const typename ExternalFunctionChunkVector::Position& start, const typename ExternalFunctionChunkVector::Position &end){
      ExtFuncEvaluator evaluator(start.inner, *this);

      externalFunctions.forEach(start, end, evaluator);

      // Iterate over the reminder also covers the case if there have been no external functions.
      evaluateData(evaluator.curInnerPos, end.inner);
    }

  public:
    /**
     * @brief Perform the adjoint evaluation from start to end.
     *
     * It has to hold start >= end.
     *
     * @param[in] start  The starting position for the adjoint evaluation.
     * @param[in]   end  The ending position for the adjoint evaluation.
     */
    void evaluate(const Position& start, const Position& end) {
      if(adjointsSize <= indexHandler.getMaximumGlobalIndex()) {
        resizeAdjoints(indexHandler.getMaximumGlobalIndex() + 1);
      }

      evaluateExtFunc(start, end);
    }


    /**
     * @brief Perform the adjoint evaluation from the current position to the initial position.
     */
    void evaluate() {
      evaluate(getPosition(), Position());
    }

    /**
     * @brief Add an external function with a void handle as user data.
     *
     * The data handle provided to the tape is considered in possession of the tape. The tape will now be responsible to
     * free the handle. For this it will use the delete function provided by the user.
     *
     * @param[in] extFunc  The external function which is called by the tape.
     * @param[inout] data  The data for the external function. The tape takes ownership over the data.
     * @param[in] delData  The delete function for the data.
     */
    void pushExternalFunctionHandle(ExternalFunction::CallFunction extFunc, void* data, ExternalFunction::DeleteFunction delData){
      pushExternalFunctionHandle(ExternalFunction(extFunc, data, delData));
    }


    /**
     * @brief Add an external function with a specific data type.
     *
     * The data pointer provided to the tape is considered in possession of the tape. The tape will now be responsible to
     * free the data. For this it will use the delete function provided by the user.
     *
     * @param[in] extFunc  The external function which is called by the tape.
     * @param[inout] data  The data for the external function. The tape takes ownership over the data.
     * @param[in] delData  The delete function for the data.
     */
    template<typename Data>
    void pushExternalFunction(typename ExternalFunctionDataHelper<Data>::CallFunction extFunc, Data* data, typename ExternalFunctionDataHelper<Data>::DeleteFunction delData){
      pushExternalFunctionHandle(ExternalFunctionDataHelper<Data>::createHandle(extFunc, data, delData));\
    }

  private:
    /**
     * @brief Private common method to add to the external function stack.
     *
     * @param[in] function The external function structure to push.
     */
    void pushExternalFunctionHandle(const ExternalFunction& function){
      externalFunctions.reserveItems(1);
      externalFunctions.setDataAndMove(std::make_tuple(function, data.getPosition()));
    }

    /**
     * @brief Delete the data of the external function.
     * @param extFunction The external function in the vector.
     */
    static void popExternalFunction(typename ExternalFunctionChunk::DataPointer& extFunction) {
      /* we just need to call the delete function */
      std::get<0>(extFunction)->deleteData();
    }
  public:

    /**
     * @brief Register a variable as an active variable.
     *
     * The index of the variable is set to a non zero number.
     * @param[inout] value The value which will be marked as an active variable.
     */
    inline void registerInput(ActiveReal<Real, ChunkIndexTape<Real, IndexType> >& value) {
      indexHandler.checkIndex(value.getGradientData());
    }

    /**
     * @brief Not needed in this implementation.
     *
     * @param[in] value Not used.
     */
    inline void registerOutput(ActiveReal<Real, ChunkIndexTape<Real, IndexType> >& value) {
      CODI_UNUSED(value);
      /* do nothing */
    }

    /**
     * @brief Start recording.
     */
    inline void setActive(){
      active = true;
    }

    /**
     * @brief Stop recording.
     */
    inline void setPassive(){
      active = false;
    }

    /**
     * @brief Check if the tape is active.
     * @return true if the tape is active.
     */
    inline bool isActive(){
      return active;
    }

    /**
     * @brief Prints statistics about the tape on the screen
     *
     * Prints information such as stored statements/adjoints and memory usage on screen.
     */
    void printStatistics(){
      const double BYTE_TO_MB = 1.0/1024.0/1024.0;
      
      size_t nAdjoints      = (size_t)indexHandler.getMaximumGlobalIndex() + 1;
      double memoryAdjoints = (double)nAdjoints * (double)sizeof(Real) * BYTE_TO_MB;

      size_t nChunksStmts  = statements.getNumChunks();
      size_t totalStmts    = (nChunksStmts-1)*statements.getChunkSize()
                             +statements.getChunkUsedData(nChunksStmts-1);
      double  memoryUsedStmts = (double)totalStmts*(double)(sizeof(StatementInt) + sizeof(IndexType))* BYTE_TO_MB;
      double  memoryAllocStmts= (double)nChunksStmts*(double)statements.getChunkSize()
                                *((double)sizeof(StatementInt) + sizeof(IndexType))* BYTE_TO_MB;
      size_t nChunksData  = data.getNumChunks();
      size_t totalData    = (nChunksData-1)*data.getChunkSize()
                             +data.getChunkUsedData(nChunksData-1);
      double  memoryUsedData = (double)totalData*(double)(sizeof(Real)+sizeof(IndexType))* BYTE_TO_MB;
      double  memoryAllocData= (double)nChunksData*(double)data.getChunkSize()
                                *(double)(sizeof(Real)+sizeof(IndexType))* BYTE_TO_MB;
      size_t maximumGlobalIndex     = (size_t)indexHandler.getMaximumGlobalIndex();
      size_t storedIndices          = (size_t)indexHandler.getNumberStoredIndices();
      size_t currentLiveIndices     = (size_t)indexHandler.getCurrentIndex() - indexHandler.getNumberStoredIndices();

      double memoryStoredIndices    = (double)storedIndices*(double)(sizeof(IndexType)) * BYTE_TO_MB;
      double memoryAllocatedIndices = (double)indexHandler.getNumberAllocatedIndices()*(double)(sizeof(IndexType)) * BYTE_TO_MB;

      size_t nExternalFunc = (externalFunctions.getNumChunks()-1)*externalFunctions.getChunkSize()
          +externalFunctions.getChunkUsedData(externalFunctions.getNumChunks()-1);


      std::cout << std::endl
                << "---------------------------------------------" << std::endl
                << "CoDi Tape Statistics (ChunkIndexTape)"    << std::endl
                << "---------------------------------------------" << std::endl
                << "Statements " << std::endl
                << "---------------------------------------------" << std::endl
                << "  Number of Chunks:  " << std::setw(10) << nChunksStmts << std::endl
                << "  Total Number:      " << std::setw(10) << totalStmts   << std::endl
                << "  Memory allocated:  " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryAllocStmts << " MB" << std::endl
                << "  Memory used:       " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryUsedStmts << " MB" << std::endl
                << "---------------------------------------------" << std::endl
                << "Jacobi entries "                               << std::endl
                << "---------------------------------------------" << std::endl
                << "  Number of Chunks:  " << std::setw(10) << nChunksData << std::endl
                << "  Total Number:      " << std::setw(10) << totalData   << std::endl
                << "  Memory allocated:  " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryAllocData << " MB" << std::endl
                << "  Memory used:       " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryUsedData << " MB" << std::endl
                << "---------------------------------------------" << std::endl
                << "Adjoint vector"                                << std::endl
                << "---------------------------------------------" << std::endl
                << "  Number of Adjoints: " << std::setw(10) << nAdjoints << std::endl
                << "  Memory allocated:   " << std::setiosflags(std::ios::fixed)
                                            << std::setprecision(2)
                                            << std::setw(10)
                                            << memoryAdjoints << " MB" << std::endl
                << "---------------------------------------------" << std::endl
                << "Indices"                                       << std::endl
                << "---------------------------------------------" << std::endl
                << "  Max. live indices: " << std::setw(10) << maximumGlobalIndex << std::endl
                << "  Cur. live indices: " << std::setw(10) << currentLiveIndices << std::endl
                << "  Indices stored:    " << std::setw(10) << storedIndices << std::endl
                << "  Memory allocated:  " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryAllocatedIndices << " MB" << std::endl
                << "  Memory used:       " << std::setiosflags(std::ios::fixed)
                                           << std::setprecision(2)
                                           << std::setw(10)
                                           << memoryStoredIndices << " MB" << std::endl
                << "---------------------------------------------" << std::endl
                << "External functions  "                          << std::endl
                << "---------------------------------------------" << std::endl
                << "  Total Number:     " << std::setw(10) << nExternalFunc << std::endl
                << std::endl;

    }

  };
}