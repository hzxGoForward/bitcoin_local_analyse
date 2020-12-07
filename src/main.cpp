#include <cstdio>
#include <string>
#include "chainparams.h"
#include "time.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include "chain.h"
#include "txdb.h"
#include <cstddef>
#include <unordered_map>
#include "shutdown.h"
#include "pow.h"
#include <fstream>
#include "serialize.h"
#include "blkFile.h"

std::string data_dir = ""; // 数据路径
static const long long nDefaultDbCache = 450L;
std::unique_ptr<CBlockTreeDB> pblocktree;

struct BlockHasher
{
    // this used to call `GetCheapHash()` in uint256, which was later moved; the
    // cheap hash function simply calls ReadLE64() however, so the end result is
    // identical
    size_t operator()(const uint256 &hash) const { return ReadLE64(hash.begin()); }
};

struct CBlockIndexWorkComparator
{
    bool operator()(const CBlockIndex *pa, const CBlockIndex *pb) const
    {
        // First sort by most total work, ...
        if (pa->nChainWork > pb->nChainWork)
            return false;
        if (pa->nChainWork < pb->nChainWork)
            return true;

        // ... then by earliest time received, ...
        if (pa->nSequenceId < pb->nSequenceId)
            return false;
        if (pa->nSequenceId > pb->nSequenceId)
            return true;

        // Use pointer address as tie breaker (should only happen with blocks
        // loaded from disk, as those all have id 0).
        if (pa < pb)
            return false;
        if (pa > pb)
            return true;

        // Identical blocks.
        return false;
    }
};

typedef std::unordered_map<uint256, CBlockIndex *, BlockHasher> BlockMap;
BlockMap m_block_index; // 区块树结构
/**
     * All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
std::multimap<CBlockIndex *, CBlockIndex *> m_blocks_unlinked;

/** Dirty block index entries. */
std::set<CBlockIndex *> setDirtyBlockIndex;

/**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
std::set<CBlockIndex *, CBlockIndexWorkComparator> setBlockIndexCandidates;

// hzx pindex BestInvalid 记录块号最大的非法块
CBlockIndex *pindexBestInvalid = nullptr;

// 工作量最大,最早接收到的区块
CBlockIndex *pindexBestHeader = nullptr;

void AppInit(const string &network)
{
    SelectParams(network);
    return;
}

// hzx 向m_block_index 中插入hash索引
CBlockIndex *InsertBlockIndex(const uint256 &hash)
{

    if (hash.IsNull())
        return nullptr;

    // Return existing
    BlockMap::iterator mi = m_block_index.find(hash);
    if (mi != m_block_index.end())
        return (*mi).second;

    // Create new
    CBlockIndex *pindexNew = new CBlockIndex();
    mi = m_block_index.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    printf("%s: 成功插入区块: %s\n", __func__, hash.ToString().data());
    return pindexNew;
}

// 载入区块索引
bool loadBlock(const string &index_path)
{
    printf("%s: Loading block index...\n", __func__);
    std::chrono::milliseconds load_block_index_start_time = GetTimeMillis();
    unsigned long nTotalCache = nDefaultDbCache << 20;
    unsigned long nBlockTreeDBCache = std::min(nTotalCache / 8, 2UL << 20);
    pblocktree.reset();
    // 打开leveldb 数据库
    pblocktree.reset(new CBlockTreeDB(index_path, nBlockTreeDBCache, false, false));
    const CChainParams &chainparams = Params();
    pblocktree->LoadBlockIndexGuts(chainparams.GetConsensus(), InsertBlockIndex);
    // 对m_block中所有的区块按照高度排序
    std::vector<std::pair<int, CBlockIndex *>> vSortedByHeight;
    vSortedByHeight.reserve(m_block_index.size());
    for (const std::pair<const uint256, CBlockIndex *> &item : m_block_index)
    {
        CBlockIndex *pindex = item.second;
        vSortedByHeight.push_back(std::make_pair(pindex->nHeight, pindex));
    }

    // hzx 根据高度排序,从0~当前块
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    for (const std::pair<const uint256, CBlockIndex *> &item : m_block_index)
    {
        CBlockIndex *pindex = item.second;
        vSortedByHeight.push_back(std::make_pair(pindex->nHeight, pindex));
    }
    // hzx 根据高度排序,从0~当前块
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    printf("%s 成功载入 %lu 个区块: \n", __func__, vSortedByHeight.size());
    for (const std::pair<int, CBlockIndex *> &item : vSortedByHeight)
    {
        if (ShutdownRequested())
            return false;
        CBlockIndex *pindex = item.second;
        // 计算工作量
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        pindex->nTimeMax = (pindex->pprev ? std::max(pindex->pprev->nTimeMax, pindex->nTime) : pindex->nTime);
        // We can link the chain of blocks for which we've received transactions at some point.
        // Pruned nodes may have deleted the block.
        if (pindex->nTx > 0)
        {
            if (pindex->pprev)
            {
                if (pindex->pprev->HaveTxsDownloaded())
                {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                }
                else
                {
                    pindex->nChainTx = 0;
                    m_blocks_unlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            }
            else
            {
                pindex->nChainTx = pindex->nTx;
            }
        }
        // hzx 该区块合法,但是该区块的父区块不合法,则将该区块插入脏区块集合中
        if (!(pindex->nStatus & BLOCK_FAILED_MASK) && pindex->pprev && (pindex->pprev->nStatus & BLOCK_FAILED_MASK))
        {
            pindex->nStatus |= BLOCK_FAILED_CHILD;
            setDirtyBlockIndex.insert(pindex);
        }
        // 如果该区块合法,并且所有父区块到该区块的交易都已经被下载,放入candidate区块中
        // pindex中nStatus状态字段, 对应于BlockStatus字段,根据字段选择
        // 第一次时,只有创世块会被放入block_index_candidates 中,因为创世块的pprev为nullptr
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->HaveTxsDownloaded() || pindex->pprev == nullptr))
        {
            setBlockIndexCandidates.insert(pindex);
        }
        //
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        // 如果前一个区块存在,求解Skipi,
        // 96->64, 95->89,
        // 580000->579968, 10001->99841, 10000->99968
        if (pindex->pprev)
            pindex->BuildSkip();
        // 记录目前为止最长的合法的blockHeader的CBlockIndex索引
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == nullptr || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }
    for (const CBlockIndex *e : setBlockIndexCandidates)
        printf("%s\n", (e->ToString()).data());
    return true;
}

// 从本地磁盘读取区块
bool ReadRawBlockFromDisk(std::vector<uint8_t> &block, const string &path, const CBlockIndex *blkIndex, const Consensus::Params &consensusParams)
{
    string file_num = std::to_string(blkIndex->nFile);
    while (file_num.size() < 5)
        file_num.insert(file_num.begin(), '0');
    string file_dir = path + "/blk" + file_num + ".dat";
    fstream fs(file_dir, std::ios::binary | std::ios::in);
    unsigned int blk_size = 0;
    char blk_start[4] = {'\0'};
    if (!fs.is_open())
    {
        printf("%s 打开失败!\n", file_num.data());
        return false;
    }
    try
    {
        fs.seekp(blkIndex->nDataPos - 8);
        fs.read(blk_start, sizeof(blk_start));
        if (memcmp(blk_start, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE))
        {
            printf("%s: 区块起始为止不匹配, 文件: %d, 偏移位置:%d, %s versus expected %s", __func__, blkIndex->nFile, blkIndex->nDataPos,
                   HexStr(blk_start, blk_start + CMessageHeader::MESSAGE_START_SIZE).data(),
                   HexStr(Params().MessageStart(), Params().MessageStart() + CMessageHeader::MESSAGE_START_SIZE).data());
            return false;
        }

        fs.read(reinterpret_cast<char *>(&blk_size), sizeof(blk_size));
        if (blk_size > MAX_SIZE)
        {
            printf("%s : 区块大小超过范围, 文件位置: %d, 偏移位置: %d \n", __func__, blkIndex->nFile, blkIndex->nDataPos);
            throw std::runtime_error("");
        }
        block.resize(blk_size);
        fs.read(reinterpret_cast<char *>(block.data()), blk_size);
    }
    catch (const std::exception &e)
    {
        printf("%s : 读取原始区块数据出错: %s, 文件:%d, 位置: %d\n", __func__, e.what(), blkIndex->nFile, blkIndex->nDataPos);
        fs.close();
        return false;
    }
    if (fs.is_open())
        fs.close();
    std::string header = HexStr(Params().MessageStart(), Params().MessageStart() + CMessageHeader::MESSAGE_START_SIZE);
    char *s = reinterpret_cast<char *>(&blk_size);
    std::string sz = HexStr(s, s + 4);
    std::string data = HexStr(block);
    std::string blk = header + sz + data;
    printf("%s\n", blk.data());
    return true;
}

// 从本地磁盘读取区块
bool ReadBlockFromDisk(CBlock &block, const string &path, const CBlockIndex *blkIndex, const Consensus::Params &consensusParams)
{
    string file_num = std::to_string(blkIndex->nFile);
    while (file_num.size() < 5)
        file_num.insert(file_num.begin(), '0');
    string file_dir = path + "/blk" + file_num + ".dat";
    CAutoFile filein(file_dir, SER_DISK, CLIENT_VERSION, blkIndex->nDataPos);
    if (filein.IsNull()){
        printf("%s: 文件打开失败 %s \n", __func__, file_dir.data());
        return false;
    }
    // Read block
    try{
        filein >> block;
    }
    catch (const std::exception &e){
        printf("%s: Deserialize or I/O error - %s at %u", __func__, e.what(), blkIndex->nDataPos);
        return false;
    }
    // Check the header
    printf("%s \n", block.ToString().data());
    if (!CheckProofOfWork(block.GetHash(), block.nBits, consensusParams)){
        printf("%s 读取区块出错.\n", __func__);
        return false;
    }

    return true;
}

int main()
{
    AppInit("main");
    const string root_path = "/home/hzx/Documents/github/cpp/blockchain/Bitcoin";
    const string blk_path = root_path + "/blocks";
    const string index_path = root_path + "/blocks/index_hzxpc";
    loadBlock(index_path);
    const CBlockIndex *blk_index = *(setBlockIndexCandidates.begin());
    // std::vector<uint8_t> blockraw;
    // ReadRawBlockFromDisk(blockraw, blk_path, blk_index, Params().GetConsensus());
    CBlock block;
    ReadBlockFromDisk(block, blk_path, blk_index, Params().GetConsensus());
    // CBlockHeaderAndShortTxIDs cmpctBlock(block,true);
    // int cmplock_sz = GetSerializeSize(block, PROTOCOL_VERSION);
    // printf("压缩区块大小为: %d \n", cmplock_sz);
    return 0;
}