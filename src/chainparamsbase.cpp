#include "chainparamsbase.h"
#include "tinyformat.h"
#include <memory>
#include <assert.h>
#include <cstdio>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::REGTEST = "regtest";

template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;


const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}


std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    printf("%s : 创建CBaseChainParams指针 \n", __func__);
    if (chain == CBaseChainParams::MAIN)
        return MakeUnique<CBaseChainParams>("", 8332);
    else if (chain == CBaseChainParams::TESTNET)
        return MakeUnique<CBaseChainParams>("testnet3", 18332);
    else if (chain == CBaseChainParams::REGTEST)
        return MakeUnique<CBaseChainParams>("regtest", 18443);
    else
    {
        // 抛出异常
        std::string fun_name = std::string(__func__) + ": Unknown chain " + chain;
        throw std::runtime_error(fun_name);
    }
}

void SelectBaseParams(const std::string& chain){
    printf("%s : 初始化 globalChainParams : %s\n", __func__, chain.data());
    globalChainBaseParams = CreateBaseChainParams(chain);

}

