#include <fstream>
#include <stdexcept>
#include "serialize.h"
using namespace std;
class CAutoFile
{
private:
    const std::string file_path;
    const int nType;
    const int nVersion;
    std::fstream* fs;

public:
    CAutoFile(const std::string& file_path, int nTypeIn, int nVersionIn, int npos) : nType(nTypeIn), nVersion(nVersionIn)
    {
        fs = new fstream(file_path, std::ios::in|std::ios::binary);
        fs->seekp(npos);
    }

    ~CAutoFile()
    {
        if (fs->is_open())
            fs->close();
        delete fs;
        fs = nullptr;
    }

    fstream *release()
    {
        fstream *ret = fs;
        fs = nullptr;
        return ret;
    }

    fstream *Get() const { return fs; }

    bool IsNull() const { return fs == nullptr; }

    int GetType() const { return nType; }

    int GetVersion() const { return nVersion; }

    void read(char *pch, size_t nSize)
    {
        if (fs == nullptr)
            throw std::ios_base::failure("CAutoFile::read: file handle is nullptr");
        fs->read(pch, nSize);
        if (static_cast<size_t>(fs->gcount()) != nSize)
            throw std::ios_base::failure(fs->eof() ? "CAutoFile::read: end of file" : "CAutoFile::read: fread failed");
    }

    void write(const char* pch, size_t nSize)
    {
        if (fs == nullptr)
            throw std::ios_base::failure("CAutoFile::write: file handle is nullptr");
        fs->write(pch, nSize);
        if (fs->bad())
            throw std::ios_base::failure("CAutoFile::write: write failed");
    }

    void ignore(size_t nSize)
    {
        if (fs == nullptr)
            throw std::ios_base::failure("CAutoFile::ignore: file handle is nullptr");
        unsigned char data[4096];
        while (nSize > 0)
        {
            size_t nNow = std::min<size_t>(nSize, sizeof(data));
            fs->write(reinterpret_cast<char*>(data), static_cast<std::streamsize>(nNow ));
            if (fs->bad())
                throw std::ios_base::failure(fs->eof() ? "CAutoFile::ignore: end of file" : "CAutoFile::read: fread failed");
            nSize -= nNow;
        }
    }

    template <typename T>
    CAutoFile& operator<<(const T& obj)
    {
        // Serialize to this stream
        if (!fs)
            throw std::ios_base::failure("CAutoFile::operator<<: file handle is nullptr");
        ::Serialize(*this, obj);
        return (*this);
    }

    template <typename T>
    CAutoFile& operator>>(T&& obj)
    {
        // Unserialize from this stream
        if (fs==nullptr)
            throw std::ios_base::failure("CAutoFile::operator>>: file handle is nullptr");
        ::Unserialize(*this, obj);
        return (*this);
    }
};