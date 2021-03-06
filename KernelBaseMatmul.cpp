//    Copyright 2010 Chris Jang
//
//    This file is part of GATLAS.
//
//    GATLAS is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    GATLAS is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with GATLAS.  If not, see <http://www.gnu.org/licenses/>.

#include "KernelBaseMatmul.hpp"

#include "declare_namespace"

void KernelBaseMatmul::setUseAttrAutoVec(const bool value) {
    _useAttrAutoVec = value;
}

bool KernelBaseMatmul::getUseAttrAutoVec() const {
    return _useAttrAutoVec;
}

size_t KernelBaseMatmul::dimM() const { return _dimM; }
size_t KernelBaseMatmul::dimN() const { return _dimN; }
size_t KernelBaseMatmul::dimK() const { return _dimK; }
size_t KernelBaseMatmul::groupSize() const { return _groupSize; }
size_t KernelBaseMatmul::blockHeight() const { return _blockHeight; }
size_t KernelBaseMatmul::extraParam() const { return _extraParam; }

bool KernelBaseMatmul::mnkChanged() const { return _mnkChanged; }

bool KernelBaseMatmul::transposeA() const { return _transposeA; }
bool KernelBaseMatmul::transposeB() const { return _transposeB; }

bool KernelBaseMatmul::validExtraParam() const {
    bool isValid = true;
    if (-1 != _predicateInlineMNK) {
        const bool predicateVal = 1 == _predicateInlineMNK;
        if (inlineMNK() != predicateVal) isValid = false;
    }
    if (-1 != _predicateLoopOrder) {
        const size_t predicateVal = _predicateLoopOrder;
        if (loopOrder() != predicateVal) isValid = false;
    }
    return isValid;
}

void KernelBaseMatmul::setInlineMNK(const bool value) {
    _predicateInlineMNK = value;
}

void KernelBaseMatmul::setLoopOrder(const size_t value) {
    _predicateLoopOrder = value;
}

void KernelBaseMatmul::clearInlineMNK() {
    _predicateInlineMNK = -1;
}

void KernelBaseMatmul::clearLoopOrder() {
    _predicateLoopOrder = -1;
}

// for memory buffer kernels
size_t KernelBaseMatmul::localWidth() const {
    // add one to width to avoid local memory bank conflicts
    const size_t LOCALMEM_PAD = 1;
    return groupSize() + LOCALMEM_PAD;
}

size_t KernelBaseMatmul::wholeQuads() const { return blockHeight() / VECTOR_LENGTH; }
size_t KernelBaseMatmul::fractQuads() const { return blockHeight() % VECTOR_LENGTH; }

ConstantValue<std::string> KernelBaseMatmul::multQuads(const Value& valSize) const {
    switch (fractQuads()) {
        case (0) : return wholeQuads() * valSize;
        case (1) : return (wholeQuads() * valSize + valSize / VECTOR_LENGTH);
        case (2) : return (wholeQuads() * valSize + valSize / 2);
        case (3) : return (wholeQuads() * valSize + 3 * valSize / VECTOR_LENGTH);
    }
    return ConstantValue<std::string>(""); // should never happen
}

size_t KernelBaseMatmul::multQuads(const size_t valSize) const {
    switch (fractQuads()) {
        case (0) : return wholeQuads() * valSize;
        case (1) : return (wholeQuads() * valSize + valSize / VECTOR_LENGTH);
        case (2) : return (wholeQuads() * valSize + valSize / 2);
        case (3) : return (wholeQuads() * valSize + 3 * valSize / VECTOR_LENGTH);
    }
    return 0; // should never happen
}

// inner product accumulation
std::string KernelBaseMatmul::assignMAD(const Vector< scalarN >& accum,
                                        const Vector< scalarN >& valA,
                                        const Vector< scalarN >& valB,
                                        const size_t j,           // output row
                                        const size_t k,           // output vector element component
                                        const size_t l) const {   // inner product component
    if (_transposeA) {
        if (_transposeB)
            // At Bt
            return assign(accum[j][k], MADValue(valA[l][j], valB[k][l], accum[j][k]));
        else
            // At B
            return assign(accum[j][k], MADValue(valA[l][j], valB[l][k], accum[j][k]));
    } else {
        if (_transposeB)
            // A Bt
            return assign(accum[j][k], MADValue(valA[j][l], valB[k][l], accum[j][k]));
        else
            // A B
            return assign(accum[j][k], MADValue(valA[j][l], valB[l][k], accum[j][k]));
    }
}

// inner product loop reordering
std::ostream& KernelBaseMatmul::assignMAD(std::ostream& os,
                                          const size_t loopOrder,
                                          const Vector< scalarN >& accum,
                                          const Vector< scalarN >& valA,
                                          const Vector< scalarN >& valB) const {
    switch (loopOrder) {
        case (0) : // (j, k, l)
            for (size_t j = 0; j < blockHeight(); j++) // vector element
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
                os << assignMAD(accum, valA, valB, j, k, l);
            break;
        case (1) : // (k, j, l)
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
            for (size_t j = 0; j < blockHeight(); j++) // vector element
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
                 os << assignMAD(accum, valA, valB, j, k, l);
            break;
        case (2) : // (l, j, k)
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
            for (size_t j = 0; j < blockHeight(); j++) // vector element
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
                 os << assignMAD(accum, valA, valB, j, k, l);
            break;
        case (3) : // (j, l, k)
            for (size_t j = 0; j < blockHeight(); j++) // vector element
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
                 os << assignMAD(accum, valA, valB, j, k, l);
            break;
        case (4) : // (k, l, j)
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
            for (size_t j = 0; j < blockHeight(); j++) // vector element
                 os << assignMAD(accum, valA, valB, j, k, l);
            break;
        case (5) : // (l, k, j)
            for (size_t l = 0; l < VECTOR_LENGTH; l++) // component of temporary values
            for (size_t k = 0; k < VECTOR_LENGTH; k++) // component of vector element
            for (size_t j = 0; j < blockHeight(); j++) // vector element
                 os << assignMAD(accum, valA, valB, j, k, l);
            break;
    }
    return os;
}

KernelBaseMatmul::KernelBaseMatmul(const bool transposeA,
                                   const bool transposeB)
    : _dimM(0),
      _dimN(0),
      _dimK(0),
      _groupSize(0),
      _blockHeight(0),
      _extraParam(0),
      _mnkChanged(false),
      _transposeA(transposeA),
      _transposeB(transposeB),
      _predicateInlineMNK(-1),
      _predicateLoopOrder(-1),
      _useAttrAutoVec(true),
      globalRow(func_string<size_t>("get_global_id", 1)),
      globalCol(func_string<size_t>("get_global_id", 0)),
      blockRow(func_string<size_t>("get_group_id", 1)),
      blockCol(func_string<size_t>("get_group_id", 0)),
      row(func_string<size_t>("get_local_id", 1)),
      col(func_string<size_t>("get_local_id", 0))
{ }

std::string KernelBaseMatmul::kernelName() const {
    std::stringstream ss;
    ss << namePrefix() << blockHeight() << "x" << blockWidth()
       << "_" << dimM()
       << "_" << dimN()
       << "_" << dimK()
       << "_" << groupSize()
       << "_" << extraParam();
    if (transposeA()) ss << "_At";
    if (transposeB()) ss << "_Bt";
    return ss.str();
}

std::vector<size_t> KernelBaseMatmul::globalWorkItems() const {
    std::vector<size_t> dims;
    dims.push_back(_dimN / blockWidth());
    dims.push_back(_dimM / _blockHeight);
    return dims;
}

std::vector<size_t> KernelBaseMatmul::localWorkItems() const {
    std::vector<size_t> dims;
    dims.push_back(_groupSize);
    dims.push_back(_groupSize);
    return dims;
}

// skip small work group sizes as they are always slow
size_t KernelBaseMatmul::minGroupSize(const size_t M, const size_t N, const size_t K) const {
    // 8 * 8 = 64 = wavefront size on HD 5870
    return 8;
}

size_t KernelBaseMatmul::minBlockHeight() const { return VECTOR_LENGTH; }
size_t KernelBaseMatmul::stepBlockHeight() const { return 1; }

size_t KernelBaseMatmul::blockWidth() const { return VECTOR_LENGTH; }

bool KernelBaseMatmul::inlineMNK() const { return _inlineMNK(extraParam()); }
size_t KernelBaseMatmul::loopOrder() const { return _loopOrder(extraParam()); }

// validate kernel parameters
bool KernelBaseMatmul::validateParams(const size_t M,
                                      const size_t N,
                                      const size_t K,
                                      const size_t groupSize,
                                      const size_t blockHeight,
                                      const size_t extraParam) const {

    return

        // work group size
        groupSize >= 1 &&                       // allow small work groups (will be slow)
        groupSize <= maxGroupSize(M, N, K) &&   // limit group size to avoid kernel hangs

        // inner blocking height
        blockHeight >= 1 &&                     // allow short inner blocks (will be slow)
        blockHeight <= maxBlockHeight() &&      // limit block height to avoid kernel hangs and for speed

        // all matrix dimensions must be a multiple of VECTOR_LENGTH
        0 == M % VECTOR_LENGTH &&
        0 == N % VECTOR_LENGTH &&
        0 == K % VECTOR_LENGTH &&

        // check for blocking compatible with matrix dimensions
        0 == N % (groupSize * blockWidth()) &&
        0 == M % (groupSize * blockHeight) &&
        0 == K % (groupSize * VECTOR_LENGTH) &&
        groupSize * blockWidth() <= N &&
        groupSize * blockHeight <= M &&
        groupSize * VECTOR_LENGTH <= K &&

        // extra parameter
        extraParam < numberExtraParam() &&
        validExtraParam();
}

bool KernelBaseMatmul::setParams(const size_t M,
                                 const size_t N,
                                 const size_t K,
                                 const size_t groupSize,
                                 const size_t blockHeight,
                                 const size_t extraParam) {
    if (validateParams(M, N, K, groupSize, blockHeight, extraParam)) {
        _mnkChanged = (M != _dimM) || (N != _dimN) || (K != _dimK);
        _dimM = M;
        _dimN = N;
        _dimK = K;
        _groupSize = groupSize;
        _blockHeight = blockHeight;
        _extraParam  = extraParam;
        return true;
    } else {
        return false;
    }
}

bool KernelBaseMatmul::setParams(const std::vector<size_t>& args) {
    if (args.size() < 6) return false; // not enough parameters
    size_t argIndex = 0;
    const size_t newM = args[argIndex++];
    const size_t newN = args[argIndex++];
    const size_t newK = args[argIndex++];
    const size_t newGroupSize   = args[argIndex++];
    const size_t newBlockHeight = args[argIndex++];
    const size_t newExtraParam  = args[argIndex++];
    return setParams(newM, newN, newK, newGroupSize, newBlockHeight, newExtraParam);
}

size_t KernelBaseMatmul::getGroupSize(const std::vector<size_t>& args) const {
    if (args.size() < 6) return false; // not enough parameters
    return args[3];
}

size_t KernelBaseMatmul::getBlockHeight(const std::vector<size_t>& args) const {
    if (args.size() < 6) return false; // not enough parameters
    return args[4];
}

size_t KernelBaseMatmul::getExtraParam(const std::vector<size_t>& args) const {
    if (args.size() < 6) return false; // not enough parameters
    return args[5];
}

// generate parameter arguments
std::vector< std::vector<size_t> > KernelBaseMatmul::parameters(const size_t M,
                                                                const size_t N,
                                                                const size_t K,
                                                                const int GroupSize,
                                                                const int BlockHeight) const {
    std::vector< std::vector<size_t> > pargs;

    const size_t minGS = (-1 == GroupSize) ? minGroupSize(M, N, K) : GroupSize;
    const size_t maxGS = (-1 == GroupSize) ? maxGroupSize(M, N, K) : GroupSize;

    const size_t minBH = (-1 == BlockHeight) ? minBlockHeight() : BlockHeight;
    const size_t maxBH = (-1 == BlockHeight) ? maxBlockHeight() : BlockHeight;

    // work group size
    for (size_t gs = minGS; gs <= maxGS; gs++)

        // inner blocking height
        for (size_t bh = minBH; bh <= maxBH; bh += stepBlockHeight())

            // extra parameters
            for (size_t xp = 0; xp < numberExtraParam(); xp++)

                if (validateParams(M, N, K, gs, bh, xp)) {

                    std::vector<size_t> a;
                    a.push_back(M);
                    a.push_back(N);
                    a.push_back(K);
                    a.push_back(gs);
                    a.push_back(bh);
                    a.push_back(xp);
                    pargs.push_back(a);
                }

    return pargs;
}

// generate parameter args likely to contain optimal solutions
std::vector< std::vector<size_t> > KernelBaseMatmul::parameters(const size_t M,
                                                                const size_t N,
                                                                const size_t K) const {
    std::vector< std::vector<size_t> > pargs;

    // empirically observed maximum performance occurs
    // when group size is largest possible (<= 16) or 8
    for (size_t i = maxGroupSize(M, N, K); i > minGroupSize(M, N, K); i--) {
        pargs = parameters(M, N, K, i);
        if (! pargs.empty()) break;
    }
    std::vector< std::vector<size_t> > p8 = parameters(M, N, K, minGroupSize(M, N, K));
    pargs.insert(pargs.begin(), p8.begin(), p8.end());

    return pargs;
}

// generate all parameters args
std::vector< std::vector<size_t> > KernelBaseMatmul::parametersAll(const size_t M,
                                                                   const size_t N,
                                                                   const size_t K) const {
    return parameters(M, N, K, -1);
}

// base for matrix multiply
KernelMatmul::KernelMatmul(const bool transposeA,
                           const bool transposeB)
    : KernelBaseMatmul(transposeA, transposeB)
    { }

size_t KernelMatmul::numberFlops() const { return dimM() * dimN() * (2 * dimK() - 1); }

// base for GEMM
KernelGenMatmul::KernelGenMatmul(const bool transposeA,
                                 const bool transposeB)
    : KernelBaseMatmul(transposeA, transposeB)
    { }

size_t KernelGenMatmul::numberFlops() const { return 2 * dimM() * dimN() * (dimK() + 1); }

}; // namespace
