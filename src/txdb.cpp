#include "txdb.h"
#include "pow.h"
#include "shutdown.h"
#include <thread>
#include "system_hzx.h"


CBlockTreeDB::CBlockTreeDB(const std::string &path, unsigned long nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(path, nCacheSize, fMemory, fWipe)
{
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo& info)
{
    return Read(std::make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing)
{
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

void CBlockTreeDB::ReadReindexing(bool& fReindexing)
{
    fReindexing = Exists(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadLastBlockFile(int& nFile)
{
    return Read(DB_LAST_BLOCK, nFile);
}

// bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*>>& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo)
// {
//     CDBBatch batch(*this);
//     for (std::vector<std::pair<int, const CBlockFileInfo*>>::const_iterator it = fileInfo.begin(); it != fileInfo.end(); it++) {
//         batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
//     }
//     batch.Write(DB_LAST_BLOCK, nLastFile);
//     for (std::vector<const CBlockIndex*>::const_iterator it = blockinfo.begin(); it != blockinfo.end(); it++) {
//         batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
//     }
//     return WriteBatch(batch, true);
// }

// bool CBlockTreeDB::WriteFlag(const std::string& name, bool fValue)
// {
//     return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
// }

// bool CBlockTreeDB::ReadFlag(const std::string& name, bool& fValue)
// {
//     char ch;
//     if (!Read(std::make_pair(DB_FLAG, name), ch))
//         return false;
//     fValue = ch == '1';
//     return true;
// }


// hzx 使用blocktree载入BlockIndex
// hzx, 这个函数中并没有使用consensusParams
bool CBlockTreeDB::LoadBlockIndexGuts(const Consensus::Params &consensusParams, std::function<CBlockIndex *(const uint256 &)> insertBlockIndex)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    // hzx levelDB 迭代器使用前必须Seek
    // uint256 中用0初始化一个uint8的数组,大小为32,
    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));

    // Load m_block_index
    while (pcursor->Valid())
    {
        // std::this_thread::interruption_point();          hzx 尽量不适用boost线程库
        if (ShutdownRequested())
            return false;
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX)
        {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex))
            {
                // Construct block index object
                // validation中m_block_index中插入CBlockIndex
                CBlockIndex *pindexNew = insertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev = insertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight = diskindex.nHeight;
                pindexNew->nFile = diskindex.nFile;
                pindexNew->nDataPos = diskindex.nDataPos;
                pindexNew->nUndoPos = diskindex.nUndoPos;
                pindexNew->nVersion = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime = diskindex.nTime;
                pindexNew->nBits = diskindex.nBits;
                pindexNew->nNonce = diskindex.nNonce;
                pindexNew->nStatus = diskindex.nStatus; // hzx block合法状态也保留在leveldb中
                pindexNew->nTx = diskindex.nTx;
                if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits, consensusParams))
                {
                    printf("%s: CheckProofOfWork failed: %s \n", __func__, pindexNew->ToString().data());
                    return false;
                }

                pcursor->Next();
            }
            else
            {
                // return error("%s: failed to read value", __func__);
                printf("%s: failed to read value\n", __func__);
                return false;
            }
        }
        else
        {
            break;
        }
    }

    return true;
}