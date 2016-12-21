
/*

  Copyright (C) 2014, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <zlib.h>
#include <libstub4.h>

#include <foobar/Common.hxx>
#include <foobar/RccPtr.hxx>
#include <foobar/Refcounted.hxx>
#include <foobar/Strarg.hxx>
#include <foobar/Win32Stream.hxx>
#include <foobar/BufferDataStream.hxx>
#include <foobar/Path.hxx>
#include <foobar/Format.hxx>
#include <foobar/StringTool.hxx>
#include <foobar/LineReader.hxx>
#include <foobar/Win32Popen.hxx>

#include <libhash/MD5.h>
#include <libhash/SHA1.h>
#include <libhash/CRC16.h>
#include <libhash/CRC32.h>
#include <libhash/BF.h>
#include <libpe.h>

#include <molepkgr.hxx>
#include <layout.h>
#include <stbinfo.h>

FOOBAR_ENUM_FORMATTER(MXBPAK_ERROR,
                      MXBPAK_SUCCESS,
                      MXBPAK_ERROR_FILE_DSNT_EXIST,
                      MXBPAK_ERROR_FILE_EXISTS,
                      MXBPAK_ERROR_FILE_ACCESS_DNI,
                      MXBPAK_ERROR_INVALID_ARGUMENT,
                      MXBPAK_ERROR_UNSUPPORTED,
                      MXBPAK_ERROR_NO_FILE,
                      MXBPAK_ERROR_INTERNAL,
                      MXBPAK_ERROR_WRITE,
                      MXBPAK_ERROR_FILE_CANT_CREATE,
                      MXBPAK_ERROR_ASSERT,
                      MXBPAK_ERROR_CONFIG
                     );

enum { FT_FILE = 0, FT_DIRECTORY = 0x80000000 };

struct FileBlock : SVFS_FILE_BLOCK
{
    FileBlock(uint32_t o, uint16_t a, uint16_t b, uint16_t c, uint16_t crc = 0)
    {
        packed_size = a;
        real_size   = b;
        flags       = c;
        crc         = crc;
        offset      = o;
    }
};

struct FileInfo : foobar::Refcounted
{
    typedef foobar::RccPtr<FileInfo> Ptr;
    std::wstring name;
    FileInfo* dir;
    std::vector<Ptr> ls;
    std::vector<FileBlock> blocks;
    unsigned flags;
    unsigned size;

    uint32_t spack_offs;
    uint8_t sign[16];
    FileInfo* copy;

    FileInfo() : dir(0), flags(0), size(0), copy(0) {};
    std::vector<FileBlock>& Blocks() { return copy ? copy->Blocks() : blocks; }
};

typedef FileInfo::Ptr FileInfoPtr;

struct ErrorHelper
{
    MXBPAK_ERROR error_occured;
    std::string error;

    MXBPAK_ERROR Error(MXBPAK_ERROR err, const foobar::Strarg<char>& errval = foobar::None)
    {
        if (!err)
        {
            error_occured = MXBPAK_SUCCESS;
        }
        else
        {
            if (errval == foobar::None)
            {
                switch (err)
                {
                    default: error = foobar::format("error %08x", err, 0);
                }
            }
            else
                error = errval.Str();
            error_occured = err;
        }
        return err;
    }
};

struct MOLEBOX_PKG : foobar::Refcounted, ErrorHelper
{
    FileInfoPtr root;
    uint64_t package_base;
    uint8_t skey[16];

    foobar::Stream ds;

    struct StreamInfo
    {
        uint32_t kind, offset, size;
        StreamInfo(uint32_t kind, uint32_t offset, uint32_t size)
            : kind(kind), offset(offset), size(size)
        {}
    };
    std::vector<StreamInfo> stream_arr;

    FileInfo* Lookup(const foobar::Strarg<wchar_t>& name);

    std::map<std::wstring, FileInfo*> sources;
    FileInfoPtr current;
    std::vector<uint8_t> acc;

    unsigned opt_encrypt;
    unsigned opt_compress;
    unsigned opt_hide;
    unsigned opt_conflict;

    MXBPAK_ERROR Start(foobar::Stream ds, const void* passwd, size_t passwd_len, unsigned opt);
    MXBPAK_ERROR File(const foobar::Strarg<wchar_t>& internal_name, unsigned opt);
    MXBPAK_ERROR Fill(const uint8_t* data, size_t length);
    MXBPAK_ERROR Flush();
    MXBPAK_ERROR Close();
    MXBPAK_ERROR Finish(MOLEBOX_PAKNFO* paknfo);
    MXBPAK_ERROR WriteStream(int kind, const uint8_t* data, size_t count, unsigned opt);
    MXBPAK_ERROR Percent(unsigned* persent);

    bool WriteDir(FileInfo* finfo, foobar::Stream xds);
    bool WriteInfo(FileInfo* finfo, foobar::Stream xds);
    bool WriteNames(FileInfo* finfo, SVFS_FILE_INFO* sfilinfo, foobar::Stream xds);
    size_t WriteMap(FileInfo* root, foobar::Stream xds);
    bool UpdateHiddensOnly(FileInfo* finfo);
    void AppendInfos(FileInfo* i, std::vector<FileInfo*>& b);

    MOLEBOX_PKG();
    virtual ~MOLEBOX_PKG();
};

MOLEBOX_PKG::MOLEBOX_PKG()
{
    acc.reserve(SVFS_FILE_BLOCK_SIZE);
    error_occured = MXBPAK_SUCCESS;
}

MOLEBOX_PKG::~MOLEBOX_PKG()
{
}

MXBPAK_ERROR Molebox_Pkg_Start(
    const wchar_t* container_name,
    const char* passwd, size_t passwd_len, unsigned opt,
    MOLEBOX_PKG /* out */ **pkg)
{
    foobar::RccPtr<MOLEBOX_PKG> mpk = foobar::New;
    foobar::Stream ds;

    *pkg = 0;
    unsigned rw_opt = opt & _MXBPAK_PKG_OPT_REWRITE_MASK;
    if (!rw_opt) rw_opt = MXBPAK_PKG_OPT_REWRITE;

    if (rw_opt == MXBPAK_PKG_OPT_REWRITE || !foobar::Path<wchar_t>(container_name).Exists())
    {
        ds = foobar::Win32Stream::Open(container_name, "Prw");
        if (!ds->Good())
            return MXBPAK_ERROR_FILE_ACCESS_DNI;
    }
    else
    {
        ds = foobar::Win32Stream::Open(container_name, "r+");
        if (!ds->Good())
            return MXBPAK_ERROR_FILE_DSNT_EXIST;
    }

    MXBPAK_ERROR err = mpk->Start(ds, passwd, passwd_len, opt);
    if (!err)
        *pkg = foobar::refe(mpk);
    return err;
}

MXBPAK_ERROR Molebox_Pkg_Finish(
    MOLEBOX_PKG* pkg,
    MOLEBOX_PAKNFO* /* out */ paknfo)
{
    MOLEBOX_PAKNFO paknfo_;
    if (!paknfo) paknfo = &paknfo_;
    return pkg->Finish(paknfo);
}

namespace
{
    std::pair<MXBPAK_ERROR, std::string> hash_password(
        const void* passwd, size_t passwd_len, unsigned opt, uint8_t (&out)[16])
    {
        unsigned pwd_opt = opt & _MXBPAK_PKG_OPT_PASSWD_MASK;
        if (!pwd_opt) pwd_opt = MXBPAK_PKG_OPT_TEXT_PASSWD;

        if (pwd_opt == MXBPAK_PKG_OPT_TEXT_PASSWD)
        {
            if (passwd_len == 0) passwd_len = strlen((char*)passwd);
            libhash::md5_digest(passwd, passwd_len, out, sizeof(out));
        }
        if (pwd_opt == MXBPAK_PKG_OPT_WIDE_PASSWD)
        {
            std::string pwd = foobar::strarg_t((wchar_t*)passwd).Str();
            if (passwd_len == 0) passwd_len = pwd.length();
            libhash::md5_digest(pwd.c_str(), passwd_len, out, sizeof(out));
        }
        else if (pwd_opt == MXBPAK_PKG_OPT_BINARY_PASSWD)
        {
            if (passwd_len == 0 || passwd_len > 512)
                return std::pair<MXBPAK_ERROR, std::string>(
                           MXBPAK_ERROR_INVALID_ARGUMENT, "bad password len");
            libhash::md5_digest(passwd, passwd_len, out, sizeof(out));
        }
        else if (pwd_opt == MXBPAK_PKG_OPT_HEXCHR_PASSWD)
        {
            return std::pair<MXBPAK_ERROR, std::string>(
                       MXBPAK_ERROR_UNSUPPORTED,
                       "hex encoded password is unsupported yet");
        }
        else
        {
            return std::pair<MXBPAK_ERROR, std::string>(
                       MXBPAK_ERROR_UNSUPPORTED,
                       "unkwnown password encoding");
        }

        return std::pair<MXBPAK_ERROR, std::string>();
    }


    struct BLOWFISH
    {
        enum { BLOCK_BYTES = 8 };
        enum { KEY_BYTES   = 15 };
        enum DIRECTION { ENCRYPTION, DECRYPTION };

        BF_CONTEXT self;
        DIRECTION direction;

        void SetupEncipher(void const* key, size_t count = KEY_BYTES)
        {
            Blowfish_Init(&self,key,count);
            direction = ENCRYPTION;
        }
        void SetupDecipher(void const* key, size_t count = KEY_BYTES)
        {
            Blowfish_Init(&self,key,count);
            direction = DECRYPTION;
        }

        void DoCipherBlock(void* b)
        {
            if ( direction = ENCRYPTION )
                Blowfish_Encrypt8(&self,b);
            else
                Blowfish_Decrypt8(&self,b);
        }

        void DoCipher(void* data, uint32_t count_of_blocks)
        {
            for (uint32_t i = 0; i < count_of_blocks; ++i)
                DoCipherBlock((uint8_t*)data+BLOCK_BYTES*i);
        }

        void DoCipherCBCI(void* data, uint32_t count_of_blocks)
        {
            uint8_t q[BLOCK_BYTES] = {0};
            for (uint32_t i = 0; i < count_of_blocks; ++i)
            {
                for ( int j =0; j < BLOCK_BYTES; ++j )
                    *((uint8_t*)data+BLOCK_BYTES*i+j) ^= *(q+j);
                DoCipherBlock((uint8_t*)data+BLOCK_BYTES*i);
                memcpy(q,(uint8_t*)data+BLOCK_BYTES*i,BLOCK_BYTES);
            }

        }

        void DoCipherCBCO(void* data, uint32_t count_of_blocks)
        {
            uint8_t q[BLOCK_BYTES] = {0};
            for (uint32_t i = 0; i < count_of_blocks; ++i)
            {
                uint8_t w[BLOCK_BYTES];
                memcpy(w,(uint8_t*)data+BLOCK_BYTES*i,BLOCK_BYTES);
                DoCipherBlock((uint8_t*)data+BLOCK_BYTES*i);
                for ( int j =0; j < BLOCK_BYTES; ++j )
                    *((uint8_t*)data+BLOCK_BYTES*i+j) ^= *(q+j);
                memcpy(q,w,BLOCK_BYTES);
            }
        }
    };
}

MXBPAK_ERROR MOLEBOX_PKG::Start(foobar::Stream ds, const void* passwd, size_t passwd_len, unsigned opt)
{
    this->ds = ds;
    ds->Seek(0, SEEK_END);

    root = foobar::New;
    root->flags = FT_DIRECTORY | SVFS_HIDDEN;
    root->name = L"";
    package_base = ds->Tell();
    sources.clear();

    if (passwd)
    {
        std::pair<MXBPAK_ERROR, std::string> err = hash_password(passwd, passwd_len, opt, skey);
        if (err.first)
            return Error(err.first, err.second);
    }
    else
    {
        hash_password("password", 0, 0, skey);
    }

    BLOWFISH cipher;

    cipher.SetupEncipher(skey);
    char pkg_first_sign[8] = { 'Q', 'U', 'I', 'C', 'K', '5', 0, 0 };
    pkg_first_sign[7] = skey[7] ^ skey[0];
    pkg_first_sign[6] = skey[6] ^ skey[1];
    cipher.DoCipher(pkg_first_sign, 1);
    ds->Write(pkg_first_sign, 8);
    ds->Write32le(package_base);

    opt_encrypt = opt & _MXBPAK_PKG_OPT_ENCRYPT_MASK;
    if (! opt_encrypt) opt_encrypt = passwd ? MXBPAK_PKG_OPT_ENCRYPT : MXBPAK_PKG_OPT_DNT_ENCRYPT;
    opt_compress = opt & _MXBPAK_PKG_OPT_COMPRESS_MASK;
    if (! opt_compress) opt_compress = MXBPAK_PKG_OPT_COMPRESS;
    opt_hide = opt & _MXBPAK_PKG_OPT_HIDE_MASK;
    if (! opt_hide) opt_hide = MXBPAK_PKG_OPT_HIDE;
    opt_conflict = opt & _MXBPAK_PKG_OPT_CONFLICT_MASK;
    if (! opt_conflict) opt_conflict = MXBPAK_PKG_OPT_CONFLICT_INTERNAL;

    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Pkg_Write(MOLEBOX_PKG* pkg, const void* data, size_t length)
{
    return pkg->Fill((uint8_t*)data, length);
}

MXBPAK_ERROR MOLEBOX_PKG::Fill(const uint8_t* data, size_t length)
{
    if (!current) return Error(MXBPAK_ERROR_NO_FILE);

    while (length)
    {
        uint32_t to_append = min(SVFS_FILE_BLOCK_SIZE - acc.size(), length);
        acc.insert(acc.end(), data, data + to_append);
        length -= to_append;
        data += to_append;
        if (acc.size() == SVFS_FILE_BLOCK_SIZE)
        {
            if (MXBPAK_ERROR err = Flush())
                return err;
        }
    }

    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR MOLEBOX_PKG::Flush()
{
    if (!current) return Error(MXBPAK_ERROR_NO_FILE);
    if (acc.size())
    {
        std::vector<uint8_t> z;
        uint8_t* m = &acc[0];
        BLOWFISH cipher;
        cipher.SetupEncipher(skey);

        if (current->flags & SVFS_COMPRESSED) z.resize(SVFS_FILE_BLOCK_SIZE);
        unsigned flags = 0;
        size_t count = acc.size();
        current->size += count;
        uint16_t crc = libhash::crc16_digest(m, count);

        if (current->flags & SVFS_COMPRESSED)
        {
            uLongf z_count = SVFS_FILE_BLOCK_SIZE;
            int z_fail = compress2(&z[0], &z_count, m, count, 9);
            if (!z_fail && z_count < count)
            {
                flags |= SVFS_COMPRESSED;
                count = z_count;
                m = &z[0];
            }
        }

        if (current->flags & SVFS_ENCRYPTED)
        {
            flags |= SVFS_ENCRYPTED;
            count = (count + 7) & ~7;
            cipher.DoCipherCBCI(m, count / 8);
        }

        current->blocks.push_back(FileBlock(ds->Tell() - package_base, count, acc.size(), flags, crc));
        ds->Write(m, count);
    }

    acc.resize(0);
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Pkg_Stream(
    MOLEBOX_PKG* pkg, int kind, unsigned opt,
    const void* data, size_t length)
{
    pkg->Close();
    return pkg->WriteStream(kind, (uint8_t*)data, length, opt);
}

MXBPAK_ERROR MOLEBOX_PKG::WriteStream(int kind, const uint8_t* data, unsigned count, unsigned opt)
{
    unsigned opt_compress = opt & _MXBPAK_FILE_OPT_COMPRESS_MASK;
    if (!opt_compress)
        opt_compress = (this->opt_compress == MXBPAK_PKG_OPT_COMPRESS)
                       ? MXBPAK_FILE_OPT_COMPRESS : MXBPAK_FILE_OPT_DNT_COMPRESS;

    bool xzip = opt_compress == MXBPAK_FILE_OPT_COMPRESS;

    if (count > 0)
    {
        uint32_t foo = count;
        std::vector<uint8_t> compressed(count);
        uLongf z_count = compressed.size();

        int z_fail = xzip ? compress2(&compressed[0], &z_count, data, count, 9) : -1;
        if (!z_fail)
            compressed.resize(z_count);
        else
        {
            foo = xzip ? -1 : -2;
            compressed.clear();
            compressed.insert(compressed.end(), data, data + count);
        }

        if (xzip)
        {
            BLOWFISH cipher;
            cipher.SetupEncipher(skey);
            cipher.DoCipherCBCI(&compressed[0], compressed.size() / 8);
        }

        compressed.insert(compressed.begin(), (uint8_t*)&foo, (uint8_t*)&foo + 4);
        //stream_ptr = ds->Tell();
        //stream_size = compressed.size();
        stream_arr.push_back(StreamInfo(kind, ds->Tell() - package_base, compressed.size()));
        ds->Write(&compressed[0], compressed.size());
    }

    return MXBPAK_SUCCESS;
}

FileInfo* MOLEBOX_PKG::Lookup(const foobar::Strarg<wchar_t>& name)
{
    std::vector<std::wstring> l = foobar::Path<wchar_t>(name).Parent().Split();
    FileInfo* dir = root.Get();

    for (int i = 0; i < l.size(); ++i)
    {
        if (dir->flags & FT_DIRECTORY)
        {
            FileInfo* new_dir = 0;
            for (int j = 0; j < dir->ls.size(); ++j)
            {
                if (dir->ls[j]->name == l[i])
                    new_dir = dir->ls[j].Get();
            }
            if (!new_dir)
            {
                FileInfoPtr n = foobar::New;
                n->flags = FT_DIRECTORY;
                n->name  = l[i];
                n->dir   = dir;
                dir->ls.push_back(n);
                new_dir = n.Get();
            }
            dir = new_dir;
        }
        else
            return Error(MXBPAK_ERROR_INTERNAL,
                         foobar::format("failed to add file in directory '%s', is not directory", name)),
                   0;
    }

    return dir;
}

MXBPAK_ERROR Molebox_Pkg_File(MOLEBOX_PKG* pkg, const wchar_t* internal_name, unsigned opt)
{
    return pkg->File(internal_name, opt);
}

MXBPAK_ERROR Molebox_Pkg_Copy(MOLEBOX_PKG* pkg, const wchar_t* from_file_name)
{
    std::array<uint8_t, 4096> copybuf;
    foobar::Stream ff = foobar::Win32Stream::Open(from_file_name, "r");
    if (!ff->Good())
        return pkg->Error(MXBPAK_ERROR_WRITE, foobar::format("couldn't open file '%s'", from_file_name));
    while (ff->Good() && !ff->Eof())
    {
        int r = ff->ReadData(&copybuf[0], copybuf.size(), 0);
        if (r > 0)
        {
            if (MXBPAK_ERROR err = pkg->Fill(&copybuf[0], r))
                return err;
        }
    }
    if (ff->Good())
        return MXBPAK_SUCCESS;
    else
        return pkg->Error(MXBPAK_ERROR_WRITE, foobar::format("failed read file '%s'", from_file_name));
}

MXBPAK_EXPORTABLE MXBPAK_ERROR Molebox_Pkg_Commit(MOLEBOX_PKG* pkg, size_t* percent)
{
    MXBPAK_ERROR err;
    if ((err = pkg->Flush())) return err;
    if (percent && ((err = pkg->Percent(percent)))) return err;
    return pkg->Close();
}

MXBPAK_ERROR MOLEBOX_PKG::File(const foobar::Strarg<wchar_t>& internal_name, unsigned opt)
{
    Close();

    FileInfoPtr finfo = foobar::New;

    finfo->flags = opt;

    if (!(finfo->dir  = Lookup(internal_name)))
        return Error(MXBPAK_ERROR_FILE_EXISTS, "file already exists in package");

    finfo->name = foobar::Path<wchar_t>(internal_name).Name();
    current = finfo;
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR MOLEBOX_PKG::Percent(size_t* percent)
{
    if (!current) return Error(MXBPAK_ERROR_NO_FILE);
    if (!percent) return Error(MXBPAK_ERROR_INVALID_ARGUMENT, "pointer to parsent value should be not NULL");
    unsigned packed = 0;
    for (int i = 0; i < current->Blocks().size(); ++i) packed += current->Blocks()[i].packed_size;
    if (current->size)
        *percent = packed * 100 / current->size;
    else
        *percent = 100;
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR MOLEBOX_PKG::Close()
{
    if (current)
    {
        Flush();
        current->dir->ls.push_back(current);
        current = foobar::None;
    }
    return MXBPAK_SUCCESS;
}

namespace
{
    std::wstring get_fullname(FileInfo* finfo)
    {
        if (finfo->dir->dir)   // not in root folder
            return get_fullname(finfo->dir) + L"\\";
        else
            return finfo->name;
    }
}

bool MOLEBOX_PKG::WriteNames(FileInfo* finfo, SVFS_FILE_INFO* sfilinfo, foobar::Stream xds)
{
    std::wstring fullname = foobar::w_upper(get_fullname(finfo));
    sfilinfo->key.crc = libhash::crc32_digest(fullname.c_str(), fullname.length() * 2);
    libhash::md5_digest(fullname.c_str(), fullname.length() * 2, sfilinfo->key.sign, sizeof(sfilinfo->key.sign));
    memcpy(finfo->sign, sfilinfo->key.sign, 16);
    if (!(finfo->flags & SVFS_HIDDEN) || (finfo->flags & FT_DIRECTORY))
    {
        sfilinfo->name.S = (wchar_t*)xds->Tell();
        sfilinfo->name.length = finfo->name.length();
        xds->Write(finfo->name.c_str(), finfo->name.length() * 2 + 2);
    }
    return xds->Good();
}

bool MOLEBOX_PKG::WriteInfo(FileInfo* finfo, foobar::Stream xds)
{
    SVFS_FILE_INFO sfilinfo = {0};
    sfilinfo.flags = finfo->flags;
    sfilinfo.count = finfo->Blocks().size();
    sfilinfo.size  = finfo->size;
    WriteNames(finfo, &sfilinfo, xds);
    finfo->spack_offs = xds->Tell();
    xds->Write(&sfilinfo, sizeof(sfilinfo));
    foobar::for_each(finfo->Blocks(), [xds](SVFS_FILE_BLOCK & i)
    {
        xds->Write(&i, sizeof(i));
    });
    return xds->Good();
}

bool MOLEBOX_PKG::WriteDir(FileInfo* finfo, foobar::Stream xds)
{
    foobar::for_each(finfo->ls, [this, xds](FileInfoPtr & ptr)
    {
        if (ptr->flags & FT_DIRECTORY)
            WriteDir(ptr.Get(), xds);
        else
            WriteInfo(ptr.Get(), xds);
    });
    SVFS_FILE_INFO sfilinfo = {0};
    sfilinfo.flags = finfo->flags;
    sfilinfo.count = finfo->ls.size();
    if (finfo->dir) // is not root
        WriteNames(finfo, &sfilinfo, xds);
    finfo->spack_offs = ds->Tell();
    xds->Write(&sfilinfo, sizeof(sfilinfo));
    foobar::for_each(finfo->ls, [xds](FileInfoPtr & ptr)
    {
        xds->Write32le(ptr->spack_offs);
    });
    xds->Write32le(0xdeadbeaf);
    return xds->Good();
}

bool MOLEBOX_PKG::UpdateHiddensOnly(FileInfo* finfo)
{
    bool hiddens_only = true;
    foobar::for_each(finfo->ls, [this, &hiddens_only](FileInfoPtr & ptr)
    {
        if (ptr->flags & FT_DIRECTORY)
        {
            hiddens_only = UpdateHiddensOnly(ptr.Get()) && hiddens_only;
        }
        else
        {
            hiddens_only = hiddens_only && !!(ptr->flags & SVFS_HIDDEN);
        }
    });
    if (hiddens_only)
    {
        finfo->flags |= SVFS_HIDDEN;
    }
    return hiddens_only;
}

void MOLEBOX_PKG::AppendInfos(FileInfo* i, std::vector<FileInfo*>& b)
{
    if (i->dir)
        b.push_back(i);

    if (i->flags & FT_DIRECTORY)
        for (int j = 0; j < i->ls.size(); ++j)
            AppendInfos(i->ls[j].Get(), b);
}

namespace
{
    struct FiLess
    {
        bool operator()(FileInfo* a, FileInfo* b)
        {
            return memcmp(a->sign, b->sign, 16) < 0;
        }
    };
}

size_t MOLEBOX_PKG::WriteMap(FileInfo* root, foobar::Stream xds)
{
    std::vector<FileInfo*> infos;
    AppendInfos(root, infos);
    std::sort(infos.begin(), infos.end(), FiLess());
    for (int i = 0; i < infos.size(); ++i)
        xds->Write32le(infos[i]->spack_offs);
    return infos.size();
}

MXBPAK_ERROR MOLEBOX_PKG::Finish(MOLEBOX_PAKNFO* paknfo)
{
    BLOWFISH cipher;
    SVFS_HEADER shdr;
    memset(&shdr, 0, sizeof(shdr));

    foobar::BufferDataStream ads;
    std::vector<uint8_t> b;

    ads.Write32le(123456789);

    if (stream_arr.size())
    {
        shdr.stream_ptr = ds->Tell() - package_base;
        shdr.stream_size = 0; /* compatibility */
        ds->Write("STRM", 4);
        ds->Write32le(stream_arr.size());
        ds->Write(&libhash::sha1_digest(&stream_arr[0], stream_arr.size() * 12)[0], 20);
        ds->Write(&stream_arr[0], stream_arr.size() * sizeof(stream_arr[0]));
    }

    cipher.SetupEncipher(skey);

    UpdateHiddensOnly(+root);
    root->flags = root->flags&~SVFS_HIDDEN;

    WriteDir(root.Get(), ads.Ref());

    shdr.map_roffs = ads.Tell();
    shdr.map_count = WriteMap(root.Get(), ads.Ref());

    ads.SwapBuffer(b);
    while (b.size() % 8) b.push_back(0);

    shdr.crc = Crc_32(0, &b[0], b.size());
    cipher.DoCipherCBCI(&b[0], b.size() / 8);

    int64_t start = ds->Tell();
    ds->Write(&b[0], b.size());

    shdr.tree_boffs = ds->Tell() - b.size(); // + crc
    shdr.tree_size  = b.size();
    shdr.root_roffs = root->spack_offs;
    shdr.base_boffs = ds->Tell() - package_base;

    memcpy(shdr.signature, "STELPACK", 8);
    cipher.DoCipher(shdr.signature, 1);
    cipher.DoCipher(shdr.signature, 1);
    cipher.DoCipherCBCI(&shdr, sizeof(shdr) / 8);

    paknfo->catalog = ds->Tell();
    paknfo->start = package_base;
    memset(paknfo->sha1, 0, 20);

    ds->Write(&shdr, sizeof(shdr));
    ds->Flush();

    paknfo->size = ds->Tell() - package_base;

    if (ds->Good())
        return MXBPAK_SUCCESS;
    else
        return Error(MXBPAK_ERROR_WRITE, "write file error");
}

MXBPAK_EXPORTABLE MXBPAK_ERROR Molebox_Pkg_Free(MOLEBOX_PKG* pkg)
{
    if (pkg)
        foobar::unrefe(pkg);
    return MXBPAK_SUCCESS;
}

const char* Molebox_Pkg_Error(MOLEBOX_PKG* pkg)
{
    return pkg->error.c_str();
}

struct MOLEBOX_CATALOG: foobar::Refcounted, ErrorHelper
{
    struct Record
    {
        int64_t offset;
        std::string name;
        uint8_t passwd[16];
        uint8_t sha1[20];
        int spos;
    };

    std::vector<Record> embedded;
    std::vector<Record> external;
};

MXBPAK_ERROR Molebox_Catalog_Create(MOLEBOX_CATALOG** catalog)
{
    foobar::RccPtr<MOLEBOX_CATALOG> ctlg = foobar::New;
    *catalog = foobar::refe(ctlg);
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Catalog_Internal(
    MOLEBOX_CATALOG* catalog,
    const char* name,
    int64_t catalog_offs)
{
    auto r = MOLEBOX_CATALOG::Record();

    r.offset = catalog_offs;
    r.name = name ? name : "";

    catalog->embedded.push_back(r);
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Catalog_External(
    MOLEBOX_CATALOG* catalog,
    const char* mask,
    const char* passwd, size_t passwd_len, unsigned opt)
{
    uint8_t key[16];

    if (!mask)
    {
        return catalog->Error(MXBPAK_ERROR_INVALID_ARGUMENT, "mask should be not null");
    }

    if (passwd)
    {
        std::pair<MXBPAK_ERROR, std::string> err = hash_password(passwd, passwd_len, opt, key);
        if (err.first)
            return catalog->Error(err.first, err.second);
    }

    auto r = MOLEBOX_CATALOG::Record();
    r.name = mask;
    memcpy(r.passwd, key, 16);
    catalog->external.push_back(r);

    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Catalog_Write(
    MOLEBOX_CATALOG* catalog,
    const wchar_t* container_name,
    int64_t* catalog_offs)
{
    foobar::BufferDataStream ads;

    ads.Skip(6 * 4);
    foobar::for_each(catalog->embedded, [&](MOLEBOX_CATALOG::Record & r)
    {
        r.spos = ads.Tell();
        ads.Write8((uint8_t)r.name.length());
        ads.Write(r.name.c_str(), r.name.length() + 1);
    });
    foobar::for_each(catalog->external, [&](MOLEBOX_CATALOG::Record & r)
    {
        r.spos = ads.Tell();
        ads.Write8((uint8_t)r.name.length());
        ads.Write(r.name.c_str(), r.name.length() + 1);
    });
    int64_t internal_table_offs = ads.Tell();
    foobar::for_each(catalog->embedded, [&](MOLEBOX_CATALOG::Record & r)
    {
        ads.Write32le(r.offset);
        ads.Write32le(r.spos);
    });
    int64_t external_table_offs = ads.Tell();
    foobar::for_each(catalog->external, [&](MOLEBOX_CATALOG::Record & r)
    {
        ads.Write32le(r.offset);
        ads.Write32le(r.spos);
        ads.Write(r.passwd, 16);
    });
    ads.Seek(0);
    ads.Write32le((uint32_t)internal_table_offs);
    ads.Write32le(catalog->embedded.size());
    ads.Write32le((uint32_t)external_table_offs);
    ads.Write32le(catalog->external.size());

    std::vector<uint8_t> buffer;
    ads.SwapBuffer(buffer);

    while (buffer.size() % 8 != 0) buffer.push_back(0);

    uint8_t sha1[20];
    libhash::sha1_digest(&buffer[0], buffer.size(), &sha1[0], 20);
    uint32_t crc32 = libhash::crc32_digest(&buffer[0], buffer.size());

    uint8_t key[16];
    libhash::md5_digest("g46dgsfet567etwh501bhsd-=352", 28, &key[0], 16);
    BLOWFISH cipher;
    cipher.SetupEncipher(key);
    cipher.DoCipherCBCI(&buffer[0], buffer.size() / 8);

    std::vector<uint8_t> descriptor;
    ads.Write32le(buffer.size());
    ads.Write32le(crc32);
    ads.SwapBuffer(descriptor);
    cipher.DoCipher(&descriptor[0], 1);

    auto ds = foobar::Win32Stream::Open(container_name, "r+");
    if (!ds->Good())
        return catalog->Error(MXBPAK_ERROR_FILE_DSNT_EXIST, ds->ErrorString());

    ds->Seek(0, SEEK_END);
    if (!ds->Good())
        return catalog->Error(MXBPAK_ERROR_WRITE, ds->ErrorString());

    if (catalog_offs)
        *catalog_offs = ds->Tell();

    ds->Write(descriptor);
    ds->Write(buffer);
    ds->Write(&sha1[0], 20);

    if (ds->Good())
        return MXBPAK_SUCCESS;
    else
        return catalog->Error(MXBPAK_ERROR_WRITE, ds->ErrorString());
}

MXBPAK_ERROR Molebox_Catalog_Free(MOLEBOX_CATALOG* catalog)
{
    if (catalog)
        foobar::unrefe(catalog);
    return MXBPAK_SUCCESS;
}

const char* Molebox_Catalog_Error(MOLEBOX_CATALOG* catalog)
{
    return catalog->error.c_str();
}

namespace
{
    struct char_iless : std::binary_function<const char*, const char*, bool>
    {
        bool operator()(const char* a, const char* b) const
        {
            return stricmp(a, b) < 0;
        }
    };
}

namespace
{
    typedef std::unique_ptr<MOLEBOX_CATALOG, MXBPAK_ERROR(MOLEBOX_CATALOG*)> MoleboxCatalogPtr;

    struct MoleboxCatalog
    {
        static std::unique_ptr<MoleboxCatalog> Create()
        {
            auto ret = std::unique_ptr<MoleboxCatalog>(new MoleboxCatalog);
            if (MXBPAK_SUCCESS == Molebox_Catalog_Create(&ret->ctlg))
                return ret;
            else
                throw std::runtime_error("failed to create package catalog");
        }

        void AddInternal(foobar::strarg_t name, int64_t catalog_offs)
        {
            if (MXBPAK_SUCCESS != Molebox_Catalog_Internal(ctlg, name.Cstr(), catalog_offs))
                throw std::runtime_error(Molebox_Catalog_Error(ctlg));
        }

        void AddExternal(foobar::strarg_t name, foobar::chars_t passwd, unsigned opt)
        {
            if (MXBPAK_SUCCESS != Molebox_Catalog_External(ctlg, name.Cstr(), begin(passwd), length_of(passwd), opt))
                throw std::runtime_error(Molebox_Catalog_Error(ctlg));
        }

        void Write(foobar::wcsarg_t filename, int64_t* offset)
        {
            if (MXBPAK_SUCCESS != Molebox_Catalog_Write(ctlg, filename.Cstr(), offset))
                throw std::runtime_error(Molebox_Catalog_Error(ctlg));
        }

        ~MoleboxCatalog()
        {
            Molebox_Catalog_Free(ctlg);
        }

        MOLEBOX_CATALOG* ctlg;
    };

    std::string normalize_size(uint64_t size)
    {
        if (size < 1024 * 10) return foobar::format("%d", size);
        if (size < 1024 * 1024 * 10) return foobar::format("%dK", size / 1024);
        if (size < 1024 * 1024 * 1024 * 10) return foobar::format("%dM", size / (1024 * 1024));
        return foobar::format("%dG", size / (1024 * 1024 * 1024));
    }

    bool can_register(std::string inname, std::string outname, std::vector<uint8_t> reginfo, MOLEBOX_NOTIFY* notify)
    {
        foobar::Path<wchar_t> fullpath = foobar::Path<wchar_t>(inname).Fullpath();

        if (1)
        {
            std::unique_ptr<PE_SOURCE,PE_BOOL(*)(PE_SOURCE*)> dllpe(Libpe_Open_File_W(fullpath.Cstr()), Libpe_Close);
            if (!dllpe.get()
                || Libpe_Type(dllpe.get()) != 32
                //|| Libpe_Is_DLL(dllpe.get()) != 1
                || Libpe_Find_Export_No(dllpe.get(), "DllRegisterServer") == PE_INVALID_NO)
                return false;
        }

        foobar::Popen p = foobar::Popen::Exec(L"mkreg.exe -s "+fullpath.Str());

        if (p->Good())
        {
            reginfo.clear();
            reginfo.push_back(0xff);
            reginfo.push_back(0xfe);
            foobar::LineReader lr = foobar::LineReader::Chain(p->Out());
            std::string line;

            while ( lr->NextLine(line) )
            {
                foobar::wcsarg_t w(line);
                reginfo.insert(reginfo.end(),(uint8_t*)w.Cstr(),(uint8_t*)(w.Cstr() + w.Length()));
                reginfo.push_back('\r');
                reginfo.push_back(0);
                reginfo.push_back('\n');
                reginfo.push_back(0);
            }

            if ( p->Good() && p->ExitCode() == 0 )
                return true;
        }

        return false;
    }

    void ntf_begin(MOLEBOX_NOTIFY* notify, foobar::wcsarg_t details)
    {
        if (notify)
            notify->f_begin(notify, details.Cstr());
    }

    void ntf_end(MOLEBOX_NOTIFY* notify, foobar::wcsarg_t details, bool succeeded)
    {
        if (notify)
            notify->f_end(notify, details.Cstr(), succeeded);
    }

    void ntf_info(MOLEBOX_NOTIFY* notify, foobar::wcsarg_t details)
    {
        if (notify)
            notify->f_info(notify, details.Cstr());
    }

    void ntf_error(MOLEBOX_NOTIFY* notify, foobar::wcsarg_t details, int err)
    {
        if (notify)
            notify->f_error(notify, details.Cstr(), err);
    }
}

MXBPAK_ERROR Molebox_Conf_Read(const wchar_t* config_name, XNODE* xnode_config_root, MOLEBOX_NOTIFY* notify)
{
    foobar::LineReader sr;
    auto ds = foobar::Win32Stream::Open(config_name, "r");

    if (!ds->Good())
    {
        ntf_error(notify, foobar::format("couldn't open file '%s'", foobar::c_right(config_name, 32)),
                  MXBPAK_ERROR_FILE_DSNT_EXIST);
        return MXBPAK_ERROR_FILE_DSNT_EXIST;
    }

    sr = foobar::LineReader::Chain(ds);

    enum { NONE, OPTIONS, FILES } section = NONE;

    libconf::Xnode root = libconf::Xnode(xnode_config_root, 1);
    libconf::Xnode opts = root;
    libconf::Xnode package = root.Append("package");

    package.Name() = "default";
    package["embedded"] = foobar::True;

    static const char* const bool_true_c[] =
    {
        "yes", "#yes", "1", "true", "#true"
    };
    auto bool_true = std::set<const char*, char_iless>(bool_true_c, foobar::end(bool_true_c));

    static const char* const bool_names_c[] =
    {
        "isexecutable", "dologging", "buildvreg",
        "relink", "compatible", "anticrack", "savextra",
        "autobundle", "saveiat", "saversc", "useact",
        "usecmdl", "useenvr", "useregfile", "hideall",
        "inject", "regfull", "use_regmask", "ncr", "compress", "encrypt"
    };
    auto bool_names = std::set<const char*, char_iless>(bool_names_c, foobar::end(bool_names_c));

    static const char* const str_names_c[] =
    {
        "target", "source", "pkgpwd", "extpwd", "extmask",
        "activator", "regfile", "envr", "cmdline", "regmask",
    };
    auto str_names = std::set<const char*, char_iless>(str_names_c, foobar::end(str_names_c));

    static const char* const int_names_c[] =
    {
        "conflict", "autoreg",
    };
    auto int_names = std::set<const char*, char_iless>(int_names_c, foobar::end(int_names_c));

    std::string s;
    while (sr->NextLine(s))
    {
        s = foobar::c_trim(s);
        if ( !s.length() || s[0] == ';') continue;
        if (s == "<end>") break;
        if (s == "<options>")
            section = OPTIONS;
        else if (s == "<files>")
            section = FILES;
        else if (s.length() != 0)
        {
            if (section == FILES)
            {
                const char* S = s.c_str();
                libconf::Xnode file = package.Append("file");
                if (*S == '!')
                {
                    file["hidden"] = foobar::True;
                    ++S;
                }
                else if (*S == '#')
                {
                    file["hidden"] = foobar::False;
                    ++S;
                }
                else if (*S == '*')
                {
                    ++S;
                }

                if (*S == '~' && (S[1] == '0' || S[1] == '1' || S[1] == '2' || S[1] == '*'))
                {
                    if (S[1] != '*')
                    {
                        file["conflict"] = S[1] - '0';
                    }
                    S += 2;
                }

                s = std::string(S);
                auto pos = s.find(';');
                if (pos != s.npos)
                {
                    file["rename"] = s.substr(pos + 1);
                    file.Name() = s.substr(0, pos);
                }
                else
                    file.Name() = s;
            }
            else if (section == OPTIONS)
            {
                auto pos = s.find('=');
                if (pos != s.npos)
                {
                    std::string name = foobar::c_trim(s.substr(0, pos));
                    std::string value = foobar::c_trim(s.substr(pos + 1));
                    if (str_names.find(name.c_str()) != str_names.end())
                        opts[foobar::c_lower(name)] = value;
                    else if (bool_names.find(name.c_str()) != bool_names.end())
                        opts[foobar::c_lower(name)] = foobar::Boolean(bool_true.find(value.c_str()) != bool_true.end());
                    else if (int_names.find(name.c_str()) != int_names.end())
                        opts[foobar::c_lower(name)] = strtol(value.c_str(), 0, 10);
                }
            }
            else
                ; // nothing
        }
    }

    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Conf_Write(const wchar_t* config_name, XNODE* conf, MOLEBOX_NOTIFY* notify)
{
    auto ds = foobar::Win32Stream::Open(config_name, "w+");
    if (!ds->Good())
    {
        ntf_error(notify, foobar::format("couldn't create file '%s'", foobar::c_right(config_name, 32)),
                  MXBPAK_ERROR_FILE_CANT_CREATE);
        return MXBPAK_ERROR_FILE_CANT_CREATE;
    }
    return MXBPAK_SUCCESS;
}

MXBPAK_ERROR Molebox_Conf_Pack(XNODE* conf, MOLEBOX_NOTIFY* notify)
{
    try
    {
        libconf::Xnode r = libconf::Xnode(conf, true);
        libconf::Xnode pkg = r.DownIf("package");

        bool target_exists = false;

        foobar::Path<wchar_t> outdir;
        foobar::Path<wchar_t> target;

        std::unique_ptr<MoleboxCatalog> catalog = MoleboxCatalog::Create();
        std::string mainexe;

        if (r["target"].Exists())
            target = foobar::Path<wchar_t>(r["target"].Str()).Fullpath();

        if (!target)
        {
            ntf_error(notify, "target file for packaging is not specified", MXBPAK_ERROR_CONFIG);
            return MXBPAK_ERROR_CONFIG;
        }

        if (r["outdir"].Exists())
            outdir = r["outdir"].Str();
        else if (target != foobar::None)
            outdir = target.Dirname();
        else
            outdir = foobar::Path<wchar_t>::Cwd();

        if (r["isexecutable"])
        {
            uint32_t flags = 0;
            foobar::Option<std::string> activator;
            foobar::Option<std::string> stub;
            foobar::Option<std::string> cmdline;
            foobar::Option<std::string> regmask;

            if (!r["source"].Exists())
            {
                ntf_error(notify, "source executable is not specified", MXBPAK_ERROR_CONFIG);
                return MXBPAK_ERROR_CONFIG;
            }

            mainexe = r["source"];

            if (r["dologging"].Bool() == foobar::True)
                flags |= LIBSTUB4_LOGGING;
            if (r["debug"].Bool() == foobar::True)
                flags |= LIBSTUB4_DEBUGCOR;
            if (r["relink"].Bool() == foobar::True)
                flags |= LIBSTUB4_RELINK;
            if (r["saversc"].Bool() == foobar::True)
                flags |= LIBSTUB4_SAVERSC;
            if (r["saveiat"].Bool() == foobar::True)
                flags |= LIBSTUB4_SAVEIAT;
            if (r["saveiat"].Bool() == foobar::True)
                flags |= LIBSTUB4_SAVEIAT;
            if (r["saveiat"].Bool() == foobar::True)
                flags |= LIBSTUB4_SAVEIAT;
            if (r["usevreg"].Bool() == foobar::True)
                flags |= LIBSTUB4_RGS;
            if (r["regfull"].Bool() == foobar::True)
                flags |= LIBSTUB4_RGS | LIBSTUB4_RGS_FULL;

            if (r["activator"].Exists() && (!r["useact"].Exists() || r["useact"].Bool()))
            {
                activator = r["activator"].Str();
                flags |= LIBSTUB4_EMBACT;
            }

            if (r["cmdlie"].Exists() && (!r["usecmdl"].Exists() || r["usecmdl"].Bool()))
            {
                cmdline = r["cmdline"].Str();
            }

            if (r["regmask"].Exists()
                && (!r["use_regmask"].Exists() || r["use_regmask"].Bool())
                && (!r["useregmask"].Exists() || r["useregmask"].Bool()))
            {
                regmask = r["regmask"].Str();
            }

            if (r["inject"].Bool() == foobar::True)
                flags |= LIBSTUB4_INJECT;
            if (r["anticrack"].Bool() == foobar::True)
                flags |= LIBSTUB4_ANTICRACK;
            if (r["anticrack"].Bool() == foobar::True)
                flags |= LIBSTUB4_ANTICRACK;
            if (r["savextra"].Bool() == foobar::True)
                flags |= LIBSTUB4_SAVEXTRA;
            if (r["rawextra"].Bool() == foobar::True)
                flags |= LIBSTUB4_RAWEXTRA;

            flags |= LIBSTUB4_HWID;

            if (r["stub"].Exists())
                stub = r["stub"].Str();

            struct MyLogger
            {
                LIBSTUB4_LOGGER l;
                MOLEBOX_NOTIFY* ntf;
                static void println(LIBSTUB4_LOGGER* l, const wchar_t* text)
                {
                    ntf_info(((MyLogger*)l)->ntf, text);
                }
                static void error(LIBSTUB4_LOGGER* l, const wchar_t* text)
                {
                    ntf_error(((MyLogger*)l)->ntf, text, MXBPAK_ERROR_INTERNAL);
                }
            }
            logger = {{&MyLogger::println, &MyLogger::error}, notify };

            if (!LibStub4_Create(flags,
                                 target.Str().c_str(),
                                 (stub ? foobar::wcsarg_t(*stub).Cstr() : 0),
                                 foobar::wcsarg_t(mainexe).Cstr(),
                                 (activator ? foobar::wcsarg_t(*activator).Cstr() : 0),
                                 (cmdline ? foobar::wcsarg_t(*cmdline).Cstr() : 0),
                                 (regmask ? foobar::wcsarg_t(*regmask).Cstr() : 0),
                                 0, 0,
                                 &logger.l))
                throw std::runtime_error("could noty generate new executable:" +
                                         foobar::strarg_t(Libstub4_Error_String()).Str());
            target_exists = true;
        }

        int64_t package_dir_offs = 0;

        for (; pkg; pkg = pkg.NextIf("package"))
        {
            foobar::Stream ds;
            foobar::Option<std::string> passwd;

            std::string name = pkg.Name();

            if (pkg["password"].Exists())
                passwd = pkg["password"].Str();

            bool embedded = pkg["embedded"].Bool();
            bool all_is_activex = pkg["treatallasocx"].Bool();
            bool register_onpack = pkg["autoreg"].IsEqualNocaseTo("onpack");

            foobar::Enum<MXBPAK_PKG_OPT> pkg_flags = MXBPAK_PKG_OPT_NONE;

            if (pkg["encrypted"])  pkg_flags |= MXBPAK_PKG_OPT_ENCRYPT;
            if (pkg["compressed"]) pkg_flags |= MXBPAK_PKG_OPT_COMPRESS;
            if (pkg["hidden"])     pkg_flags |= MXBPAK_PKG_OPT_HIDE;

            foobar::Path<wchar_t> package_name;

            if (embedded || !target_exists)
            {
                package_name = target;
                if (!target_exists)
                {
                    foobar::Stream container = foobar::Win32Stream::Open(target, "w+P");
                    if (!container->Good())
                    {
                        ntf_error(notify, foobar::format("couldn't create file '%s'", foobar::c_right(target, 32)),
                                  MXBPAK_ERROR_FILE_CANT_CREATE);
                        return MXBPAK_ERROR_FILE_CANT_CREATE;
                    }
                    //container->Write("\0\0\0\0\0\0\0\0", 8);
                    package_dir_offs = container->Tell();
                    container->Write("\0\0\0\0\0\0\0\0", 8);
                }
            }
            else
            {
                package_name = outdir.Append(name);
            }

            ntf_begin(notify,
                      foobar::format(L"package %?",
                                     foobar::c_right(package_name, 48)));

            std::unique_ptr<MoleboxPkg> pak = MoleboxPkg::Start(package_name.Cstr(),
                                              pkg_flags | MXBPAK_PKG_OPT_APPEND,
                                              passwd );

            for (libconf::Xnode file = pkg.DownIf("file"); file; file = file.NextIf("file"))
            {
                foobar::Enum<MXBPAK_FILE_OPT> file_flags = MXBPAK_FILE_OPT_NONE;
                if (file["encrypted"].Exists())
                    file_flags |= file["encrypted"] ? MXBPAK_FILE_OPT_ENCRYPT : MXBPAK_FILE_OPT_DNT_ENCRYPT;
                if (file["compressed"].Exists())
                    file_flags |= file["compressed"] ? MXBPAK_FILE_OPT_COMPRESS : MXBPAK_FILE_OPT_DNT_COMPRESS;
                if (file["hidden"].Exists())
                    file_flags |= file["hidden"] ? MXBPAK_FILE_OPT_HIDE : MXBPAK_FILE_OPT_DNT_HIDE;

                std::string inname = file.Name();
                std::string outname = file["rename"].Str();
                if (outname.length() == 0) outname = inname;
                inname = foobar::c_replace(inname, "/", "\\");
                inname = foobar::c_replace(inname, "\\\\", "\\");
                outname = foobar::c_replace(outname, "/", "\\");
                outname = foobar::c_replace(outname, "\\\\", "\\");

                pak->File(outname, file_flags);
                pak->Copy(inname);
                unsigned percent = pak->Commit();
                // notify here

                ntf_begin(notify,
                          foobar::format("[%3d%%] %? -> %?",
                                         percent,
                                         foobar::w_right(inname, 40),
                                         foobar::w_right(outname, 30)));

                if (all_is_activex || file["activex"])
                {
                    std::vector<uint8_t> reginfo;
                    if ( 0 && can_register(inname, outname, reginfo, notify))
                    {
                        pak->Stream(1, 0, &reginfo[0], reginfo.size());

                        ntf_info(notify,
                                 foobar::format("%s is a COM-component",
                                                foobar::w_right(outname, 30)));
                        ntf_info(notify,
                                 "windows registry runtime patch added");
                    }
                }
                ntf_end(notify, foobar::None, true);
            }

            MoleboxPkg::Info paknfo;
            pak->Finish(paknfo);

            if (embedded)
                catalog->AddInternal(name, paknfo.catalog);
            else
                catalog->AddExternal(name,
                                     passwd,
                                     pkg_flags);
            ntf_end(notify,
                    foobar::format("successfully packaged, total size %?",
                                   normalize_size(paknfo.size)),
                    1);
        }

        if (r["isexecutable"])
        {
            STB_INFO2 nfo;
            int64_t offset;
            catalog->Write(target.Cstr(), &offset);
            if (!LibStub4_Read_Nfo(target.Cstr(), &nfo, sizeof(nfo)))
                throw std::runtime_error("could not update target file");
            nfo.catalog = (uint32_t)offset;
            if (!LibStub4_Write_Nfo(target.Cstr(), &nfo, sizeof(nfo)))
                throw std::runtime_error("could not update target file");
        }

        return MXBPAK_SUCCESS;
    }
    catch (MoleboxException& e)
    {
        ntf_error(notify, e.what(), e.Error());
        return e.Error();
    }
    catch (std::exception& e)
    {
        ntf_error(notify, e.what(), MXBPAK_ERROR_INTERNAL);
        return MXBPAK_ERROR_INTERNAL;
    }
}

